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
const long clParagraphView = 81;

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
// display (displayParams, as CView::justifyBounds).
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

// A view's bounds on the display: its viewBounds placed in its parent's
// (global) bounds by viewJustify (CView::justifyBounds: parent
// justification; not yet sibling justification and ratios).
Bounds Justify(Bounds inBounds, long inJustify, const Bounds & inParent)
{
  long originH = inParent.left, originV = inParent.top;
  switch (inJustify & vjParentVMask) {
    case vjParentCenterV: originV += (inParent.Height() - inBounds.Height()) / 2; break;
    case vjParentBottomV: originV += inParent.Height(); break;
    case vjParentFullV:   originV = 0; inBounds.top += inParent.top; inBounds.bottom += inParent.bottom; break;
  }
  switch (inJustify & vjParentHMask) {
    case vjParentCenterH: originH += (inParent.Width() - inBounds.Width()) / 2; break;
    case vjParentRightH:  originH += inParent.Width(); break;
    case vjParentFullH:   originH = 0; inBounds.left += inParent.left; inBounds.right += inParent.right; break;
  }
  inBounds.left += originH; inBounds.right += originH;
  inBounds.top += originV; inBounds.bottom += originV;
  return inBounds;
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
    const Bounds & window = Window()->GlobalBounds();
    return NewWidget<Group>(int(fBounds.left - window.left), int(fBounds.top - window.top),
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
    const Bounds & window = Window()->GlobalBounds();
    Fl_Box * box = NewWidget<Fl_Box>(int(fBounds.left - window.left), int(fBounds.top - window.top),
                                     int(fBounds.Width()), int(fBounds.Height()));
    box->copy_label(TextSlot(fContext, "text").c_str());
    box->align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT | FL_ALIGN_WRAP);
    box->labelsize(12);
    return box;
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
    return NewWidget<FloatNGo>(int(kDesktopLeft + fBounds.left), int(kDesktopTop + fBounds.top),
                               int(fBounds.Width()), int(fBounds.Height()), title.c_str(), this);
  }
};


// The link class for a view (see Links.h).
Link * NewLink(RefArg inContext, Link * inParent)
{
  if (inParent == nullptr)
    return new WindowLink(inContext, inParent);
  if (IntSlot(inContext, "viewClass") == clParagraphView)
    return new ParagraphLink(inContext, inParent);
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
    RunScript(inContext, "viewSetupFormScript");

    RefVar viewBounds(GetProtoVariable(inContext, SYMA(viewBounds)));
    if (!IsFrame(viewBounds))
      ThrowErr(exRootException, -8505);   // no view bounds
    link->fBounds = Justify(BoundsOf(viewBounds), IntSlot(inContext, "viewJustify"),
                            inParent ? inParent->GlobalBounds() : AppArea());

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

} // namespace nfl
