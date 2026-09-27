/*
 File: Timers.h

 Calls later: NewtonScript's delayed and deferred calls and sends.

   AddDelayedCall(function, args, milliseconds)
   AddDelayedSend(receiver, message, args, milliseconds)
   AddDeferredCall(function, args)      after the current script
   AddDeferredSend(receiver, message, args)
   AddDelayedAction, AddDeferredAction  (1.x names for the calls)

 Each runs as an event (Matt/EventLoop.h: one script at a time): if a
 script runs when it is due (a nested event loop: TrackHilite, Drag, a
 modal dialog), it waits for that script, it isn't dropped. A program
 without windows keeps running (RunEventLoop) while calls are waiting; one
 that had windows ends when they are closed (its timers may repeat). With FLTK they are Fl timeouts; without, RunEventLoop sleeps
 until the next is due. (The ROM's procrastinated calls are NewtonScript
 on top of AddDelayedCall.)

 Calls the ROM schedules while newtc starts (before EnableTimers()) are
 dropped: they belong to the built-in apps, which newtc doesn't have (the
 first: setting up the owner from the Names soup, which doesn't exist).
 */

#ifndef HOST_TIMERS_H
#define HOST_TIMERS_H

#include "Frames/Objects.h"

/** From now on, calls are scheduled (newtc's main, after the ROM's
    start). */
void EnableTimers(void);

/** Calls are waiting. */
bool TimersPending(void);

/** Seconds until the next call is due (0 if one is due), or -1 if none
    waits. */
double NextTimerDelay(void);

/** Run the calls that are due (without FLTK; RunEventLoop). */
void RunDueTimers(void);

extern "C" {
Ref FAddDelayedCall(RefArg rcvr, RefArg inFunction, RefArg inArgs, RefArg inDelay);
Ref FAddDelayedAction(RefArg rcvr, RefArg inFunction, RefArg inArgs, RefArg inDelay);
Ref FAddDelayedSend(RefArg rcvr, RefArg inReceiver, RefArg inMessage, RefArg inArgs, RefArg inDelay);
Ref FAddDeferredCall(RefArg rcvr, RefArg inFunction, RefArg inArgs);
Ref FAddDeferredAction(RefArg rcvr, RefArg inFunction, RefArg inArgs);
Ref FAddDeferredSend(RefArg rcvr, RefArg inReceiver, RefArg inMessage, RefArg inArgs);
}

#endif // HOST_TIMERS_H
