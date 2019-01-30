#ifndef HT_WINDOW_H
#define HT_WINDOW_H

/*-------------------------------------------------------------------- MACROS */

/* Default OpenGL pixel format values */
#define HT_DEFAULT_GL_ACCELERATED        1
#define HT_DEFAULT_GL_ACCUM_BUFFER       0
#define HT_DEFAULT_GL_AUX_BUFFERS        0
#define HT_DEFAULT_GL_BACKING_STORE      0
#define HT_DEFAULT_GL_COLOR_BUFFER      32
#define HT_DEFAULT_GL_DEPTH_BUFFER      24
#define HT_DEFAULT_GL_DOUBLE_BUFFERING   1
#define HT_DEFAULT_GL_MAJOR_VERSION      1
#define HT_DEFAULT_GL_MINOR_VERSION      0
#define HT_DEFAULT_GL_PIXEL_TYPE         1
#define HT_DEFAULT_GL_PROFILE            0
#define HT_DEFAULT_GL_RGBA_CHANNEL       8
#define HT_DEFAULT_GL_SAMPLE_BUFFERS     0
#define HT_DEFAULT_GL_SAMPLES            0
#define HT_DEFAULT_GL_STENCIL_BUFFER     8
#define HT_DEFAULT_GL_STEREO             0
#define HT_DEFAULT_GL_SWAP_INTERVAL      1

/* Maximum OpenGL pixel format values */
#define HT_MAX_GL_AUX_BUFFERS            4
#define HT_MAX_GL_COLOR_BUFFER          32
#define HT_MAX_GL_DEPTH_BUFFER          32
#define HT_MAX_GL_MAJOR_VERSION          9
#define HT_MAX_GL_MINOR_VERSION          9
#define HT_MAX_GL_RGBA_CHANNEL           8
#define HT_MAX_GL_SAMPLE_BUFFERS         1
#define HT_MAX_GL_SAMPLES               16
#define HT_MAX_GL_STENCIL_BUFFER         8
#define HT_MAX_GL_ACCUM_BUFFER         128

/* Raw input value masks */
#define HT_INPUT_MASK_X          0xFFFF
#define HT_INPUT_MASK_Y          0xFFFF
#define HT_INPUT_MASK_BUTTON(x) (1 << (x))

/* Helper macro functions */
#define HT_MIN(x, y) ((y) ^ (((x) ^ (y)) & -((x) < (y))))

/*--------------------------------------------------------------------- ENUMS */

typedef enum {
  /* SUCCESS: No error occurred, but status information may be available    */
  HT_ERROR_NONE = 0,
  /* VALIDATION ERROR: Errors that are the result of incorrect API usage    */
  HT_ERROR_INVALID_ARGUMENT = -64,
  HT_ERROR_UNINITIALIZED_GL_CONTEXT,
  HT_ERROR_UNINITIALIZED_INPUT_MANAGER,
  HT_ERROR_UNINITIALIZED_WINDOW,
  /* RUNTIME ERROR: Errors occurring at runtime even with correct API usage */
  HT_ERROR_GL_CONTEXT_CREATION = -128,
  HT_ERROR_GL_PIXEL_FORMAT_NONE,
  HT_ERROR_GL_EXTENSIONS_MISSING,
  HT_ERROR_INPUT_MANAGER_CREATION,
  HT_ERROR_MEMORY_ALLOCATION,
  HT_ERROR_POOL_EMPTY,
  HT_ERROR_STACK_EMPTY,
  HT_ERROR_STACK_OVERFLOW,
  HT_ERROR_WINDOW_SERVER
} HTResult;

typedef enum {
  HT_EVENT_NULL,     /* Dummy event                       */
  HT_EVENT_CLOSE,    /* Window was signaled to close      */
  HT_EVENT_DRAW,     /* Window was signaled to redraw     */
  HT_EVENT_FOCUS,    /* Window was focused/unfocused      */
  HT_EVENT_MOVE,     /* Window was minimized/restored     */
  HT_EVENT_MINIMIZE, /* Window was repositioned           */
  HT_EVENT_RESIZE,   /* Wndow was resized                 */
  HT_EVENT_KEYBOARD, /* Keyboard was pressed/released     */
  HT_EVENT_MOUSE,    /* Mouse was moved/clicked           */
  HT_EVENT_GAMEPAD,  /* Gamepad was pressed/release/moved */
  HT_EVENT_COUNT     /* Number of window events           */
} HTEvent;

typedef enum {
  HT_PAGE_GENERIC_DESKTOP = 0x01
} HTUsagePage;

typedef enum {
  HT_GD_USAGE_MOUSE                 = 0x02,
  HT_GD_USAGE_JOYSTICK              = 0x04,
  HT_GD_USAGE_GAMEPAD               = 0x05,
  HT_GD_USAGE_KEYBOARD              = 0x06,
  HT_GD_USAGE_KEYPAD                = 0x07,
  HT_GD_USAGE_MULTI_AXIS_CONTROLLER = 0x08
} HTGDUsage;

typedef enum {
  HT_WINDOW_NULL,           /* [--] Dummy window attribute              */
  HT_GL_ACCELERATED,        /* [RW] Renderer is hardware acceleration   */
  HT_GL_ACCUM_BUFFER,       /* [RW] Number of accumulation buffers      */
  HT_GL_ALPHA,              /* [RW] Number of alpha channel bits        */
  HT_GL_AUX_BUFFERS,        /* [RW] Number of auxilary buffers          */
  HT_GL_BACKING_STORE,      /* [RW] Back buffer is valid after swap     */
  HT_GL_BLUE,               /* [RW] Number of blue channel bits         */
  HT_GL_COLOR_BUFFER,       /* [R-] Number of color (RGBA) buffer bits  */
  HT_GL_DEPTH_BUFFER,       /* [RW] Number of depth buffer bits         */
  HT_GL_DOUBLE_BUFFERING,   /* [RW] Pixel format is double buffered     */
  HT_GL_GREEN,              /* [RW] Number of green channel bits        */
  HT_GL_MAJOR_VERSION,      /* [RW] Context major version               */
  HT_GL_MINOR_VERSION,      /* [RW] Context minor version               */
  HT_GL_PIXEL_TYPE,         /* [R-] Pixel format is true color          */
  HT_GL_PROFILE,            /* [R-] Core profile is used                */
  HT_GL_RED,                /* [RW] Number of red channel bits          */
  HT_GL_SAMPLE_BUFFERS,     /* [RW] Number of sample (MSAA) buffers     */
  HT_GL_SAMPLES,            /* [RW] Number of samples per sample buffer */
  HT_GL_STENCIL_BUFFER,     /* [RW] Number of stencil buffer bits       */
  HT_GL_STEREO,             /* [RW] Stereoscopic buffering is used      */
  HT_GL_SWAP_INTERVAL,      /* [RW] Vsync is enabled                    */
  HT_INPUT_KEYBOARD_CODE,   /* [R-] Last key pressed/released           */
  HT_INPUT_KEYBOARD_STATE,  /* [R-] State of the last key press/release */
  HT_INPUT_MOUSE_BUTTON,    /* [R-] Raw input for mouse button states   */
  HT_INPUT_MOUSE_RELATIVE,  /* [R-] Mouse uses relative positions       */
  HT_INPUT_MOUSE_X,         /* [R-] X position of mouse                 */
  HT_INPUT_MOUSE_Y,         /* [R-] Y position of mouse                 */
  HT_WINDOW_HEIGHT,         /* [RW] Height of the content area          */
  HT_WINDOW_STYLE,          /* [RW] Window style property values        */
  HT_WINDOW_TITLE,          /* [-W] Window title                        */
  HT_WINDOW_WIDTH,          /* [RW] Width of the content area           */
  HT_WINDOW_USER,           /* [RW] User-defined data to pass to window */
  HT_WINDOW_X,              /* [RW] X position of the top-left corner   */
  HT_WINDOW_Y,              /* [RW] Y position of the top-left corner   */
  HT_WINDOW_ATTRIBUTE_COUNT /* [--] Number of window attributes         */
} HTWindowAttribute;

typedef enum {
  HT_WINDOW_TITLED         = 0x01, /* Window has a title bar       */
  HT_WINDOW_CLOSABLE       = 0x02, /* Window has a close button    */
  HT_WINDOW_MINIATURIZABLE = 0x04, /* Window has a minimize button */
  HT_WINDOW_RESIZABLE      = 0x08, /* Window can be resized        */
  HT_WINDOW_STYLE_DEFAULT  = 0x0F  /* Window has all of the above  */
} HTWindowStyle;

/*------------------------------------------------------------------- STRUCTS */

typedef struct HTWindow HTWindow; /* Opaque pointer to window */

typedef struct htErrorInfo {
  char*    file;     /* File where the error occurred        */
  char*    function; /* Function where the error occurred    */
  unsigned line;     /* Line number where the error occurred */
  int      result;   /* Return value of the error            */
} htErrorInfo;

/*--------------------------------------------------------- FUNCTION POINTERS */

typedef void (*HTEventHandler)(HTWindow* window);
typedef void (*HTWindowErrorCallback)(htErrorInfo* info);

/*----------------------------------------------------------------- FUNCTIONS */

#ifdef __cplusplus
extern "C" {
#endif

int htCreateWindow(HTWindow**, short, short, unsigned short, unsigned short);
int htCreateGLContext(HTWindow*);
int htCreateInputManager(HTWindow*);
int htDestroyWindow(HTWindow**);
int htDestroyGLContext(HTWindow*);
int htDestroyInputManager(HTWindow*);
int htSetCurrentGLContext(HTWindow*);
int htSwapGLBuffers(HTWindow*);
int htPollWindowEvents(HTWindow*);
int htPollInputEvents(HTWindow*);
int htSetWindowInteger(HTWindow*, HTWindowAttribute, int);
int htSetWindowUntyped(HTWindow*, HTWindowAttribute, unsigned char*);
int htGetWindowInteger(HTWindow*, HTWindowAttribute, int*);
int htGetWindowUntyped(HTWindow*, HTWindowAttribute, unsigned char**);
int htSetEventHandler(HTWindow*, HTEvent, HTEventHandler);
int htSetWindowErrorCallback(HTWindowErrorCallback);

#ifdef __cplusplus
}
#endif

#endif
