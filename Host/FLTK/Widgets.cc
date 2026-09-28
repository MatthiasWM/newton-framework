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

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "ROMResources.h"

namespace nfl {

namespace {

// The colors of viewFormat's fill and frame (vfWhite ... vfBlack).
Fl_Color NewtonColor(long inColor)
{
  switch (inColor) {
    case 1: return FL_WHITE;                   // vfWhite
    case 2: return fl_rgb_color(192, 192, 192); // vfLtGray
    case 3: return fl_rgb_color(128, 128, 128); // vfGray
    case 4: return fl_rgb_color(64, 64, 64);   // vfDkGray
    default: return FL_BLACK;                  // vfBlack, custom, matte
  }
}

} // namespace


Fl_Boxtype BoxForFormat(long inViewFormat, Fl_Color * outColor)
{
  long fill = inViewFormat & 0x0F;             // vfFillMask
  long frame = (inViewFormat >> 4) & 0x0F;     // vfFrameMask
  long round = (inViewFormat >> 24) & 0x0F;    // vfRoundMask
  *outColor = fill ? NewtonColor(fill) : FL_WHITE;
  if (frame && round)
    return nfl::UP_BOX;       // the NewtonOS button (Boxtypes.cc)
  if (frame)
    return FL_BORDER_BOX;
  if (fill)
    return FL_FLAT_BOX;
  return FL_NO_BOX;
}


void DrawViewFormat(int x, int y, int w, int h, long inViewFormat)
{
  long fill = inViewFormat & 0x0F;             // vfFillMask
  long frame = (inViewFormat >> 4) & 0x0F;     // vfFrameMask
  int pen = int((inViewFormat >> 8) & 0x0F);   // vfPenMask
  int round = int((inViewFormat >> 24) & 0x0F);
  if (fill) {
    fl_color(NewtonColor(fill));
    if (round) fl_rounded_rectf(x, y, w, h, round);
    else fl_rectf(x, y, w, h);
  }
  if (frame && pen > 0) {   // outside the bounds: the widget's edge, pen wide
    fl_color(NewtonColor(frame));
    for (int i = 0; i < pen && 2 * i < std::min(w, h); ++i) {
      if (round) fl_rounded_rect(x + i, y + i, w - 2 * i, h - 2 * i, round);
      else fl_rect(x + i, y + i, w - 2 * i, h - 2 * i);
    }
  }
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


// The ROM inverts a hilited view. A button's box has its own look for that.
void ViewWidget::DrawBox()
{
  if (!Hilited())
    draw_box();
  else if (box() == FL_UP_BOX)
    draw_box(FL_DOWN_BOX, color());
  else
    fl_rectf(x(), y(), w(), h(), FL_BLACK);
}


TextView::TextView(int x, int y, int w, int h, const std::string & inText,
                   Fl_Font inFont, Fl_Fontsize inSize, Fl_Align inAlign)
: ViewWidget(x, y, w, h), fText(inText), fFont(inFont), fSize(inSize), fAlign(inAlign)
{ }


void TextView::draw()
{
  DrawBox();
  fl_font(fFont, fSize);
  fl_color(Hilited() ? FL_WHITE : FL_BLACK);
  fl_push_clip(x(), y(), w(), h());
  // in the view's bounds, inside its frame; draw_symbols 0: '@' is just a character
  fl_draw(fText.c_str(), x() + fInset, y() + fInset, w() - 2 * fInset, h() - 2 * fInset,
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
  DrawBox();
  if (fImage)
    DrawImage();
  RunDrawScript(this);
  DrawOverlay(this);
}


void PictureView::DrawImage()
{
  // in the view's bounds, inside its frame
  int bx = x() + fInset, by = y() + fInset, bw = w() - 2 * fInset, bh = h() - 2 * fInset;
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
