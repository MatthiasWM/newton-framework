/*
 File: Boxtypes.cc

 FLTK box types for the NewtonOS look. See Boxtypes.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include "Host/FLTK/Boxtypes.h"

namespace nfl {

namespace {

// FLOATER_BOX:
// Arounded black frame with a gray inner frame and a white background.
void DrawFloaterBox(int x, int y, int w, int h, Fl_Color c)
{
  constexpr int ift = 5;
  constexpr int oft = 2;
  constexpr int oft2 = oft/2;
  constexpr int oftr = 4;
  // -- background
  fl_color(c);
  fl_rectf(x, y, w, h);
  // -- inner frame
  fl_color(FL_GRAY);
  // top
  fl_rectf(x+oft, y+oft, w-2*oft, ift);
  // bottom
  fl_rectf(x+oft, y+h-ift-oft, w-2*oft, ift);
  // left
  fl_rectf(x+oft, y+oft, ift, h-2*oft);
  // right
  fl_rectf(x+w-ift-oft, y+oft, ift, h-2*oft);
  // -- outer frame
  fl_color(FL_BLACK);
  fl_line_style(FL_SOLID, oft);
  fl_begin_loop();
  double x0 = x+oftr+oft2-0.5;
  double y0 = y+oftr+oft2-0.5;
  double x2 = x+w-oftr-oft2-0.5;
  double y2 = y+h-oftr-oft2-0.5;
  fl_arc(x2, y0, oftr, 90.0, 0.0);
  fl_arc(x2, y2, oftr, 0.0, -90.0);
  fl_arc(x0, y2, oftr, 270.0, 180.0);
  fl_arc(x0, y0, oftr, 180.0, 90.0);
  fl_end_loop();
  fl_line_style(FL_SOLID, 1.0);
}

} // namespace


void RegisterBoxtypes()
{
  static bool registered = false;
  if (registered)
    return;
  registered = true;
  // dx, dy, dw, dh: how much of the box is frame (left, top, and the width
  // and height taken off), for the widget's inside
  Fl::set_boxtype(FLOATER_BOX, DrawFloaterBox, 1, 1, 2, 2);
}

} // namespace nfl
