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
  Fl::delete_widget(gWindow);   // later: this may run inside the button's callback
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

#endif // NEWTC_USES_FLTK
