/*
 File: InvertMode.cc

 SetInvertMode() (Drawing.h): BlendInvert() on X11 and Windows. A file of
 its own: Xlib's and Windows' headers clash with the Newton's (GC, ...).
 */

#include <FL/Fl.H>
#include <FL/Fl_Graphics_Driver.H>
#include <FL/platform.H>

namespace nfl {

void SetInvertMode(bool inOn)
{
#if defined(_WIN32)
  if (HDC dc = (HDC)fl_graphics_driver->gc())
    SetROP2(dc, inOn ? R2_NOT : R2_COPYPEN);
#elif defined(FLTK_USE_X11)
  if (GC gc = (GC)fl_graphics_driver->gc())
    XSetFunction(fl_display, gc, inOn ? GXinvert : GXcopy);
#else
  (void)inOn;
#endif
}

} // namespace nfl
