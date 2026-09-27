/*
 File: TestWindow.cc

 A window for testing the event loop. See TestWindow.h.
 */

#if NEWTC_USES_FLTK

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/fl_draw.H>

#include "Matt/TestWindow.h"
#include "Host/FLTK/Links.h"
#include "Host/FLTK/Widgets.h"
#include "Matt/EventLoop.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"

#include <deque>
#include <memory>

namespace {

// A button that sends a message to a NewtonScript object when clicked. It
// owns what it sends as RefStructs: the garbage collector updates them when
// it moves the objects (a plain Ref in user_data() would go stale), and they
// go away with the button. The pattern for widgets that stand for Newton
// objects. Not RefVar: a RefVar's handle belongs to the stack position it
// was made at, and when a NewtonScript exception unwinds past it (longjmp:
// no destructors), ClearRefHandles() frees it; a RefStruct's handle is
// freed only by its destructor.
class MessageButton : public Fl_Button
{
public:
  MessageButton(int x, int y, int w, int h, const char * label, RefArg inReceiver, RefArg inMessage)
  : Fl_Button(x, y, w, h, label), receiver(inReceiver), message(inMessage)
  {
    callback([](Fl_Widget * w, void *) {
      MessageButton * button = static_cast<MessageButton *>(w);
      RefVar args(MakeArray(0));
      SendEventMessage(button->receiver, button->message, args);
    });
  }

private:
  RefStruct receiver;
  RefStruct message;
};

Fl_Window * gWindow = nullptr;
MessageButton * gButton = nullptr;

void CloseWindow(void)
{
  if (gWindow == nullptr)
    return;
  gWindow->hide();
  delete gWindow;   // fine also inside the button's callback (Fl_Widget_Tracker)
  gWindow = nullptr;
  gButton = nullptr;
}

} // namespace


Ref FTestWindow(RefArg rcvr, RefArg inTitle, RefArg inReceiver, RefArg inMessage)
{
  if (!IsSymbol(inMessage))
    ThrowBadTypeWithFrameData(kNSErrNotASymbol, inMessage);
  CloseWindow();
  gWindow = new Fl_Window(240, 100);
  gWindow->copy_label(IsString(inTitle) ? UTF8FromString(inTitle).c_str() : "newtc");
  gButton = new MessageButton(20, 30, 200, 40, "Click", inReceiver, inMessage);
  gWindow->end();
  gWindow->show();
  return NILREF;
}


Ref FTestWindowClick(RefArg rcvr)
{
  if (gButton != nullptr)
    Fl::add_timeout(0.0, [](void *) { if (gButton) gButton->do_callback(); });
  return NILREF;
}


Ref FTestWindowClose(RefArg rcvr)
{
  CloseWindow();
  return NILREF;
}

// The window is looked up when the timeout fires, in the event loop: by
// then the view is open even if this was called earlier (e.g. while the
// program still installs its app).
Ref FTestCloseWindow(RefArg rcvr, RefArg inView)
{
  Fl::add_timeout(0.0, [](void * data) {
    RefStruct * view = static_cast<RefStruct *>(data);
    nfl::Link * link = nfl::Link::Of(*view);
    if (link && link->Widget()->top_window())
      link->Widget()->top_window()->do_callback();
    delete view;
  }, new RefStruct(inView));
  return NILREF;
}

namespace {

// A pen event at x, y in the window (FLTK's event state, as the platform
// code sets it), through FLTK's dispatch.
// x, y on the screen: a mouse is where it is, even if the window moved
// under it (a window being dragged).
void SendPen(Fl_Window * inWindow, int inEvent, int x, int y)
{
  Fl::e_x_root = x;
  Fl::e_y_root = y;
  Fl::e_x = x - inWindow->x_root();
  Fl::e_y = y - inWindow->y_root();
  Fl::e_keysym = FL_Button + FL_LEFT_MOUSE;
  if (inEvent == FL_RELEASE)
    Fl::e_state &= ~FL_BUTTON1;
  else
    Fl::e_state |= FL_BUTTON1;
  Fl::handle(inEvent, inWindow);   // may close the window
}

// The window is still there (the tap may have closed it).
bool WindowExists(Fl_Window * inWindow)
{
  for (Fl_Window * w = Fl::first_window(); w; w = Fl::next_window(w))
    if (w == inWindow)
      return true;
  return false;
}

struct Snapshot
{
  RefStruct view;
  std::string path;
  int tries = 0;
};

// The window can be captured once it is on the screen: until then
// fl_capture_window() gives nothing, so try again a little later.
void TakeSnapshot(void * data)
{
  Snapshot * snapshot = static_cast<Snapshot *>(data);
  nfl::Link * link = nfl::Link::Of(snapshot->view);
  Fl_Window * window = link ? link->Widget()->top_window() : nullptr;
  if (window == nullptr || !window->shown()) {
    fprintf(stderr, "newtc: TestSnapshot: the view has no window\n");
    delete snapshot;
    return;
  }
  Fl::flush();   // drawn as it is now
  window->make_current();
  Fl_RGB_Image * image = fl_capture_window(window, 0, 0, window->w(), window->h());
  if (image == nullptr && ++snapshot->tries < 40) {
    Fl::add_timeout(0.05, TakeSnapshot, snapshot);
    return;
  }
  if (image == nullptr || fl_write_png(snapshot->path.c_str(), image) != 0)
    fprintf(stderr, "newtc: TestSnapshot: can't write %s\n", snapshot->path.c_str());
  delete image;
  delete snapshot;
}


struct Tap
{
  RefStruct view;         // the view to tap, or
  std::string text;       // the text of the button to tap
  bool outside;
  int dx = 0, dy = 0;     // a drag: where the pen goes (TestDrag)
  bool at = false;        // pen down at fromX, fromY in the view (TestPen), not its center
  int fromX = 0, fromY = 0;
  std::string snapshot;   // not a tap: a snapshot of the view's window (TestSnapshot)
  bool later = false;     // not a tap: call the function in view (TestLater)
  Fl_Window * window = nullptr;
  int x = 0, y = 0;       // where the pen goes down, on the screen
  int step = 0;
};

const int kDragSteps = 4;

// The open text view (a button) with this text, in any window.
Fl_Widget * FindText(Fl_Widget * inWidget, const std::string & inText)
{
  if (auto * text = dynamic_cast<nfl::TextView *>(inWidget))
    return text->Text() == inText ? text : nullptr;
  if (Fl_Group * group = inWidget->as_group())
    for (int i = 0; i < group->children(); ++i)
      if (Fl_Widget * found = FindText(group->child(i), inText))
        return found;
  return nullptr;
}

// Taps one after the other: the next one starts when the pen of the one
// before is up, even if its pen-down's script still runs (a button that
// opens a modal dialog waits in a nested event loop for the next tap).
std::deque<Tap *> gTaps;
bool gPenDown = false;

void StartTap(void *);

void EndTap(void * data)
{
  Tap * tap = static_cast<Tap *>(data);
  if (WindowExists(tap->window) && (tap->dx || tap->dy) && tap->step < kDragSteps) {
    // a drag: one step per turn of the event loop, then up at the end
    tap->step++;
    SendPen(tap->window, FL_DRAG, tap->x + tap->dx * tap->step / kDragSteps,
            tap->y + tap->dy * tap->step / kDragSteps);
    Fl::add_timeout(0.0, EndTap, tap);
    return;
  }
  if (WindowExists(tap->window)) {
    if (tap->outside)
      SendPen(tap->window, FL_DRAG, tap->window->x_root() - 10, tap->window->y_root() - 10);
    if (tap->outside)
      SendPen(tap->window, FL_RELEASE, tap->window->x_root() - 10, tap->window->y_root() - 10);
    else
      SendPen(tap->window, FL_RELEASE, tap->x + tap->dx, tap->y + tap->dy);
  }
  delete tap;
  gPenDown = false;
  Fl::add_timeout(0.0, StartTap);   // runs after the event loop got the pen up
}

// The pen goes down now and up in the next timeout (so a nested event loop
// in the pen-down's script gets it).
void StartTap(void *)
{
  if (gPenDown || gTaps.empty())
    return;
  Tap * tap = gTaps.front();
  gTaps.pop_front();
  if (tap->later) {   // after the taps before it
    RefVar fn(tap->view);
    RefVar args(MakeArray(0));
    delete tap;
    gPenDown = true;   // the next one waits (TestPixel may wait for the screen)
    SendEventCall(fn, args);
    gPenDown = false;
    Fl::add_timeout(0.0, StartTap);
    return;
  }
  if (!tap->snapshot.empty()) {   // after the taps before it
    TakeSnapshot(new Snapshot{RefStruct(tap->view), tap->snapshot});
    delete tap;
    Fl::add_timeout(0.0, StartTap);
    return;
  }
  Fl_Widget * widget = nullptr;
  nfl::Link * link = nullptr;
  if (tap->text.empty()) {
    link = nfl::Link::Of(tap->view);
    widget = link ? link->Widget() : nullptr;
  } else {
    for (Fl_Window * w = Fl::first_window(); w && !widget; w = Fl::next_window(w))
      widget = FindText(w, tap->text);
  }
  tap->window = widget ? (widget->as_window() ? widget->as_window() : widget->window()) : nullptr;
  if (tap->window == nullptr) {
    fprintf(stderr, "newtc: TestTap: nothing to tap\n");
    delete tap;
    Fl::add_timeout(0.0, StartTap);
    return;
  }
  bool isWindow = widget == tap->window;
  if (tap->at && link) {   // relative to the view's bounds (inside its frame)
    tap->x = (isWindow ? 0 : widget->x()) + link->Outset() + tap->fromX;
    tap->y = (isWindow ? 0 : widget->y()) + link->Outset() + tap->fromY;
  } else {
    tap->x = (isWindow ? 0 : widget->x()) + widget->w() / 2;
    tap->y = (isWindow ? 0 : widget->y()) + widget->h() / 2;
  }
  tap->x += tap->window->x_root();   // on the screen
  tap->y += tap->window->y_root();
  gPenDown = true;
  Fl::add_timeout(0.0, EndTap, tap);
  SendPen(tap->window, FL_PUSH, tap->x, tap->y);
}

} // namespace


Ref FTestTap(RefArg rcvr, RefArg inView, RefArg inOutside)
{
  Tap * tap = new Tap{RefStruct(IsString(inView) ? NILREF : (Ref)inView),
                      IsString(inView) ? UTF8FromString(inView) : std::string(), NOTNIL(inOutside)};
  gTaps.push_back(tap);
  Fl::add_timeout(0.0, StartTap);
  return NILREF;
}


Ref FTestPen(RefArg rcvr, RefArg inView, RefArg inX, RefArg inY, RefArg inDX, RefArg inDY)
{
  Tap * tap = new Tap{RefStruct(inView), std::string(), false};
  tap->at = true;
  tap->fromX = int(RINT(inX));
  tap->fromY = int(RINT(inY));
  tap->dx = int(RINT(inDX));
  tap->dy = int(RINT(inDY));
  gTaps.push_back(tap);
  Fl::add_timeout(0.0, StartTap);
  return NILREF;
}


Ref FTestDrag(RefArg rcvr, RefArg inView, RefArg inDX, RefArg inDY)
{
  Tap * tap = new Tap{RefStruct(inView), std::string(), false};
  tap->dx = int(RINT(inDX));
  tap->dy = int(RINT(inDY));
  gTaps.push_back(tap);
  Fl::add_timeout(0.0, StartTap);
  return NILREF;
}

namespace {

} // namespace


Ref FTestSnapshot(RefArg rcvr, RefArg inView, RefArg inPath)
{
  if (!IsString(inPath))
    ThrowBadTypeWithFrameData(kNSErrNotAString, inPath);
  Tap * tap = new Tap{RefStruct(inView), std::string(), false};
  tap->snapshot = UTF8FromString(inPath);
  gTaps.push_back(tap);
  Fl::add_timeout(0.0, StartTap);
  return NILREF;
}

Ref FTestLater(RefArg rcvr, RefArg inFunction)
{
  Tap * tap = new Tap{RefStruct(inFunction), std::string(), false};
  tap->later = true;
  gTaps.push_back(tap);
  Fl::add_timeout(0.0, StartTap);
  return NILREF;
}


Ref FTestPixel(RefArg rcvr, RefArg inView, RefArg inX, RefArg inY)
{
  nfl::Link * link = nfl::Link::Of(inView);
  if (link == nullptr)
    return NILREF;
  Fl_Widget * widget = link->Widget();
  Fl_Window * window = widget->as_window() ? widget->as_window() : widget->window();
  if (window == nullptr || !window->shown())
    return NILREF;
  int x = (widget == window ? 0 : widget->x()) + link->Outset() + int(RINT(inX));
  int y = (widget == window ? 0 : widget->y()) + link->Outset() + int(RINT(inY));
  std::unique_ptr<Fl_RGB_Image> image;
  for (int tries = 0; tries < 40 && !image; ++tries) {   // until the window is on the screen
    if (tries)
      Fl::wait(0.05);
    Fl::flush();
    window->make_current();
    image.reset(fl_capture_window(window, x, y, 1, 1));
  }
  if (!image || image->data_w() < 1)
    return NILREF;
  const uchar * p = (const uchar *)image->data()[0];
  int d = image->d();
  int gray = d >= 3 ? (p[0] + p[1] + p[2]) / 3 : p[0];
  return MAKEINT(gray);
}

#endif // NEWTC_USES_FLTK
