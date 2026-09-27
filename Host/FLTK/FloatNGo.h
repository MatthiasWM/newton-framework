/*
 File: FloatNGo.h

 nfl::FloatNGo: a desktop window for a window-like Newton view (a child of
 the root view, like an app's base view; the first one was protoFloatNGo,
 a floating view with a close box). Closing the window (the window manager's
 close button, or Escape) sends the view Close(), as its close box does on a
 Newton. The NewtonOS look (its frame, ...) comes later.
 */

#ifndef HOST_FLTK_FLOATNGO_H
#define HOST_FLTK_FLOATNGO_H

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Image.H>

namespace nfl {

class Link;

/** The window holds the widgets of the view's children, which belong to
    their links: it doesn't delete them (see GroupLink in Links.h). */
class FloatNGo : public Fl_Double_Window
{
public:
  /** A window at x, y (screen coordinates) of w by h pixels for the view
      of inLink (user_data()). */
  FloatNGo(int x, int y, int w, int h, const char * inTitle, Link * inLink);

  // clear() here, not only in ~Fl_Group(): there, the object is an Fl_Group
  // already, and clear() would call Fl_Group::delete_child() (which deletes)
  ~FloatNGo() override;

  /** A picture behind the children (a clPictureView's icon, e.g. an
      alert's; shared, not deleted), at x, y in the window. */
  void Picture(Fl_Image * inPicture, int x, int y) { fPicture = inPicture; fPictureX = x; fPictureY = y; }

  /** Pen events the children don't take go to the view (a floater's
      dragger: DragWindow, Link::HandlePen). */
  int handle(int inEvent) override;

protected:
  int delete_child(int inIndex) override;
  void draw() override;

private:
  Fl_Image * fPicture = nullptr;
  int fPictureX = 0, fPictureY = 0;
};

} // namespace nfl

#endif // HOST_FLTK_FLOATNGO_H
