/*
 File: FloatNGo.cc

 A desktop window for a window-like Newton view. See FloatNGo.h.
 */

#include <FL/fl_draw.H>

#include "Host/FLTK/FloatNGo.h"
#include "Host/FLTK/Links.h"
#include "Host/FLTK/Widgets.h"
#include "Host/FLTK/Boxtypes.h"
#include "Host/FLTK/Drawing.h"

namespace nfl {

FloatNGo::FloatNGo(int x, int y, int w, int h, const char * inTitle, Link * inLink)
: Fl_Double_Window(x, y, w, h)
{
  copy_label(inTitle);
  user_data(inLink);
  color(FL_WHITE); // NewtonOS background color
  box(VIEW_BOX);   // draw() draws the view's frame
  // closing the window closes the view: view:Close(), from the event loop
  // (no link: the view is closed already)
  callback([](Fl_Widget * w, void *) {
    if (Link * link = static_cast<Link *>(w->user_data()))
      link->SendClose();
  });
}


FloatNGo::~FloatNGo()
{
  clear();
}


int FloatNGo::handle(int inEvent)
{
  Link * link = static_cast<Link *>(user_data());
  switch (inEvent) {
    case FL_PUSH:
      if (Fl_Double_Window::handle(inEvent))
        return 1;   // a child took it
      return link ? link->HandlePen(this, inEvent) : 0;
    case FL_DRAG:
    case FL_RELEASE:
      if (link && link->HandlePen(this, inEvent))
        return 1;
      break;
  }
  return Fl_Double_Window::handle(inEvent);
}

void FloatNGo::resize(int X, int Y, int W, int H)
{
  Fl_Double_Window::resize(X, Y, W, H);
  if (Link * link = static_cast<Link *>(user_data()))
    link->UpdatePosition(this);
}

void FloatNGo::draw()
{
  // a window's own coordinates; its every pixel: white under the frame
  fl_rectf(0, 0, w(), h(), color());
  Link * link = static_cast<Link *>(user_data());
  if (link)
    DrawViewFormat(0, 0, w(), h(), link->ViewFormat(), false, kViewFill);
  if (fPicture)
    fPicture->draw(fPictureX, fPictureY);
  RunDrawScript(this);
  // all the children, not only the changed ones: FLTK draws a changed
  // widget with the window clipped to it, and white under it (above); the
  // views under and over it there must be drawn again too (views are
  // transparent where they have no fill: a picker's value is a widget over
  // its label's)
  for (int i = 0; i < children(); ++i)
    draw_child(*child(i));
  if (link)   // as the ROM: the frame over the children (protoTitle reaches into it)
    DrawViewFormat(0, 0, w(), h(), link->ViewFormat(), false, kViewFrame);
  DrawOverlay(this);
}


int FloatNGo::delete_child(int inIndex)
{
  return GroupLink::RemoveChild(this, inIndex);
}

} // namespace nfl
