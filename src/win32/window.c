/*------------------------------------------------------------------- HEADERS */

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <Windows.h>
#include <GL/gl.h>
#include <GL/wglext.h>
#include <stdlib.h>
#include "window.h"

/*-------------------------------------------------------------------- MACROS */

/* Missing macros on older versions of Windows */
#ifndef WM_NCXBUTTONDOWN
#define WM_NCXBUTTONDOWN 0x00AB
#endif
#ifndef WM_NCXBUTTONUP
#define WM_NCXBUTTONUP 0x00AC
#endif
#ifndef WM_NCXBUTTONDBLCLK
#define WM_NCXBUTTONDBLCLK 0x00AD
#endif
#ifndef WM_XBUTTONDOWN
#define WM_XBUTTONDOWN 0x020B
#endif
#ifndef WM_XBUTTONUP
#define WM_XBUTTONUP 0x020C
#endif
#ifndef WM_XBUTTONDBLCLK
#define WM_XBUTTONDBLCLK 0x020D
#endif
#ifndef MK_XBUTTON1
#define MK_XBUTTON1 0x0020
#endif
#ifndef MK_XBUTTON2
#define MK_XBUTTON2 0x0040
#endif
#ifndef XBUTTON1
#define XBUTTON1 0x0001
#endif
#ifndef XBUTTON2
#define XBUTTON2 0x0002
#endif
#ifndef GET_KEYSTATE_WPARAM
#define GET_KEYSTATE_WPARAM(wp) (LOWORD(wp))
#endif
#ifndef GET_XBUTTON_WPARAM
#define GET_XBUTTON_WPARAM(wp) (HIWORD(wp))
#endif
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int) (short) LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int) (short) HIWORD(lp))
#endif
#ifndef SM_CXVIRTUALSCREEN
#define SM_CXVIRTUALSCREEN 78
#endif
#ifndef SM_CYVIRTUALSCREEN
#define SM_CYVIRTUALSCREEN 79
#endif

#define HT_INPUT_QUEUE_BITS  4
#define HT_INPUT_QUEUE_SIZE (1 << HT_INPUT_QUEUE_BITS)
#define HT_INPUT_QUEUE_PREV(index) (((index) - 1) & (HT_INPUT_QUEUE_SIZE - 1))
#define MOUSE_X(window) (window)->hid.mouse.x[(window)->hid.tail]
#define MOUSE_Y(window) (window)->hid.mouse.y[(window)->hid.tail]
#define MOUSE_BUTTON(window) (window)->hid.mouse.button[(window)->hid.tail]
#define PREV_MOUSE_X(window)\
  (window)->hid.mouse.x[HT_INPUT_QUEUE_PREV((window)->hid.tail)]
#define PREV_MOUSE_Y(window)\
  (window)->hid.mouse.y[HT_INPUT_QUEUE_PREV((window)->hid.tail)]
#define PREV_MOUSE_BUTTON(window)\
  (window)->hid.mouse.button[HT_INPUT_QUEUE_PREV((window)->hid.tail)]
#ifdef WM_INPUT
#define MOUSE_UP_MASK(flag)\
  ((((flag) &   2) >> 1) |\
  (((flag)  &   8) >> 2) |\
  (((flag)  &  32) >> 3) |\
  (((flag)  & 128) >> 4) |\
  (((flag)  & 512) >> 5))
#define MOUSE_DOWN_MASK(flag)\
  (((flag) &   1)       |\
  (((flag) &   4) >> 1) |\
  (((flag) &  16) >> 2) |\
  (((flag) &  64) >> 3) |\
  (((flag) & 256) >> 4))
#endif

#define HANDLE_ERROR(func, result) HandleError(__FILE__, func, __LINE__, result)
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

#define STYLE_TITLED         (WS_CAPTION | WS_SYSMENU)
#define STYLE_MINIATURIZABLE (WS_MINIMIZEBOX)
#define STYLE_RESIZABLE      (WS_MAXIMIZEBOX | WS_SIZEBOX)

#define LOAD_WGL(type, name) const type name = (type) wglGetProcAddress(#name)
#define REMOVE_GL_CONTEXT(window)\
  wglMakeCurrent((window)->hdc, NULL);\
  wglDeleteContext((window)->gl.context);\
  (window)->gl.context = NULL
#define UPDATE_GL_INFO(window, format, pfd)\
  DescribePixelFormat((window)->hdc, (format), sizeof (pfd), &(pfd));\
  (window)->gl.color       = (pfd).cColorBits;\
  (window)->gl.red         = (pfd).cRedBits;\
  (window)->gl.green       = (pfd).cGreenBits;\
  (window)->gl.blue        = (pfd).cBlueBits;\
  (window)->gl.alpha       = (pfd).cAlphaBits;\
  (window)->gl.depth       = (pfd).cDepthBits;\
  (window)->gl.stencil     = (pfd).cStencilBits;\
  (window)->gl.accum       = (pfd).cAccumBits;\
  (window)->gl.aux_buffers = (pfd).cAuxBuffers;\
  (window)->gl.pixel_type  = (pfd).iPixelType == PFD_TYPE_RGBA;\
  (window)->gl.major       = glGetString(GL_VERSION)[0] - '0';\
  (window)->gl.minor       = glGetString(GL_VERSION)[2] - '0';\
  (window)->gl.profile     = (window)->gl.major > 2
#define STYLE_MASK(window)\
  (((window)->info.titled         & HT_WINDOW_TITLED) |\
  ( (window)->info.closable       & HT_WINDOW_CLOSABLE) |\
  ( (window)->info.miniaturizable & HT_WINDOW_MINIATURIZABLE) |\
  ( (window)->info.resizable      & HT_WINDOW_RESIZABLE))

/*------------------------------------------------------------------- STRUCTS */

struct HTWindow {
  struct {
    struct {
      short         x[HT_INPUT_QUEUE_SIZE];
      short         y[HT_INPUT_QUEUE_SIZE];
      unsigned char button[HT_INPUT_QUEUE_SIZE];
      struct {
        float x; /* X offset for a virtual screen with a 16-bit unit width  */
        float y; /* Y offset for a virtual screen with a 16-bit unit height */
      } virtual_offset;
    } mouse;
    unsigned char   page[HT_INPUT_QUEUE_SIZE];
    unsigned        head: HT_INPUT_QUEUE_BITS;
    unsigned        tail: HT_INPUT_QUEUE_BITS;
    unsigned        absolute: 1;
#if HT_INPUT_QUEUE_BITS << 1 != 32
    const int reserved: (sizeof (int) << 3) - (HT_INPUT_QUEUE_BITS << 1) - 1;
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
    HGLRC context;              /* WGL OpenGL context                    */
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
    int titled:           1; /* Window has title bar          */
    int closable:         1; /* Window has close button       */
    int miniaturizable:   1; /* Window has minimize button    */
    int resizable:        1; /* Window has resize button      */
    unsigned width:      13; /* Width of the content area     */
    unsigned height:     13; /* Height of the content area    */
    unsigned fullscreen:  1; /* Window is in fullscreen mode  */
    unsigned moving:      1; /* Window is currently moving    */
    const int padding:    4;
  } info;
  unsigned char*   user; /* Pointer to user-supplied data    */
  HDC   hdc;  /* Handle to a Win32 device context */
  HWND  win;  /* Handle to a Win32 window object  */
#ifndef HT_DISABLE_DEBUG
  unsigned uid; /* Used to verify that the window was properly initialized */
#endif
};

/*---------------------------------------------------------- STATIC VARIABLES */

static HTWindowErrorCallback ht_error_handler;

/*----------------------------------------------------------------- FUNCTIONS */

static int
HandleError(const char* file, const char* func, unsigned line, int result) {
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

static int
InitGLContextLegacy(HTWindow* window) {
  const char* func = "htInitGLContext";
  int format = 0;
  PIXELFORMATDESCRIPTOR pfd = {0};
  pfd.nSize = sizeof (pfd);
  pfd.nVersion = 1;
  pfd.dwFlags =
    PFD_DRAW_TO_WINDOW |
    PFD_SUPPORT_OPENGL |
    (PFD_DOUBLEBUFFER & -((int) window->gl.double_buffer)) |
    (PFD_STEREO & -((int) window->gl.stereo));
  pfd.cColorBits =
    (unsigned char) (window->gl.red + window->gl.green + window->gl.blue);
  pfd.cAlphaBits = (unsigned char) window->gl.alpha;
  pfd.cDepthBits = (unsigned char) window->gl.depth;
  pfd.cAccumBits = (unsigned char) window->gl.accum;
  pfd.cStencilBits = (unsigned char) window->gl.stencil;
  pfd.cAccumBits = (unsigned char) window->gl.accum;
  pfd.iLayerType = PFD_MAIN_PLANE;
  format = ChoosePixelFormat(window->hdc, &pfd);
  if (!SetPixelFormat(window->hdc, format, &pfd)) {
    return HANDLE_ERROR(func, HT_ERROR_GL_PIXEL_FORMAT_NONE);
  }
  window->gl.context = wglCreateContext(window->hdc);
  if (!window->gl.context) {
    return HANDLE_ERROR(func, HT_ERROR_GL_CONTEXT_CREATION);
  }
  wglMakeCurrent(window->hdc, window->gl.context);
  return format;
}

static void
SetWindowPosition(HTWindow* window) {
  const DWORD style = (DWORD) GetWindowLongPtr(window->win, GWL_STYLE);
  RECT frame = {0};
  GetClientRect(window->win, &frame);
  frame.left    = window->info.x;
  frame.right  += window->info.x;
  frame.top     = window->info.y;
  frame.bottom += window->info.y;
  AdjustWindowRect(&frame, style, FALSE);
  /* Prevent positioning title bar above (0, 0) */
  frame.bottom += -(frame.top < 0) & -frame.top;
  frame.top    += -(frame.top < 0) & -frame.top;
  SetWindowPos(
    window->win,
    HWND_TOP,
    frame.left,
    frame.top,
    frame.right - frame.left,
    frame.bottom - frame.top,
    SWP_NOZORDER | SWP_NOREPOSITION | SWP_FRAMECHANGED);
}

static void
SetWindowResolution(HTWindow* window) {
  RECT frame = {0};
  RECT content = {0};
  GetWindowRect(window->win, &frame);
  GetClientRect(window->win, &content);
  SetWindowPos(
    window->win,
    HWND_TOP,
    frame.left,
    frame.top,
    /* Add border/title bar size to new content area width and height */
    frame.right - frame.left - content.right + window->info.width,
    frame.bottom - frame.top - content.bottom + window->info.height,
    SWP_NOZORDER | SWP_NOREPOSITION | SWP_FRAMECHANGED);
}

static void
SetWindowStyle(HTWindow* window) {
  DWORD style = GetWindowLongPtr(window->win, GWL_STYLE) & ~WS_TILEDWINDOW;
  RECT frame = {0};
  frame.left   = window->info.x;
  frame.right  = window->info.x + window->info.width;
  frame.top    = window->info.y;
  frame.bottom = window->info.y + window->info.height;
  AdjustWindowRect(&frame, WS_TILEDWINDOW, FALSE);
  /* Prevent positioning title bar above (0, 0) */
  frame.bottom += -(frame.top < 0) & -frame.top;
  frame.top    += -(frame.top < 0) & -frame.top;
  if (window->info.titled) {
    const DWORD closable = -!window->info.closable & MF_DISABLED;
    EnableMenuItem(
      GetSystemMenu(window->win, FALSE), SC_CLOSE, MF_BYCOMMAND | closable);
    style |=
      (window->info.titled & STYLE_TITLED) |
      (window->info.miniaturizable & STYLE_MINIATURIZABLE) |
      (window->info.resizable & STYLE_RESIZABLE);
  } else {
    /* Position window where it would be with borders/title bar enabled */
    const DWORD border = (frame.right - frame.left - window->info.width) >> 1;
    const DWORD title_bar = frame.bottom - frame.top - window->info.height;
    frame.left   = window->info.x + border;
    frame.right  = frame.left + window->info.width;
    frame.top    = window->info.y + title_bar - border;
    frame.bottom = frame.top  + window->info.height;
  }
  SetWindowLongPtr(window->win, GWL_STYLE, style);
  SetWindowPos(
    window->win,
    HWND_TOP,
    window->info.x - ((window->info.x - frame.left) >> 1),
    frame.top,
    frame.right - frame.left,
    frame.bottom - frame.top,
  SWP_NOZORDER | SWP_NOREPOSITION | SWP_FRAMECHANGED);
}

static int
WMCreate(HTWindow* window, HWND hwnd, LPARAM lparam) {
  window = (HTWindow*) ((LPCREATESTRUCT) lparam)->lpCreateParams;
  SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) window);
  window->win = hwnd;
  window->hdc = GetDC(hwnd);
  /* Initialize OpenGL context defaults */
  window->gl.color          = HT_DEFAULT_GL_COLOR_BUFFER;
  window->gl.red            = HT_DEFAULT_GL_RGBA_CHANNEL;
  window->gl.green          = HT_DEFAULT_GL_RGBA_CHANNEL;
  window->gl.blue           = HT_DEFAULT_GL_RGBA_CHANNEL;
  window->gl.alpha          = HT_DEFAULT_GL_RGBA_CHANNEL;
  window->gl.depth          = HT_DEFAULT_GL_DEPTH_BUFFER;
  window->gl.stencil        = HT_DEFAULT_GL_STENCIL_BUFFER;
  window->gl.accum          = HT_DEFAULT_GL_ACCUM_BUFFER;
  window->gl.aux_buffers    = HT_DEFAULT_GL_AUX_BUFFERS;
  window->gl.sample_buffers = HT_DEFAULT_GL_SAMPLE_BUFFERS;
  window->gl.samples        = HT_DEFAULT_GL_SAMPLES;
  window->gl.double_buffer  = HT_DEFAULT_GL_DOUBLE_BUFFERING;
  window->gl.accelerated    = HT_DEFAULT_GL_ACCELERATED;
  window->gl.stereo         = HT_DEFAULT_GL_STEREO;
  window->gl.pixel_type     = HT_DEFAULT_GL_PIXEL_TYPE;
  window->gl.swap_interval  = HT_DEFAULT_GL_SWAP_INTERVAL;
  window->gl.profile        = HT_DEFAULT_GL_PROFILE;
  window->gl.major          = HT_DEFAULT_GL_MAJOR_VERSION;
  window->gl.minor          = HT_DEFAULT_GL_MINOR_VERSION;
  window->gl.backing_store  = HT_DEFAULT_GL_BACKING_STORE;
#ifndef HT_DISABLE_DEBUG
  /* Set GUID for argument validation */
  window->uid = GUID;
#endif
  window->info.width          = ((LPCREATESTRUCT) lparam)->cx;
  window->info.height         = ((LPCREATESTRUCT) lparam)->cy;
  window->info.titled         = 1;
  window->info.closable       = 1;
  window->info.miniaturizable = 1;
  window->info.resizable      = 1;
  window->hid.mouse.virtual_offset.x =
    GetSystemMetrics(SM_CXVIRTUALSCREEN) / 65535.0f;
  window->hid.mouse.virtual_offset.y =
    GetSystemMetrics(SM_CYVIRTUALSCREEN) / 65535.0f;
  return HT_ERROR_NONE;
}

static int
WMMove(HTWindow* window, LPARAM lparam) {
  window->info.x = LOWORD(lparam);
  window->info.y = HIWORD(lparam);
  HT_HANDLE_EVENT(window, window->event.move);
  return HT_ERROR_NONE;
}

static int
WMSize(HTWindow* window, LPARAM lparam) {
  window->info.width = LOWORD(lparam);
  window->info.height = HIWORD(lparam);
  HT_HANDLE_EVENT(window, window->event.resize);
  return HT_ERROR_NONE;
}

static int
WMPaint(HTWindow* window) {
  /* EndPaint will remove WM_PAINT message from queue */
  PAINTSTRUCT paint = {0};
  BeginPaint(window->win, &paint);
  EndPaint(window->win, &paint);
  return HT_ERROR_NONE;
}

#ifdef WM_INPUT
static int
WMInput(HTWindow* window, LPARAM lparam) {
  RAWINPUT raw = {0};
  unsigned size = sizeof (raw);
  GetRawInputData(
    (HRAWINPUT) lparam, RID_INPUT, &raw, &size, sizeof (raw.header));
  switch (raw.header.dwType) {
    case RIM_TYPEMOUSE:
      window->hid.absolute = raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE;
      window->hid.page[window->hid.tail] = HT_GD_USAGE_MOUSE;
      MOUSE_X(window) = (short) raw.data.mouse.lLastX;
      MOUSE_Y(window) = (short) raw.data.mouse.lLastY;
      /* Scale mouse position if screen is a virtual desktop */
      if (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) {
        MOUSE_X(window) = (short)
          ((unsigned) MOUSE_X(window) * window->hid.mouse.virtual_offset.x);
        MOUSE_Y(window) = (short)
          ((unsigned) MOUSE_Y(window) * window->hid.mouse.virtual_offset.y);
      }
      /* Set/Clear previous mouse button state using masks */
      MOUSE_BUTTON(window) = PREV_MOUSE_BUTTON(window);
      MOUSE_BUTTON(window) &= ~MOUSE_UP_MASK(raw.data.mouse.usButtonFlags);
      MOUSE_BUTTON(window) |= MOUSE_DOWN_MASK(raw.data.mouse.usButtonFlags);
      /* Increment tail if mouse changed position or button state */
      window->hid.tail +=
        (MOUSE_X(window) != PREV_MOUSE_X(window)) ||
        (MOUSE_Y(window) != PREV_MOUSE_Y(window)) ||
        (raw.data.mouse.usButtonFlags != 0);
      break;
  }
  return HT_ERROR_NONE;
}
#endif

static LRESULT CALLBACK
wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  HTWindow* window = (HTWindow*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
  switch (msg) {
    case WM_CREATE:     return WMCreate(window, hwnd, lparam);
    case WM_MOVE:       return WMMove(window, lparam);
    case WM_SIZE:       return WMSize(window, lparam);
    case WM_PAINT:      return WMPaint(window);
    case WM_CLOSE:
      HT_HANDLE_EVENT(window, window->event.close);
      return HT_ERROR_NONE;
    case WM_ERASEBKGND: return 1; /* Return nonzero prevents flickering */
#ifdef WM_INPUT
    case WM_INPUT:      return WMInput(window, lparam);
#endif
    default: break;
  }
  return DefWindowProc(hwnd, msg, wparam, lparam);
}

int
htCreateWindow(
    HTWindow** window, short x, short y, unsigned short w, unsigned short h) {
  const char* func = "htCreateWindow";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(!*window, func, HT_ERROR_INVALID_ARGUMENT);
  *window = calloc(1, sizeof (HTWindow));
  if (!*window) {
    return HANDLE_ERROR(func, HT_ERROR_MEMORY_ALLOCATION);
  } else {
    RECT frame = {0};
    /* Class registration fails on subsequent class by design */
    WNDCLASSEX cls = {0};
    cls.cbSize = sizeof (cls);
    cls.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc = (WNDPROC) wndproc;
    cls.hInstance = GetModuleHandle(NULL);
    cls.lpszClassName = "Application Window";
    RegisterClassEx(&cls);
    frame.left   = x;
    frame.right  = x + w;
    frame.top    = y;
    frame.bottom = y + h;
    AdjustWindowRect(&frame, WS_TILEDWINDOW, FALSE);
    CreateWindow(
      cls.lpszClassName,
      NULL,
      WS_TILEDWINDOW,
      frame.left,
      frame.top,
      frame.right - frame.left,
      frame.bottom - frame.top,
      NULL,
      NULL,
      cls.hInstance,
      *window);
    if (!(*window)->win) return HANDLE_ERROR(func, HT_ERROR_WINDOW_SERVER);
    ShowWindow((*window)->win, SW_SHOWNORMAL);
    /* Store Y position of content area instead of title bar */
    (*window)->info.y -= frame.top;
  }
  return HT_ERROR_NONE;
}

int
htCreateGLContext(HTWindow* window) {
  const char* func = "htInitGLContext";
  const HGLRC prev = wglGetCurrentContext();
  PIXELFORMATDESCRIPTOR pfd = {0};
  int format = InitGLContextLegacy(window);
  LOAD_WGL(PFNWGLCHOOSEPIXELFORMATARBPROC, wglChoosePixelFormatARB);
  LOAD_WGL(PFNWGLCREATECONTEXTATTRIBSARBPROC, wglCreateContextAttribsARB);
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(window->gl.context, func, HT_ERROR_GL_CONTEXT_CREATION);
  if (format <= 0) {
    wglMakeCurrent(window->hdc, prev);
    return HANDLE_ERROR(func, HT_ERROR_GL_CONTEXT_CREATION);
  }
  if (!(wglChoosePixelFormatARB && wglCreateContextAttribsARB)) {
    /* Dummy OpenGL context is valid if legacy profile was requested */
    if (!window->gl.profile) {
      UPDATE_GL_INFO(window, format, pfd);
      return HT_ERROR_NONE;
    }
    /* Remove dummy OpenGL context if core profile was requested */
    REMOVE_GL_CONTEXT(window);
    wglMakeCurrent(window->hdc, prev);
    return HANDLE_ERROR(func, HT_ERROR_GL_CONTEXT_CREATION);
  } else {
#ifdef WGL_ACCELERATION_ARB
    const int acceleration[] =
      {WGL_NO_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB};
#endif
#ifdef WGL_SWAP_METHOD_ARB
    const int backing_store[] = {WGL_SWAP_UNDEFINED_ARB, WGL_SWAP_EXCHANGE_ARB};
#endif
    int pfa[] = {
      WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
      WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
      WGL_COLOR_BITS_ARB,     -1,
      WGL_ALPHA_BITS_ARB,     -1,
      WGL_DEPTH_BITS_ARB,     -1,
      WGL_STENCIL_BITS_ARB,   -1,
      WGL_ACCUM_BITS_ARB,     -1,
      WGL_AUX_BUFFERS_ARB,    -1,
      WGL_DOUBLE_BUFFER_ARB,  -1,
      WGL_STEREO_ARB,         -1,
      WGL_SAMPLE_BUFFERS_ARB, -1,
      WGL_SAMPLES_ARB,        -1,
#ifdef WGL_ACCELERATION_ARB
      WGL_ACCELERATION_ARB,   -1,
#endif
#ifdef WGL_SWAP_METHOD_ARB
      WGL_SWAP_METHOD_ARB,    -1,
#endif
      0};
    unsigned num = 0;
    unsigned i = 5; /* Initialize to the first user defined element */
    pfa[i +  0] = window->gl.color;
    pfa[i +  2] = window->gl.alpha;
    pfa[i +  4] = window->gl.depth;
    pfa[i +  6] = window->gl.stencil;
    pfa[i +  8] = window->gl.accum;
    pfa[i + 10] = window->gl.aux_buffers;
    pfa[i + 12] = window->gl.double_buffer;
    pfa[i + 14] = window->gl.stereo;
    pfa[i + 16] = window->gl.sample_buffers;
    pfa[i + 18] = window->gl.samples;
#if defined WGL_ACCELERATION_ARB || defined WGL_SWAP_METHOD_ARB
    i += 20;
#ifdef WGL_ACCELERATION_ARB
    pfa[i] = acceleration[window->gl.accelerated];
    i += 2;
#endif
#ifdef WGL_SWAP_METHOD_ARB
    pfa[i] = backing_store[window->gl.backing_store];
#endif
#endif
    wglChoosePixelFormatARB(window->hdc, pfa, NULL, 1, &format, &num);
    if (!wglChoosePixelFormatARB(window->hdc, pfa, NULL, 1, &format, &num)) {
      /* Dummy OpenGL context is valid if legacy profile was requested */
      if (!window->gl.profile) {
        UPDATE_GL_INFO(window, format, pfd);
        return HT_ERROR_NONE;
      }
      /* Remove dummy OpenGL context if core profile was requested */
      REMOVE_GL_CONTEXT(window);
      wglMakeCurrent(window->hdc, prev);
      return HANDLE_ERROR(func, HT_ERROR_GL_PIXEL_FORMAT_NONE);
    } else {
      HGLRC hglrc = NULL;
      const int profile[] = {
        WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB};
      int attributes[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, -1,
        WGL_CONTEXT_MINOR_VERSION_ARB, -1,
        WGL_CONTEXT_PROFILE_MASK_ARB,  -1,
        0};
      i = 1; /* Initialize to the first user defined element */
      attributes[i + 0] = window->gl.major;
      attributes[i + 2] = window->gl.minor;
      attributes[i + 4] = profile[window->gl.major > 2];
      SetPixelFormat(window->hdc, format, &pfd);
      hglrc = wglCreateContextAttribsARB(window->hdc, 0, attributes);
      if (!hglrc) {
        if (!window->gl.profile) {
          UPDATE_GL_INFO(window, format, pfd);
          return HT_ERROR_NONE;
        }
        REMOVE_GL_CONTEXT(window);
        wglMakeCurrent(window->hdc, prev);
        return HANDLE_ERROR(func, HT_ERROR_GL_CONTEXT_CREATION);
      }
      wglMakeCurrent(window->hdc, hglrc);
      wglDeleteContext(window->gl.context);
      window->gl.context = hglrc;
    }
  }
  /* Overwrite user values with pixel format attributes obtained */
  UPDATE_GL_INFO(window, format, pfd);
  wglMakeCurrent(window->hdc, prev);
  return HT_ERROR_NONE;
}

int
htCreateInputManager(HTWindow* window) {
  const char* func = "htCreateInputManager";
#ifdef WM_INPUT
  RAWINPUTDEVICE rid[6] = {0};
#endif
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
#ifdef WM_INPUT
  rid[0].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[1].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[2].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[3].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[4].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[5].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[0].usUsage = HT_GD_USAGE_MOUSE;
  rid[1].usUsage = HT_GD_USAGE_JOYSTICK;
  rid[2].usUsage = HT_GD_USAGE_GAMEPAD;
  rid[3].usUsage = HT_GD_USAGE_KEYBOARD;
  rid[4].usUsage = HT_GD_USAGE_KEYPAD;
  rid[5].usUsage = HT_GD_USAGE_MULTI_AXIS_CONTROLLER;
  rid[3].dwFlags = RIDEV_NOLEGACY;
  RegisterRawInputDevices(rid, 6, sizeof (*rid));
#endif
  return HT_ERROR_NONE;
}

int
htDestroyWindow(HTWindow** window) {
  const char* func = "htDestroyWindow";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(*window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(window && VALID_WINDOW(*window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  DestroyWindow((*window)->win);
#ifndef HT_DISABLE_DEBUG
  (*window)->uid = 0;
#endif
  (*window)->hdc = NULL;
  (*window)->win = NULL;
  free(*window);
  *window = NULL;
  return HT_ERROR_NONE;
}

int
htDestroyGLContext(HTWindow* window) {
  const char* func = "htDestroyGLContext";
  const HGLRC context = wglGetCurrentContext();
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  if (context == window->gl.context) wglMakeCurrent(window->hdc, NULL);
  REMOVE_GL_CONTEXT(window);
  INIT_GL_DEFAULTS(window);
  return HT_ERROR_NONE;
}

int
htDestroyInputManager(HTWindow* window) {
  const char* func = "htDestroyWindow";
#ifdef WM_INPUT
  RAWINPUTDEVICE rid[6] = {0};
#endif
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
#ifdef WM_INPUT
  rid[0].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[1].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[2].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[3].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[4].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[5].usUsagePage = HT_PAGE_GENERIC_DESKTOP;
  rid[0].usUsage = HT_GD_USAGE_MOUSE;
  rid[1].usUsage = HT_GD_USAGE_JOYSTICK;
  rid[2].usUsage = HT_GD_USAGE_GAMEPAD;
  rid[3].usUsage = HT_GD_USAGE_KEYBOARD;
  rid[4].usUsage = HT_GD_USAGE_KEYPAD;
  rid[5].usUsage = HT_GD_USAGE_MULTI_AXIS_CONTROLLER;
  rid[0].dwFlags = RIDEV_REMOVE;
  rid[1].dwFlags = RIDEV_REMOVE;
  rid[2].dwFlags = RIDEV_REMOVE;
  rid[3].dwFlags = RIDEV_REMOVE;
  rid[4].dwFlags = RIDEV_REMOVE;
  rid[5].dwFlags = RIDEV_REMOVE;
  RegisterRawInputDevices(rid, 6, sizeof (*rid));
#endif
  return HT_ERROR_NONE;
}

int
htSetCurrentGLContext(HTWindow* window) {
  const char* func = "htSetCurrentGLContext";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  if (window) wglMakeCurrent(window->hdc, window->gl.context);
  else wglMakeCurrent(window->hdc, NULL);
  return HT_ERROR_NONE;
}

int
htSwapGLBuffers(HTWindow* window) {
  const char* func = "htSwapGLBuffers";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  SwapBuffers(window->hdc);
  return HT_ERROR_NONE;
}

int
htSetWindowInteger(HTWindow* window, HTWindowAttribute type, int data) {
  const char* func = "htSetWindowInteger";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  switch (type) {
    case HT_GL_ACCELERATED: window->gl.accelerated = data != 0;
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
      window->gl.backing_store = data != 0;
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
      SetWindowResolution(window);
      break;
    case HT_WINDOW_WIDTH:
      window->info.width = data & 0x3FFF;
      SetWindowResolution(window);
      break;
    case HT_WINDOW_STYLE:
      window->info.titled         = (data & HT_WINDOW_TITLED)         != 0;
      window->info.closable       = (data & HT_WINDOW_CLOSABLE)       != 0;
      window->info.miniaturizable = (data & HT_WINDOW_MINIATURIZABLE) != 0;
      window->info.resizable      = (data & HT_WINDOW_RESIZABLE)      != 0;
      SetWindowStyle(window);
      break;
    case HT_WINDOW_X:
      window->info.x = data;
      SetWindowPosition(window);
      break;
    case HT_WINDOW_Y:
      window->info.y = data;
      SetWindowPosition(window);
      break;
    default: return HANDLE_ERROR(func, HT_ERROR_INVALID_ARGUMENT);
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
    case HT_WINDOW_TITLE: SetWindowText(window->win, (LPCSTR) data); break;
    case HT_WINDOW_USER:  window->user = data;                       break;
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
    case HT_GL_ACCELERATED:      *data = window->gl.accelerated;    break;
    case HT_GL_ACCUM_BUFFER:     *data = window->gl.accum;          break;
    case HT_GL_ALPHA:            *data = window->gl.alpha;          break;
    case HT_GL_AUX_BUFFERS:      *data = window->gl.aux_buffers;    break;
    case HT_GL_BACKING_STORE:    *data = window->gl.backing_store;  break;
    case HT_GL_BLUE:             *data = window->gl.blue;           break;
    case HT_GL_COLOR_BUFFER:     *data = window->gl.color;          break;
    case HT_GL_DEPTH_BUFFER:     *data = window->gl.depth;          break;
    case HT_GL_DOUBLE_BUFFERING: *data = window->gl.double_buffer;  break;
    case HT_GL_GREEN:            *data = window->gl.green;          break;
    case HT_GL_MAJOR_VERSION:    *data = window->gl.major;          break;
    case HT_GL_MINOR_VERSION:    *data = window->gl.minor;          break;
    case HT_GL_PIXEL_TYPE:       *data = window->gl.pixel_type;     break;
    case HT_GL_PROFILE:          *data = window->gl.profile;        break;
    case HT_GL_RED:              *data = window->gl.red;            break;
    case HT_GL_SAMPLE_BUFFERS:   *data = window->gl.sample_buffers; break;
    case HT_GL_SAMPLES:          *data = window->gl.samples;        break;
    case HT_GL_STENCIL_BUFFER:   *data = window->gl.stencil;        break;
    case HT_GL_STEREO:           *data = window->gl.stereo;         break;
    case HT_GL_SWAP_INTERVAL:    *data = window->gl.swap_interval;  break;
    case HT_INPUT_MOUSE_BUTTON:
      *data = window->hid.mouse.button[window->hid.head];
      break;
    case HT_INPUT_MOUSE_X:
      *data =
        window->hid.mouse.x[window->hid.head] -
        (-((short) window->hid.absolute) &
        window->hid.mouse.x[HT_INPUT_QUEUE_PREV(window->hid.head)]);
      break;
    case HT_INPUT_MOUSE_Y:
      *data =
        window->hid.mouse.y[window->hid.head] -
        (-((short) window->hid.absolute) &
        window->hid.mouse.y[HT_INPUT_QUEUE_PREV(window->hid.head)]);
      break;
    case HT_WINDOW_HEIGHT:       *data = window->info.height;       break;
    case HT_WINDOW_STYLE:        *data = STYLE_MASK(window);        break;
    case HT_WINDOW_WIDTH:        *data = window->info.width;        break;
    case HT_WINDOW_X:            *data = window->info.x;            break;
    case HT_WINDOW_Y:            *data = window->info.y;            break;
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
  const char* func = "htSetWindowEventCallback";
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
  MSG msg = {0};
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  while (PeekMessage(&msg, window->win, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
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
