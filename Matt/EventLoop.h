/*
 File: EventLoop.h

 The host's event loop (FLTK) and NewtonScript.

 newtc is single threaded. A Newton program runs its scripts, then waits for
 events (taps, timers), and each event runs a script again. newtc does the
 same: after the program (a -script, a package's InstallScript) returns,
 RunEventLoop() waits for host events while the program has a window open;
 each event that concerns NewtonScript goes through SendEventMessage().

 Scripts never overlap: host events are only delivered while no
 NewtonScript runs. While a script runs, or while it is stopped in a break
 loop, nobody calls Fl::wait(), so no events come; an event that still
 arrives then (from a nested event loop, e.g. an FLTK dialog opened by a
 native function) is dropped. The exception is a modal Newton dialog
 (ModalDialog()): it runs a nested event loop (RunModalEventLoop()) whose
 events run scripts inside the one that opened it, as in NewtonOS.

 With -dap, the DAP requests are one more event source (Fl::add_fd): while
 the program waits for events, VS Code can still set breakpoints, evaluate,
 or pause (the pause then stops at the first instruction of the next
 event's script).

 Without FLTK (NEWTC_USES_FLTK off), there are no windows and
 RunEventLoop() returns at once.
 */

#ifndef MATT_EVENTLOOP_H
#define MATT_EVENTLOOP_H

#include "Frames/Objects.h"

/** Wait for host events while the program has a window open; returns when
    the last one is closed, or the DAP client is gone. */
void RunEventLoop(void);

/** Send a message to a NewtonScript object for a host event (e.g. a button
    click): the one way host callbacks run NewtonScript. Only while no
    NewtonScript runs (see above); an exception is reported like one in the
    program and doesn't leave the callback. Returns the result, or nil if
    the event was dropped, the method isn't defined, or it threw. */
Ref SendEventMessage(RefArg inReceiver, RefArg inMessage, RefArg inArgs);

/** The same for a function: inFunction(inArgs...). */
Ref SendEventCall(RefArg inFunction, RefArg inArgs);

/** SendEventMessage() would deliver an event now (no script runs above
    the event loop). */
bool EventsDelivered(void);

/** From a native function called by a script (ModalDialog()): wait for
    host events until inDone(inData) says so (or no window is left, or the
    DAP client is gone). Meanwhile events run their scripts, on top of the
    one waiting here; then it goes on. */
void RunModalEventLoop(bool (*inDone)(void *), void * inData);

#endif // MATT_EVENTLOOP_H
