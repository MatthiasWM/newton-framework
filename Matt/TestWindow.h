/*
 File: TestWindow.h

 A window for testing the event loop (Matt/EventLoop.h) until newtc has
 Newton views: one button that sends a message to a NewtonScript object.
 Only with FLTK (NEWTC_USES_FLTK).

 NewtonScript functions, registered in newtc.cc:
   TestWindow(title, receiver, message) -> nil
       open the window (replacing an open one); a click on its button sends
       receiver:message() (through SendEventMessage)
   TestWindowClick() -> nil
       click the button from the event loop, as a user would (so tests and
       the Debug Console can make events)
   TestWindowClose() -> nil
       close the window
 */

#ifndef MATT_TESTWINDOW_H
#define MATT_TESTWINDOW_H

#include "Frames/Objects.h"

#if NEWTC_USES_FLTK
extern "C" Ref FTestWindow(RefArg rcvr, RefArg inTitle, RefArg inReceiver, RefArg inMessage);
extern "C" Ref FTestWindowClick(RefArg rcvr);
extern "C" Ref FTestWindowClose(RefArg rcvr);
#endif

#endif // MATT_TESTWINDOW_H
