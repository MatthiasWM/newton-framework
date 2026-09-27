/*
 File: FloatNGo.cc

 A desktop window for a window-like Newton view. See FloatNGo.h.
 */

#include "Host/FLTK/FloatNGo.h"
#include "Host/FLTK/Links.h"

namespace nfl {

FloatNGo::FloatNGo(int x, int y, int w, int h, const char * inTitle, Link * inLink)
: Fl_Double_Window(x, y, w, h)
{
  copy_label(inTitle);
  user_data(inLink);
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


int FloatNGo::delete_child(int inIndex)
{
  return GroupLink::RemoveChild(this, inIndex);
}

} // namespace nfl
