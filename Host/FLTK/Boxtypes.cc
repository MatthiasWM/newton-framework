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

void DrawBox(int x, int y, int w, int h, Fl_Color c)
{
  fl_color(c);
  fl_rectf(x, y, w, h);
}

void DrawUpFrame(int x, int y, int w, int h, Fl_Color c)
{
  constexpr int oft = 2;
  constexpr int oft2 = oft/2;
  constexpr int oftr = 3;
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

void DrawUpBox(int x, int y, int w, int h, Fl_Color c)
{
  DrawBox(x, y, w, h, c);
  DrawUpFrame(x, y, w, h, FL_BLACK);
}

void DrawDownFrame(int x, int y, int w, int h, Fl_Color c)
{
  constexpr int oft = 0;
  constexpr int oft2 = 0;
  constexpr int oftr = 4;
  // -- outer frame
  fl_color(FL_BLACK);
  fl_line_style(FL_SOLID, oft);
  fl_begin_polygon();
  double x0 = x+oftr+oft2-0.5;
  double y0 = y+oftr+oft2-0.5;
  double x2 = x+w-oftr-oft2-0.5;
  double y2 = y+h-oftr-oft2-0.5;
  fl_arc(x2, y0, oftr, 90.0, 0.0);
  fl_arc(x2, y2, oftr, 0.0, -90.0);
  fl_arc(x0, y2, oftr, 270.0, 180.0);
  fl_arc(x0, y0, oftr, 180.0, 90.0);
  fl_end_polygon();
}

void DrawDownBox(int x, int y, int w, int h, Fl_Color c)
{
  DrawBox(x, y, w, h, c);
  DrawDownFrame(x, y, w, h, FL_BLACK);
}


// FLOATER_BOX:
// A rounded black frame with a gray inner frame and a white background.
void DrawFloaterFrame(int x, int y, int w, int h, Fl_Color c)
{
  constexpr int ift = 5;
  constexpr int oft = 2;
  constexpr int oft2 = oft/2;
  constexpr int oftr = 3;
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
  DrawUpFrame(x, y, w, h, c);
}

void DrawFloaterBox(int x, int y, int w, int h, Fl_Color c)
{
  DrawBox(x, y, w, h, c);
  DrawFloaterFrame(x, y, w, h, FL_BLACK);
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
  Fl::set_boxtype(FLOATER_BOX, DrawFloaterBox, 8, 8, 16, 16);
  // A typical push button box and down box
  // Note: we will need another box for the default "Enter" button
  // Note: yes, NewtonOS offers to customize the frame radius and thickness.
  //       We will have to think about that when it actually occurs. Our drawing
  //       Code is parametric, so we can change that later.
  // Note: drawing the down box requires toggeling labelcolor!
  Fl::set_boxtype(FL_UP_BOX, DrawUpBox, 4, 4, 8, 8);
  Fl::set_boxtype(FL_DOWN_BOX, DrawDownBox, 4, 4, 8, 8);
}

} // namespace nfl
