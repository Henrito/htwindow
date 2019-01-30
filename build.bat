cl /W4 /LD /Iinclude src/win32/window.c gdi32.lib opengl32.lib user32.lib /link /DLL /OUT:lib/htwindow.dll
