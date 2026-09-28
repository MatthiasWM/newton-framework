/*
 File: Links.cc

 Links between Newton views and FLTK widgets. See Links.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <FL/platform.H>   // fl_open_display()

#include "Host/FLTK/Links.h"
#include "Host/FLTK/FloatNGo.h"
#include "Host/FLTK/Boxtypes.h"
#include "Host/FLTK/Widgets.h"
#include "Host/FLTK/RomImages.h"
#include "Host/FLTK/Pen.h"
#include "Host/FLTK/Drawing.h"
#include "Host/Pict.h"
#include <FL/Fl_Image_Surface.H>
#include "Host/Root.h"
#include "Host/Views.h"
#include "Matt/EventLoop.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"
#include "Frames/Funcs.h"
#include "Frames/Lookup.h"
#include "Frames/Globals.h"
#include "ViewFlags.h"
#include "ROMResources.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace nfl {

namespace {

// Where the Newton display's top left corner is on the desktop (the Newton
// has no desktop: window-like views are placed as on its screen).
const int kDesktopLeft = 100;
const int kDesktopTop = 100;

// View classes (the templates' viewClass)
const long clPictureView = 76;
const long clParagraphView = 81;
const long clTextView = 98;

long IntSlot(RefArg inFrame, const char * inSlot)
{
  Ref value = GetProtoVariable(inFrame, MakeSymbol(inSlot));
  return ISINT(value) ? RINT(value) : 0;
}

Bounds BoundsOf(RefArg inFrame)
{
  Bounds bounds;
  bounds.left = IntSlot(inFrame, "left");
  bounds.top = IntSlot(inFrame, "top");
  bounds.right = IntSlot(inFrame, "right");
  bounds.bottom = IntSlot(inFrame, "bottom");
  return bounds;
}

// The area a child of the root view is placed in: the app area of the
// display (displayParams, as the ROM).
Bounds AppArea(void)
{
  RefVar params(GetGlobalVar(MakeSymbol("displayParams")));
  Bounds area;
  area.left = IntSlot(params, "appAreaGlobalLeft");
  area.top = IntSlot(params, "appAreaGlobalTop");
  area.right = area.left + IntSlot(params, "appAreaWidth");
  area.bottom = area.top + IntSlot(params, "appAreaHeight");
  return area;
}

// A view's bounds on the display, as the ROM's TView::JustifyBounds (the
// port only declares CView::justifyBounds): inBounds (its viewBounds) placed by
// inJustify (viewJustify) in its parent's bounds (inParent, global), or
// relative to its previous sibling (inSibling, nullptr for the first child).
// Ratios make the bounds percentages of the sibling's or the parent's size.
Bounds Justify(Bounds inBounds, long inJustify, const Bounds & inParent, const Bounds * inSibling)
{
  long originH = inParent.left, originV = inParent.top;
  bool justifyH = true, justifyV = true;

  if (inSibling && (inJustify & vjSiblingMask)) {
    const Bounds & sibling = *inSibling;
    if (long siblingV = inJustify & vjSiblingVMask) {
      justifyV = false;
      if (inJustify & vjTopRatio)
        inBounds.top = inBounds.top * sibling.Height() / 100;
      if (inJustify & vjBottomRatio)
        inBounds.bottom = inBounds.bottom * sibling.Height() / 100;
      switch (siblingV) {
        case vjSiblingCenterV: originV = sibling.top + (sibling.Height() - inBounds.Height()) / 2; break;
        case vjSiblingBottomV: originV = sibling.bottom; break;
        case vjSiblingFullV:   originV = 0; inBounds.top += sibling.top; inBounds.bottom += sibling.bottom; break;
        case vjSiblingTopV:    originV = sibling.top; break;
      }
    }
    if (long siblingH = inJustify & vjSiblingHMask) {
      justifyH = false;
      if (inJustify & vjLeftRatio)
        inBounds.left = inBounds.left * sibling.Width() / 100;
      if (inJustify & vjRightRatio)
        inBounds.right = inBounds.right * sibling.Width() / 100;
      switch (siblingH) {
        case vjSiblingCenterH: originH = sibling.left + (sibling.Width() - inBounds.Width()) / 2; break;
        case vjSiblingRightH:  originH = sibling.right; break;
        case vjSiblingFullH:   originH = 0; inBounds.left += sibling.left; inBounds.right += sibling.right; break;
        case vjSiblingLeftH:   originH = sibling.left; break;
      }
    }
  }

  if (justifyH) {
    if (inJustify & vjLeftRatio)
      inBounds.left = inBounds.left * inParent.Width() / 100;
    if (inJustify & vjRightRatio)
      inBounds.right = inBounds.right * inParent.Width() / 100;
  }
  if (justifyV) {
    if (inJustify & vjTopRatio)
      inBounds.top = inBounds.top * inParent.Height() / 100;
    if (inJustify & vjBottomRatio)
      inBounds.bottom = inBounds.bottom * inParent.Height() / 100;
  }

  if (justifyV) {
    switch (inJustify & vjParentVMask) {
      case vjParentCenterV: originV += (inParent.Height() - inBounds.Height()) / 2; break;
      case vjParentBottomV: originV += inParent.Height(); break;
      case vjParentFullV:   originV = 0; inBounds.top += inParent.top; inBounds.bottom += inParent.bottom; break;
    }
  }
  if (justifyH) {
    switch (inJustify & vjParentHMask) {
      case vjParentCenterH: originH += (inParent.Width() - inBounds.Width()) / 2; break;
      case vjParentRightH:  originH += inParent.Width(); break;
      case vjParentFullH:   originH = 0; inBounds.left += inParent.left; inBounds.right += inParent.right; break;
    }
  }

  inBounds.left += originH; inBounds.right += originH;
  inBounds.top += originV; inBounds.bottom += originV;
  return inBounds;
}

// A bounds frame, {left, top, right, bottom}
Ref BoundsFrame(const Bounds & inBounds)
{
  RefVar frame(AllocateFrame());
  SetFrameSlot(frame, SYMA(left), MAKEINT(inBounds.left));
  SetFrameSlot(frame, SYMA(top), MAKEINT(inBounds.top));
  SetFrameSlot(frame, SYMA(right), MAKEINT(inBounds.right));
  SetFrameSlot(frame, SYMA(bottom), MAKEINT(inBounds.bottom));
  return frame;
}

// The link of an open view, or throw "nil view" (the ROM's FailGetView).
Link * OpenLink(RefArg inContext)
{
  Link * link = Link::Of(inContext);
  if (link == nullptr)
    ThrowMsg("nil view");
  return link;
}

// A view script, found through _proto only (CView::runScript): nil if the
// view has none.
Ref RunScript(RefArg inContext, const char * inScript)
{
  RefVar script(MakeSymbol(inScript));
  if (ISNIL(GetProtoVariable(inContext, script)))
    return NILREF;
  RefVar args(MakeArray(0));
  return DoProtoMessage(inContext, script, args);
}

std::string TextSlot(RefArg inContext, const char * inSlot)
{
  RefVar text(GetProtoVariable(inContext, MakeSymbol(inSlot)));
  return IsString(text) ? DisplayText(UTF8FromString(text)) : std::string();
}

// FLTK's alignment for the H and V bits of a viewJustify (how a view places
// its text or picture: vjLeftH, vjCenterV, ...). FLTK can't justify text
// (vjFullH, vjFullV): left, top.
Fl_Align AlignOf(long inJustify)
{
  Fl_Align align = FL_ALIGN_CENTER;
  switch (inJustify & vjHMask) {
    case vjLeftH:
    case vjFullH:  align |= FL_ALIGN_LEFT; break;
    case vjRightH: align |= FL_ALIGN_RIGHT; break;
  }
  switch (inJustify & vjVMask) {
    case vjTopV:
    case vjFullV:   align |= FL_ALIGN_TOP; break;
    case vjBottomV: align |= FL_ALIGN_BOTTOM; break;
  }
  return align;
}

// A view's icon slot (see ToNewtonBitmap). Empty if there is none.
NewtonBitmap IconOf(RefArg inContext)
{
  return ToNewtonBitmap(GetProtoVariable(inContext, SYMA(icon)));
}

// How far a view's frame reaches out of its bounds (viewFormat), as the
// ROM's CView::outerBounds: only a view with a pen has a frame; it reaches
// out inset + pen (the pen only with a frame color), on every side. A
// dragger or matte frame (protoFloater, protoFloatNGo, an app's base view):
// kDraggerBorderWidth, for Matt's FLOATER_BOX (not parametrized yet).
// Measured on a Newton (Screenshot1, nBattleship's Play button: 48 by 13,
// pen 2): 52 by 17, nothing more. (Not yet: the ROM's 3 more above and
// below the default button, for its keyboard indicator.)
int FrameOutset(long inViewFormat)
{
  long frame = (inViewFormat & vfFrameMask) >> vfFrameShift;
  if (frame == vfDragger || frame == vfMatte)
    return kDraggerBorderWidth;
  long pen = (inViewFormat & vfPenMask) >> vfPenShift;
  if (pen == 0)
    return 0;
  long inset = (inViewFormat & vfInsetMask) >> vfInsetShift;
  return int(inset + (frame ? pen : 0));
}

// A view's shadow (viewFormat): that much more at the right and the bottom,
// outside its frame (CView::outerBounds; only a view with a pen has one).
int FrameShadow(long inViewFormat)
{
  long frame = (inViewFormat & vfFrameMask) >> vfFrameShift;
  if (frame == vfDragger || frame == vfMatte || (inViewFormat & vfPenMask) == 0)
    return 0;
  return int((inViewFormat & vfShadowMask) >> vfShadowShift);
}

// The ROM's image for a view's icon slot, if it is one of the ROM's (a
// magic pointer, as in protoClosebox: @334) and newtc has it; else nullptr.
Fl_Image * RomIconOf(RefArg inContext, bool inInverted = false)
{
  Ref icon = GetProtoVariable(inContext, SYMA(icon));
  return ISMAGICPTR(icon) ? RomImage(RVALUE(icon), inInverted) : nullptr;
}

// The children's view frames a view's scripts use before the children open
// (TView::Constructor): allocateContext or stepAllocateContext, [slot,
// template, ...]: each template's view frame, in the view's slot (e.g.
// protoLabelPicker's entryLine, which its viewSetupFormScript sets up).
void AllocateContexts(RefArg inContext, RefArg inSlot)
{
  RefVar list(GetProtoVariable(inContext, inSlot));
  if (!IsArray(list))
    return;
  for (ArrayIndex i = 0, n = Length(list); i + 1 < n; i += 2) {
    RefVar child(BuildViewContext(GetArraySlot(list, i + 1), true));
    SetFrameSlot(child, SYMA(_parent), inContext);
    SetFrameSlot(inContext, GetArraySlot(list, i), child);
  }
}

// The view frame for a child template (TView::AddView): the one allocated
// for it (its preAllocatedContext names the view's slot), if it is
// visible; else a new one (nil if the template isn't visible).
Ref ChildContext(RefArg inContext, RefArg inTemplate)
{
  RefVar tag(GetProtoVariable(inTemplate, SYMA(preAllocatedContext)));
  if (NOTNIL(tag)) {
    RefVar child(GetVariable(inContext, tag));
    if (IsFrame(child))
      return (IntSlot(child, "viewFlags") & vVisible) ? (Ref)child : NILREF;
  }
  return BuildViewContext(inTemplate, false);
}

// Widgets are added to their parent explicitly: FLTK must not add a new one
// to the group made last (Fl_Group::current()).
template <class W, class... Args> W * NewWidget(Args... args)
{
  Fl_Group::current(nullptr);
  W * widget = new W(args...);
  Fl_Group::current(nullptr);
  return widget;
}


// clView, and any view class that has no link class yet: a group that
// holds the children's widgets.
class ViewLink : public GroupLink
{
public:
  ViewLink(RefArg inContext, Link * inParent) : GroupLink(inContext, inParent) { }
protected:
  Fl_Widget * MakeWidget() override
  {
    return NewWidget<Group>(WidgetX(), WidgetY(), WidgetW(), WidgetH());
  }
};


// clParagraphView: its text.
class ParagraphLink : public Link
{
public:
  ParagraphLink(RefArg inContext, Link * inParent) : Link(inContext, inParent) { }
protected:
  Fl_Widget * MakeWidget() override
  {
    // a text view that wraps its lines (pen events, its viewFormat, what
    // scripts draw: as the other views)
    TextView * view = NewWidget<TextView>(WidgetX(), WidgetY(), WidgetW(), WidgetH(), Text());
    view->TransferMode(IntSlot(fContext, "viewTransferMode"));
    return view;
  }
  // its lines from the top, wrapped; left, centered or right (viewJustify)
  Fl_Align AlignFor(long inJustify) const override
  {
    return Fl_Align((Link::AlignFor(inJustify) & (FL_ALIGN_LEFT | FL_ALIGN_RIGHT)) | FL_ALIGN_TOP | FL_ALIGN_WRAP);
  }
  void Update(RefArg inTag) override
  {
    if (EQ(inTag, SYMA(text)))
      static_cast<TextView *>(fWidget)->Text(Text());
    Link::Update(inTag);
  }
  std::string Text()
  {
    std::string text = TextSlot(fContext, "text");
    std::replace(text.begin(), text.end(), '\r', '\n');   // a Newton new line
    return text;
  }
};


// clTextView (protoTextButton): its text, in the box of its viewFormat.
class TextLink : public Link
{
public:
  TextLink(RefArg inContext, Link * inParent) : Link(inContext, inParent) { }
protected:
  Fl_Widget * MakeWidget() override
  {
    TextView * view = NewWidget<TextView>(WidgetX(), WidgetY(), WidgetW(), WidgetH(), Text());
    view->TransferMode(IntSlot(fContext, "viewTransferMode"));
    return view;
  }
  void Update(RefArg inTag) override
  {
    if (EQ(inTag, SYMA(text)))
      static_cast<TextView *>(fWidget)->Text(Text());
    Link::Update(inTag);
  }
private:
  // text as NewtonScript finds it: in the view's protos, else in its
  // parents' (protoCheckbox's clTextView child shows the checkbox's)
  std::string Text()
  {
    RefVar text(GetVariable(fContext, SYMA(text)));
    return IsString(text) ? DisplayText(UTF8FromString(text)) : std::string();
  }
};


// clPictureView (protoPictureButton, protoClosebox): its icon.
class PictureLink : public Link
{
public:
  PictureLink(RefArg inContext, Link * inParent) : Link(inContext, inParent) { }
protected:
  Fl_Widget * MakeWidget() override
  {
    PictureView * view;
    if (Fl_Image * image = RomIconOf(fContext))
      view = NewWidget<PictureView>(WidgetX(), WidgetY(), WidgetW(), WidgetH(), image);
    else
      view = NewWidget<PictureView>(WidgetX(), WidgetY(), WidgetW(), WidgetH(),
                                    IconOf(fContext));
    view->TransferMode(IntSlot(fContext, "viewTransferMode"));
    return view;
  }
  // a Newton reads the icon slot when it draws: on any SetValue (a script
  // may have set the slot itself before) and Dirty, here too
  void Update(RefArg inTag) override
  {
    LoadIcon();
    Link::Update(inTag);
  }
  void Reread() override
  {
    LoadIcon();
  }
  // as the ROM's CPictureView: a view without a viewJustify (not 0: none)
  // centers its icon (protoInfoButton)
  Fl_Align AlignFor(long inJustify) const override
  {
    if ((inJustify & vjEverything) == 0 && ISNIL(GetProtoVariable(fContext, SYMA(viewJustify))))
      return FL_ALIGN_CENTER;
    return Link::AlignFor(inJustify);
  }
private:
  void LoadIcon()
  {
    PictureView * view = static_cast<PictureView *>(fWidget);
    if (Fl_Image * image = RomIconOf(fContext))
      view->Image(image);
    else
      view->Icon(IconOf(fContext));
  }
};


// A child of the root view: a desktop window of its own.
class WindowLink : public GroupLink
{
public:
  WindowLink(RefArg inContext, Link * inParent) : GroupLink(inContext, inParent) { }
protected:
  Fl_Widget * MakeWidget() override
  {
    std::string title = TextSlot(fContext, "title");
    if (title.empty())
      title = TextSlot(fContext, "appName");
    if (title.empty())
      title = "Newton";
    // the frame (fOutset) is outside the view's bounds (as on a Newton)
    FloatNGo * window = NewWidget<FloatNGo>(int(kDesktopLeft + fBounds.left - fOutset), int(kDesktopTop + fBounds.top - fOutset),
                                            WidgetW(), WidgetH(), title.c_str(), this);
    if (IntSlot(fContext, "viewClass") == clPictureView)
      if (Fl_Image * picture = RomIconOf(fContext))   // an alert: its frame
        window->Picture(picture, fOutset, fOutset);
    return window;
  }
};


// The link class for a view (see Links.h).
Link * NewLink(RefArg inContext, Link * inParent)
{
  if (inParent == nullptr)
    return new WindowLink(inContext, inParent);
  switch (IntSlot(inContext, "viewClass")) {
    case clParagraphView: return new ParagraphLink(inContext, inParent);
    case clTextView:      return new TextLink(inContext, inParent);
    case clPictureView:   return new PictureLink(inContext, inParent);
  }
  return new ViewLink(inContext, inParent);
}

} // namespace


// Newton's own characters (the Private Use Area of its fonts), as Unicode
// shows them: U+FC01, the picker diamond, is BLACK DIAMOND U+25C6.
std::string DisplayText(std::string inText)
{
  static const struct { const char * newton, * unicode; } kChars[] = {
  //{ "\xEF\xB0\x81", "\xE2\x97\x86" },   // U+FC01 -> U+25C6 (the picker diamond)
    { "\xEF\xB0\x81", "\xE2\xAC\xA5" },   // Trying a bigger diamond
    { "\xEF\xB0\x8B", "\xE2\x9C\x93" },   // U+FC0B -> U+2713 (the check mark)
  };
  for (const auto & c : kChars)
    for (size_t at = inText.find(c.newton); at != std::string::npos; at = inText.find(c.newton, at))
      inText.replace(at, strlen(c.newton), c.unicode);
  return inText;
}


// The view whose viewClickScript took the pen down: it gets the pen's moves
// and its coming up, whichever widget FLTK gives them to.
static Link * gPenOwner = nullptr;

Link::Link(RefArg inContext, Link * inParent)
: fContext(inContext), fParent(inParent)
{
  if (fParent)
    fParent->fChildren.push_back(this);
}


Link::~Link()
{
  if (gPenOwner == this)
    gPenOwner = nullptr;
  delete fCanvas;
  delete fMask;
  delete fOverlay;
  delete fInvert;
  delete fInvertOverlay;
  Fl::remove_timeout(IdleTimeout, this);
  if (fParent) {
    auto & siblings = fParent->fChildren;
    for (auto it = siblings.begin(); it != siblings.end(); ++it)
      if (*it == this) { siblings.erase(it); break; }
  }
}


Link * Link::Of(RefArg inContext)
{
  if (!IsFrame(inContext))
    return nullptr;
  Ref cObject = GetFrameSlot(inContext, SYMA(viewCObject));
  return ISINT(cObject) ? static_cast<Link *>(RefToAddress(cObject)) : nullptr;
}


int Link::WidgetX()
{
  Link * window = Window();
  return int(fBounds.left - fOutset - window->fBounds.left + window->fOutset);
}


int Link::WidgetY()
{
  Link * window = Window();
  return int(fBounds.top - fOutset - window->fBounds.top + window->fOutset);
}


Link * Link::Window()
{
  Link * link = this;
  while (link->fParent)
    link = link->fParent;
  return link;
}


int GroupLink::RemoveChild(Fl_Group * inGroup, int inIndex)
{
  if (inIndex < 0 || inIndex >= inGroup->children())
    return 1;
  inGroup->remove(inIndex);
  return 0;
}


Bounds Link::JustifiedBounds()
{
  RefVar viewBounds(GetProtoVariable(fContext, SYMA(viewBounds)));
  if (!IsFrame(viewBounds))
    ThrowErr(exRootException, -8505);   // no view bounds
  const Bounds * sibling = nullptr;
  if (fParent) {
    auto & siblings = fParent->fChildren;
    for (size_t i = 1; i < siblings.size(); ++i)
      if (siblings[i] == this) { sibling = &siblings[i - 1]->fBounds; break; }
  }
  return Justify(BoundsOf(viewBounds), IntSlot(fContext, "viewJustify"),
                 fParent ? fParent->fBounds : AppArea(), sibling);
}


// A view script for an event, found through _proto only (as runScript).
static Ref SendViewEvent(RefArg inContext, const char * inScript, RefArg inArgs)
{
  RefVar script(MakeSymbol(inScript));
  if (ISNIL(GetProtoVariable(inContext, script)))
    return NILREF;
  return SendEventMessage(inContext, script, inArgs);
}


void Link::SetupIdle(long inMilliseconds)
{
  Fl::remove_timeout(IdleTimeout, this);
  if (inMilliseconds > 0)
    Fl::add_timeout(inMilliseconds / 1000.0, IdleTimeout, this);
}


// viewIdleScript: again after the milliseconds it returns; nil stops.
void Link::IdleTimeout(void * inLink)
{
  Link * link = static_cast<Link *>(inLink);
  if (!EventsDelivered()) {
    Fl::add_timeout(0.05, IdleTimeout, link);   // a script runs: a bit later
    return;
  }
  RefVar context(link->fContext);
  RefVar args(MakeArray(0));
  RefVar result(SendViewEvent(context, "viewIdleScript", args));
  // the script may have closed the view (the timeout is gone with the link)
  if (Link::Of(context) == link && ISINT(result))
    link->SetupIdle(RINT(result));
}


// Where the pen is on the Newton display: the event's screen position, from
// the window's (so that it holds while a window moves).
PenPoint Link::PenAt(Fl_Widget * inWidget)
{
  Link * window = Window();
  Fl_Window * fltkWindow = inWidget->as_window() ? inWidget->as_window() : inWidget->window();
  PenPoint point;
  point.x = Fl::event_x_root() - fltkWindow->x_root() + window->fBounds.left - window->fOutset;
  point.y = Fl::event_y_root() - fltkWindow->y_root() + window->fBounds.top - window->fOutset;
  return point;
}


void Link::ScreenPoint(long inX, long inY, int * outX, int * outY)
{
  Link * window = Window();
  Fl_Window * fltkWindow = window->fWidget->as_window();
  *outX = int(fltkWindow->x_root() + inX - window->fBounds.left + window->fOutset);
  *outY = int(fltkWindow->y_root() + inY - window->fBounds.top + window->fOutset);
}


void Link::MoveBy(long inDX, long inDY)
{
  if (inDX == 0 && inDY == 0)
    return;
  DropCanvas();
  OffsetBounds(inDX, inDY);
  if (fParent == nullptr) {   // a window: on the desktop
    fWidget->position(fWidget->x() + int(inDX), fWidget->y() + int(inDY));
    return;
  }
  Place();
  if (Fl_Group * parent = fWidget->parent())
    parent->redraw();
}


void Link::OffsetBounds(long inDX, long inDY)
{
  fBounds.left += inDX; fBounds.right += inDX;
  fBounds.top += inDY; fBounds.bottom += inDY;
  for (Link * child : fChildren)
    child->OffsetBounds(inDX, inDY);
}


// The widgets where the bounds are (FLTK moves a group's children with it;
// the others, e.g. a paragraph's, are placed here too).
void Link::Place()
{
  fWidget->position(WidgetX(), WidgetY());
  for (Link * child : fChildren)
    child->Place();
}


// Recognition of a stroke no viewClickScript took (the ROM's recognizers;
// newtc knows taps only): a tap stays within kTapSlop pixels and is up within
// kTapTicks; it goes to viewGestureScript(unit, aeTap) of the view under it
// if it allows gestures (vGesturesAllowed), else of the views it is in, up
// while they return nil (Battleship: a tap on a ship turns it).
const long kTapSlop = 4;
const long kTapTicks = 30;   // half a second
const long kTapGesture = 49;   // aeTap (Views/Responder.h)

// The stroke that waits for its pen up to be recognized, and its view.
static Stroke * gRecognized = nullptr;
static RefStruct * gRecognizedView = nullptr;

static void Recognize(Stroke * inStroke, RefArg inContext)
{
  const std::vector<PenPoint> & points = inStroke->Points();
  long left = points.front().x, right = left, top = points.front().y, bottom = top;
  for (const PenPoint & p : points) {
    left = std::min(left, p.x);
    right = std::max(right, p.x);
    top = std::min(top, p.y);
    bottom = std::max(bottom, p.y);
  }
  if (right - left > kTapSlop || bottom - top > kTapSlop
      || inStroke->UpTime() - inStroke->DownTime() > kTapTicks)
    return;   // not a tap
  RefVar args(MakeArray(2));
  SetArraySlot(args, 0, inStroke->Unit());
  SetArraySlot(args, 1, MAKEINT(kTapGesture));
  for (RefVar context(inContext); IsFrame(context) && Link::Of(context) != nullptr; ) {
    if ((IntSlot(context, "viewFlags") & vGesturesAllowed)
        && NOTNIL(SendViewEvent(context, "viewGestureScript", args)))
      break;
    context = GetFrameSlot(context, SYMA(_parent));
  }
}


int Link::HandlePen(Fl_Widget * inWidget, int inEvent)
{
  switch (inEvent) {
    case FL_PUSH: {
      if ((IntSlot(fContext, "viewFlags") & vClickable) == 0)
        return 0;
      Stroke * stroke = Stroke::Begin(PenAt(inWidget));
      // FLTK makes the widget the pushed one only after this returns; but
      // TrackHilite() waits for the pen to come up in here
      Fl::pushed(inWidget);
      RefVar args(MakeArray(1));
      SetArraySlot(args, 0, stroke->Unit());
      // the view, and if its viewClickScript doesn't take the pen (returns
      // nil), the clickable views it is in, as on a Newton
      RefVar hitView(fContext);
      RefVar context(fContext);
      bool taken = false;
      for (Link * link = this; link != nullptr; ) {
        RefVar parent(link->fParent ? (Ref)link->fParent->fContext : NILREF);
        if (IntSlot(context, "viewFlags") & vClickable) {
          gPenOwner = link;
          link->fPenDown = link->fPenInside = true;
          link->fStroke = stroke;
          RefVar took(SendViewEvent(context, "viewClickScript", args));
          // the script may have closed the view: this link may be gone
          if (NOTNIL(took) || Link::Of(context) != link) {
            taken = true;
            break;
          }
          link->fPenDown = false;
        }
        context = parent;
        link = IsFrame(parent) ? Link::Of(parent) : nullptr;
      }
      if (!taken && Link::Of(hitView) != nullptr) {
        // for the recognizers, when the pen is up (it may be already: a
        // script that waited for it, Drag)
        if (stroke->Done())
          Recognize(stroke, hitView);
        else {
          gRecognized = stroke;
          delete gRecognizedView;
          gRecognizedView = new RefStruct(hitView);
        }
      }
      return 1;
    }
    case FL_DRAG:
    case FL_RELEASE: {
      Link * owner = gPenOwner ? gPenOwner : this;
      if (!owner->fPenDown) {
        if (gRecognized == nullptr)
          return 0;
        // a stroke no view took, going on to its recognition
        Stroke * stroke = gRecognized;
        if (inEvent == FL_DRAG) {
          stroke->Add(PenAt(inWidget));
          return 1;
        }
        stroke->End(PenAt(inWidget));
        gRecognized = nullptr;
        RefVar view(*gRecognizedView);
        delete gRecognizedView;
        gRecognizedView = nullptr;
        Recognize(stroke, view);
        return 1;
      }
      owner->fStroke->Add(owner->PenAt(inWidget));
      bool inside = Fl::event_inside(owner->fWidget);
      if (inEvent == FL_RELEASE) {
        owner->fStroke->End(owner->PenAt(inWidget));
        owner->fPenInside = inside;
        owner->fPenDown = false;
        gPenOwner = nullptr;
      } else if (inside != owner->fPenInside) {
        owner->fPenInside = inside;
        if (owner->fTracking)
          owner->SetHilite(inside);
      }
      return 1;
    }
  }
  return 0;
}


void Link::Update(RefArg inTag)
{
  DropCanvas();
  if (EQ(inTag, SYMA(viewBounds)) || EQ(inTag, SYMA(viewJustify))) {
    ReadStyle();
    ApplyStyle();
    fBounds = JustifiedBounds();
    if (fParent) {   // a window keeps its place (the user may have moved it)
      Layout();
      if (Fl_Group * parent = fWidget->parent())
        parent->redraw();
    }
  } else if (EQ(inTag, SYMA(viewFont))) {
    // the view's, and its children's that have none of their own
    std::function<void(Link *)> restyle = [&](Link * inLink) {
      inLink->ReadStyle();
      inLink->ApplyStyle();
      inLink->fWidget->redraw();
      for (Link * child : inLink->fChildren)
        restyle(child);
    };
    restyle(this);
  } else if (EQ(inTag, SYMA(viewFormat))) {
    fViewFormat = IntSlot(fContext, "viewFormat");
    int outset = FrameOutset(fViewFormat), shadow = FrameShadow(fViewFormat);
    if (outset != fOutset || shadow != fShadow) {   // the frame reaches out more (or less)
      fOutset = outset;
      fShadow = shadow;
      Layout();
    }
    if (Fl_Group * parent = fWidget->parent())
      parent->redraw();   // the old frame was outside the view
  }
  fWidget->redraw();
}


// The widget at its place and size (fBounds, fOutset), and the children's
// at theirs (they are relative to the window, and a group that resizes
// scales its children). A window stays where it is on the screen, its
// frame around it.
void Link::Layout()
{
  if (fParent)
    fWidget->resize(WidgetX(), WidgetY(), WidgetW(), WidgetH());
  else
    fWidget->resize(int(kDesktopLeft + fBounds.left - fOutset), int(kDesktopTop + fBounds.top - fOutset),
                    WidgetW(), WidgetH());
  for (Link * child : fChildren)
    child->Layout();
}


void Link::ReadStyle()
{
  FontFromSpec(GetVariable(fContext, SYMA(viewFont)), &fFont, &fFontSize);
  fViewJustify = IntSlot(fContext, "viewJustify");
}


void Link::ApplyStyle()
{
  fWidget->labelfont(fFont);
  fWidget->labelsize(fFontSize);
  fWidget->align(AlignFor(fViewJustify));
}


Fl_Align Link::AlignFor(long inJustify) const
{
  return AlignOf(inJustify);
}


void Link::UpdatePosition(Fl_Window * inWindow)
{
  fBounds.left = inWindow->x() - kDesktopLeft + fOutset;
  fBounds.top = inWindow->y() - kDesktopTop + fOutset;
  fBounds.right = inWindow->x() + inWindow->w() - kDesktopLeft - fOutset - fShadow;
  fBounds.bottom = inWindow->y() + inWindow->h() - kDesktopTop - fOutset - fShadow;
}


void Link::Dirty()
{
  std::function<void(Link *)> reread = [&](Link * inLink) {
    inLink->Reread();
    for (Link * child : inLink->fChildren)
      reread(child);
  };
  reread(this);   // (Battleship sets a ship's icon slot, then its parent's Dirty())
  DropCanvas();   // drawn anew: what scripts drew is gone
  fWidget->redraw();
}


// As the ROM's CView::hilite: a view with a viewHiliteScript hilites
// itself: the script runs with the new state (true or nil), and if it
// returns non-nil it has done the hiliting (protoLabelPicker: an XOR round
// rectangle over its label only; unhiliting, the same XOR again undoes it);
// else the widget draws the view inverted.
void Link::SetHilite(bool inOn)
{
  if (fHilited == inOn)
    return;
  fHilited = inOn;
  RefVar script(MakeSymbol("viewHiliteScript"));
  if (NOTNIL(GetProtoVariable(fContext, script))) {
    RefVar args(MakeArray(1));
    SetArraySlot(args, 0, inOn ? TRUEREF : NILREF);
    RefVar done;
    newton_try
    {
      // called from a script (Hilite) or while one waits (TrackHilite)
      done = DoMessage(fContext, script, args);
    }
    newton_catch_all
    { }
    end_try;
    if (inOn && NOTNIL(done)) {
      fScriptHilited = true;   // the script's drawing is the hilite
      return;
    }
    if (!inOn && fScriptHilited) {
      fScriptHilited = false;
      return;
    }
  }
  DropCanvas();
  if (fWidget)
    fWidget->redraw();
}


Group::Group(int x, int y, int w, int h)
: Fl_Group(x, y, w, h)
{
  box(VIEW_BOX);
}


void Group::draw()
{
  // (a group doesn't show hiliting yet: the ROM inverts it, children too)
  Link * link = static_cast<Link *>(user_data());
  if (link)
    DrawViewFormat(x(), y(), w(), h(), link->ViewFormat(), false, kViewFill);
  RunDrawScript(this);
  for (int i = 0; i < children(); ++i)   // all of them (FloatNGo::draw)
    draw_child(*child(i));
  if (link)   // as the ROM: the frame over the children
    DrawViewFormat(x(), y(), w(), h(), link->ViewFormat(), false, kViewFrame);
  DrawOverlay(this);
  if (link && link->Hilited())   // inverted, its children too
    DrawHilite(x(), y(), w(), h(), link->ViewFormat());
}


int Group::handle(int inEvent)
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


void Link::SendClose()
{
  RefVar args(MakeArray(0));
  SendEventMessage(fContext, MakeSymbol("Close"), args);
}


// Open a view and its children (CView::init). inParent: the parent's link,
// nullptr for a child of the root view.
Link * Build(RefArg inContext, Link * inParent)
{
  Link * link = NewLink(inContext, inParent);
  SetFrameSlot(inContext, SYMA(_parent), inParent ? (Ref)inParent->Context() : RootView());
  SetFrameSlot(inContext, SYMA(viewCObject), AddressToRef(link));
  newton_try
  {
    AllocateContexts(inContext, SYMA(allocateContext));
    AllocateContexts(inContext, SYMA(stepAllocateContext));
    link->fInSetupForm = true;
    RunScript(inContext, "viewSetupFormScript");
    link->fInSetupForm = false;

    link->fBounds = link->JustifiedBounds();

    RefVar declareSelf(GetProtoVariable(inContext, SYMA(declareSelf)));
    if (IsSymbol(declareSelf))
      SetFrameSlot(inContext, declareSelf, inContext);

    link->fViewFormat = IntSlot(inContext, "viewFormat");
    link->fOutset = FrameOutset(link->fViewFormat);
    link->fShadow = FrameShadow(link->fViewFormat);
    link->ReadStyle();
    link->fWidget = link->MakeWidget();
    link->fWidget->user_data(link);
    link->ApplyStyle();
    if (inParent) {
      inParent->Widget()->as_group()->add(link->fWidget);   // (every view's widget is a group)
      inParent->DropCanvas();
    }

    RunScript(inContext, "viewSetupChildrenScript");
    RefVar kids[2] = { GetProtoVariable(inContext, SYMA(viewChildren)),
                       GetProtoVariable(inContext, SYMA(stepChildren)) };
    for (RefVar & list : kids) {
      if (!IsArray(list))
        continue;
      for (ArrayIndex i = 0, n = Length(list); i < n; ++i) {
        RefVar child(ChildContext(inContext, GetArraySlot(list, i)));
        if (NOTNIL(child))
          Build(child, link);
      }
    }

    RunScript(inContext, "viewSetupDoneScript");
  }
  cleanup
  {
    Dispose(link);   // what was built so far; the exception goes on
  }
  end_try;
  return link;
}


// Close a view and its children (CView::dispose). Each link deletes its own
// widget, the children's first (their group doesn't, see GroupLink).
void Dispose(Link * inLink)
{
  if (inLink->fParent)
    inLink->fParent->DropCanvas();
  RefVar context(inLink->fContext);
  bool postQuit = false;
  newton_try
  {
    postQuit = EQ(RunScript(context, "viewQuitScript"), MakeSymbol("postQuit"));
  }
  newton_catch(exRootException)
  { }
  end_try;
  SetFrameSlot(context, SYMA(viewCObject), NILREF);

  while (!inLink->fChildren.empty())
    Dispose(inLink->fChildren.back());   // removes itself from fChildren

  if (postQuit) {
    newton_try
    {
      RunScript(context, "viewPostQuitScript");
    }
    newton_catch(exRootException)
    { }
    end_try;
  }

  // Deleting the widget right away is fine, also from its own callback (a
  // window's close button): FLTK notices (Fl_Widget_Tracker).
  Fl_Widget * widget = inLink->fWidget;
  if (widget) {
    widget->hide();
    if (Fl_Group * parent = widget->parent()) {
      parent->remove(widget);
      parent->redraw();
    }
    delete widget;
  }
  delete inLink;
}


Ref OpenView(RefArg inContext, bool inModal)
{
  if (Link::Of(inContext))
    return TRUEREF;   // open already
  RegisterBoxtypes();   // before the first window is shown (only once)
  RegisterFonts();
  RefVar parent(GetProtoVariable(inContext, SYMA(_parent)));
  Link * parentLink = Link::Of(parent);
  if (parentLink == nullptr && !EQ(parent, RootView()))
    ThrowMsg("Open: the parent view is not open");
  Link * link = Build(inContext, parentLink);
  if (parentLink == nullptr) {
    if (inModal)
      link->Widget()->as_window()->set_modal();
    link->Widget()->show();
    RunScript(inContext, "viewShowScript");
  } else {
    parentLink->Widget()->redraw();
  }
  return TRUEREF;
}


Ref CloseView(RefArg inContext)
{
  Link * link = Link::Of(inContext);
  if (link == nullptr)
    return NILREF;
  Dispose(link);
  return TRUEREF;
}

Ref GlobalBox(RefArg inContext)
{
  Link * link = OpenLink(inContext);
  return BoundsFrame(link->fInSetupForm ? link->JustifiedBounds() : link->GlobalBounds());
}


Ref LocalBox(RefArg inContext)
{
  Link * link = OpenLink(inContext);
  Bounds bounds = link->fInSetupForm ? link->JustifiedBounds() : link->GlobalBounds();
  Bounds local;
  local.right = bounds.Width();
  local.bottom = bounds.Height();
  return BoundsFrame(local);
}


Ref ChildViewFrames(RefArg inContext)
{
  Link * link = OpenLink(inContext);
  RefVar frames(MakeArray(0));
  for (Link * child : link->fChildren)
    AddArraySlot(frames, child->Context());
  return frames;
}


Ref HideView(RefArg inContext)
{
  Link * link = OpenLink(inContext);
  if (!link->fHidden) {
    link->fHidden = true;
    link->DropCanvas();
    link->Widget()->hide();
    if (Fl_Group * parent = link->Widget()->parent())
      parent->redraw();
    RunScript(inContext, "viewHideScript");
  }
  return NILREF;
}


Ref ShowView(RefArg inContext)
{
  Link * link = OpenLink(inContext);
  if (link->fHidden) {
    link->fHidden = false;
    link->DropCanvas();
    link->Widget()->show();
    RunScript(inContext, "viewShowScript");
  }
  return NILREF;
}


Ref DirtyView(RefArg inContext)
{
  Link * link = Link::Of(inContext);   // the ROM's FDirtyX: nothing to do if closed
  if (link)
    link->Dirty();
  return NILREF;
}

Ref HiliteView(RefArg inContext, RefArg inOn)
{
  // the ROM's FHiliteX: nothing to do if closed (a button's click script
  // unhilites it after buttonClickScript, which may have closed it)
  if (Link * link = Link::Of(inContext))
    link->SetHilite(NOTNIL(inOn));
  return NILREF;
}


Ref TrackHilite(RefArg inContext, RefArg inUnit)
{
  Link * link = OpenLink(inContext);
  if (!link->fPenDown)
    return NILREF;
  link->fTracking = true;
  link->SetHilite(link->fPenInside);
  // Other events come meanwhile (drawing, timers, DAP requests); scripts of
  // other views don't run (SendEventMessage: one at a time).
  Fl_Widget_Tracker widget(link->fWidget);
  while (!widget.deleted() && link->fPenDown)
    Fl::wait();
  if (widget.deleted())
    return NILREF;   // the view was closed (and the link deleted)
  link->fTracking = false;
  return link->fPenInside ? TRUEREF : NILREF;
}

Ref ModalDialog(RefArg inContext)
{
  if (Link::Of(inContext))
    ThrowMsg("ModalDialog: the view is open already");
  OpenView(inContext, true);
  RefStruct view(inContext);
  RunModalEventLoop([](void * data) { return Link::Of(*static_cast<RefStruct *>(data)) == nullptr; }, &view);
  return NILREF;
}


Ref SetupIdle(RefArg inContext, RefArg inMilliseconds)
{
  OpenLink(inContext)->SetupIdle(ISINT(inMilliseconds) ? RINT(inMilliseconds) : 0);
  return NILREF;
}


Ref StrFontWidth(RefArg inString, RefArg inFontSpec)
{
  if (!IsString(inString))
    ThrowBadTypeWithFrameData(kNSErrNotAString, inString);
  Fl_Font font;
  Fl_Fontsize size;
  FontFromSpec(inFontSpec, &font, &size);
  fl_open_display();   // measuring needs the display, even before a window
  fl_font(font, size);
  return MAKEINT(long(fl_width(UTF8FromString(inString).c_str()) + 0.5));
}

// FontAscent(fontSpec), FontDescent(fontSpec), FontLeading(fontSpec):
// inWhich 1, 2, 3 (FLTK has no leading: 0).
Ref FontMetric(RefArg inFontSpec, int inWhich)
{
  Fl_Font font;
  Fl_Fontsize size;
  FontFromSpec(inFontSpec, &font, &size, true);   // the size asked for
  fl_open_display();
  fl_font(font, size);
  switch (inWhich) {
    case 1: return MAKEINT(fl_height() - fl_descent());
    case 2: return MAKEINT(fl_descent());
  }
  return MAKEINT(0);
}


Ref FontHeight(RefArg inFontSpec)
{
  Fl_Font font;
  Fl_Fontsize size;
  FontFromSpec(inFontSpec, &font, &size, true);   // the size asked for
  fl_open_display();
  fl_font(font, size);
  return MAKEINT(fl_height());
}

Ref ViewFlags(RefArg inContext)
{
  Link * link = IsFrame(inContext) ? Link::Of(inContext) : nullptr;
  if (link == nullptr)
    return MAKEINT(0);
  long flags = IntSlot(inContext, "viewFlags");
  return MAKEINT(link->fHidden ? (flags & ~vVisible) : (flags | vVisible));
}

void ValueChanged(RefArg inContext, RefArg inTag)
{
  if (Link * link = IsFrame(inContext) ? Link::Of(inContext) : nullptr)
    link->Update(inTag);
}

Ref DragView(RefArg inContext, RefArg inUnit, RefArg inBounds)
{
  Link * link = OpenLink(inContext);
  Stroke * stroke = Stroke::Of(inUnit);
  stroke->Ink(false);
  bool limited = IsFrame(inBounds);
  Bounds limit = limited ? BoundsOf(inBounds) : Bounds();
  Bounds start = link->GlobalBounds();
  PenPoint first = stroke->First();
  long movedX = 0, movedY = 0;
  auto clamp = [](long d, long lo, long hi) { return lo > hi ? 0 : std::max(lo, std::min(hi, d)); };
  auto follow = [&]() {
    long dx = stroke->Last().x - first.x, dy = stroke->Last().y - first.y;
    if (limited) {   // the view stays within the bounds
      dx = clamp(dx, limit.left - start.left, limit.right - start.right);
      dy = clamp(dy, limit.top - start.top, limit.bottom - start.bottom);
    }
    link->MoveBy(dx - movedX, dy - movedY);
    movedX = dx;
    movedY = dy;
  };
  // the view follows the pen until it comes up (a nested event loop, as
  // TrackHilite; the ROM moves a picture of the view, then the view). It
  // is drawn over its siblings meanwhile, then back in its place.
  Fl_Widget_Tracker widget(link->Widget());
  Fl_Group * parent = link->Widget()->parent();
  int place = parent ? parent->find(link->Widget()) : 0;
  if (parent)
    parent->add(link->Widget());   // the last: drawn over the others
  while (!widget.deleted() && !stroke->Done()) {
    Fl::wait();
    if (!widget.deleted())
      follow();
  }
  if (!widget.deleted() && parent && parent->find(link->Widget()) < parent->children())
    parent->insert(*link->Widget(), place);
  return NILREF;
}

} // namespace nfl
