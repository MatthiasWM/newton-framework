/*
 File: Drawing.cc

 Drawing shapes on views. See Drawing.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Graphics_Driver.H>
#include <FL/Fl_Bitmap.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/fl_draw.H>

#include "Host/FLTK/Drawing.h"
#include "Host/FLTK/Fonts.h"
#include "Host/FLTK/Links.h"
#include "Host/FLTK/Widgets.h"
#include "Host/Shapes.h"
#include "Host/Pict.h"
#include "Matt/EventLoop.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"
#include "Frames/Funcs.h"
#include "Frames/Lookup.h"
#include "Frames/Globals.h"
#include "ROMResources.h"

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <vector>
#include <functional>
#include <memory>

namespace nfl {

namespace {

// A viewDrawScript runs: its drawing goes directly onto this widget.
Fl_Widget * gDirect = nullptr;

enum { kModeCopy = 0, kModeOr = 1, kModeXor = 2, kModeBic = 3 };

struct Style
{
  int penSize = 1;
  bool pen = true;
  int penGray = 0;
  bool fill = false;
  int fillGray = 255;
  int mode = kModeCopy;
  long dx = 0, dy = 0;
  Fl_Font font = FONT_SYSTEM;   // Fonts.h
  Fl_Fontsize size = 10;
  Fl_Align align = FL_ALIGN_TOP_LEFT;
};

// A pattern's gray (0 black ... 255 white), or -1 for none.
int PatternGray(RefArg inPattern)
{
  if (ISINT(inPattern)) {
    switch (RINT(inPattern)) {
      case 1: return 255;   // vfWhite
      case 2: return 191;   // vfLtGray
      case 3: return 128;   // vfGray
      case 4: return 64;    // vfDkGray
      case 5: return 0;     // vfBlack
      default: return -1;   // vfNone
    }
  }
  if (IsBinary(inPattern) && Length(inPattern) >= 8) {   // 8 by 8 bits
    const unsigned char * bits = (const unsigned char *)BinaryData(inPattern);
    int set = 0;
    for (int i = 0; i < 8; ++i)
      for (int b = 0; b < 8; ++b)
        set += (bits[i] >> b) & 1;
    return 255 - 255 * set / 64;
  }
  return -1;
}

void Apply(Style & ioStyle, RefArg inFrame)
{
  if (!IsFrame(inFrame))
    return;
  if (FrameHasSlot(inFrame, SYMA(penSize))) {
    RefVar v(GetFrameSlot(inFrame, SYMA(penSize)));
    ioStyle.penSize = ISINT(v) ? int(RINT(v)) : 1;
  }
  if (FrameHasSlot(inFrame, SYMA(penPattern))) {
    int gray = PatternGray(GetFrameSlot(inFrame, SYMA(penPattern)));
    ioStyle.pen = gray >= 0;
    ioStyle.penGray = std::max(gray, 0);
  }
  if (FrameHasSlot(inFrame, SYMA(fillPattern))) {
    int gray = PatternGray(GetFrameSlot(inFrame, SYMA(fillPattern)));
    ioStyle.fill = gray >= 0;
    ioStyle.fillGray = std::max(gray, 0);
  }
  if (FrameHasSlot(inFrame, SYMA(transferMode))) {
    RefVar v(GetFrameSlot(inFrame, SYMA(transferMode)));
    ioStyle.mode = ISINT(v) ? int(RINT(v)) : kModeCopy;
  }
  RefVar transform(GetFrameSlot(inFrame, SYMA(transform)));
  if (IsArray(transform) && Length(transform) == 2
   && ISINT(GetArraySlot(transform, 0)) && ISINT(GetArraySlot(transform, 1))) {
    ioStyle.dx += RINT(GetArraySlot(transform, 0));
    ioStyle.dy += RINT(GetArraySlot(transform, 1));
  }
  if (FrameHasSlot(inFrame, SYMA(font)))
    FontFromSpec(GetFrameSlot(inFrame, SYMA(font)), &ioStyle.font, &ioStyle.size);
  if (FrameHasSlot(inFrame, SYMA(justification))) {
    RefVar j(GetFrameSlot(inFrame, SYMA(justification)));
    ioStyle.align = EQ(j, SYMA(center)) ? FL_ALIGN_TOP : EQ(j, SYMA(right)) ? FL_ALIGN_TOP_RIGHT : FL_ALIGN_TOP_LEFT;
  }
}

// Where Draw() draws (DrawOnView): straight onto the widget (in its
// viewDrawScript), or onto the view's canvas, its mask (every pixel drawn
// black) or its invert layer (what XOR shapes cover, white).
enum Target { kDirect, kCanvas, kMask, kInvert };
Target gTarget = kDirect;
bool gInverting = false;   // drawing an XOR shape (Gray: white)

Fl_Color Gray(int inGray)
{
  if (gTarget == kMask)
    return FL_BLACK;
  if (gInverting)
    return FL_WHITE;
  return fl_rgb_color(uchar(inGray), uchar(inGray), uchar(inGray));
}

// Whether a shape in transfer mode inMode goes where Draw() draws now: XOR
// only to the invert layer (and straight onto a widget), the others not.
bool DrawsHere(int inMode)
{
  if (gTarget == kDirect)
    return true;
  return (inMode == kModeXor) == (gTarget == kInvert);
}

// A bitmap at x, y: in copy mode its zero bits white, else only its one bits.
void DrawBitmap(Fl_Bitmap * inBitmap, int x, int y, int w, int h, int inMode)
{
  if (!DrawsHere(inMode))
    return;
  bool inverting = inMode == kModeXor;
  if (inMode == kModeCopy) {
    fl_color(gTarget == kMask ? FL_BLACK : FL_WHITE);
    fl_rectf(x, y, w, h);
  }
  fl_color(gTarget == kMask ? FL_BLACK : inverting ? FL_WHITE : FL_BLACK);
  if (inverting)
    BlendInvert(true);
  inBitmap->draw(x, y);
  if (inverting)
    BlendInvert(false);
}

// One shape (or a list) at origin ox, oy (where the view's 0, 0 is).
// Where: gTarget (DrawOnView).
void Draw(RefArg inShape, Style style, long ox, long oy)
{
  if (IsArray(inShape)) {
    for (ArrayIndex i = 0, n = Length(inShape); i < n; ++i) {
      RefVar item(GetArraySlot(inShape, i));
      if (shapes::IsStyleFrame(item))
        Apply(style, item);   // for the shapes after it
      else if (NOTNIL(item))
        Draw(item, style, ox, oy);
    }
    return;
  }
  if (!shapes::IsPrimShape(inShape))
    return;
  RefVar cls(ClassOf(inShape));
  bool bitmapShape = EQ(cls, SYMA(bitmap)) || EQ(cls, SYMA(picture));
  if (!bitmapShape && !DrawsHere(style.mode))   // (DrawBitmap decides for itself)
    return;
  gInverting = !bitmapShape && style.mode == kModeXor;
  if (gInverting)
    BlendInvert(true);
  ox += style.dx;
  oy += style.dy;
  shapes::Box b = shapes::Bounds(inShape);
  int x = int(ox + b.left), y = int(oy + b.top), w = int(b.right - b.left), h = int(b.bottom - b.top);
  int p = std::max(style.penSize, 0);
  fl_line_style(FL_SOLID, p);
  if (EQ(cls, SYMA(rectangle))) {
    if (style.fill) { fl_color(Gray(style.fillGray)); fl_rectf(x, y, w, h); }
    if (style.pen && p) {
      fl_color(Gray(style.penGray));
      for (int i = 0; i < p && 2 * i < std::min(w, h); ++i)   // the pen inside the shape
        fl_rect(x + i, y + i, w - 2 * i, h - 2 * i);
    }
  } else if (EQ(cls, SYMA(oval))) {
    if (style.fill) { fl_color(Gray(style.fillGray)); fl_pie(x, y, w, h, 0, 360); }
    if (style.pen && p) { fl_color(Gray(style.penGray)); fl_arc(x, y, w, h, 0, 360); }
  } else if (EQ(cls, SYMA(roundRectangle))) {
    int r = int(shapes::GetShort(inShape, 8) / 2);
    if (style.fill) { fl_color(Gray(style.fillGray)); fl_rounded_rectf(x, y, w, h, r); }
    if (style.pen && p) { fl_color(Gray(style.penGray)); fl_rounded_rect(x, y, w, h, r); }
  } else if (EQ(cls, SYMA(wedge))) {
    // Newton: 0 degrees up, clockwise; FLTK: 0 to the right, counterclockwise
    double start = shapes::GetShort(inShape, 8), arc = shapes::GetShort(inShape, 10);
    double a1 = 90 - start - arc, a2 = 90 - start;
    if (style.fill) { fl_color(Gray(style.fillGray)); fl_pie(x, y, w, h, a1, a2); }
    if (style.pen && p) { fl_color(Gray(style.penGray)); fl_arc(x, y, w, h, a1, a2); }
  } else if (EQ(cls, SYMA(line))) {
    shapes::Box l = shapes::GetBox(inShape);   // y1, x1, y2, x2
    if (style.pen && p) {
      fl_color(Gray(style.penGray));
      fl_line(int(ox + l.left), int(oy + l.top), int(ox + l.right), int(oy + l.bottom));
    }
  } else if (EQ(cls, SYMA(polygon))) {
    RefVar data(GetProtoVariable(inShape, SYMA(data)));
    long count = (Length(data) - 12) / 4;
    auto vertices = [&]() {
      for (long i = 0; i < count; ++i)
        fl_vertex(double(ox + shapes::GetShort(data, 12 + i * 4 + 2)), double(oy + shapes::GetShort(data, 12 + i * 4)));
    };
    if (style.fill) { fl_color(Gray(style.fillGray)); fl_begin_complex_polygon(); vertices(); fl_end_complex_polygon(); }
    if (style.pen && p) { fl_color(Gray(style.penGray)); fl_begin_loop(); vertices(); fl_end_loop(); }
  } else if (EQ(cls, SYMA(text))) {
    RefVar text(GetProtoVariable(inShape, SYMA(data)));   // class 'textData
    if (IsBinary(text)) {
      fl_font(style.font, style.size);
      fl_color(Gray(style.penGray));
      RefVar string(Clone(text));   // a string of another class
      SetClass(string, SYMA(string));
      fl_draw(UTF8FromString(string).c_str(), x, y, w, h, style.align | FL_ALIGN_INSIDE | FL_ALIGN_WRAP, nullptr, 0);
    }
  } else if (EQ(cls, SYMA(bitmap)) || EQ(cls, SYMA(picture))) {
    RefVar data(GetProtoVariable(inShape, SYMA(data)));
    NewtonBitmap bits;   // a picture's data: a PICT, whatever its class
    if (EQ(cls, SYMA(bitmap)))
      bits = ToNewtonBitmap(data);
    else if (IsBinary(data) && !pict::Decode((const unsigned char *)BinaryData(data), Length(data),
                                             &bits.width, &bits.height, &bits.rowBytes, &bits.bits))
      bits = NewtonBitmap();
    std::unique_ptr<Fl_Bitmap> bitmap(ToFlImage(bits));
    if (bitmap) {
      bitmap->scale(w, h, 0, 1);   // drawn at the shape's bounds
      DrawBitmap(bitmap.get(), x, y, w, h, style.mode);
    }
  }
  fl_line_style(0);
  if (gInverting)
    BlendInvert(false);
  gInverting = false;
}

Link * LinkOfWidget(Fl_Widget * inWidget)
{
  return inWidget ? static_cast<Link *>(inWidget->user_data()) : nullptr;
}

} // namespace


// What follows inverts what is under it, drawn in white (a Newton's XOR with
// black): the difference blend mode, |white - D| = 1 - D. XOR shapes go to
// the invert layer (Link::fInvert: drawn twice, a shape is gone again),
// which DrawOverlay draws so; in a viewDrawScript onto the widget.
void BlendInvert(bool inOn)
{
#if defined(__APPLE__)
  if (CGContextRef gc = (CGContextRef)fl_graphics_driver->gc())
    CGContextSetBlendMode(gc, inOn ? kCGBlendModeDifference : kCGBlendModeNormal);
#else
  // FLTK has no blend mode (an issue is open at github.com/fltk/fltk): until
  // it does, XOR (transferMode 2) is macOS only
#error "BlendInvert(): XOR drawing needs a difference blend mode in FLTK"
#endif
}


// Text (CoreText) ignores the blend mode: what inDraw draws (in black, with
// the area's top left at ox, oy) goes into an image first, whose coverage
// then inverts x, y, w, h (a white image in the blend mode).
void DrawInverted(int x, int y, int w, int h, const std::function<void(int, int)> & inDraw)
{
  if (w <= 0 || h <= 0)
    return;
  Fl_Image_Surface surface(w, h, 1);
  Fl_Surface_Device::push_current(&surface);
  fl_color(FL_WHITE);
  fl_rectf(0, 0, w, h);
  fl_color(FL_BLACK);
  inDraw(0, 0);
  Fl_Surface_Device::pop_current();
  std::unique_ptr<Fl_RGB_Image> drawn(surface.image());
  int dw = drawn->data_w(), dh = drawn->data_h(), d = drawn->d();
  int ld = drawn->ld() ? drawn->ld() : dw * d;
  const uchar * c = (const uchar *)drawn->data()[0];
  uchar * rgba = new uchar[size_t(dw) * dh * 4];
  for (int yy = 0; yy < dh; ++yy)
    for (int xx = 0; xx < dw; ++xx) {
      uchar * p = rgba + (size_t(yy) * dw + xx) * 4;
      p[0] = p[1] = p[2] = 255;
      p[3] = uchar(255 - c[yy * ld + xx * d]);   // black: covered
    }
  Fl_RGB_Image image(rgba, dw, dh, 4);
  image.alloc_array = 1;
  image.scale(w, h, 0, 1);
  BlendInvert(true);
  image.draw(x, y);
  BlendInvert(false);
}


// Draws on a view: straight onto its widget in its viewDrawScript, else onto
// its canvas, its mask and its invert layer (Drawing.h; gTarget says which).
// inDraw(ox, oy) draws with the view's 0, 0 at ox, oy.
void DrawOnView(Link * inLink, const std::function<void(long, long)> & inDraw);


Ref DrawShape(RefArg inContext, RefArg inShape, RefArg inStyle)
{
  Link * link = Link::Of(inContext);
  if (link == nullptr)
    ThrowMsg("nil view");
  Style style;
  Apply(style, inStyle);
  DrawOnView(link, [&](long ox, long oy) { Draw(inShape, style, ox, oy); });
  return NILREF;
}


void DrawOnView(Link * inLink, const std::function<void(long, long)> & inDraw)
{
  Fl_Widget * widget = inLink->Widget();
  int inset = inLink->Outset();
  if (gDirect == widget) {   // in its viewDrawScript
    gTarget = kDirect;
    inDraw(widget->x() + inset, widget->y() + inset);
    return;
  }
  inLink->Canvas();   // made if it has none
  const std::pair<Fl_Image_Surface *, Target> passes[] = {
    { inLink->fCanvas, kCanvas }, { inLink->fMask, kMask }, { inLink->fInvert, kInvert } };
  for (const auto & pass : passes) {
    Fl_Surface_Device::push_current(pass.first);
    gTarget = pass.second;
    inDraw(inset, inset);
    Fl_Surface_Device::pop_current();
  }
  gTarget = kDirect;
  delete inLink->fOverlay;   // made anew when drawn
  inLink->fOverlay = nullptr;
  delete inLink->fInvertOverlay;
  inLink->fInvertOverlay = nullptr;
  widget->redraw();
}


Ref DrawXBitmap(RefArg inContext, RefArg inBounds, RefArg inBitmap, RefArg inIndex, RefArg inMode)
{
  Link * link = Link::Of(inContext);
  if (link == nullptr)
    ThrowMsg("nil view");
  RefVar bits(IsFrame(inBitmap) ? GetProtoVariable(inBitmap, SYMA(bits)) : (Ref)inBitmap);
  if (!IsBinary(bits) || Length(bits) < 16 || !IsFrame(inBounds) || !ISINT(inIndex))
    ThrowMsg("DrawXBitmap: bad arguments");
  long left = RINT(GetProtoVariable(inBounds, SYMA(left))), top = RINT(GetProtoVariable(inBounds, SYMA(top)));
  int w = int(RINT(GetProtoVariable(inBounds, SYMA(right))) - left);
  int h = int(RINT(GetProtoVariable(inBounds, SYMA(bottom))) - top);
  const unsigned char * data = (const unsigned char *)BinaryData(bits);
  auto word = [data](int at) { return int(short((data[at] << 8) | data[at + 1])); };
  int rowBytes = word(4), stripH = word(12) - word(8), stripW = word(14) - word(10);
  int cellX = int(RINT(inIndex)) * w;
  if (w <= 0 || h <= 0 || cellX < 0 || cellX + w > stripW || h > stripH
      || Length(bits) < ArrayIndex(16 + rowBytes * stripH))
    return NILREF;
  // the cell: w by h pixels at cellX of the strip
  NewtonBitmap cell;
  cell.width = w;
  cell.height = h;
  cell.rowBytes = (w + 7) / 8;
  cell.bits.assign(size_t(cell.rowBytes) * h, 0);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      int from = cellX + x;
      if (data[16 + y * rowBytes + (from >> 3)] & (0x80 >> (from & 7)))
        cell.bits[size_t(y) * cell.rowBytes + (x >> 3)] |= (0x80 >> (x & 7));
    }
  std::unique_ptr<Fl_Bitmap> bitmap(ToFlImage(cell));
  int mode = ISINT(inMode) ? int(RINT(inMode)) : kModeCopy;
  DrawOnView(link, [&](long ox, long oy) {
    DrawBitmap(bitmap.get(), int(ox + left), int(oy + top), w, h, mode);
  });
  return NILREF;
}


/* Offscreen bitmaps (MakeBitmap): drawn with FLTK into an image of the
   bitmap's size, then back into its pixels, 1 bit deep: black where dark,
   grays as the Newton's patterns (vfLtGray, vfGray, vfDkGray; from the
   bitmap's top left), as a Newton draws them into a 1-bit bitmap. */

namespace {

// The rows of the Newton's gray patterns (8 by 8, repeating every 2 rows).
const unsigned char kLtGray[2] = { 0x88, 0x22 }, kGray[2] = { 0xAA, 0x55 }, kDkGray[2] = { 0x77, 0xDD };

bool BlackAt(int inGray, int x, int y)
{
  static const int kLevels[5] = { 0, 64, 128, 191, 255 };
  int level = 0;
  for (int i = 1; i < 5; ++i)
    if (std::abs(inGray - kLevels[i]) < std::abs(inGray - kLevels[level]))
      level = i;
  const unsigned char * pattern = level == 1 ? kDkGray : level == 2 ? kGray : level == 3 ? kLtGray : nullptr;
  if (pattern == nullptr)
    return level == 0;
  return (pattern[y & 1] & (0x80 >> (x & 7))) != 0;
}

// inDraw draws on the bitmap (its pixels as they are, in an image of its
// size, 0, 0 its top left); then the image goes back into its pixels.
void DrawOnBitmap(RefArg inBitmap, const std::function<void()> & inDraw)
{
  RefVar pixels;
  shapes::PixelsInfo info;
  if (!shapes::GetPixels(inBitmap, pixels, &info))
    ThrowErr(exGraf, -8804);
  int w = info.width, h = info.height;
  std::unique_ptr<Fl_Bitmap> current(ToFlImage(ToNewtonBitmap(inBitmap)));
  Fl_Image_Surface surface(w, h, 0);
  Fl_Surface_Device::push_current(&surface);
  fl_color(FL_WHITE);
  fl_rectf(0, 0, w, h);
  fl_color(FL_BLACK);
  if (current)
    current->draw(0, 0);
  Target target = gTarget;
  gTarget = kDirect;
  inDraw();
  gTarget = target;
  Fl_Surface_Device::pop_current();
  std::unique_ptr<Fl_RGB_Image> drawn(surface.image());
  int dw = drawn->data_w(), dh = drawn->data_h(), d = drawn->d();
  int ld = drawn->ld() ? drawn->ld() : dw * d;
  const uchar * c = (const uchar *)drawn->data()[0];
  std::vector<unsigned char> rows(size_t(info.rowBytes) * h, 0);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      int sx = std::min(dw - 1, (2 * x + 1) * dw / (2 * w)), sy = std::min(dh - 1, (2 * y + 1) * dh / (2 * h));
      if (BlackAt(c[sy * ld + sx * d], x, y))
        rows[size_t(y) * info.rowBytes + (x >> 3)] |= (0x80 >> (x & 7));
    }
  // (the pixels' rows keep the bytes after the width as they were)
  unsigned char * data = (unsigned char *)BinaryData(pixels) + info.offset;
  for (int y = 0; y < h; ++y)
    for (int i = 0; i < (w + 7) / 8; ++i) {
      unsigned char mask = (i + 1) * 8 <= w ? 0xFF : (unsigned char)(0xFF << (8 - (w & 7)));
      unsigned char & byte = data[size_t(y) * info.rowBytes + i];
      byte = (unsigned char)((byte & ~mask) | (rows[size_t(y) * info.rowBytes + i] & mask));
    }
}

// A rect frame ({left, top, right, bottom}), or inDefault if it is nil.
shapes::Box RectOr(RefArg inRect, const shapes::Box & inDefault)
{
  if (!IsFrame(inRect))
    return inDefault;
  return shapes::Box{RINT(GetProtoVariable(inRect, SYMA(top))), RINT(GetProtoVariable(inRect, SYMA(left))),
                     RINT(GetProtoVariable(inRect, SYMA(bottom))), RINT(GetProtoVariable(inRect, SYMA(right)))};
}

} // namespace


Ref DrawIntoBitmap(RefArg inShape, RefArg inStyle, RefArg inBitmap)
{
  Style style;
  Apply(style, inStyle);
  DrawOnBitmap(inBitmap, [&]() { Draw(inShape, style, 0, 0); });
  return NILREF;
}


Ref ViewIntoBitmap(RefArg inView, RefArg inSource, RefArg inDest, RefArg inBitmap)
{
  Link * link = Link::Of(inView);
  if (link == nullptr || link->Widget() == nullptr)
    return NILREF;   // not open: nothing to copy
  Fl_Widget * widget = link->Widget();
  int inset = link->Outset();
  shapes::Box src = RectOr(inSource, shapes::Box{0, 0, widget->h() - 2 * inset, widget->w() - 2 * inset});
  RefVar pixels;
  shapes::PixelsInfo info;
  if (!shapes::GetPixels(inBitmap, pixels, &info))
    ThrowErr(exGraf, -8804);
  long sw = src.right - src.left, sh = src.bottom - src.top;
  // (the ROM's: no destRect, the source's size at the bitmap's top left)
  shapes::Box dest = RectOr(inDest, shapes::Box{0, 0, sh, sw});
  long dw = dest.right - dest.left, dh = dest.bottom - dest.top;
  if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
    return NILREF;
  // the view as it is on the screen (its children, what scripts drew), in
  // the Newton's pixels (high_res 0: w by h; not the screen's, whose
  // pixels, shrunk, would give grays where lines are)
  Fl_Image_Surface surface(widget->w(), widget->h(), 0);
  Fl_Surface_Device::push_current(&surface);
  fl_color(FL_WHITE);
  fl_rectf(0, 0, widget->w(), widget->h());
  surface.draw(widget, 0, 0);
  Fl_Surface_Device::pop_current();
  std::unique_ptr<Fl_RGB_Image> view(surface.image());
  // its source rect onto the destination rect (scaled if they differ)
  int iw = int(widget->w() * dw / sw), ih = int(widget->h() * dh / sh);
  view->scale(iw, ih, 0, 1);
  int x = int(dest.left - (src.left + inset) * dw / sw), y = int(dest.top - (src.top + inset) * dh / sh);
  DrawOnBitmap(inBitmap, [&]() {
    fl_push_clip(int(dest.left), int(dest.top), int(dw), int(dh));
    view->draw(x, y);
    fl_pop_clip();
  });
  return NILREF;
}


Ref DoDrawing(RefArg inContext, RefArg inMethod, RefArg inArgs)
{
  if (Link::Of(inContext) == nullptr)
    ThrowMsg("nil view");
  RefVar args(IsArray(inArgs) ? (Ref)inArgs : MakeArray(0));
  return DoMessage(inContext, inMethod, args);
}


void DrawOverlay(Fl_Widget * inWidget)
{
  Link * link = LinkOfWidget(inWidget);
  if (link == nullptr || !link->HasCanvas())
    return;
  if (link->fOverlay == nullptr) {
    // what was drawn, with the mask as its transparency
    std::unique_ptr<Fl_RGB_Image> color(link->fCanvas->image());
    std::unique_ptr<Fl_RGB_Image> mask(link->fMask->image());
    int w = color->data_w(), h = color->data_h(), d = color->d(), md = mask->d();
    int ld = color->ld() ? color->ld() : w * d, mld = mask->ld() ? mask->ld() : mask->data_w() * md;
    const uchar * c = (const uchar *)color->data()[0];
    const uchar * m = (const uchar *)mask->data()[0];
    uchar * rgba = new uchar[size_t(w) * h * 4];
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        const uchar * cp = c + y * ld + x * d;
        uchar * p = rgba + (size_t(y) * w + x) * 4;
        p[0] = cp[0]; p[1] = cp[d > 1 ? 1 : 0]; p[2] = cp[d > 2 ? 2 : 0];
        p[3] = uchar(255 - m[y * mld + x * md]);   // drawn: black in the mask
      }
    link->fOverlay = new Fl_RGB_Image(rgba, w, h, 4);
    link->fOverlay->alloc_array = 1;
    link->fOverlay->scale(inWidget->w(), inWidget->h(), 0, 1);
  }
  int x = inWidget->as_window() ? 0 : inWidget->x(), y = inWidget->as_window() ? 0 : inWidget->y();
  link->fOverlay->draw(x, y);
  // what XOR shapes cover: inverted (white, in the difference blend mode)
  if (link->fInvertOverlay == nullptr) {
    std::unique_ptr<Fl_RGB_Image> invert(link->fInvert->image());
    int w = invert->data_w(), h = invert->data_h(), d = invert->d();
    int ld = invert->ld() ? invert->ld() : w * d;
    const uchar * c = (const uchar *)invert->data()[0];
    bool any = false;
    uchar * rgba = new uchar[size_t(w) * h * 4];
    for (int yy = 0; yy < h; ++yy)
      for (int xx = 0; xx < w; ++xx) {
        uchar * p = rgba + (size_t(yy) * w + xx) * 4;
        p[0] = p[1] = p[2] = 255;
        p[3] = c[yy * ld + xx * d] >= 128 ? 255 : 0;   // white: inverted
        any = any || p[3];
      }
    link->fInvertOverlay = new Fl_RGB_Image(rgba, w, h, 4);
    link->fInvertOverlay->alloc_array = 1;
    link->fInvertOverlay->scale(inWidget->w(), inWidget->h(), 0, 1);
    link->fInverts = any;
  }
  if (link->fInverts) {
    BlendInvert(true);
    link->fInvertOverlay->draw(x, y);
    BlendInvert(false);
  }
}


void RunDrawScript(Fl_Widget * inWidget)
{
  Link * link = LinkOfWidget(inWidget);
  if (link == nullptr)
    return;
  RefVar context(link->Context());
  RefVar script(MakeSymbol("viewDrawScript"));
  if (ISNIL(GetProtoVariable(context, script)))
    return;
  Fl_Widget * outer = gDirect;
  gDirect = inWidget;
  RefVar args(MakeArray(0));
  if (EventsDelivered())
    SendEventMessage(context, script, args);   // an event of its own
  else {
    // drawn while a script runs (a nested event loop: TrackHilite, Drag, a
    // modal dialog): the view system draws there too, and so does this
    newton_try
    {
      DoMessage(context, script, args);
    }
    newton_catch_all
    { }
    end_try;
  }
  gDirect = outer;
}


// The canvas: what scripts drew, and a mask of the pixels they drew (see
// Drawing.h).
Fl_Image_Surface * Link::Canvas()
{
  if (fCanvas == nullptr) {
    fCanvas = new Fl_Image_Surface(fWidget->w(), fWidget->h(), 1);
    fMask = new Fl_Image_Surface(fWidget->w(), fWidget->h(), 1);
    fInvert = new Fl_Image_Surface(fWidget->w(), fWidget->h(), 1);
    for (Fl_Image_Surface * surface : { fCanvas, fMask, fInvert }) {
      Fl_Surface_Device::push_current(surface);
      fl_color(surface == fInvert ? FL_BLACK : FL_WHITE);   // nothing inverted
      fl_rectf(0, 0, fWidget->w(), fWidget->h());
      Fl_Surface_Device::pop_current();
    }
  }
  return fCanvas;
}


// The view's canvas goes (the view is drawn anew); in the views it is in,
// what scripts drew where its widget is (as a Newton draws the view's area
// anew, not more: a picker's XOR hilite on its label stays when its value
// changes).
void Link::DropCanvas()
{
  for (Link * link = fParent; link != nullptr && fWidget != nullptr; link = link->fParent)
    if (link->fCanvas) {
      // the area in the canvas' coordinates (a window's are its own)
      Fl_Widget * canvasWidget = link->fWidget;
      int x = fWidget->x(), y = fWidget->y();
      if (fWidget->as_window())
        x = y = 0;
      if (!canvasWidget->as_window()) {
        x -= canvasWidget->x();
        y -= canvasWidget->y();
      }
      const std::pair<Fl_Image_Surface *, Fl_Color> surfaces[] = {
        { link->fCanvas, FL_WHITE }, { link->fMask, FL_WHITE }, { link->fInvert, FL_BLACK } };
      for (const auto & surface : surfaces) {
        Fl_Surface_Device::push_current(surface.first);
        fl_color(surface.second);
        fl_rectf(x, y, fWidget->w(), fWidget->h());
        Fl_Surface_Device::pop_current();
      }
      delete link->fOverlay;   // made anew when drawn
      delete link->fInvertOverlay;
      link->fOverlay = link->fInvertOverlay = nullptr;
      link->fWidget->redraw();
    }
  if (fCanvas) {
    delete fCanvas;
    delete fMask;
    delete fInvert;
    delete fOverlay;
    delete fInvertOverlay;
    fCanvas = fMask = fInvert = nullptr;
    fOverlay = fInvertOverlay = nullptr;
    fInverts = false;
    fWidget->redraw();
  }
}

} // namespace nfl
