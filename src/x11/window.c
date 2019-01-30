/*------------------------------------------------------------------- HEADERS */

#include <GL/glx.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>
#include <stdlib.h>
#include "window.h"

/*-------------------------------------------------------------------- MACROS */

#define HT_EVENT_MASK (FocusChangeMask | StructureNotifyMask)
#define HT_INPUT_QUEUE_BITS  4
#define HT_INPUT_QUEUE_SIZE (1 << HT_INPUT_QUEUE_BITS)
#define HT_INPUT_QUEUE_PREV(index) (((index) - 1) & (HT_INPUT_QUEUE_SIZE - 1))
#define HEAD_MOUSE_BUTTON(window) (window)->hid.mouse.button[(window)->hid.head]
#define HEAD_MOUSE_X(window)      (window)->hid.mouse.x[(window)->hid.head]
#define HEAD_MOUSE_Y(window)      (window)->hid.mouse.y[(window)->hid.head]
#define MOUSE_BUTTON(window)      (window)->hid.mouse.button[(window)->hid.tail]
#define MOUSE_X(window)           (window)->hid.mouse.x[(window)->hid.tail]
#define MOUSE_Y(window)           (window)->hid.mouse.y[(window)->hid.tail]
#define PREV_MOUSE_X(window)\
  (window)->hid.mouse.x[HT_INPUT_QUEUE_PREV((window)->hid.tail)]
#define PREV_MOUSE_Y(window)\
  (window)->hid.mouse.y[HT_INPUT_QUEUE_PREV((window)->hid.tail)]
#define PREV_MOUSE_BUTTON(window)\
  (window)->hid.mouse.button[HT_INPUT_QUEUE_PREV((window)->hid.tail)]

#define HANDLE_ERROR(func, result)\
  htHandleError(__FILE__, func, __LINE__, result)
#define HT_HANDLE_EVENT(window, callback) if (callback) (callback)(window)
#define INIT_GL_DEFAULTS(window)\
  (window)->gl.color          = HT_DEFAULT_GL_COLOR_BUFFER;\
  (window)->gl.red            = HT_DEFAULT_GL_RGBA_CHANNEL;\
  (window)->gl.green          = HT_DEFAULT_GL_RGBA_CHANNEL;\
  (window)->gl.blue           = HT_DEFAULT_GL_RGBA_CHANNEL;\
  (window)->gl.alpha          = HT_DEFAULT_GL_RGBA_CHANNEL;\
  (window)->gl.depth          = HT_DEFAULT_GL_DEPTH_BUFFER;\
  (window)->gl.stencil        = HT_DEFAULT_GL_STENCIL_BUFFER;\
  (window)->gl.accum          = HT_DEFAULT_GL_ACCUM_BUFFER;\
  (window)->gl.aux_buffers    = HT_DEFAULT_GL_AUX_BUFFERS;\
  (window)->gl.sample_buffers = HT_DEFAULT_GL_SAMPLE_BUFFERS;\
  (window)->gl.samples        = HT_DEFAULT_GL_SAMPLES;\
  (window)->gl.double_buffer  = HT_DEFAULT_GL_DOUBLE_BUFFERING;\
  (window)->gl.accelerated    = HT_DEFAULT_GL_ACCELERATED;\
  (window)->gl.stereo         = HT_DEFAULT_GL_STEREO;\
  (window)->gl.pixel_type     = HT_DEFAULT_GL_PIXEL_TYPE;\
  (window)->gl.swap_interval  = HT_DEFAULT_GL_SWAP_INTERVAL;\
  (window)->gl.profile        = HT_DEFAULT_GL_PROFILE;\
  (window)->gl.major          = HT_DEFAULT_GL_MAJOR_VERSION;\
  (window)->gl.minor          = HT_DEFAULT_GL_MINOR_VERSION;\
  (window)->gl.backing_store  = HT_DEFAULT_GL_BACKING_STORE

#ifndef HT_DISABLE_DEBUG
#define ASSERT(exp, func, result) if (!(exp)) HANDLE_ERROR(func, result)
#define GUID 0x1234
#define VALID_WINDOW(window) ((window) && (window)->uid == GUID)
#else
#define ASSERT(exp, func, result) (void) func
#endif

#define LOAD_GLX(type, name)\
  const type name = (type) glXGetProcAddress((const GLubyte*) #name)
#define MOVE_RESIZE_WINDOW(dpy, window)\
  XMoveResizeWindow(\
    (dpy),\
    (window)->win,\
    (window)->info.x,\
    (window)->info.y,\
    (window)->info.width,\
    (window)->info.height)

/*------------------------------------------------------------------- STRUCTS */

struct HTWindow {
  struct {
    struct {
      short         x[HT_INPUT_QUEUE_SIZE];
      short         y[HT_INPUT_QUEUE_SIZE];
      unsigned char button[HT_INPUT_QUEUE_SIZE];
    } mouse;
    int             id[HT_INPUT_QUEUE_SIZE];
    unsigned char   page[HT_INPUT_QUEUE_SIZE];
    unsigned        head: HT_INPUT_QUEUE_BITS;
    unsigned        tail: HT_INPUT_QUEUE_BITS;
#if HT_INPUT_QUEUE_BITS << 1 != 32
    const int reserved: (sizeof (int) << 3) - (HT_INPUT_QUEUE_BITS << 1);
#endif
    int opcode;
  } hid;
  struct {
    HTEventHandler close;    /* Window was signaled to close       */
    HTEventHandler draw;     /* Window was signaled to redraw      */
    HTEventHandler focus;    /* Window was focused/unfocused       */
    HTEventHandler minimize; /* Window was minimized/restored      */
    HTEventHandler move;     /* Window was repositioned            */
    HTEventHandler resize;   /* Window was resized                 */
    HTEventHandler keyboard; /* Keyboard was pressed/release       */
    HTEventHandler mouse;    /* Mouse was moved/clicked            */
    HTEventHandler gamepad;  /* Gamepad was pressed/released/moved */
  } event;
  struct {
    GLXContext context;         /* GLX OpenGL context                    */
    unsigned color:          6; /* 0 -  32: RGBA buffer size             */
    unsigned red:            4; /* 0 -   8: Red bits                     */
    unsigned green:          4; /* 0 -   8: Green bits                   */
    unsigned blue:           4; /* 0 -   8: Blue bits                    */
    unsigned alpha:          4; /* 0 -   8: Alpha bits                   */
    unsigned depth:          6; /* 0 -  32: Depth bits                   */
    unsigned stencil:        4; /* 0 -   8: Stencil bits                 */
    unsigned accum:          8; /* 0 - 128: RGBA Accum bits              */
    unsigned aux_buffers:    3; /* 0 -   4: Auxilary buffer count        */
    unsigned sample_buffers: 1; /* 0 -   1: Sample buffer count          */
    unsigned samples:        5; /* 0 -  16: MSAA sample count            */
    unsigned double_buffer:  1; /* 0: Single Buffer,    1: Double Buffer */
    unsigned accelerated:    1; /* 0: No Acceleration,  1: Acceleration  */
    unsigned stereo:         1; /* 0: Monoscopic,       1: Stereoscopic  */
    unsigned pixel_type:     1; /* 0: Color Index,      1: True Color    */
    unsigned swap_interval:  1; /* 0: Vsync Disabled,   1: Vsync Enabled */
    unsigned profile:        1; /* 0: Legacy Profile,   1: Core Profile  */
    unsigned major:          4; /* 0 -   9: OpenGL major version         */
    unsigned minor:          4; /* 0 -   9: OpenGL minor version         */
    unsigned backing_store:  1; /* 0: No Backing Store, 1: Backing Store */
  } gl;
  struct {
    int x:               14; /* X position of top-left corner */
    int y:               14; /* Y position of top-left corner */
    unsigned style:       4; /* Style mask of the window      */
    unsigned width:      13; /* Width of the content area     */
    unsigned height:     13; /* Height of the content area    */
    unsigned focus:       1; /* Window is currently in focus  */
    unsigned fullscreen:  1; /* Window is in fullscreen mode  */
  } info;
  unsigned char* user;   /* Pointer to user-supplied data */
  Window win; /* ID of the X11 window          */
#ifndef HT_DISABLE_DEBUG
  unsigned uid; /* Used to verify that the window was properly initialized */
#endif
};

/*---------------------------------------------------------- STATIC VARIABLES */

static HTWindowErrorCallback ht_error_handler;
static Display* dpy;    /* Display connection for X11 windows */
static Display* xi_dpy; /* Display connection for XInput      */

/*----------------------------------------------------------------- FUNCTIONS */

static int
htHandleError(const char* file, const char* func, unsigned line, int result) {
  if (ht_error_handler) {
    htErrorInfo info = {0};
    info.file = (char*) file;
    info.function = (char*) func;
    info.line = line;
    info.result = result;
    ht_error_handler(&info);
  }
  return result;
}

static void
htReadMouseButton(HTWindow* window, const XIRawEvent* raw) {
  window->hid.page[window->hid.tail] = HT_GD_USAGE_MOUSE;
  MOUSE_BUTTON(window) &= ~(1 << (raw->detail - 1));
  MOUSE_BUTTON(window) |=
    (raw->evtype == XI_RawButtonPress) << (raw->detail - 1);
  ++window->hid.tail;
}

static void
htReadMouseXY(HTWindow* window, const XIRawEvent* raw) {
  window->hid.page[window->hid.tail] = HT_GD_USAGE_MOUSE;
  MOUSE_X(window) = raw->raw_values[0];
  MOUSE_Y(window) = raw->raw_values[1];
  ++window->hid.tail;
}

static int
htIsRelative(HTWindow* window) {
  int i = 0;
  const XIDeviceInfo* device =
    XIQueryDevice(xi_dpy, window->hid.id[window->hid.head], &i);
  for (i = 0; device->classes[i]->type != XIValuatorClass; ++i);
  return ((XIValuatorClassInfo*) device->classes[i])->mode == XIModeRelative;
}

static int
htPollRawInput(HTWindow* window) {
  const char* func = "htPollRawInput";
  XEvent event = {0};
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(xi_dpy, func, HT_ERROR_WINDOW_SERVER);
  /* Dequeue raw input events */
  while (XPending(xi_dpy)) {
    XIRawEvent* raw = NULL;
    XNextEvent(xi_dpy, &event);
    if (event.xcookie.type != GenericEvent ||
        event.xcookie.extension != window->hid.opcode ||
        !XGetEventData(xi_dpy, &event.xcookie)) {
      continue;
    }
    raw = (XIRawEvent*) event.xcookie.data;
    window->hid.id[window->hid.tail] = raw->deviceid;
    /* Store previous states in tail since not all states have new values */
    MOUSE_BUTTON(window) = PREV_MOUSE_BUTTON(window);
    MOUSE_X(window)      = PREV_MOUSE_X(window);
    MOUSE_Y(window)      = PREV_MOUSE_Y(window);
    switch (raw->evtype) {
      case XI_RawButtonPress:
      case XI_RawButtonRelease: htReadMouseButton(window, raw); break;
      case XI_RawMotion:        htReadMouseXY(window, raw);     break;
      default: break;
    }
    XFreeEventData(dpy, &event.xcookie);
  }
  return HT_ERROR_NONE;
}

static void
htShouldCloseDisplay() {
  Window root = DefaultRootWindow(dpy);
  Window parent = 0;
  Window* children = 0;
  unsigned count = 0;
  if (XQueryTree(dpy, root, &root, &parent, &children, &count) && count < 3) {
    /* Close display connection for client windows */
    XFlush(dpy);
    XCloseDisplay(dpy);
    dpy = NULL;
  }
}

static int
htCreateGLPixelFormat(HTWindow* window, GLXFBConfig** fbc) {
  const char* func = "htCreateGLPixelFormat";
  const int caveat[] = {GLX_DONT_CARE, GLX_NONE};
  int pfa[] = {
    GLX_RED_SIZE,         -1,
    GLX_GREEN_SIZE,       -1,
    GLX_BLUE_SIZE,        -1,
    GLX_ALPHA_SIZE,       -1,
    GLX_DEPTH_SIZE,       -1,
    GLX_STENCIL_SIZE,     -1,
    GLX_ACCUM_RED_SIZE,   -1,
    GLX_ACCUM_GREEN_SIZE, -1,
    GLX_ACCUM_BLUE_SIZE,  -1,
    GLX_ACCUM_ALPHA_SIZE, -1,
    GLX_AUX_BUFFERS,      -1,
    GLX_SAMPLE_BUFFERS,   -1,
    GLX_SAMPLES,          -1,
    GLX_DOUBLEBUFFER,     -1,
    GLX_CONFIG_CAVEAT,    -1,
    GLX_STEREO,           -1,
    None};
  /* Overwrite default pixel format attribtues with user values */
  int count = 0;
  unsigned i = 1; /* Index of first rewritable element */
  pfa[i +  0] = window->gl.red;
  pfa[i +  2] = window->gl.green;
  pfa[i +  4] = window->gl.blue;
  pfa[i +  6] = window->gl.alpha;
  pfa[i +  8] = window->gl.depth;
  pfa[i + 10] = window->gl.stencil;
  pfa[i + 12] = window->gl.accum >> 2;
  pfa[i + 14] = window->gl.accum >> 2;
  pfa[i + 16] = window->gl.accum >> 2;
  pfa[i + 18] = window->gl.accum >> 2;
  pfa[i + 20] = window->gl.aux_buffers;
  pfa[i + 22] = window->gl.sample_buffers;
  pfa[i + 24] = window->gl.samples;
  pfa[i + 26] = window->gl.double_buffer;
  pfa[i + 28] = caveat[window->gl.accelerated];
  pfa[i + 30] = window->gl.stereo;
  *fbc = glXChooseFBConfig(dpy, DefaultScreen(dpy), pfa, &count);
  if (!*fbc) return HANDLE_ERROR(func, HT_ERROR_GL_PIXEL_FORMAT_NONE);
  return HT_ERROR_NONE;
}

static void
htSetSwapInterval(HTWindow* window) {
  /* Set swap interval for vsync */
#ifdef GLX_EXT_swap_control
  LOAD_GLX(PFNGLXSWAPINTERVALEXTPROC, glXSwapIntervalEXT);
  const int interval = window->gl.swap_interval;
  if (glXSwapIntervalEXT) glXSwapIntervalEXT(dpy, window->win, interval);
#elif defined GLX_MESA_swap_control
  LOAD_GLX(PFNGLXSWAPINTERVALMESAPROC, glXSwapIntervalMESA);
  if (glXSwapIntervalMESA) glXSwapIntervalMESA(window->gl.swap_interval);
#endif
}

static void
htConfigureNotify(HTWindow* window, XEvent* event) {
  if (window->info.width  != event->xconfigure.width ||
      window->info.height != event->xconfigure.height) {
    window->info.width  = event->xconfigure.width;
    window->info.height = event->xconfigure.height;
    HT_HANDLE_EVENT(window, window->event.resize);
  } else {
    window->info.x = event->xconfigure.x;
    window->info.y = event->xconfigure.y;
    HT_HANDLE_EVENT(window, window->event.move);
  }
}

int
htCreateWindow(
    HTWindow** window, short x, short y, unsigned short w, unsigned short h) {
  const char* func = "htCreateWindow";
  Atom delete = 0;
  XWindowAttributes attribute = {0};
  XSizeHints hint = {0};
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(!*window, func, HT_ERROR_INVALID_ARGUMENT);
  if (!dpy) {
    /* Open connection to X server */
    dpy = XOpenDisplay(NULL);
    if (!dpy) return HANDLE_ERROR(func, HT_ERROR_WINDOW_SERVER);
  }
  *window = calloc(1, sizeof (HTWindow));
  if (!*window) return HANDLE_ERROR(func, HT_ERROR_MEMORY_ALLOCATION);
  (*window)->win = XCreateSimpleWindow(
    dpy, DefaultRootWindow(dpy), x, y, w, h, 1, 0, 0);
  if (!(*window)->win) return HANDLE_ERROR(func, HT_ERROR_WINDOW_SERVER);
  delete = XInternAtom(dpy, "WM_DELETE_WINDOW", True);
  XSetWMProtocols(dpy, (*window)->win, &delete, 1);
  /* Set hints to ensure window is positioned and sized correctly */
  hint.flags  = PPosition | PSize;
  hint.x      = x;
  hint.y      = y;
  hint.width  = w;
  hint.height = h;
  XSetNormalHints(dpy, (*window)->win, &hint);
  XSelectInput(dpy, (*window)->win, HT_EVENT_MASK);
  XMapRaised(dpy, (*window)->win);
  /* Force X to write buffered requests */
  XFlush(dpy);
  /* Initialize OpenGL context defaults */
  INIT_GL_DEFAULTS(*window);
#ifndef HT_DISABLE_DEBUG
  /* Set GUID for argument validation */
  (*window)->uid = GUID;
#endif
  /* Store window info */
  XGetWindowAttributes(dpy, (*window)->win, &attribute);
  /* TODO: Value shouldn't be 0 because of title bar/dock */
  (*window)->info.x      = attribute.x;
  (*window)->info.y      = attribute.y;
  (*window)->info.width  = attribute.width;
  (*window)->info.height = attribute.height;
  return HT_ERROR_NONE;
}

int
htCreateGLContext(HTWindow* window) {
  const char* func = "htCreateGLContext";
  LOAD_GLX(PFNGLXCREATECONTEXTATTRIBSARBPROC, glXCreateContextAttribsARB);
  const GLXContext prev = glXGetCurrentContext();
  GLXFBConfig* fbc = NULL;
  int value = 0;
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(!window->gl.context, func, HT_ERROR_GL_CONTEXT_CREATION);
  ASSERT(dpy, func, HT_ERROR_WINDOW_SERVER);
  if (htCreateGLPixelFormat(window, &fbc) != HT_ERROR_NONE) {
    return HANDLE_ERROR(func, HT_ERROR_GL_CONTEXT_CREATION);
  }
  /* Create OpenGL context */
  if (glXCreateContextAttribsARB) {
    int version[] = {
      GLX_CONTEXT_MAJOR_VERSION_ARB, -1,
      GLX_CONTEXT_MINOR_VERSION_ARB, -1,
      None};
    version[1] = window->gl.major;
    version[3] = window->gl.minor;
    window->gl.context = glXCreateContextAttribsARB(dpy, *fbc, 0, 1, version);
  } else if (window->gl.major < 3) {
    XVisualInfo* info = glXGetVisualFromFBConfig(dpy, *fbc);
    window->gl.context = glXCreateContext(dpy, info, 0, 1);
    XFree(info);
  } else {
    return HANDLE_ERROR(func, HT_ERROR_GL_CONTEXT_CREATION);
  }
  if (!(window->gl.context &&
      glXMakeCurrent(dpy, window->win, window->gl.context))) {
    XFree(fbc);
    glXMakeCurrent(dpy, -(prev != NULL) & window->win, prev);
    return HANDLE_ERROR(func, HT_ERROR_GL_CONTEXT_CREATION);
  }
  htSetSwapInterval(window);
  /* Overwrite user values with pixel format attributes obtained */
  glXGetFBConfigAttrib(dpy, *fbc, GLX_RED_SIZE, &value);
  window->gl.red = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_BLUE_SIZE, &value);
  window->gl.green = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_GREEN_SIZE, &value);
  window->gl.blue = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_ALPHA_SIZE, &value);
  window->gl.alpha = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_DEPTH_SIZE, &value);
  window->gl.depth = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_STENCIL_SIZE, &value);
  window->gl.stencil = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_ACCUM_RED_SIZE, &value);
  window->gl.accum = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_ACCUM_GREEN_SIZE, &value);
  window->gl.accum += value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_ACCUM_BLUE_SIZE, &value);
  window->gl.accum += value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_ACCUM_ALPHA_SIZE, &value);
  window->gl.accum += value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_AUX_BUFFERS, &value);
  window->gl.aux_buffers = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_SAMPLE_BUFFERS, &value);
  window->gl.sample_buffers = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_SAMPLES, &value);
  window->gl.samples = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_DOUBLEBUFFER, &value);
  window->gl.double_buffer = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_CONFIG_CAVEAT, &value);
  window->gl.accelerated = value == GLX_NONE;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_STEREO, &value);
  window->gl.stereo = value;
  glXGetFBConfigAttrib(dpy, *fbc, GLX_RENDER_TYPE, &value);
  window->gl.pixel_type = value == GLX_RGBA_BIT;
  window->gl.major = glGetString(GL_VERSION)[0] - '0';
  window->gl.minor = glGetString(GL_VERSION)[2] - '0';
  window->gl.profile = window->gl.major > 2;
  window->gl.color =
    window->gl.red + window->gl.green + window->gl.blue + window->gl.alpha;
  XFree(fbc);
  glXMakeCurrent(dpy, -(prev != NULL) & window->win, prev);
  return HT_ERROR_NONE;
}

int
htCreateInputManager(HTWindow* window) {
  const char* func = "htCreateInputManager";
  int error = 0;
  int count = 0;
  XIEventMask event_mask = {0};
  unsigned char raw_mask[XIMaskLen(XI_LASTEVENT)] = {0};
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(!xi_dpy, func, HT_ERROR_WINDOW_SERVER);
  if (!xi_dpy) {
    /* Open connection to X server */
    xi_dpy = XOpenDisplay(NULL);
    if (!xi_dpy) return HANDLE_ERROR(func, HT_ERROR_WINDOW_SERVER);
  }
  /* Raw input requires XInput 2.0 extension */
  if (!XQueryExtension(
        xi_dpy, "XInputExtension", &window->hid.opcode, &count, &error)) {
    HANDLE_ERROR(func, HT_ERROR_INPUT_MANAGER_CREATION);
  }
  /* Setup raw input events */
  event_mask.deviceid = XIAllMasterDevices;
  event_mask.mask_len = sizeof (raw_mask);
  event_mask.mask = raw_mask;
  XISetMask(event_mask.mask, XI_RawButtonPress);
  XISetMask(event_mask.mask, XI_RawButtonRelease);
  XISetMask(event_mask.mask, XI_RawMotion);
  /* Raw input must be sent to root or else XISelectEvents() returns BadValue */
  XISelectEvents(xi_dpy, DefaultRootWindow(xi_dpy), &event_mask, 1);
  return HT_ERROR_NONE;
}

int
htDestroyWindow(HTWindow** window) {
  const char* func = "htDestroyWindow";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(window && VALID_WINDOW(*window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(dpy, func, HT_ERROR_WINDOW_SERVER);
  XDestroyWindow(dpy, (*window)->win);
  htShouldCloseDisplay();
#ifndef HT_DISABLE_DEBUG
  (*window)->uid = 0;
#endif
  free(*window);
  *window = NULL;
  return HT_ERROR_NONE;
}

int
htDestroyGLContext(HTWindow* window) {
  const char* func = "htDestroyGLContext";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(dpy, func, HT_ERROR_WINDOW_SERVER);
  if (glXGetCurrentContext() == window->gl.context) glXMakeCurrent(dpy, 0, 0);
  glXDestroyContext(dpy, window->gl.context);
  return HT_ERROR_NONE;
}

int
htDestroyInputManager(HTWindow* window) {
  const char* func = "htDestroyInputManager";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(xi_dpy, func, HT_ERROR_UNINITIALIZED_INPUT_MANAGER);
  /* Close display conntect for raw input */
  XFlush(xi_dpy);
  XCloseDisplay(xi_dpy);
  xi_dpy = NULL;
  window->hid.opcode = 0;
  return HT_ERROR_NONE;
}

int
htSetCurrentGLContext(HTWindow* window) {
  const char* func = "htSetCurrentGLContext";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(dpy, func, HT_ERROR_WINDOW_SERVER);
  if (window) glXMakeCurrent(dpy, window->win, window->gl.context);
  else glXMakeCurrent(dpy, 0, 0);
  return HT_ERROR_NONE;
}

int
htSwapGLBuffers(HTWindow* window) {
  const char* func = "htSwapGLBuffers";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(dpy, func, HT_ERROR_WINDOW_SERVER);
  glXSwapBuffers(dpy, window->win);
  return HT_ERROR_NONE;
}

int
htSetWindowInteger(HTWindow* window, HTWindowAttribute type, int data) {
  const char* func = "htSetWindowInteger";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  switch (type) {
    case HT_GL_ACCELERATED:
      window->gl.accelerated = data != 0;
      break;
    case HT_GL_ACCUM_BUFFER:
      window->gl.accum = HT_MIN(data, HT_MAX_GL_ACCUM_BUFFER);
      break;
    case HT_GL_ALPHA:
      window->gl.alpha = HT_MIN(data, HT_MAX_GL_RGBA_CHANNEL);
      window->gl.color =
        window->gl.red + window->gl.green + window->gl.blue + window->gl.alpha;
      break;
    case HT_GL_AUX_BUFFERS:
      window->gl.aux_buffers = HT_MIN(data, HT_MAX_GL_AUX_BUFFERS);
      break;
    case HT_GL_BACKING_STORE:
      window->gl.backing_store = 0; /* Backing store unsupported with GLX */
      break;
    case HT_GL_BLUE:
      window->gl.blue = HT_MIN(data, HT_MAX_GL_RGBA_CHANNEL);
      window->gl.color =
        window->gl.red + window->gl.green + window->gl.blue + window->gl.alpha;
      break;
    case HT_GL_DEPTH_BUFFER:
      window->gl.depth = HT_MIN(data, HT_MAX_GL_DEPTH_BUFFER);
      break;
    case HT_GL_DOUBLE_BUFFERING:
      window->gl.double_buffer = data != 0;
      break;
    case HT_GL_GREEN:
      window->gl.green = HT_MIN(data, HT_MAX_GL_RGBA_CHANNEL);
      window->gl.color =
        window->gl.red + window->gl.green + window->gl.blue + window->gl.alpha;
      break;
    case HT_GL_MAJOR_VERSION:
      window->gl.major = HT_MIN(data, HT_MAX_GL_MAJOR_VERSION);
      window->gl.profile = window->gl.major > 2;
      break;
    case HT_GL_MINOR_VERSION:
      window->gl.minor = HT_MIN(data, HT_MAX_GL_MINOR_VERSION);
      break;
    case HT_GL_RED:
      window->gl.red = HT_MIN(data, HT_MAX_GL_RGBA_CHANNEL);
      window->gl.color =
        window->gl.red + window->gl.green + window->gl.blue + window->gl.alpha;
      break;
    case HT_GL_SAMPLE_BUFFERS:
      window->gl.sample_buffers = HT_MIN(data, HT_MAX_GL_SAMPLE_BUFFERS);
      break;
    case HT_GL_SAMPLES:
      window->gl.samples = HT_MIN(data, HT_MAX_GL_SAMPLES);
      break;
    case HT_GL_STENCIL_BUFFER:
      window->gl.stencil = HT_MIN(data, HT_MAX_GL_STENCIL_BUFFER);
      break;
    case HT_GL_STEREO:
      window->gl.stereo = data != 0;
      break;
    case HT_GL_SWAP_INTERVAL:
      window->gl.swap_interval = data != 0;
      break;
    case HT_WINDOW_HEIGHT:
      window->info.height = data & 0x3FFF;
      MOVE_RESIZE_WINDOW(dpy, window);
      break;
    case HT_WINDOW_STYLE:
      /* TODO: Window decorations managed by window manager, not X11 */
      window->info.style = HT_WINDOW_STYLE_DEFAULT;
      break;
    case HT_WINDOW_WIDTH:
      window->info.width = data & 0x3FFF;
      MOVE_RESIZE_WINDOW(dpy, window);
      break;
    case HT_WINDOW_X:
      window->info.x = data;
      MOVE_RESIZE_WINDOW(dpy, window);
      break;
    case HT_WINDOW_Y:
      window->info.y = data;
      MOVE_RESIZE_WINDOW(dpy, window);
      break;
    default:
      return HANDLE_ERROR(func, HT_ERROR_INVALID_ARGUMENT);
  }
  return HT_ERROR_NONE;
}

int
htSetWindowUntyped(
    HTWindow* window, HTWindowAttribute type, unsigned char* data) {
  const char* func = "htSetWindowUntyped";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  switch (type) {
    case HT_WINDOW_TITLE: XStoreName(dpy, window->win, (char*) data); break;
    case HT_WINDOW_USER:  window->user = data;                        break;
    default: return HANDLE_ERROR(func, HT_ERROR_INVALID_ARGUMENT);
  }
  return HT_ERROR_NONE;
}

int
htGetWindowInteger(HTWindow* window, HTWindowAttribute type, int* data) {
  const char* func = "htGetWindowInteger";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  switch (type) {
    case HT_GL_ACCELERATED:       *data = window->gl.accelerated;    break;
    case HT_GL_ACCUM_BUFFER:      *data = window->gl.accum;          break;
    case HT_GL_ALPHA:             *data = window->gl.alpha;          break;
    case HT_GL_AUX_BUFFERS:       *data = window->gl.aux_buffers;    break;
    case HT_GL_BACKING_STORE:     *data = window->gl.backing_store;  break;
    case HT_GL_BLUE:              *data = window->gl.blue;           break;
    case HT_GL_COLOR_BUFFER:      *data = window->gl.color;          break;
    case HT_GL_DEPTH_BUFFER:      *data = window->gl.depth;          break;
    case HT_GL_DOUBLE_BUFFERING:  *data = window->gl.double_buffer;  break;
    case HT_GL_GREEN:             *data = window->gl.green;          break;
    case HT_GL_MAJOR_VERSION:     *data = window->gl.major;          break;
    case HT_GL_MINOR_VERSION:     *data = window->gl.minor;          break;
    case HT_GL_PIXEL_TYPE:        *data = window->gl.pixel_type;     break;
    case HT_GL_PROFILE:           *data = window->gl.profile;        break;
    case HT_GL_RED:               *data = window->gl.red;            break;
    case HT_GL_SAMPLE_BUFFERS:    *data = window->gl.sample_buffers; break;
    case HT_GL_SAMPLES:           *data = window->gl.samples;        break;
    case HT_GL_STENCIL_BUFFER:    *data = window->gl.stencil;        break;
    case HT_GL_STEREO:            *data = window->gl.stereo;         break;
    case HT_GL_SWAP_INTERVAL:     *data = window->gl.swap_interval;  break;
    case HT_INPUT_MOUSE_BUTTON:   *data = HEAD_MOUSE_BUTTON(window); break;
    case HT_INPUT_MOUSE_RELATIVE: *data = htIsRelative(window);     break;
    case HT_INPUT_MOUSE_X:        *data = HEAD_MOUSE_X(window);      break;
    case HT_INPUT_MOUSE_Y:        *data = HEAD_MOUSE_Y(window);      break;
    case HT_WINDOW_HEIGHT:        *data = window->info.height;       break;
    case HT_WINDOW_STYLE:         *data = window->info.style;        break;
    case HT_WINDOW_WIDTH:         *data = window->info.width;        break;
    case HT_WINDOW_X:             *data = window->info.x;            break;
    case HT_WINDOW_Y:             *data = window->info.y;            break;
    default: return HANDLE_ERROR(func, HT_ERROR_INVALID_ARGUMENT);
  }
  return HT_ERROR_NONE;
}

int
htGetWindowUntyped(
    HTWindow* window, HTWindowAttribute type, unsigned char** data) {
  const char* func = "htGetWindowUntyped";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(data, func, HT_ERROR_INVALID_ARGUMENT);
  switch (type) {
    case HT_WINDOW_USER: *data = window->user; break;
    default: return HANDLE_ERROR(func, HT_ERROR_INVALID_ARGUMENT);
  }
  return HT_ERROR_NONE;
}

int
htSetEventHandler(HTWindow* window, HTEvent type, HTEventHandler callback) {
  const char* func = "htSetEventHandler";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  switch (type) {
    case HT_EVENT_CLOSE:    window->event.close    = callback; break;
    case HT_EVENT_DRAW:     window->event.draw     = callback; break;
    case HT_EVENT_FOCUS:    window->event.focus    = callback; break;
    case HT_EVENT_MOVE:     window->event.move     = callback; break;
    case HT_EVENT_MINIMIZE: window->event.minimize = callback; break;
    case HT_EVENT_RESIZE:   window->event.resize   = callback; break;
    case HT_EVENT_KEYBOARD: window->event.keyboard = callback; break;
    case HT_EVENT_MOUSE:    window->event.mouse    = callback; break;
    case HT_EVENT_GAMEPAD:  window->event.gamepad  = callback; break;
    default: return HANDLE_ERROR(func, HT_ERROR_INVALID_ARGUMENT);
  }
  return HT_ERROR_NONE;
}

int
htSetWindowErrorCallback(HTWindowErrorCallback callback) {
  ht_error_handler = callback;
  return HT_ERROR_NONE;
}

int
htPollWindowEvents(HTWindow* window) {
  const char* func = "htPollWindowEvents";
  XEvent event = {0};
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(dpy, func, HT_ERROR_WINDOW_SERVER);
  if (window->info.focus) htPollRawInput(window);
  while (XCheckWindowEvent(dpy, window->win, HT_EVENT_MASK, &event)) {
    switch (event.type) {
      case ConfigureNotify:
        htConfigureNotify(window, &event);
        break;
      case FocusIn:
      case FocusOut:
        window->info.focus = event.type == FocusIn;
        HT_HANDLE_EVENT(window, window->event.focus);
        break;
      default: break;
    }
  }
  /* XCheckWindowEvent does not dequeue ClientMessage events */
  if (XCheckTypedWindowEvent(dpy, window->win, ClientMessage, &event) &&
      *event.xclient.data.l ==
      (long) XInternAtom(dpy, "WM_DELETE_WINDOW", True) &&
      window->event.close) {
    window->event.close(window);
  }
  return HT_ERROR_NONE;
}

int
htPollInputEvents(HTWindow* window) {
  const char* func = "htPollInputEvents";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  for (; window->hid.head != window->hid.tail; ++window->hid.head) {
    switch (window->hid.page[window->hid.head]) {
      case HT_GD_USAGE_MOUSE:
        HT_HANDLE_EVENT(window, window->event.mouse);
        break;
      default: break;
    }
  }
  return HT_ERROR_NONE;
}
