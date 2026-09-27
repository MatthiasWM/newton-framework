/*
 File: Widgets.cc

 The widgets of views that draw something themselves. See Widgets.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include "Host/FLTK/Widgets.h"
#include "Host/FLTK/Links.h"

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
    return FL_UP_BOX;       // the NewtonOS button (Boxtypes.cc)
  if (frame)
    return FL_BORDER_BOX;
  if (fill)
    return FL_FLAT_BOX;
  return FL_NO_BOX;
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
}


PictureView::PictureView(int x, int y, int w, int h, const NewtonBitmap & inIcon, Fl_Align inAlign)
: ViewWidget(x, y, w, h), fAlign(inAlign)
{
  if (inIcon.width <= 0 || inIcon.height <= 0)
    return;
  // XBM, as Fl_Bitmap wants it: the leftmost pixel in the low bit
  int xbmRowBytes = (inIcon.width + 7) / 8;
  fXbm.resize(size_t(xbmRowBytes) * inIcon.height);
  for (int row = 0; row < inIcon.height; ++row) {
    for (int i = 0; i < xbmRowBytes; ++i) {
      unsigned char b = inIcon.bits[size_t(row) * inIcon.rowBytes + i], r = 0;
      for (int bit = 0; bit < 8; ++bit)
        if (b & (0x80 >> bit))
          r |= 1 << bit;
      fXbm[size_t(row) * xbmRowBytes + i] = r;
    }
  }
  fImage = std::make_unique<Fl_Bitmap>(fXbm.data(), inIcon.width, inIcon.height);
}


void PictureView::draw()
{
  DrawBox();
  if (!fImage)
    return;
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
  fl_color(Hilited() ? FL_WHITE : FL_BLACK);
  fImage->draw(px, py);
}

} // namespace nfl
