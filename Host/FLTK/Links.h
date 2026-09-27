/*
 File: Links.h

 Links between Newton views and FLTK widgets (namespace nfl, "Newton on
 FLTK"). They take the place of NewtonOS's C++ views (CView, Views/View.cc,
 which newtc doesn't compile) and follow what CView does.

 An open view has a link: the view frame's viewCObject slot refers to it (as
 in NewtonOS, where it refers to the CView; AddressToRef), and so does its
 widget's user_data(). A closed view has no link (viewCObject nil). The link
 keeps the view frame (a RefStruct).

 Every link owns its widget and deletes it when the view closes, children
 before their parent. So a widget that holds the children's widgets (a
 GroupLink's) must not delete them itself, as an Fl_Group normally does:
 nfl::Group and nfl::FloatNGo only take them out (delete_child(), see
 GroupLink::RemoveChild).

 The link class follows the view: a child of the root view is window-like
 and gets a desktop window (WindowLink, nfl::FloatNGo); a clParagraphView
 shows its text (ParagraphLink); any other view is a group for its children
 (ViewLink, nfl::Group) for now.

 Opening (FOpenX, the ROM's view:_Open(); CView::init): viewSetupFormScript;
 the bounds (viewBounds, placed in the parent with viewJustify); declareSelf;
 the widget; viewSetupChildrenScript; the children (viewChildren, then
 stepChildren: each a view frame from BuildContext, opened the same way);
 viewSetupDoneScript; then a window is shown, and viewShowScript runs.
 Closing (FCloseX, view:Close(); CView::dispose): viewQuitScript, viewCObject
 nil, the children closed the same way, viewPostQuitScript if the quit
 script returned 'postQuit; then the widget goes (a window closes).
 View scripts are found through _proto only, not _parent (as CView does).

 The view methods (Host/ViewMethods.cc) act on a view's link: GlobalBox,
 LocalBox, ChildViewFrames, Hide, Show, Dirty. Like the ROM's
 (FailGetView), they throw "nil view" for a view that isn't open.

 Not yet: reflow, allocateContext, drawing, events other than closing a
 window.
 */

#ifndef HOST_FLTK_LINKS_H
#define HOST_FLTK_LINKS_H

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl_Group.H>

#include "Frames/Objects.h"

#include <vector>

namespace nfl {

/** A rectangle in Newton coordinates (the whole Newton display). */
struct Bounds
{
  long left = 0, top = 0, right = 0, bottom = 0;
  long Width() const { return right - left; }
  long Height() const { return bottom - top; }
};

class Link
{
public:
  virtual ~Link();

  /** The link of an open view (its viewCObject), or nullptr. */
  static Link * Of(RefArg inContext);

  RefArg Context() const { return fContext; }
  Link * Parent() const { return fParent; }
  Fl_Widget * Widget() const { return fWidget; }
  const Bounds & GlobalBounds() const { return fBounds; }

  /** The user closed the view's window: view:Close(), as an event. */
  void SendClose();

  /** The view's bounds on the display, from its viewBounds and viewJustify
      now (in its parent, or its previous sibling). */
  Bounds JustifiedBounds();

protected:
  Link(RefArg inContext, Link * inParent);

  /** Make the widget for fBounds (not added to a parent yet). */
  virtual Fl_Widget * MakeWidget() = 0;

  /** The link of the window this view is in (its own for a WindowLink). */
  Link * Window();

  RefStruct fContext;
  Link * fParent;
  std::vector<Link *> fChildren;
  Bounds fBounds;
  Fl_Widget * fWidget = nullptr;
  bool fInSetupForm = false;   // viewSetupFormScript runs (fBounds not set yet)
  bool fHidden = false;        // Hide()

  friend Link * Build(RefArg inContext, Link * inParent);
  friend void Dispose(Link * inLink);
  friend Ref GlobalBox(RefArg inContext);
  friend Ref LocalBox(RefArg inContext);
  friend Ref ChildViewFrames(RefArg inContext);
  friend Ref HideView(RefArg inContext);
  friend Ref ShowView(RefArg inContext);
};

/** A link whose widget holds the widgets of the view's children (a group,
    a window). Those widgets belong to the children's links, so the group
    widget must not delete them: its delete_child() calls RemoveChild(). */
class GroupLink : public Link
{
public:
  /** For a group widget's delete_child() (FLTK calls it from clear(), and
      so from the destructor): take the child out of the group without
      deleting it (Fl_Group::delete_child() would); its link deletes it.
      Returns 0, or 1 for an index out of range (as Fl_Group's).
      Doesn't use a link: the group's link may be gone already. */
  static int RemoveChild(Fl_Group * inGroup, int inIndex);

protected:
  GroupLink(RefArg inContext, Link * inParent) : Link(inContext, inParent) { }
};

/** The widget of a ViewLink: a group for the children's widgets that
    doesn't delete them (see GroupLink). */
class Group : public Fl_Group
{
public:
  Group(int x, int y, int w, int h) : Fl_Group(x, y, w, h) { }
  // clear() here, not only in ~Fl_Group(): there, the object is an Fl_Group
  // already, and clear() would call Fl_Group::delete_child() (which deletes)
  ~Group() override { clear(); }
protected:
  int delete_child(int inIndex) override { return GroupLink::RemoveChild(this, inIndex); }
};

/** view:_Open() (FOpenX): open the view (and its children) if it isn't
    open yet. Throws if its parent isn't open. Returns true. */
Ref OpenView(RefArg inContext);

/** view:Close() (FCloseX): close the view and its children. Returns true,
    nil if it wasn't open. */
Ref CloseView(RefArg inContext);

/** view:GlobalBox(), view:LocalBox(): the view's bounds on the display, or
    with its top left at 0, 0 ({left, top, right, bottom}). While its
    viewSetupFormScript runs, from its viewBounds as they are then (as the
    ROM's CommonBox). */
Ref GlobalBox(RefArg inContext);
Ref LocalBox(RefArg inContext);

/** view:ChildViewFrames(): the view frames of its open children. */
Ref ChildViewFrames(RefArg inContext);

/** view:Hide(), view:Show(): hide or show the view (viewHideScript,
    viewShowScript). It stays open. (The ROM queues these; newtc does them
    at once.) */
Ref HideView(RefArg inContext);
Ref ShowView(RefArg inContext);

/** view:Dirty(): draw the view again. */
Ref DirtyView(RefArg inContext);

} // namespace nfl

#endif // HOST_FLTK_LINKS_H
