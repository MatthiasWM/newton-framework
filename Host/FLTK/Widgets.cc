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


void DrawViewFormat(int x, int y, int w, int h, long inViewFormat, bool inHilited)
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
    fl_draw_box(FLOATER_BOX, x, y, w, h, fillColor);
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
  if (shadow && frame) {
    int ds = shadow - (shadow-1)/2;   // so that the line covers the last shadow pixels
    fl_color(fl_rgb_color(128, 128, 128));
    fl_line_style(0, shadow);
    fl_xyline(x+shadow+round, y+h-ds, x+w-ds, y+shadow+round);
    fl_line_style(0, 1);
  }
  // Draw the inside first
  if (fill || inHilited) {
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
  if (frame && pen) {
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


ViewWidget::ViewWidget(int x, int y, int w, int h)
: Fl_Widget(x, y, w, h)
{
  box(VIEW_BOX);
}


int ViewWidget::handle(int inEvent)
{
  Link * link = static_cast<Link *>(user_data());
  switch (inEvent) {
    case FL_PUSH:
    case FL_DRAG:
    case FL_RELEASE:
      if (link)
        return link->HandlePen(this, inEvent);
      break;
  }
  return Fl_Widget::handle(inEvent);
}


bool ViewWidget::Hilited() const
{
  Link * link = static_cast<Link *>(user_data());
  return link && link->Hilited();
}


void ViewWidget::DrawFormat()
{
  if (Link * link = static_cast<Link *>(user_data()))
    DrawViewFormat(x(), y(), w(), h(), link->ViewFormat(), link->Hilited());
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


TextView::TextView(int x, int y, int w, int h, const std::string & inText,
                   Fl_Font inFont, Fl_Fontsize inSize, Fl_Align inAlign)
: ViewWidget(x, y, w, h), fText(inText), fFont(inFont), fSize(inSize), fAlign(inAlign)
{ }


void TextView::draw()
{
  DrawFormat();
  int inset = Inset(), shadow = Shadow();
  fl_font(fFont, fSize);
  fl_color(Hilited() ? FL_WHITE : FL_BLACK);
  fl_push_clip(x(), y(), w(), h());
  // in the view's bounds, inside its frame; draw_symbols 0: '@' is just a character
  fl_draw(fText.c_str(), x() + inset, y() + inset, w() - 2 * inset - shadow, h() - 2 * inset - shadow,
          fAlign | FL_ALIGN_INSIDE, nullptr, 0);
  fl_pop_clip();
  RunDrawScript(this);
  DrawOverlay(this);
}


PictureView::PictureView(int x, int y, int w, int h, Fl_Image * inImage, Fl_Image * inHilited, Fl_Align inAlign)
: ViewWidget(x, y, w, h), fImage(inImage), fHilitedImage(inHilited), fAlign(inAlign)
{ }


PictureView::PictureView(int x, int y, int w, int h, const NewtonBitmap & inIcon, Fl_Align inAlign)
: ViewWidget(x, y, w, h), fAlign(inAlign)
{
  Icon(inIcon);
}


void PictureView::Images(Fl_Image * inImage, Fl_Image * inHilited)
{
  fBitmap.reset();
  fImage = inImage;
  fHilitedImage = inHilited;
  redraw();
}


void PictureView::Icon(const NewtonBitmap & inIcon)
{
  Images(nullptr, nullptr);
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
  DrawFormat();
  if (fImage)
    DrawImage();
  RunDrawScript(this);
  DrawOverlay(this);
}


void PictureView::DrawImage()
{
  // in the view's bounds, inside its frame
  int inset = Inset(), shadow = Shadow();
  int bx = x() + inset, by = y() + inset, bw = w() - 2 * inset - shadow, bh = h() - 2 * inset - shadow;
  int px = bx + (bw - fImage->w()) / 2, py = by + (bh - fImage->h()) / 2;
  if (fAlign & FL_ALIGN_LEFT)
    px = bx;
  else if (fAlign & FL_ALIGN_RIGHT)
    px = bx + bw - fImage->w();
  if (fAlign & FL_ALIGN_TOP)
    py = by;
  else if (fAlign & FL_ALIGN_BOTTOM)
    py = by + bh - fImage->h();
  bool hilited = Hilited();
  Fl_Image * image = (hilited && fHilitedImage) ? fHilitedImage : fImage;
  if (fCopy) {   // copy: the icon's 0 bits too (white, black when hilited)
    fl_color(hilited ? FL_BLACK : FL_WHITE);
    fl_rectf(px, py, image->w(), image->h());
  }
  fl_color(hilited ? FL_WHITE : FL_BLACK);   // a bitmap's color
  image->draw(px, py);
}

} // namespace nfl
