/*
 File: FloatNGo.cc

 A desktop window for a window-like Newton view. See FloatNGo.h.
 */

#include "Host/FLTK/FloatNGo.h"
#include "Host/FLTK/Links.h"
#include "Host/FLTK/Boxtypes.h"

namespace nfl {

FloatNGo::FloatNGo(int x, int y, int w, int h, const char * inTitle, Link * inLink)
: Fl_Double_Window(x, y, w, h)
{
  copy_label(inTitle);
  user_data(inLink);
  color(FL_WHITE); // NewtonOS background color
  box(FLOATER_BOX);
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


void FloatNGo::draw()
{
  if (fPicture == nullptr) {
    Fl_Double_Window::draw();
    return;
  }
  draw_box();
  fPicture->draw(fPictureX, fPictureY);
  draw_children();
}


int FloatNGo::delete_child(int inIndex)
{
  return GroupLink::RemoveChild(this, inIndex);
}

} // namespace nfl
