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
   TestCloseWindow(view) -> nil
       close the view's desktop window from the event loop, as a user would
       (its close button): the view gets Close() (Host/FLTK/Links.h). The
       view needs to be open when the event loop gets to it.
   TestTap(view, outside) -> nil
       tap the view with the pen (the mouse) from the event loop, as a user
       would: down at its center and up again; with outside non-nil, the
       pen moves out of the view before it comes up. view can be a string:
       the text of the button to tap (e.g. "OK" in an alert). Taps run one
       after the other. The events go through
       FLTK (Fl::handle), to the view's widget (Host/FLTK/Links.h, "Pen").
   TestDrag(view, dx, dy) -> nil
       as TestTap, but the pen moves by dx, dy (in steps, each in a turn of
       the event loop) before it comes up.
   TestPen(view, x, y, dx, dy) -> nil
       as TestDrag, but the pen goes down at x, y from the view's top left
       (its bounds; negative: in its frame, e.g. a floater's dragger).
   TestSnapshot(view, path) -> nil
       from the event loop, after the taps before it: save the view's
       window as a PNG file at path,
       as drawn (in the screen's resolution: twice the size on Retina).
       Prints to stderr if it can't.
 */

#ifndef MATT_TESTWINDOW_H
#define MATT_TESTWINDOW_H

#include "Frames/Objects.h"

#if NEWTC_USES_FLTK
extern "C" Ref FTestWindow(RefArg rcvr, RefArg inTitle, RefArg inReceiver, RefArg inMessage);
extern "C" Ref FTestWindowClick(RefArg rcvr);
extern "C" Ref FTestWindowClose(RefArg rcvr);
extern "C" Ref FTestCloseWindow(RefArg rcvr, RefArg inView);
extern "C" Ref FTestTap(RefArg rcvr, RefArg inView, RefArg inOutside);
extern "C" Ref FTestDrag(RefArg rcvr, RefArg inView, RefArg inDX, RefArg inDY);
extern "C" Ref FTestPen(RefArg rcvr, RefArg inView, RefArg inX, RefArg inY, RefArg inDX, RefArg inDY);
extern "C" Ref FTestSnapshot(RefArg rcvr, RefArg inView, RefArg inPath);
#endif

#endif // MATT_TESTWINDOW_H
