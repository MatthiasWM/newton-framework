/*
 File: Drawing.cc

 Drawing shapes on views. See Drawing.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Bitmap.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/fl_draw.H>

#include "Host/FLTK/Drawing.h"
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

#include <algorithm>
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
  Fl_Font font = FL_HELVETICA;
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

bool gMask = false;   // drawing a mask (Draw)

Fl_Color Gray(int inGray)
{
  if (gMask)
    return FL_BLACK;
  return fl_rgb_color(uchar(inGray), uchar(inGray), uchar(inGray));
}

// A bitmap at x, y: in copy mode its zero bits white, else only its one bits.
void DrawBitmap(Fl_Bitmap * inBitmap, int x, int y, int w, int h, int inMode)
{
  if (inMode == kModeCopy) {
    fl_color(gMask ? FL_BLACK : FL_WHITE);
    fl_rectf(x, y, w, h);
  }
  fl_color(FL_BLACK);
  inBitmap->draw(x, y);
}

// One shape (or a list) at origin ox, oy (where the view's 0, 0 is).
// inMask: the mask of what is drawn: every pixel drawn is black.
void Draw(RefArg inShape, Style style, long ox, long oy, bool inMask)
{
  if (IsArray(inShape)) {
    for (ArrayIndex i = 0, n = Length(inShape); i < n; ++i) {
      RefVar item(GetArraySlot(inShape, i));
      if (shapes::IsStyleFrame(item))
        Apply(style, item);   // for the shapes after it
      else if (NOTNIL(item))
        Draw(item, style, ox, oy, inMask);
    }
    return;
  }
  if (!shapes::IsPrimShape(inShape))
    return;
  gMask = inMask;
  ox += style.dx;
  oy += style.dy;
  RefVar cls(ClassOf(inShape));
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
  gMask = false;
}

Link * LinkOfWidget(Fl_Widget * inWidget)
{
  return inWidget ? static_cast<Link *>(inWidget->user_data()) : nullptr;
}

} // namespace


// Draws on a view: straight onto its widget in its viewDrawScript, else onto
// its canvas and its mask (Drawing.h). inDraw(ox, oy, mask) draws with the
// view's 0, 0 at ox, oy.
void DrawOnView(Link * inLink, const std::function<void(long, long, bool)> & inDraw);


Ref DrawShape(RefArg inContext, RefArg inShape, RefArg inStyle)
{
  Link * link = Link::Of(inContext);
  if (link == nullptr)
    ThrowMsg("nil view");
  Style style;
  Apply(style, inStyle);
  DrawOnView(link, [&](long ox, long oy, bool inMask) { Draw(inShape, style, ox, oy, inMask); });
  return NILREF;
}


void DrawOnView(Link * inLink, const std::function<void(long, long, bool)> & inDraw)
{
  Fl_Widget * widget = inLink->Widget();
  int inset = inLink->Outset();
  if (gDirect == widget) {   // in its viewDrawScript
    inDraw(widget->x() + inset, widget->y() + inset, false);
    return;
  }
  inLink->Canvas();   // made if it has none
  Fl_Surface_Device::push_current(inLink->fCanvas);
  inDraw(inset, inset, false);
  Fl_Surface_Device::pop_current();
  Fl_Surface_Device::push_current(inLink->fMask);
  inDraw(inset, inset, true);
  Fl_Surface_Device::pop_current();
  delete inLink->fOverlay;   // made anew when drawn
  inLink->fOverlay = nullptr;
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
  DrawOnView(link, [&](long ox, long oy, bool inMask) {
    gMask = inMask;
    DrawBitmap(bitmap.get(), int(ox + left), int(oy + top), w, h, mode);
    gMask = false;
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
  link->fOverlay->draw(inWidget->as_window() ? 0 : inWidget->x(), inWidget->as_window() ? 0 : inWidget->y());
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
    for (Fl_Image_Surface * surface : { fCanvas, fMask }) {
      Fl_Surface_Device::push_current(surface);
      fl_color(FL_WHITE);
      fl_rectf(0, 0, fWidget->w(), fWidget->h());
      Fl_Surface_Device::pop_current();
    }
  }
  return fCanvas;
}


void Link::DropCanvas()
{
  for (Link * link = this; link != nullptr; link = link->fParent)
    if (link->fCanvas) {
      delete link->fCanvas;
      delete link->fMask;
      delete link->fOverlay;
      link->fCanvas = link->fMask = nullptr;
      link->fOverlay = nullptr;
      link->fWidget->redraw();
    }
}

} // namespace nfl
