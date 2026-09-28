/*
 File: Widgets.cc

 The widgets of views that draw something themselves. See Widgets.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include "Host/FLTK/Widgets.h"
#include "Host/FLTK/Boxtypes.h"

#include <algorithm>
#include "Host/FLTK/Links.h"
#include "Host/FLTK/Drawing.h"
#include "Host/Pict.h"
#include "ViewFlags.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "ROMResources.h"

namespace nfl {

namespace {

// The colors of viewFormat's fill and frame (vfWhite ... vfBlack).
Fl_Color NewtonColor(long inColor)
{
  switch (inColor) {
    case 1: return FL_WHITE;    // vfWhite
    case 2: return fl_rgb_color(192, 192, 192);   // vfLtGray
    case 3: return fl_rgb_color(128, 128, 128);    // vfGray
    case 4: return fl_rgb_color(64, 64, 64);    // vfDkGray
    default: return FL_BLACK;   // vfBlack, custom, matte
  }
}

} // namespace


void DrawViewFormat(int x, int y, int w, int h, long inViewFormat, bool inHilited, int inParts)
{
  // Color of inner fill or 0, or custom
  uint32_t fill = (inViewFormat & vfFillMask) >> vfFillShift;         // 4 bits
  // Color of outer frame or 0, or custom, or dragger, or matte
  uint32_t frame = (inViewFormat & vfFrameMask) >> vfFrameShift;      // 4 bits
  // Width in pixels of the frame, at the widget's edge (outside the view's bounds)
  uint32_t pen = (inViewFormat & vfPenMask) >> vfPenShift;            // 4 bits
  // Lines is used in text views to draw horizontal guides
  //uint32_t lines = (inViewFormat & vfLinesMask) >> vfLineShift;       // 4 bits
  // Space between the view's bounds and the inside of the frame
  uint32_t inset = (inViewFormat & vfInsetMask) >> vfInsetShift;      // 2 bits
  // Gray lines this wide at the bottom and the right, outside the frame
  uint32_t shadow = (inViewFormat & vfShadowMask) >> vfShadowShift;   // 2 bits
  // invert or bullet(?) or trienagle(?) - can;t be selected in NTK
  uint32_t hilite = (inViewFormat & vfHiliteMask) >> vfHiliteShift;   // 4 bits
  // Radius for the infill corners. The outer corners are affected by the roundness of the frame.
  uint32_t round = (inViewFormat & vfRoundMask) >> vfRoundShift;      // 4 bits

  // TODO: vfMatte: Thick gray frame bordered by a black frame, giving a matte effect.
  // TODO: vfDragge: Similar effect to vfFrameMatte, plus a small control nub in the
  //       top portion of the frame at the center.
  // The ROM (CView::postDraw): the frame pen wide in the frame's pattern (the
  // matte), then a black one 2 wide (4 on the view with the caret) on top,
  // same rounding; a dragger then its hook (ROM picture @691) centered at
  // the frame's top + 2.
  Fl_Color fillColor = fill ? NewtonColor(fill) : FL_WHITE;
  if (frame == vfDragger || frame == vfMatte) {   // a floater
    if (inParts & kViewFill) {
      fl_color(fillColor);
      fl_rectf(x, y, w, h);
    }
    if (inParts & kViewFrame)
      fl_draw_box(FLOATER_FRAME, x, y, w, h, fillColor);
    return;
  }

  // The widget is the ROM's outer bounds (CView::outerBounds): the view's
  // bounds, inset + pen around them (only a view with a pen has a frame),
  // and the shadow at the right and the bottom. The frame's bounds (the
  // ROM's frameBounds) are all of it but the shadow; the frame is at their
  // edge, pen wide, the inset between it and the view's bounds.
  (void)inset;   // it is only in the widget's size (FrameOutset, Links.cc)
  if (!pen)
    shadow = 0;
  int fw = w - shadow, fh = h - shadow;
  // where fl_rect() goes so that the pen covers the outer pen pixels: a
  // line p wide at column c covers c - (p-1)/2 ... c + p/2
  int d = (frame && pen) ? (pen-1)/2 : 0;
  int dd = (frame && pen) ? pen-1 : 0;
  // Draw the shadow at the bottom right of the widget first.
  if ((inParts & kViewFrame) && shadow && frame) {
    int ds = shadow - (shadow-1)/2;   // so that the line covers the last shadow pixels
    fl_color(fl_rgb_color(128, 128, 128));
    fl_line_style(0, shadow);
    fl_xyline(x+shadow+round, y+h-ds, x+w-ds, y+shadow+round);
    fl_line_style(0, 1);
  }
  // Draw the inside first
  if ((inParts & kViewFill) && (fill || inHilited)) {
    if (inHilited) { // Invert inner area to highlight it
      if ((fill>vfGray) && (fill <= vfBlack)) {
        fillColor = FL_WHITE;
      } else {
        fillColor = FL_BLACK;
      }
    }
    fl_color(fillColor);
    if (round)
      fl_rounded_rectf(x+d, y+d, fw-dd, fh-dd, round);
    else
      fl_rectf(x+d, y+d, fw-dd, fh-dd);
  }
  // Draw the frame. The outer lines of the frame touch the widget outline (plus shadow)
  if ((inParts & kViewFrame) && frame && pen) {
    fl_color(NewtonColor(frame));
    fl_line_style(0, pen);
    if (round)
      fl_rounded_rect(x+d, y+d, fw-dd, fh-dd, round);
    else
      fl_rect(x+d, y+d, fw-dd, fh-dd);
    fl_line_style(0, 0);
  }
  // Great indicatore for true widget sizes:
  // fl_color(FL_RED);
  // fl_focus_rect(x, y, w, h);
}


void DrawHilite(int x, int y, int w, int h, long inViewFormat)
{
  long frame = (inViewFormat & vfFrameMask) >> vfFrameShift;
  int pen = int((inViewFormat & vfPenMask) >> vfPenShift);
  int shadow = pen ? int((inViewFormat & vfShadowMask) >> vfShadowShift) : 0;
  int round = int((inViewFormat & vfRoundMask) >> vfRoundShift);
  int frameWidth = (frame && pen) ? pen : 0;
  if (frame == vfDragger || frame == vfMatte)
    frameWidth = 0;   // (a floater isn't hilited)
  if (pen > 1)
    round = std::max(0, round - (pen - 1));
  x += frameWidth;
  y += frameWidth;
  w -= 2 * frameWidth + shadow;
  h -= 2 * frameWidth + shadow;
  if (w <= 0 || h <= 0)
    return;
  fl_color(FL_WHITE);
  BlendInvert(true);
  if (round)   // (fl_rounded_rectf() fills a pixel less at the left and the top than fl_rectf())
    fl_rounded_rectf(x - 1, y - 1, w + 1, h + 1, round);
  else
    fl_rectf(x, y, w, h);
  BlendInvert(false);
}


ViewWidget::ViewWidget(int x, int y, int w, int h)
: Fl_Group(x, y, w, h)
{
  end();
  box(VIEW_BOX);
}


int ViewWidget::handle(int inEvent)
{
  Link * link = static_cast<Link *>(user_data());
  switch (inEvent) {
    case FL_PUSH:
      if (Fl_Group::handle(inEvent))
        return 1;   // a child took it
      return link ? link->HandlePen(this, inEvent) : 0;
    case FL_DRAG:
    case FL_RELEASE:
      // the view's own pen (FLTK clears Fl::pushed() before FL_RELEASE)
      if (link && link->HandlePen(this, inEvent))
        return 1;
      break;
  }
  return Fl_Group::handle(inEvent);
}


int ViewWidget::delete_child(int inIndex)
{
  return GroupLink::RemoveChild(this, inIndex);
}


bool ViewWidget::Hilited() const
{
  Link * link = static_cast<Link *>(user_data());
  return link && link->Hilited();
}


void ViewWidget::DrawFormat(int inParts)
{
  // (hilited: inverted at the end, DrawHilite)
  if (Link * link = static_cast<Link *>(user_data()))
    DrawViewFormat(x(), y(), w(), h(), link->ViewFormat(), false, inParts);
}


void ViewWidget::DrawChildrenAndFrame()
{
  for (int i = 0; i < children(); ++i)   // all of them (FloatNGo::draw)
    draw_child(*child(i));
  DrawFormat(kViewFrame);
  DrawOverlay(this);
  Link * link = static_cast<Link *>(user_data());
  if (link && link->Hilited())
    DrawHilite(x(), y(), w(), h(), link->ViewFormat());
}


int ViewWidget::Inset() const
{
  Link * link = static_cast<Link *>(user_data());
  return link ? link->Outset() : 0;
}


int ViewWidget::Shadow() const
{
  Link * link = static_cast<Link *>(user_data());
  return link ? link->Shadow() : 0;
}


TextView::TextView(int x, int y, int w, int h, const std::string & inText)
: ViewWidget(x, y, w, h), fText(inText)
{ }


void TextView::draw()
{
  DrawFormat(kViewFill);
  int inset = Inset(), shadow = Shadow();
  fl_font(labelfont(), labelsize());
  fl_color(fXor ? FL_WHITE : FL_BLACK);
  if (fXor)
    BlendInvert(true);
  fl_push_clip(x(), y(), w(), h());
  // in the view's bounds, inside its frame; draw_symbols 0: '@' is just a character
  fl_draw(fText.c_str(), x() + inset, y() + inset, w() - 2 * inset - shadow, h() - 2 * inset - shadow,
          align() | FL_ALIGN_INSIDE, nullptr, 0);
  fl_pop_clip();
  if (fXor)
    BlendInvert(false);
  RunDrawScript(this);
  DrawChildrenAndFrame();
}


PictureView::PictureView(int x, int y, int w, int h, Fl_Image * inImage)
: ViewWidget(x, y, w, h), fImage(inImage)
{ }


PictureView::PictureView(int x, int y, int w, int h, const NewtonBitmap & inIcon)
: ViewWidget(x, y, w, h)
{
  Icon(inIcon);
}


void PictureView::Image(Fl_Image * inImage)
{
  fBitmap.reset();
  fImage = inImage;
  redraw();
}


void PictureView::Icon(const NewtonBitmap & inIcon)
{
  Image(nullptr);
  fBitmap.reset(ToFlImage(inIcon));
  fImage = fBitmap.get();
}


NewtonBitmap ToNewtonBitmap(RefArg inImage)
{
  NewtonBitmap bitmap;
  if (IsBinary(inImage) && EQ(ClassOf(inImage), SYMA(picture))) {   // a PICT
    if (!pict::Decode((const unsigned char *)BinaryData(inImage), Length(inImage),
                      &bitmap.width, &bitmap.height, &bitmap.rowBytes, &bitmap.bits))
      bitmap = NewtonBitmap();
    return bitmap;
  }
  RefVar bits(IsFrame(inImage) ? GetFrameSlot(inImage, SYMA(bits)) : (Ref)inImage);
  if (!IsBinary(bits) || Length(bits) < 16)
    return bitmap;
  const unsigned char * data = (const unsigned char *)BinaryData(bits);
  auto word = [data](int offset) { return int(short((data[offset] << 8) | data[offset + 1])); };
  int rowBytes = word(4), top = word(8), left = word(10), bottom = word(12), right = word(14);
  if (rowBytes <= 0 || right <= left || bottom <= top
   || Length(bits) < ArrayIndex(16 + rowBytes * (bottom - top)))
    return bitmap;
  bitmap.width = right - left;
  bitmap.height = bottom - top;
  bitmap.rowBytes = rowBytes;
  bitmap.bits.assign(data + 16, data + 16 + rowBytes * bitmap.height);
  return bitmap;
}


Fl_Bitmap * ToFlImage(const NewtonBitmap & inBitmap)
{
  if (inBitmap.width <= 0 || inBitmap.height <= 0)
    return nullptr;
  // XBM, as Fl_Bitmap wants it: the leftmost pixel in the low bit
  int xbmRowBytes = (inBitmap.width + 7) / 8;
  uchar * xbm = new uchar[size_t(xbmRowBytes) * inBitmap.height];
  for (int row = 0; row < inBitmap.height; ++row)
    for (int i = 0; i < xbmRowBytes; ++i) {
      unsigned char b = inBitmap.bits[size_t(row) * inBitmap.rowBytes + i], r = 0;
      for (int bit = 0; bit < 8; ++bit)
        if (b & (0x80 >> bit))
          r |= 1 << bit;
      xbm[size_t(row) * xbmRowBytes + i] = r;
    }
  Fl_Bitmap * image = new Fl_Bitmap(xbm, inBitmap.width, inBitmap.height);
  image->alloc_array = 1;   // deletes xbm
  return image;
}


void PictureView::draw()
{
  DrawFormat(kViewFill);
  if (fImage)
    DrawImage();
  RunDrawScript(this);
  DrawChildrenAndFrame();
}


void PictureView::DrawImage()
{
  // in the view's bounds, inside its frame
  int inset = Inset(), shadow = Shadow();
  int bx = x() + inset, by = y() + inset, bw = w() - 2 * inset - shadow, bh = h() - 2 * inset - shadow;
  int px = bx + (bw - fImage->w()) / 2, py = by + (bh - fImage->h()) / 2;
  if (align() & FL_ALIGN_LEFT)
    px = bx;
  else if (align() & FL_ALIGN_RIGHT)
    px = bx + bw - fImage->w();
  if (align() & FL_ALIGN_TOP)
    py = by;
  else if (align() & FL_ALIGN_BOTTOM)
    py = by + bh - fImage->h();
  if (fCopy) {   // copy: the icon's 0 bits too (white)
    fl_color(FL_WHITE);
    fl_rectf(px, py, fImage->w(), fImage->h());
  }
  fl_color(FL_BLACK);   // a bitmap's color
  fImage->draw(px, py);
}

} // namespace nfl
