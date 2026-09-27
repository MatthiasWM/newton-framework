/*
 File: Widgets.h

 The widgets of views that draw something themselves (see Links.h for the
 link classes that make them):

   nfl::TextView     clTextView (protoTextButton): its text in one font, in
                     the box its viewFormat asks for (Boxtypes.h).
   nfl::PictureView  clPictureView (protoPictureButton, protoClosebox): its
                     icon, a Newton bitmap.

 Both draw hilited (Hilite(true), TrackHilite) as the ROM does: inverted.
 Pen events go to the view's link (user_data(), Link::HandlePen()), which
 runs the view's scripts. The widgets hold no NewtonScript objects: they
 get what they draw when they are made.
 */

#ifndef HOST_FLTK_WIDGETS_H
#define HOST_FLTK_WIDGETS_H

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl_Widget.H>
#include <FL/Fl_Bitmap.H>

#include <memory>
#include <string>
#include <vector>

namespace nfl {

/** The box for a view's viewFormat (fill, frame, pen, roundness): FL_UP_BOX
    for a rounded black frame (a button), FL_FLAT_BOX for a fill only,
    FL_BORDER_BOX for a square frame, FL_NO_BOX for neither. *outColor: the
    fill color. */
Fl_Boxtype BoxForFormat(long inViewFormat, Fl_Color * outColor);

/** A view's fill and frame (viewFormat: fill, frame colors, pen width,
    roundness) in x, y, w, h (the view's bounds and its frame around them:
    the frame is outside the bounds, as on a Newton). Grays are solid (a
    Newton's are patterns). */
void DrawViewFormat(int x, int y, int w, int h, long inViewFormat);

/** A widget that sends its pen events to its view's link. */
class ViewWidget : public Fl_Widget
{
public:
  ViewWidget(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) { }
  int handle(int inEvent) override;
  /** The widget is the view's bounds and its frame around them (a
      NewtonOS frame is outside the bounds): the frame's width. What the
      view shows goes inside. */
  void FrameInset(int inPixels) { fInset = inPixels; }
protected:
  bool Hilited() const;
  /** The box, hilited if the view is: FL_DOWN_BOX for FL_UP_BOX, else
      filled black. */
  void DrawBox();
  int fInset = 0;
};

/** clTextView: one line (or more) of text in one font. */
class TextView : public ViewWidget
{
public:
  TextView(int x, int y, int w, int h, const std::string & inText,
           Fl_Font inFont, Fl_Fontsize inSize, Fl_Align inAlign);
  const std::string & Text() const { return fText; }
  void Text(const std::string & inText) { fText = inText; redraw(); }
protected:
  void draw() override;
private:
  std::string fText;
  Fl_Font fFont;
  Fl_Fontsize fSize;
  Fl_Align fAlign;
};

/** A 1-bit bitmap from a Newton bitmap (an icon's bits): rows of rowBytes
    bytes, the leftmost pixel in the high bit. */
struct NewtonBitmap
{
  int width = 0, height = 0, rowBytes = 0;
  std::vector<unsigned char> bits;
};

/** clPictureView: its icon, placed by inAlign (viewJustify): an image of
    the ROM's (RomImages.h; shared, with an inverted one for hiliting), or
    a Newton bitmap (from a package; drawn black, white when hilited). */
class PictureView : public ViewWidget
{
public:
  PictureView(int x, int y, int w, int h, Fl_Image * inImage, Fl_Image * inHilited, Fl_Align inAlign);
  PictureView(int x, int y, int w, int h, const NewtonBitmap & inIcon, Fl_Align inAlign);
  /** viewTransferMode: 0 copy (the icon's 0 bits white: the default), 1 or
      (only its 1 bits); the others as copy (FLTK has no raster ops). */
  void TransferMode(long inMode) { fCopy = inMode != 1; }
protected:
  void draw() override;
private:
  void DrawImage();
  bool fCopy = true;
  Fl_Image * fImage = nullptr;
  Fl_Image * fHilitedImage = nullptr;
  std::vector<unsigned char> fXbm;    // Fl_Bitmap doesn't copy its data
  std::unique_ptr<Fl_Bitmap> fBitmap;
  Fl_Align fAlign;
};

} // namespace nfl

#endif // HOST_FLTK_WIDGETS_H
