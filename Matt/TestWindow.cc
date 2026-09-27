/*
 File: TestWindow.cc

 A window for testing the event loop. See TestWindow.h.
 */

#if NEWTC_USES_FLTK

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Window.H>

#include "Matt/TestWindow.h"
#include "Matt/EventLoop.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"

#include <string>

namespace {

Fl_Window * gWindow = nullptr;
Fl_Button * gButton = nullptr;

// What a click sends. GC roots: the garbage collector updates them when it
// moves the objects (a Ref in a widget's user_data would go stale).
Ref gReceiver = NILREF;
Ref gMessage = NILREF;

void Clicked(Fl_Widget *, void *)
{
  RefVar args(MakeArray(0));
  SendEventMessage(gReceiver, gMessage, args);
}

void CloseWindow(void)
{
  if (gWindow == nullptr)
    return;
  gWindow->hide();
  Fl::delete_widget(gWindow);   // may be inside its own callback
  gWindow = nullptr;
  gButton = nullptr;
}

} // namespace


Ref FTestWindow(RefArg rcvr, RefArg inTitle, RefArg inReceiver, RefArg inMessage)
{
  static bool rooted = false;
  if (!rooted) {
    AddGCRoot(&gReceiver);
    AddGCRoot(&gMessage);
    rooted = true;
  }
  if (!IsSymbol(inMessage))
    ThrowBadTypeWithFrameData(kNSErrNotASymbol, inMessage);
  CloseWindow();
  gReceiver = inReceiver;
  gMessage = inMessage;
  static std::string title;     // the window keeps the pointer
  title = IsString(inTitle) ? UTF8FromString(inTitle) : std::string("newtc");
  gWindow = new Fl_Window(240, 100, title.c_str());
  gButton = new Fl_Button(20, 30, 200, 40, "Click");
  gButton->callback(Clicked);
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

#endif // NEWTC_USES_FLTK
