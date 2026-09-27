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
void SendPen(Fl_Window * inWindow, int inEvent, int x, int y)
{
  Fl::e_x = x;
  Fl::e_y = y;
  Fl::e_x_root = inWindow->x_root() + x;
  Fl::e_y_root = inWindow->y_root() + y;
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

struct Tap
{
  RefStruct view;         // the view to tap, or
  std::string text;       // the text of the button to tap
  bool outside;
  Fl_Window * window = nullptr;
  int x = 0, y = 0;
};

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
  if (WindowExists(tap->window)) {
    if (tap->outside)
      SendPen(tap->window, FL_DRAG, -10, -10);
    SendPen(tap->window, FL_RELEASE, tap->outside ? -10 : tap->x, tap->outside ? -10 : tap->y);
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
  Fl_Widget * widget = nullptr;
  if (tap->text.empty()) {
    nfl::Link * link = nfl::Link::Of(tap->view);
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
  tap->x = (isWindow ? 0 : widget->x()) + widget->w() / 2;
  tap->y = (isWindow ? 0 : widget->y()) + widget->h() / 2;
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

namespace {

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

} // namespace


Ref FTestSnapshot(RefArg rcvr, RefArg inView, RefArg inPath)
{
  if (!IsString(inPath))
    ThrowBadTypeWithFrameData(kNSErrNotAString, inPath);
  Fl::add_timeout(0.0, TakeSnapshot, new Snapshot{RefStruct(inView), UTF8FromString(inPath)});
  return NILREF;
}

#endif // NEWTC_USES_FLTK
