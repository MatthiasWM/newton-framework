/*
 File: Timers.cc

 Calls later. See Timers.h.
 */

#if NEWTC_USES_FLTK
// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#endif

#include "Host/Timers.h"
#include "Matt/EventLoop.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"

#include <chrono>
#include <list>

namespace {

struct Action
{
  RefStruct target;    // the function, or the receiver
  RefStruct message;   // nil for a call
  RefStruct args;
  double due;          // seconds (Now())
};

std::list<Action *> gActions;   // waiting, in the order they were added
bool gEnabled = false;          // EnableTimers()

double Now(void)
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void Run(Action * inAction);

#if NEWTC_USES_FLTK
void Timeout(void * inAction)
{
  Run(static_cast<Action *>(inAction));
}
#endif

void Run(Action * inAction)
{
  if (!EventsDelivered()) {   // a script runs: after it
#if NEWTC_USES_FLTK
    Fl::add_timeout(0.02, Timeout, inAction);
#endif
    return;
  }
  gActions.remove(inAction);
  RefVar target(inAction->target), message(inAction->message), args(inAction->args);
  delete inAction;
  if (ISNIL(message))
    SendEventCall(target, args);
  else
    SendEventMessage(target, message, args);
}

Ref Add(RefArg inTarget, RefArg inMessage, RefArg inArgs, long inMilliseconds)
{
  if (!gEnabled)
    return NILREF;   // the ROM's own start (see Timers.h)
  double delay = inMilliseconds > 0 ? inMilliseconds / 1000.0 : 0.0;
  Action * action = new Action{RefStruct(inTarget), RefStruct(inMessage),
                               RefStruct(IsArray(inArgs) ? (Ref)inArgs : MakeArray(0)), Now() + delay};
  gActions.push_back(action);
#if NEWTC_USES_FLTK
  Fl::add_timeout(delay, Timeout, action);
#endif
  return NILREF;
}

long Milliseconds(RefArg inDelay)
{
  return ISINT(inDelay) ? RINT(inDelay) : 0;
}

} // namespace


void EnableTimers(void)
{
  gEnabled = true;
}


bool TimersPending(void)
{
  return !gActions.empty();
}


double NextTimerDelay(void)
{
  if (gActions.empty())
    return -1;
  double next = gActions.front()->due;
  for (Action * action : gActions)
    next = std::min(next, action->due);
  return std::max(0.0, next - Now());
}


void RunDueTimers(void)
{
  double now = Now();
  std::list<Action *> due;
  for (Action * action : gActions)
    if (action->due <= now)
      due.push_back(action);
  for (Action * action : due)
    Run(action);
}


extern "C" {

Ref FAddDelayedCall(RefArg rcvr, RefArg inFunction, RefArg inArgs, RefArg inDelay)
{
  return Add(inFunction, RA(NILREF), inArgs, Milliseconds(inDelay));
}

Ref FAddDelayedAction(RefArg rcvr, RefArg inFunction, RefArg inArgs, RefArg inDelay)
{
  return Add(inFunction, RA(NILREF), inArgs, Milliseconds(inDelay));
}

Ref FAddDelayedSend(RefArg rcvr, RefArg inReceiver, RefArg inMessage, RefArg inArgs, RefArg inDelay)
{
  return Add(inReceiver, inMessage, inArgs, Milliseconds(inDelay));
}

Ref FAddDeferredCall(RefArg rcvr, RefArg inFunction, RefArg inArgs)
{
  return Add(inFunction, RA(NILREF), inArgs, 0);
}

Ref FAddDeferredAction(RefArg rcvr, RefArg inFunction, RefArg inArgs)
{
  return Add(inFunction, RA(NILREF), inArgs, 0);
}

Ref FAddDeferredSend(RefArg rcvr, RefArg inReceiver, RefArg inMessage, RefArg inArgs)
{
  return Add(inReceiver, inMessage, inArgs, 0);
}

}
