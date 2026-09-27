/*
 File: Links.cc

 Links between Newton views and FLTK widgets. See Links.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>

#include "Host/FLTK/Links.h"
#include "Host/FLTK/FloatNGo.h"
#include "Host/FLTK/Boxtypes.h"
#include "Host/FLTK/Widgets.h"
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
  return IsString(text) ? UTF8FromString(text) : std::string();
}

// The FLTK font for a view's viewFont: a font frame ({family, face, size})
// or a font spec (an integer: family, size, face in bits, tsFamilyMask ...).
// newtc has no Newton fonts yet: Helvetica for Espy and Geneva, Times for
// New York. The face bits (bold 1, italic 2) are FLTK's.
void FontOf(RefArg inContext, Fl_Font * outFont, Fl_Fontsize * outSize)
{
  RefVar viewFont(GetProtoVariable(inContext, SYMA(viewFont)));
  long family = 0, face = 0, size = 12;
  if (IsFrame(viewFont)) {
    RefVar name(GetFrameSlot(viewFont, SYMA(family)));
    if (EQ(name, MakeSymbol("newYork")))
      family = 1;
    face = IntSlot(viewFont, "face");
    if (long s = IntSlot(viewFont, "size"))
      size = s;
  } else if (ISINT(viewFont)) {
    long spec = RINT(viewFont);
    family = (spec & tsFamilyMask) >> tsFamilyShift;
    face = (spec & tsFaceMask) >> tsFaceShift;
    if (long s = (spec & tsSizeMask) >> tsSizeShift)
      size = s;
  }
  *outFont = (family == 1 ? FL_TIMES : FL_HELVETICA) + Fl_Font(face & 3);
  *outSize = Fl_Fontsize(size);
}

// FLTK's alignment for the H and V bits of a viewJustify (how a view places
// its text or picture: vjLeftH, vjCenterV, ...).
Fl_Align AlignOf(long inJustify)
{
  Fl_Align align = FL_ALIGN_CENTER;
  switch (inJustify & vjHMask) {
    case vjLeftH:  align |= FL_ALIGN_LEFT; break;
    case vjRightH: align |= FL_ALIGN_RIGHT; break;
  }
  switch (inJustify & vjVMask) {
    case vjTopV:    align |= FL_ALIGN_TOP; break;
    case vjBottomV: align |= FL_ALIGN_BOTTOM; break;
  }
  return align;
}

// A view's icon slot: a frame with a Newton bitmap in its bits slot (a
// binary: 4 bytes, rowBytes, 2 bytes, then top, left, bottom, right, then
// the rows; all 16-bit big-endian; the leftmost pixel in the high bit).
// Empty if there is none.
NewtonBitmap IconOf(RefArg inContext)
{
  NewtonBitmap icon;
  RefVar frame(GetProtoVariable(inContext, SYMA(icon)));
  if (!IsFrame(frame))
    return icon;
  RefVar bits(GetFrameSlot(frame, SYMA(bits)));
  if (!IsBinary(bits) || Length(bits) < 16)
    return icon;
  const unsigned char * data = (const unsigned char *)BinaryData(bits);
  auto word = [data](int offset) { return int(short((data[offset] << 8) | data[offset + 1])); };
  int rowBytes = word(4), top = word(8), left = word(10), bottom = word(12), right = word(14);
  if (rowBytes <= 0 || right <= left || bottom <= top
   || Length(bits) < ArrayIndex(16 + rowBytes * (bottom - top)))
    return icon;
  icon.width = right - left;
  icon.height = bottom - top;
  icon.rowBytes = rowBytes;
  icon.bits.assign(data + 16, data + 16 + rowBytes * icon.height);
  return icon;
}

// How far a view's frame reaches out of its bounds (viewFormat): a dragger
// frame (protoFloater, protoFloatNGo) kDraggerBorderWidth, another frame
// its pen width and inset. (Not yet: the shadow, right and bottom.)
int FrameOutset(long inViewFormat)
{
  long frame = (inViewFormat & vfFrameMask) >> vfFrameShift;
  if (frame == vfDragger)
    return kDraggerBorderWidth;
  if (frame == vfNone)
    return 0;
  return int(((inViewFormat & vfPenMask) >> vfPenShift) + ((inViewFormat & vfInsetMask) >> vfInsetShift));
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
    return NewWidget<Group>(WidgetX(), WidgetY(),
                            int(fBounds.Width()), int(fBounds.Height()));
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
    Fl_Box * box = NewWidget<Fl_Box>(WidgetX(), WidgetY(),
                                     int(fBounds.Width()), int(fBounds.Height()));
    box->copy_label(TextSlot(fContext, "text").c_str());
    box->align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT | FL_ALIGN_WRAP);
    Fl_Font font;
    Fl_Fontsize size;
    FontOf(fContext, &font, &size);
    box->labelfont(font);
    box->labelsize(size);
    return box;
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
    Fl_Font font;
    Fl_Fontsize size;
    FontOf(fContext, &font, &size);
    TextView * view = NewWidget<TextView>(WidgetX(), WidgetY(),
                                          int(fBounds.Width()), int(fBounds.Height()),
                                          TextSlot(fContext, "text"), font, size,
                                          AlignOf(IntSlot(fContext, "viewJustify")));
    Fl_Color color;
    view->box(BoxForFormat(IntSlot(fContext, "viewFormat"), &color));
    view->color(color);
    return view;
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
    PictureView * view = NewWidget<PictureView>(WidgetX(), WidgetY(),
                                                int(fBounds.Width()), int(fBounds.Height()),
                                                IconOf(fContext), AlignOf(IntSlot(fContext, "viewJustify")));
    Fl_Color color;
    view->box(BoxForFormat(IntSlot(fContext, "viewFormat"), &color));
    view->color(color);
    return view;
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
    // the frame is outside the view's bounds (as on a Newton): around them
    fOutset = FrameOutset(IntSlot(fContext, "viewFormat"));
    return NewWidget<FloatNGo>(int(kDesktopLeft + fBounds.left - fOutset), int(kDesktopTop + fBounds.top - fOutset),
                               int(fBounds.Width() + 2 * fOutset), int(fBounds.Height() + 2 * fOutset),
                               title.c_str(), this);
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


Link::Link(RefArg inContext, Link * inParent)
: fContext(inContext), fParent(inParent)
{
  if (fParent)
    fParent->fChildren.push_back(this);
}


Link::~Link()
{
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
  return int(fBounds.left - window->fBounds.left + window->fOutset);
}


int Link::WidgetY()
{
  Link * window = Window();
  return int(fBounds.top - window->fBounds.top + window->fOutset);
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


int Link::HandlePen(Fl_Widget * inWidget, int inEvent)
{
  switch (inEvent) {
    case FL_PUSH: {
      if ((IntSlot(fContext, "viewFlags") & vClickable) == 0)
        return 0;
      fPenDown = fPenInside = true;
      // FLTK makes the widget the pushed one only after this returns; but
      // TrackHilite() waits for the pen to come up in here
      Fl::pushed(inWidget);
      // viewClickScript through _proto only (not a parent's), as runScript
      RefVar script(MakeSymbol("viewClickScript"));
      if (NOTNIL(GetProtoVariable(fContext, script))) {
        RefVar args(MakeArray(1));   // the unit: nil for now
        SendEventMessage(fContext, script, args);
        // the script may have closed the view: this link may be gone
      }
      return 1;
    }
    case FL_DRAG:
      if (fPenDown) {
        bool inside = Fl::event_inside(inWidget);
        if (inside != fPenInside) {
          fPenInside = inside;
          if (fTracking)
            SetHilite(inside);
        }
        return 1;
      }
      return 0;
    case FL_RELEASE:
      if (fPenDown) {
        fPenInside = Fl::event_inside(inWidget);
        fPenDown = false;
        return 1;
      }
      return 0;
  }
  return 0;
}


void Link::SetHilite(bool inOn)
{
  if (fHilited == inOn)
    return;
  fHilited = inOn;
  if (fWidget)
    fWidget->redraw();
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
      if (link && Fl::pushed() == this)
        return link->HandlePen(this, inEvent);
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
    link->fInSetupForm = true;
    RunScript(inContext, "viewSetupFormScript");
    link->fInSetupForm = false;

    link->fBounds = link->JustifiedBounds();

    RefVar declareSelf(GetProtoVariable(inContext, SYMA(declareSelf)));
    if (IsSymbol(declareSelf))
      SetFrameSlot(inContext, declareSelf, inContext);

    link->fWidget = link->MakeWidget();
    link->fWidget->user_data(link);
    if (inParent) {
      Fl_Group * group = inParent->Widget()->as_group();
      if (group == nullptr)   // a parent that can't hold widgets: the window
        group = inParent->Window()->Widget()->as_group();
      group->add(link->fWidget);
    }

    RunScript(inContext, "viewSetupChildrenScript");
    RefVar kids[2] = { GetProtoVariable(inContext, SYMA(viewChildren)),
                       GetProtoVariable(inContext, SYMA(stepChildren)) };
    for (RefVar & list : kids) {
      if (!IsArray(list))
        continue;
      for (ArrayIndex i = 0, n = Length(list); i < n; ++i) {
        RefVar child(BuildViewContext(GetArraySlot(list, i), false));
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


Ref OpenView(RefArg inContext)
{
  if (Link::Of(inContext))
    return TRUEREF;   // open already
  RegisterBoxtypes();   // before the first window is shown (only once)
  RefVar parent(GetProtoVariable(inContext, SYMA(_parent)));
  Link * parentLink = Link::Of(parent);
  if (parentLink == nullptr && !EQ(parent, RootView()))
    ThrowMsg("Open: the parent view is not open");
  Link * link = Build(inContext, parentLink);
  if (parentLink == nullptr) {
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
    link->Widget()->show();
    RunScript(inContext, "viewShowScript");
  }
  return NILREF;
}


Ref DirtyView(RefArg inContext)
{
  Link * link = Link::Of(inContext);   // the ROM's FDirtyX: nothing to do if closed
  if (link)
    link->Widget()->redraw();
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

} // namespace nfl
