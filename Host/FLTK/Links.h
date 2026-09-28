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
 GroupLink's, and every view widget: any view can have children) must not
 delete them itself, as an Fl_Group normally does: nfl::Group,
 nfl::ViewWidget and nfl::FloatNGo only take them out (delete_child(), see
 GroupLink::RemoveChild).

 The link class follows the view: a child of the root view is window-like
 and gets a desktop window (WindowLink, nfl::FloatNGo), bigger than the
 view by its frame all around (NewtonOS draws a view's frame outside its
 viewBounds; protoFloatNGo's dragger frame is 8 pixels), so its children's
 widgets sit 8 pixels in from the window's edges; a clParagraphView
 shows its text (ParagraphLink), a clTextView too (TextLink, nfl::TextView,
 a protoTextButton), a clPictureView its icon (PictureLink,
 nfl::PictureView, a protoClosebox); any other view is a group (ViewLink,
 nfl::Group) for now. Each widget holds its children's widgets and draws
 them after what it shows, then the view's frame over them (as the ROM).

 Opening (FOpenX, the ROM's view:_Open(); CView::init): the view frames of
 children its scripts use early (allocateContext, stepAllocateContext;
 TView::Constructor); viewSetupFormScript;
 the bounds (viewBounds, placed in the parent with viewJustify); declareSelf;
 the widget; viewSetupChildrenScript; the children (viewChildren, then
 stepChildren: each a view frame from BuildContext, or the one allocated
 for it (preAllocatedContext), opened the same way);
 viewSetupDoneScript; then a window is shown, and viewShowScript runs.
 Closing (FCloseX, view:Close(); CView::dispose): viewQuitScript, viewCObject
 nil, the children closed the same way, viewPostQuitScript if the quit
 script returned 'postQuit; then the widget goes (a window closes).
 View scripts are found through _proto only, not _parent (as CView does).

 The view methods (Host/ViewMethods.cc) act on a view's link: GlobalBox,
 LocalBox, ChildViewFrames, Hide, Show, Dirty. Like the ROM's
 (FailGetView), they throw "nil view" for a view that isn't open.

 Pen (the mouse): a pen down on a view with vClickable in its viewFlags
 sends it viewClickScript(unit), as NewtonOS does; the unit is the stroke
 (Pen.h), whose points the pen events add. A button's viewClickScript (protoTextButton, protoPictureButton)
 calls TrackHilite(unit): it waits while the pen is down (a nested FLTK
 event loop, as the ROM waits in its own), hilites the view while the pen
 is inside, and returns true if the pen came up inside; then the script
 sends buttonClickScript() and Hilite(nil).

 Every view widget draws its fill and frame itself, first thing in its
 draw(): DrawViewFormat (Widgets.h) with the viewFormat its link keeps
 (Link::ViewFormat(); SetValue updates it); its box is VIEW_BOX
 (Boxtypes.h), which draws nothing.

 Not yet: reflow, buttonPressedScript, the click sound, strokes and gestures.
 */

#ifndef HOST_FLTK_LINKS_H
#define HOST_FLTK_LINKS_H

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl_Window.H>

#include "Frames/Objects.h"
#include "Host/FLTK/Fonts.h"
#include "Host/FLTK/Pen.h"

class Fl_Image_Surface;
class Fl_RGB_Image;

#include <functional>
#include <string>
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
  /** How far its frame reaches out of its bounds (its widget is that much
      bigger all around). */
  int Outset() const { return fOutset; }
  /** How much more its shadow takes at the right and the bottom (its
      widget is that much wider and higher). */
  int Shadow() const { return fShadow; }
  /** Its viewFormat (fill, frame, pen, inset, roundness, ...), kept here
      for the widget's draw() (DrawViewFormat, Widgets.h): read when the
      view opens, again when SetValue changes it. */
  long ViewFormat() const { return fViewFormat; }
  /** Its viewFont (FLTK's font and size) and viewJustify, kept here as
      viewFormat is, and given to the widget: labelfont(), labelsize(),
      align() (AlignFor()). The viewFont as NewtonScript finds it: in the
      view's protos, else its parents' (the ROM's getVar). */
  Fl_Font Font() const { return fFont; }
  Fl_Fontsize FontSize() const { return fFontSize; }
  long ViewJustify() const { return fViewJustify; }

  /** The user closed the view's window: view:Close(), as an event. */
  void SendClose();

  /** The view's bounds on the display, from its viewBounds and viewJustify
      now (in its parent, or its previous sibling). */
  Bounds JustifiedBounds();

  /** A pen event (FL_PUSH, FL_DRAG, FL_RELEASE) on the view's widget
      inWidget; for its handle(). Returns 1 if the view takes the pen. */
  int HandlePen(Fl_Widget * inWidget, int inEvent);

  /** A slot of the view frame changed (SetValue): show it. The link
      classes know their own slots (text); the bounds and a redraw here. */
  virtual void Update(RefArg inTag);
  /** view:Dirty(): drawn anew (a view that reads its slots when it draws on
      a Newton reads them again). */
  virtual void Dirty();

  /** A point on the Newton display, on the screen (via the view's window). */
  void ScreenPoint(long inX, long inY, int * outX, int * outY);

  /** Move the view, its children and their widgets by dx, dy (Drag). A
      window moves on the desktop. */
  void MoveBy(long inDX, long inDY);

  /** The view's canvas, made if it has none (Drawing.h); dropping it, and
      those of the views it is in (the view system draws there anew). */
  Fl_Image_Surface * Canvas();
  bool HasCanvas() const { return fCanvas != nullptr; }
  void DropCanvas();

  /** Hilite(), TrackHilite(): the widget draws the view hilited (not if
      its viewHiliteScript did: SetHilite). */
  bool Hilited() const { return fHilited && !fScriptHilited; }

  /** The view's window moved or resized: keep the link's position in sync
      (fBounds: Newton global coordinates, inside the frame). */
  void UpdatePosition(Fl_Window * inWindow);

protected:
  Link(RefArg inContext, Link * inParent);

  /** Make the widget for fBounds (not added to a parent yet). */
  virtual Fl_Widget * MakeWidget() = 0;

  /** The link of the window this view is in (its own for a WindowLink). */
  Link * Window();

  /** Where the widget goes in its window (FLTK's window coordinates): the
      view's bounds relative to the window's content (which is the window's
      fOutset in from its edges), and around them the view's frame (its own
      fOutset; and fShadow more at the right and the bottom): NewtonOS
      draws a view's frame outside its bounds. */
  int WidgetX();
  int WidgetY();
  int WidgetW() const { return int(fBounds.Width() + 2 * fOutset + fShadow); }
  int WidgetH() const { return int(fBounds.Height() + 2 * fOutset + fShadow); }

  RefStruct fContext;
  Link * fParent;
  std::vector<Link *> fChildren;
  Bounds fBounds;
  Fl_Widget * fWidget = nullptr;
  int fOutset = 0;             // its frame, around the view's bounds
  int fShadow = 0;             // its shadow, right and bottom (Shadow())
  long fViewFormat = 0;        // its viewFormat (ViewFormat())
  Fl_Font fFont = FL_HELVETICA;   // its viewFont (Font(), FontSize())
  Fl_Fontsize fFontSize = 12;
  long fViewJustify = 0;       // its viewJustify (ViewJustify())
  /** Read viewFont and viewJustify (ReadStyle), give them to the widget
      (ApplyStyle). */
  void ReadStyle();
  void ApplyStyle();
  /** The widget's align() for a viewJustify: its H and V bits. */
  virtual Fl_Align AlignFor(long inJustify) const;
  bool fInSetupForm = false;   // viewSetupFormScript runs (fBounds not set yet)
  bool fHidden = false;        // Hide()
  bool fHilited = false;       // Hilite(), TrackHilite()
  bool fScriptHilited = false; // ... and its viewHiliteScript did it (SetHilite)
  bool fPenDown = false;       // a pen down on the view, not up yet
  bool fPenInside = false;     // ... and the pen is inside it
  bool fTracking = false;      // TrackHilite() waits for the pen to come up

  void SetHilite(bool inOn);
  PenPoint PenAt(Fl_Widget * inWidget);
  void OffsetBounds(long inDX, long inDY);
  void Place();
  void Layout();
  Stroke * fStroke = nullptr;  // the stroke of the pen down on the view
  Fl_Image_Surface * fCanvas = nullptr;   // what scripts drew (Drawing.h),
  Fl_Image_Surface * fMask = nullptr;     // which pixels they drew,
  Fl_RGB_Image * fOverlay = nullptr;      // and both, to draw on top;
  Fl_Image_Surface * fInvert = nullptr;   // what XOR shapes cover (white),
  Fl_RGB_Image * fInvertOverlay = nullptr;   // to invert on top
  bool fInverts = false;                  // ... if they cover anything
  friend void DrawOnView(Link * inLink, const std::function<void(long, long)> & inDraw);
  friend void DrawOverlay(Fl_Widget * inWidget);
  void SetupIdle(long inMilliseconds);
  static void IdleTimeout(void * inLink);

  friend Link * Build(RefArg inContext, Link * inParent);
  friend void Dispose(Link * inLink);
  friend Ref GlobalBox(RefArg inContext);
  friend Ref LocalBox(RefArg inContext);
  friend Ref ChildViewFrames(RefArg inContext);
  friend Ref HideView(RefArg inContext);
  friend Ref ShowView(RefArg inContext);
  friend Ref ViewFlags(RefArg inContext);
  friend Ref HiliteView(RefArg inContext, RefArg inOn);
  friend Ref TrackHilite(RefArg inContext, RefArg inUnit);
  friend Ref SetupIdle(RefArg inContext, RefArg inMilliseconds);
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
  Group(int x, int y, int w, int h);
  // clear() here, not only in ~Fl_Group(): there, the object is an Fl_Group
  // already, and clear() would call Fl_Group::delete_child() (which deletes)
  ~Group() override { clear(); }
  /** Its fill and frame (DrawViewFormat), its viewDrawScript, its
      children, then what scripts drew (Drawing.h). */
  void draw() override;
  /** Pen events the children don't take go to the view (Link::HandlePen). */
  int handle(int inEvent) override;
protected:
  int delete_child(int inIndex) override { return GroupLink::RemoveChild(this, inIndex); }
};

/** view:_Open() (FOpenX): open the view (and its children) if it isn't
    open yet. Throws if its parent isn't open. Returns true. inModal: a
    window that blocks the others (ModalDialog). */
Ref OpenView(RefArg inContext, bool inModal = false);

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

/** SetValue(view, slot, value) set the slot: an open view shows it (its
    text, its bounds; else it is drawn again). */
void ValueChanged(RefArg inContext, RefArg inTag);

/** GetViewFlags(view): its viewFlags now (vVisible off while hidden); 0 if
    it isn't open (the ROM's FGetFlags). */
Ref ViewFlags(RefArg inContext);

/** view:Hilite(on): draw the view hilited (on: non-nil) or not. Nothing
    for a closed view (as the ROM). */
Ref HiliteView(RefArg inContext, RefArg inOn);

/** view:Drag(unit, bounds): in a viewClickScript, while the pen is down:
    the view follows the pen (it stays within bounds, global, if given);
    then it is where the pen left it. Returns nil. */
Ref DragView(RefArg inContext, RefArg inUnit, RefArg inBounds);

/** view:TrackHilite(unit): in a viewClickScript, while the pen is down:
    hilite the view while the pen is inside it. Returns true if the pen came
    up inside (the view stays hilited), else nil. Nil at once if no pen is
    down on the view. */
Ref TrackHilite(RefArg inContext, RefArg inUnit);

/** view:ModalDialog(): open the view (a child of the root view) as a modal
    window and wait until it is closed; events run their scripts meanwhile
    (RunModalEventLoop, Matt/EventLoop.h). Returns nil. */
Ref ModalDialog(RefArg inContext);

/** view:SetupIdle(milliseconds): send the view viewIdleScript() after that
    long, and again after the milliseconds it returns, until it returns nil
    (or the view closes). 0 or nil: stop. */
Ref SetupIdle(RefArg inContext, RefArg inMilliseconds);

/** StrFontWidth(string, fontSpec): the string's width in pixels. */
Ref StrFontWidth(RefArg inString, RefArg inFontSpec);

/** FontHeight(fontSpec): a line's height in pixels (ascent and descent). */
Ref FontHeight(RefArg inFontSpec);

/** FontAscent (inWhich 1), FontDescent (2), FontLeading (3: 0 here). */
Ref FontMetric(RefArg inFontSpec, int inWhich);

/** Text for FLTK (UTF-8) with Newton's own characters (U+FC01 the picker
    diamond, U+FC0B the check mark, ...) as Unicode shows them. */
std::string DisplayText(std::string inText);

} // namespace nfl

#endif // HOST_FLTK_LINKS_H
