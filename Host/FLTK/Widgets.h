/*
 File: Widgets.h

 The widgets of views that draw something themselves (see Links.h for the
 link classes that make them):

   nfl::TextView     clTextView (protoTextButton), clParagraphView: its
                     text in one font, in the fill and frame its viewFormat
                     asks for (DrawViewFormat).
   nfl::PictureView  clPictureView (protoPictureButton, protoClosebox): its
                     icon, a Newton bitmap.

 Both draw hilited (Hilite(true), TrackHilite) as the ROM does: inverted.
 Pen events go to the view's link (user_data(), Link::HandlePen()), which
 runs the view's scripts. The widgets hold no NewtonScript objects: they
 get what they draw when they are made, or from their link (its
 viewFormat, its frame's width, whether it is hilited).
 */

#ifndef HOST_FLTK_WIDGETS_H
#define HOST_FLTK_WIDGETS_H

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl_Group.H>
#include <FL/Fl_Bitmap.H>

#include "Frames/Objects.h"

#include <memory>
#include <string>
#include <vector>

namespace nfl {

/** A view's fill and frame, as its viewFormat asks (fill and frame
    colors, frame type, pen width, inset, shadow, roundness), in x, y, w, h:
    the ROM's outer bounds (CView::outerBounds): the view's bounds, its
    frame around them (Link::Outset(): inset + pen), and its shadow at the
    right and the bottom (Link::Shadow()). Every view widget's draw() calls
    it first (its box is VIEW_BOX, which draws nothing); the view's link
    keeps its viewFormat (Link::ViewFormat()). inHilited: draw it hilited;
    each widget decides whether it shows hiliting that way.
      a dragger or matte frame (protoFloater, an app's base view): Matt's
        FLOATER_BOX;
      else the shadow (gray), the fill (inverted if hilited: black, or
        white on a dark fill; black without a fill), and the frame, pen
        wide at the edge, round corners if round; the inset is the space
        between the frame and the view's bounds.
    inParts: the fill (kViewFill: first), the frame and shadow (kViewFrame:
    after the view's content and children, as the ROM's CView::draw:
    preDraw, content, children, postDraw; a child reaching out of its
    parent, as protoTitle does, is under the parent's frame), or both.
    Grays are solid (a Newton's are patterns). */
enum { kViewFill = 1, kViewFrame = 2, kViewFormat = 3 };
void DrawViewFormat(int x, int y, int w, int h, long inViewFormat, bool inHilited, int inParts = kViewFormat);

/** A view hilited with no viewHiliteScript (the ROM's CView::hilite): its
    bounds grown by its inset (inside its frame) inverted, rounded as its
    frame (less the pen), over all it shows, its children too (Drawing.h's
    difference blend mode). x, y, w, h: its widget, as DrawViewFormat's. */
void DrawHilite(int x, int y, int w, int h, long inViewFormat);

/** A view that draws something itself (its text, its icon). A group, as any
    view can have children: their widgets are in it, drawn after what the
    view shows (DrawChildrenAndFrame). Its pen events go to its view's link
    when its children don't take them. */
class ViewWidget : public Fl_Group
{
public:
  ViewWidget(int x, int y, int w, int h);
  // clear() here: its children's links delete them (see Group, Links.h)
  ~ViewWidget() override { clear(); }
  int handle(int inEvent) override;
protected:
  int delete_child(int inIndex) override;
  bool Hilited() const;
  /** The view's fill or frame (DrawViewFormat). */
  void DrawFormat(int inParts);
  /** After what the view shows, as the ROM's CView::draw: its children,
      its frame, what scripts drew (DrawOverlay), and its hilite. */
  void DrawChildrenAndFrame();
  /** The widget is the view's bounds and its frame around them: the
      frame's width (Link::Outset()). What the view shows goes inside. */
  int Inset() const;
  /** ... and the shadow's, at the right and the bottom (Link::Shadow()). */
  int Shadow() const;
};

/** clTextView, clParagraphView: its text, in its labelfont() and
    labelsize() (the view's viewFont), placed by its align() (its
    viewJustify; Link::ApplyStyle()). */
class TextView : public ViewWidget
{
public:
  TextView(int x, int y, int w, int h, const std::string & inText);
  /** viewTransferMode: 2 (XOR) inverts what is under the text (white on
      black, e.g. a message box); the others: black text. */
  void TransferMode(long inMode) { fXor = inMode == 2; redraw(); }
  const std::string & Text() const { return fText; }
  void Text(const std::string & inText) { fText = inText; redraw(); }
protected:
  void draw() override;
private:
  std::string fText;
  bool fXor = false;
};

/** A 1-bit bitmap from a Newton bitmap (an icon's bits): rows of rowBytes
    bytes, the leftmost pixel in the high bit. */
struct NewtonBitmap
{
  int width = 0, height = 0, rowBytes = 0;
  std::vector<unsigned char> bits;
};

/** An image of NewtonScript's as a NewtonBitmap: a bitmap frame (an icon,
    what GetPictAsBits gives: {bits, bounds, mask}; the mask is left out),
    its bits alone (a binary: 4 bytes, rowBytes, 2 bytes, then top, left,
    bottom, right, then the rows; all 16-bit big-endian), or a PICT of
    bitmaps (a binary of class 'picture, Host/Pict.h). Empty (width 0) for
    anything else. */
NewtonBitmap ToNewtonBitmap(RefArg inImage);

/** A NewtonBitmap as an FLTK image: an Fl_Bitmap that owns its data (its 1
    bits drawn in the current color, its 0 bits not drawn). nullptr if the
    bitmap is empty. The caller deletes it. */
Fl_Bitmap * ToFlImage(const NewtonBitmap & inBitmap);

/** clPictureView: its icon, placed by its align() (viewJustify): an image of
    the ROM's (RomImages.h; shared), or a Newton bitmap (from a package;
    drawn black). Hilited, it is inverted (DrawHilite). */
class PictureView : public ViewWidget
{
public:
  PictureView(int x, int y, int w, int h, Fl_Image * inImage);
  PictureView(int x, int y, int w, int h, const NewtonBitmap & inIcon);
  /** A new picture: an image, or a Newton bitmap. */
  void Image(Fl_Image * inImage);
  void Icon(const NewtonBitmap & inIcon);
  /** viewTransferMode: 0 copy (the icon's 0 bits white: the default), 1 or
      (only its 1 bits); the others as copy (FLTK has no raster ops). */
  void TransferMode(long inMode) { fCopy = inMode != 1; }
protected:
  void draw() override;
private:
  void DrawImage();
  bool fCopy = true;
  Fl_Image * fImage = nullptr;
  std::unique_ptr<Fl_Bitmap> fBitmap;   // from a Newton bitmap
};

} // namespace nfl

#endif // HOST_FLTK_WIDGETS_H
