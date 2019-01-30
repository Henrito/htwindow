/*------------------------------------------------------------------- HEADERS */

#include <CoreGraphics/CGEventSource.h>
#include <IOKit/hid/IOHIDLib.h>
#include <objc/message.h>
#include <OpenGL/GL.h>
#include <OpenGL/OpenGL.h>
#include <stdlib.h>
#include "window.h"

/*-------------------------------------------------------------------- MACROS */

#define HT_INPUT_QUEUE_BITS  4
#define HT_INPUT_QUEUE_SIZE (1 << HT_INPUT_QUEUE_BITS)
#define HT_INPUT_QUEUE_PREV(index) (((index) - 1) & (HT_INPUT_QUEUE_SIZE - 1))

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

/* Objective-C runtime function pointer casting macro functions */
#define CLASS_SEND ((id (*)(Class, SEL, ...)) objc_msgSend)
#define CLASS_NEW(cls) CLASS_SEND(objc_getClass(cls), sel_getUid("new"))
#define CLASS_AUTORELEASE(cls)\
  CLASS_SEND(objc_getClass(cls), sel_getUid("alloc"), sel_getUid("autorelease"))
#define SEND_MSG_STRET(type) ((type (*)(id, SEL, ...)) objc_msgSend_stret)
#define NSSTRING(str)\
  (((id (*)(Class, SEL, const char*)) objc_msgSend)(\
    objc_getClass("NSString"),\
    sel_getUid("stringWithUTF8String:"),\
    (str) ? (char*) (str) : ""))

/* Input macro functions */
#define READ_MOUSE_BUTTON(x)\
  CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, x)
#define HT_COND_SET_MOUSE_BUTTON(window, x)\
  (window)->input.manager.mouse.button &= ~HT_INPUT_MASK_BUTTON(x);\
  (window)->input.manager.mouse.button |= READ_MOUSE_BUTTON(x) << (x)
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

/*------------------------------------------------------------------- STRUCTS */

struct HTWindow {
  struct {
    struct {
      short         x[HT_INPUT_QUEUE_SIZE];
      short         y[HT_INPUT_QUEUE_SIZE];
      unsigned char button[HT_INPUT_QUEUE_SIZE];
    } mouse;
    unsigned char   page[HT_INPUT_QUEUE_SIZE];
    IOHIDManagerRef manager;
    unsigned        head: HT_INPUT_QUEUE_BITS;
    unsigned        tail: HT_INPUT_QUEUE_BITS;
#if HT_INPUT_QUEUE_BITS << 1 != 32
    const int reserved: (sizeof (unsigned) << 3) - (HT_INPUT_QUEUE_BITS << 1);
#endif
    const int padding;
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
    CGLContextObj context;      /* CGL OpenGL context                    */
    id nsopenglcontext;         /* Cocoa OpenGL context                  */
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
    unsigned fullscreen:  1; /* Window is in fullscreen mode  */
    unsigned moving:      1; /* Window is currently moving    */
  } info;
  unsigned char* user; /* Pointer to user-supplied data    */
  id win;              /* Platform-dependent window handle */
#ifndef HT_DISABLE_DEBUG
  unsigned uid; /* Used to verify that the window was properly initialized */
#endif
};

/*---------------------------------------------------------- STATIC VARIABLES */

static HTWindowErrorCallback ht_error_handler;
static id pool; /* Drained with ht_poll_events() in main thread only */

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

static void
plug(void* context, IOReturn result, void* sender, IOHIDDeviceRef device) {
  const CFArrayRef array = IOHIDDeviceCopyMatchingElements(device, NULL, 0);
  const CFIndex count = CFArrayGetCount(array);
  const CFTypeRef device_property =
    IOHIDDeviceGetProperty(device, CFSTR(kIOHIDPrimaryUsageKey));
  int usage = -1;
  CFNumberGetValue((CFNumberRef) device_property, kCFNumberIntType, &usage);
  CFRelease(array);
  CFRelease(device_property);
  switch (usage) {
    case kHIDUsage_GD_GamePad:
      printf("GAMEPAD: %li KEYS\n", count);
      break;
    case kHIDUsage_GD_Keyboard:
      printf("KEYBOARD: %li KEYS\n", count);
      break;
    case kHIDUsage_GD_Mouse:
      printf("MOUSE: %li KEYS\n", count);
      break;
    default:
      return;
  }
  result = kIOReturnSuccess;
  (void) context;
  (void) sender;
}

static void
unplug(void* context, IOReturn result, void* sender, IOHIDDeviceRef device) {
  printf("Removed %p", (void*) device);
  result = kIOReturnSuccess;
  (void) context; /* Unused argument */
  (void) sender;  /* Unused argument */
}

static void
ReadInput(HTWindow* window, IOReturn result, void* sender, IOHIDValueRef val) {
  const IOHIDElementRef element = IOHIDValueGetElement(val);
  const unsigned char usage = IOHIDElementGetUsage(element);
  const CFNumberRef property = (CFNumberRef)
    IOHIDDeviceGetProperty(sender, CFSTR(kIOHIDPrimaryUsageKey));
  CFNumberGetValue(
    property, kCFNumberCharType, &window->hid.page[window->hid.tail]);
  CFRelease(property);
  switch (window->hid.page[window->hid.tail]) {
    case kHIDUsage_GD_Mouse:
      if (!window->info.moving && window->event.mouse) {
        MOUSE_BUTTON(window) =
          READ_MOUSE_BUTTON(0) |
          (READ_MOUSE_BUTTON(1) << 1) |
          (READ_MOUSE_BUTTON(2) << 2) |
          (READ_MOUSE_BUTTON(3) << 3) |
          (READ_MOUSE_BUTTON(4) << 4);
        /* Store previous coordinates since at most only one axis will update */
        MOUSE_X(window) = PREV_MOUSE_X(window);
        MOUSE_Y(window) = PREV_MOUSE_Y(window);
        switch (usage) {
          case kHIDUsage_GD_X:
            MOUSE_X(window) = IOHIDValueGetIntegerValue(val);
            window->hid.tail += MOUSE_X(window) != PREV_MOUSE_X(window);
            return;
          case kHIDUsage_GD_Y:
            MOUSE_Y(window) = IOHIDValueGetIntegerValue(val);
            window->hid.tail += MOUSE_Y(window) != PREV_MOUSE_Y(window);
            return;
          default:
            window->hid.tail +=
              MOUSE_BUTTON(window) != PREV_MOUSE_BUTTON(window);
            return;
        }
      }
      /* Clear moving bit if left mouse button is released */
      window->info.moving = READ_MOUSE_BUTTON(0);
      return;
    default: break;
  }
  result = kIOReturnSuccess;
}

static void
append_dictionary(CFMutableArrayRef array, unsigned usage) {
  const CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(
    NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  const int page = kHIDPage_GenericDesktop;
  const CFNumberRef num_page = CFNumberCreate(NULL, kCFNumberIntType, &page);
  const CFNumberRef num_usage = CFNumberCreate(NULL, kCFNumberIntType, &usage);
  CFDictionarySetValue(dictionary, CFSTR(kIOHIDDeviceUsagePageKey), num_page);
  CFDictionarySetValue(dictionary, CFSTR(kIOHIDDeviceUsageKey), num_usage);
  CFArrayAppendValue(array, dictionary);
  CFRelease(num_page);
  CFRelease(num_usage);
  CFRelease(dictionary);
}

static void
SetWindowPosition(HTWindow* window) {
  /* Use screen height to invert Y position due to AppKit bottom-left origin */
  const CGSize size = SEND_MSG_STRET(CGRect)(
    objc_msgSend(window->win, sel_getUid("screen")), sel_getUid("frame")).size;
  const CGPoint pos = CGPointMake(window->info.x, size.height - window->info.y);
  objc_msgSend(window->win, sel_getUid("setFrameTopLeftPoint:"), pos);
}

static void
SetWindowResolution(HTWindow* window) {
  const CGRect frame = SEND_MSG_STRET(CGRect)(window->win, sel_getUid("frame"));
  CGRect content = SEND_MSG_STRET(CGRect)(
    window->win, sel_getUid("contentRectForFrameRect:"), frame);
  /* Adjust Y position due to window resizing from AppKit bottom-left origin */
  content.origin.y -= window->info.height - content.size.height;
  content.size = CGSizeMake(window->info.width, window->info.height);
  objc_msgSend(window->win, sel_getUid("setContentSize:"), content.size);
  objc_msgSend(window->win, sel_getUid("setFrameOrigin:"), content.origin);
}

static void
DidResize(id self) {
  CGSize content = {0};
  HTWindow* window = NULL;
  object_getInstanceVariable(self, "window", (void**) &window);
  content = SEND_MSG_STRET(CGRect)(
    window->win,
    sel_getUid("contentRectForFrameRect:"),
    SEND_MSG_STRET(CGRect)(window->win, sel_getUid("frame"))).size;
  window->info.width  = content.width;
  window->info.height = content.height;
  HT_HANDLE_EVENT(window, window->event.resize);
  /* Update NSOpenGLContext to properly display resized content */
  if (window->gl.nsopenglcontext) {
    objc_msgSend(window->gl.nsopenglcontext, sel_getUid("update"));
  }
}

static void
FocusWindow(id self, SEL sel) {
  HTWindow* window = NULL;
  object_getInstanceVariable(self, "window", (void**) &window);
  if (window->hid.manager) {
    /* Read input only when window is focused */
    if (sel == sel_getUid("windowDidBecomeKey:")) {
      IOHIDManagerScheduleWithRunLoop(
        window->hid.manager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    } else {
      IOHIDManagerUnscheduleFromRunLoop(
        window->hid.manager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    }
  }
  HT_HANDLE_EVENT(window, window->event.focus);
}

static void
WillMove(id self) {
  HTWindow* window = NULL;
  object_getInstanceVariable(self, "window", (void**) &window);
  window->info.moving = 1;
}

static void
DidMove(id self) {
  CGSize screen = {0};
  CGPoint content = {0};
  HTWindow* window = NULL;
  object_getInstanceVariable(self, "window", (void**) &window);
  /* Window is still considered moving if the left mouse button is down */
  window->info.moving = READ_MOUSE_BUTTON(0);
  screen = SEND_MSG_STRET(CGRect)(
    objc_msgSend(window->win, sel_getUid("screen")), sel_getUid("frame")).size;
  content = SEND_MSG_STRET(CGRect)(
    window->win,
    sel_getUid("contentRectForFrameRect:"),
    SEND_MSG_STRET(CGRect)(window->win, sel_getUid("frame"))).origin;
  window->info.x = content.x;
  window->info.y = screen.height - content.y - window->info.height;
  HT_HANDLE_EVENT(window, window->event.move);
}

static void
CloseWindow(id self) {
  HTWindow* window = NULL;
  object_getInstanceVariable(self, "window", (void**) &window);
  HT_HANDLE_EVENT(window, window->event.close);
}

static void
InitWindowDelegate() {
  const Class cls =
    objc_allocateClassPair(objc_getClass("NSObject"), "WindowDelegate", 0);
  class_addIvar(cls, "window", sizeof (HTWindow*), 3, "^{HT_WINDOW}");
  class_addMethod(
    cls, sel_getUid("windowDidEndLiveResize:"), (IMP) DidResize, "v@");
  class_addMethod(
    cls, sel_getUid("windowDidBecomeKey:"), (IMP) FocusWindow, "v:@");
  class_addMethod(
    cls, sel_getUid("windowDidResignKey:"), (IMP) FocusWindow, "v:@");
  class_addMethod(cls, sel_getUid("windowWillMove:"), (IMP) WillMove, "v@");
  class_addMethod(cls, sel_getUid("windowDidMove:"), (IMP) DidMove, "v@");
  class_addMethod(cls, sel_getUid("windowWillClose:"), (IMP) CloseWindow, "v@");
  objc_registerClassPair(cls);
}

int
htCreateWindow(
    HTWindow** window, short x, short y, unsigned short w, unsigned short h) {
  const char* func = "htCreateWindow";
  extern id NSApp;
  id delegate = NULL;
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(!*window, func, HT_ERROR_INVALID_ARGUMENT);
  if (!NSApp) {
    CLASS_SEND(objc_getClass("NSApplication"), sel_getUid("sharedApplication"));
    objc_msgSend(NSApp, sel_getUid("setActivationPolicy:"), 0);
    objc_msgSend(NSApp, sel_getUid("finishLaunching"));
    InitWindowDelegate();
  }
  *window = calloc(1, sizeof (HTWindow));
  if (!*window) {
    return HANDLE_ERROR(func, HT_ERROR_MEMORY_ALLOCATION);
  }
  delegate = CLASS_NEW("WindowDelegate");
  /* Default window is size of the monitor with title bar and borders */
  (*window)->win = CLASS_AUTORELEASE("NSWindow");
  (*window)->info.x = x;
  (*window)->info.y = CGDisplayPixelsHigh(CGMainDisplayID()) - h - y;
  (*window)->info.width  = w;
  (*window)->info.height = h;
  (*window)->info.style  = HT_WINDOW_STYLE_DEFAULT;
  objc_msgSend(
    (*window)->win,
    sel_getUid("initWithContentRect:styleMask:backing:defer:"),
    CGRectMake(x, (*window)->info.y, w, h),
    (*window)->info.style,
    0x02,
    NO);
  objc_msgSend((*window)->win, sel_getUid("setDelegate:"), delegate);
  /* Obtain attached delegate from window for correct autorelease */
  objc_msgSend(
    (*window)->win, sel_getUid("delegate"), sel_getUid("autorelease"));
  object_setInstanceVariable(delegate, "window", *window);
  /* Set window position to top-left corner of screen */
  objc_msgSend((*window)->win, sel_getUid("orderFront:"));
  /* Initialize OpenGL context defaults */
  INIT_GL_DEFAULTS(*window);
#ifndef HT_DISABLE_DEBUG
  /* Set GUID for argument validation */
  (*window)->uid = GUID;
#endif
  /* Store adjusted window height due to menu/title bar */
  (*window)->info.height = SEND_MSG_STRET(CGRect)(
    (*window)->win,
    sel_getUid("contentRectForFrameRect:"),
    SEND_MSG_STRET(CGRect)((*window)->win, sel_getUid("frame"))).size.height;
  return HT_ERROR_NONE;
}

int
htCreateGLContext(HTWindow* window) {
  const char* func = "htCreateGLContext";
  const int accelerated[] = {0, kCGLPFAAccelerated};
  const int backing_store[] = {0, kCGLPFABackingStore};
  const int double_buffer[] = {0, kCGLPFADoubleBuffer};
  const int stereo[] = {0, 6 /* kCGLPFAStereo */};
  const int profile[] = {kCGLOGLPVersion_Legacy, kCGLOGLPVersion_3_2_Core};
  const CGLContextObj prev = CGLGetCurrentContext();
  int value = 0;
  /* Create OpenGL pixel format */
  CGLPixelFormatObj format = NULL;
  CGLPixelFormatAttribute pfa[] = {
    kCGLPFANoRecovery,
    kCGLPFAColorSize,     -1,
    kCGLPFAAlphaSize,     -1,
    kCGLPFADepthSize,     -1,
    kCGLPFAStencilSize,   -1,
    kCGLPFAAccumSize,     -1,
    kCGLPFAAuxBuffers,    -1,
    kCGLPFASampleBuffers, -1,
    kCGLPFASamples,       -1,
    kCGLPFAOpenGLProfile, -1,
    -1, /* kCGLPFAAccelerated  */
    -1, /* kCGLPFABackingStore */
    -1, /* kCGLPFADoubleBuffer */
    -1, /* kCGLPFAStereo       */
    0};
  /* Overwrite default pixel format attributes with user-supplied values */
  unsigned long i = 2; /* Index of first rewritable element */
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(!window->gl.context, func, HT_ERROR_GL_CONTEXT_CREATION);
  pfa[i +  0] = window->gl.color;
  pfa[i +  2] = window->gl.alpha;
  pfa[i +  4] = window->gl.depth;
  pfa[i +  6] = window->gl.stencil;
  pfa[i +  8] = window->gl.accum;
  pfa[i + 10] = window->gl.aux_buffers;
  pfa[i + 12] = window->gl.sample_buffers;
  pfa[i + 14] = window->gl.samples;
  pfa[i + 16] = profile[window->gl.major > 2];
  /* Increment index if "boolean" attribute exists since 0 means end of list */
  i += 17; /* Set i to first "boolean" element */
  pfa[i] = accelerated[window->gl.accelerated];
  pfa[i += window->gl.accelerated] = double_buffer[window->gl.double_buffer];
  pfa[i += window->gl.double_buffer] = backing_store[window->gl.backing_store];
  pfa[i += window->gl.backing_store] = stereo[window->gl.stereo];
  if (CGLChoosePixelFormat(pfa, &format, &value) != kCGLNoError) {
    return HANDLE_ERROR(func, HT_ERROR_GL_PIXEL_FORMAT_NONE);
  }
  /* Create OpenGL context */
  CGLCreateContext(format, 0, &window->gl.context);
  if (CGLSetCurrentContext(window->gl.context) != kCGLNoError) {
    CGLDestroyPixelFormat(format);
    CGLSetCurrentContext(prev);
    return HANDLE_ERROR(func, HT_ERROR_GL_CONTEXT_CREATION);
  }
  value = window->gl.swap_interval;
  window->gl.nsopenglcontext = CLASS_AUTORELEASE("NSOpenGLContext");
  objc_msgSend(
    window->gl.nsopenglcontext,
    sel_getUid("initWithCGLContextObj:"),
    window->gl.context);
  objc_msgSend(
    window->gl.nsopenglcontext,
    sel_getUid("setView:"),
    objc_msgSend(window->win, sel_getUid("contentView")));
  /* Send NSOpenGLContext update message to display properly on window */
  htPollWindowEvents(window);
  objc_msgSend(window->gl.nsopenglcontext, sel_getUid("update"));
  /* Set swap interval */
  CGLSetParameter(window->gl.context, kCGLCPSwapInterval, &value);
  /* Overwrite user-supplied values with pixel format attributes obtained */
  CGLDescribePixelFormat(format, 0, kCGLPFAColorSize, &value);
  window->gl.color = value;
  CGLDescribePixelFormat(format, 0, kCGLPFAAlphaSize, &value);
  window->gl.alpha = value;
  window->gl.red = (window->gl.color - value) / 3;
  window->gl.green = window->gl.red + (window->gl.color == 16 && !value);
  window->gl.blue = window->gl.red;
  CGLDescribePixelFormat(format, 0, kCGLPFADepthSize, &value);
  window->gl.depth = value;
  CGLDescribePixelFormat(format, 0, kCGLPFAStencilSize, &value);
  window->gl.stencil = value;
  CGLDescribePixelFormat(format, 0, kCGLPFAAccumSize, &value);
  window->gl.accum = value;
  CGLDescribePixelFormat(format, 0, kCGLPFAAuxBuffers, &value);
  window->gl.aux_buffers = value;
  CGLDescribePixelFormat(format, 0, kCGLPFASampleBuffers, &value);
  window->gl.sample_buffers = value;
  CGLDescribePixelFormat(format, 0, kCGLPFASamples, &value);
  window->gl.samples = value;
  window->gl.major = glGetString(GL_VERSION)[0] - '0';
  window->gl.minor = glGetString(GL_VERSION)[2] - '0';
  window->gl.profile = window->gl.major > 2;
  CGLDestroyPixelFormat(format);
  CGLSetCurrentContext(prev);
  return HT_ERROR_NONE;
}

int
htCreateInputManager(HTWindow* window) {
  const char* func = "htCreateInputManager";
  const CFMutableArrayRef array =
    CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(!window->hid.manager, func, HT_ERROR_INPUT_MANAGER_CREATION);
  window->hid.manager = IOHIDManagerCreate(NULL, 0);
  append_dictionary(array, kHIDUsage_GD_Mouse);
  append_dictionary(array, kHIDUsage_GD_Joystick);
  append_dictionary(array, kHIDUsage_GD_GamePad);
  append_dictionary(array, kHIDUsage_GD_Keyboard);
  append_dictionary(array, kHIDUsage_GD_Keypad);
  append_dictionary(array, kHIDUsage_GD_MultiAxisController);
  IOHIDManagerSetDeviceMatchingMultiple(window->hid.manager, array);
  CFRelease(array);
  if (IOHIDManagerOpen(window->hid.manager, 0) != kIOReturnSuccess) {
    CFRelease(window->hid.manager);
    window->hid.manager = NULL;
    return HT_ERROR_INPUT_MANAGER_CREATION;
  }
  /* Set callbacks for hotplugging devices and reading input */
  IOHIDManagerRegisterDeviceMatchingCallback(
    window->hid.manager, plug, window);
  IOHIDManagerRegisterDeviceRemovalCallback(
    window->hid.manager, unplug, window);
  IOHIDManagerRegisterInputValueCallback(
    window->hid.manager, (IOHIDValueCallback) ReadInput, window);
  return HT_ERROR_NONE;
}

int
htDestroyWindow(HTWindow** window) {
  const char* func = "htDestroyWindow";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(*window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(window && VALID_WINDOW(*window), func, HT_ERROR_UNINITIALIZED_WINDOW);
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
  ASSERT(window->gl.context, func, HT_ERROR_UNINITIALIZED_GL_CONTEXT);
  if (CGLGetCurrentContext() == window->gl.context) CGLSetCurrentContext(NULL);
  CGLReleaseContext(window->gl.context);
  window->gl.context = NULL;
  /* Initialize OpenGL context defaults */
  INIT_GL_DEFAULTS(window);
  return HT_ERROR_NONE;
}

int
htDestroyInputManager(HTWindow* window) {
  const char* func = "htDestroyInputManager";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(window->hid.manager, func, HT_ERROR_UNINITIALIZED_INPUT_MANAGER);
  CFRelease(window->hid.manager);
  window->hid.manager = NULL;
  return HT_ERROR_NONE;
}

int
htSetCurrentGLContext(HTWindow* window) {
  const char* func = "htSetCurrentGLContext";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  if (window) CGLSetCurrentContext(window->gl.context);
  else CGLSetCurrentContext(NULL);
  return HT_ERROR_NONE;
}

int
htSwapGLBuffers(HTWindow* window) {
  const char* func = "htSwapGLBuffers";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(window->gl.context, func, HT_ERROR_UNINITIALIZED_GL_CONTEXT);
  glSwapAPPLE();
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
    case HT_WINDOW_STYLE:
      window->info.style = data & HT_WINDOW_STYLE_DEFAULT;
      objc_msgSend(
        window->win, sel_getUid("setStyleMask:"), window->info.style);
      break;
    case HT_WINDOW_WIDTH:
      window->info.width = data & 0x3FFF;
      SetWindowResolution(window);
      break;
    case HT_WINDOW_X:
      window->info.x = data;
      SetWindowPosition(window);
      break;
    case HT_WINDOW_Y:
      window->info.y = data;
      SetWindowPosition(window);
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
    case HT_WINDOW_TITLE:
      objc_msgSend(window->win, sel_getUid("setTitle:"), NSSTRING(data));
      break;
    case HT_WINDOW_USER: window->user = data; break;
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
    case HT_GL_ACCELERATED:       *data = window->gl.accelerated;     break;
    case HT_GL_ACCUM_BUFFER:      *data = window->gl.accum;           break;
    case HT_GL_ALPHA:             *data = window->gl.alpha;           break;
    case HT_GL_AUX_BUFFERS:       *data = window->gl.aux_buffers;     break;
    case HT_GL_BACKING_STORE:     *data = window->gl.backing_store;   break;
    case HT_GL_BLUE:              *data = window->gl.blue;            break;
    case HT_GL_COLOR_BUFFER:      *data = window->gl.color;           break;
    case HT_GL_DEPTH_BUFFER:      *data = window->gl.depth;           break;
    case HT_GL_DOUBLE_BUFFERING:  *data = window->gl.double_buffer;   break;
    case HT_GL_GREEN:             *data = window->gl.green;           break;
    case HT_GL_MAJOR_VERSION:     *data = window->gl.major;           break;
    case HT_GL_MINOR_VERSION:     *data = window->gl.minor;           break;
    case HT_GL_PIXEL_TYPE:        *data = window->gl.pixel_type;      break;
    case HT_GL_PROFILE:           *data = window->gl.profile;         break;
    case HT_GL_RED:               *data = window->gl.red;             break;
    case HT_GL_SAMPLE_BUFFERS:    *data = window->gl.sample_buffers;  break;
    case HT_GL_SAMPLES:           *data = window->gl.samples;         break;
    case HT_GL_STENCIL_BUFFER:    *data = window->gl.stencil;         break;
    case HT_GL_STEREO:            *data = window->gl.stereo;          break;
    case HT_GL_SWAP_INTERVAL:     *data = window->gl.swap_interval;   break;
    case HT_INPUT_MOUSE_BUTTON:   *data = HEAD_MOUSE_BUTTON(window);  break;
    case HT_INPUT_MOUSE_RELATIVE: *data = 1; /* TODO */               break;
    case HT_INPUT_MOUSE_X:        *data = HEAD_MOUSE_X(window);       break;
    case HT_INPUT_MOUSE_Y:        *data = HEAD_MOUSE_Y(window);       break;
    case HT_WINDOW_HEIGHT:        *data = window->info.height;        break;
    case HT_WINDOW_STYLE:         *data = window->info.style;         break;
    case HT_WINDOW_WIDTH:         *data = window->info.width;         break;
    case HT_WINDOW_X:             *data = window->info.x;             break;
    case HT_WINDOW_Y:             *data = window->info.y;             break;
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
  extern id NSApp;
  extern const id NSDefaultRunLoopMode;
  id event = NULL;
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  do {
    event = objc_msgSend(
      window->win,
      sel_getUid("nextEventMatchingMask:untilDate:inMode:dequeue:"),
      ~0L, /* All event dates */
      CLASS_SEND(objc_getClass("NSDate"), sel_getUid("distantPast")),
      NSDefaultRunLoopMode,
      YES);
    objc_msgSend(NSApp, sel_getUid("sendEvent:"), event);
  } while (event);
  /* Drain NSAutotreleasePool to free memory */
  objc_msgSend(pool, sel_getUid("drain"));
  pool = CLASS_NEW("NSAutoreleasePool");
  return HT_ERROR_NONE;
}

int
htPollInputEvents(HTWindow* window) {
  const char* func = "htPollInputEvents";
  ASSERT(window, func, HT_ERROR_INVALID_ARGUMENT);
  ASSERT(VALID_WINDOW(window), func, HT_ERROR_UNINITIALIZED_WINDOW);
  ASSERT(window->hid.manager, func, HT_ERROR_INVALID_ARGUMENT);
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
  for (; window->hid.head != window->hid.tail; ++window->hid.head) {
    switch (window->hid.page[window->hid.head]) {
      case kHIDUsage_GD_Mouse:
        HT_HANDLE_EVENT(window, window->event.mouse);
        break;
      default: break;
    }
  }
  return HT_ERROR_NONE;
}
