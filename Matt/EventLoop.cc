/*
 File: EventLoop.cc

 The host's event loop (FLTK) and NewtonScript. See EventLoop.h.
 */

#if NEWTC_USES_FLTK
// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#include <FL/platform.H>   // fl_open_display()
#endif

#include "Matt/EventLoop.h"
#include "Matt/DAP.h"
#include "Host/Timers.h"

#include <chrono>
#include <thread>

#include "Frames/Frames.h"
#include "Frames/Funcs.h"
#include "Frames/Interpreter.h"
#include "Frames/NewtGlobals.h"
#include "REPTranslators.h"

namespace {

// The interpreter's control stack depth while the event loop waits: an
// event is only delivered when no NewtonScript runs above it. -1: the event
// loop isn't running.
long gEventLoopDepth = -1;

// Modal event loops running (RunModalEventLoop): their events' scripts run
// inside the scripts that wait for them.
int gModalLoops = 0;

long InterpreterDepth(void)
{
  return STACKINDEX(gInterpreter->ctrlStack);
}

} // namespace


bool EventsDelivered(void)
{
  return gEventLoopDepth >= 0 && InterpreterDepth() == gEventLoopDepth;
}


Ref SendEventMessage(RefArg inReceiver, RefArg inMessage, RefArg inArgs)
{
  if (!EventsDelivered())
    return NILREF;    // a script runs (or is stopped): one at a time
  RefVar result;
  DAPEnterScript();
  newton_try
  {
    bool defined;
    result = DoMessageIfDefined(inReceiver, inMessage, inArgs, &defined);
  }
  newton_catch_all
  {
    gREPout->exceptionNotify(CurrentException());
    gREPout->flush();
  }
  end_try;
  DAPLeaveScript(gModalLoops == 0);
  return result;
}


Ref SendEventCall(RefArg inFunction, RefArg inArgs)
{
  if (!EventsDelivered())
    return NILREF;    // a script runs (or is stopped): one at a time
  RefVar result;
  DAPEnterScript();
  newton_try
  {
    result = DoBlock(inFunction, inArgs);
  }
  newton_catch_all
  {
    gREPout->exceptionNotify(CurrentException());
    gREPout->flush();
  }
  end_try;
  DAPLeaveScript(gModalLoops == 0);
  return result;
}


#if NEWTC_USES_FLTK

namespace {

// The DAP requests are an event source while an event loop waits (the
// outermost one adds it).
bool gDAPEventSource = false;

bool AddDAPEventSource(void)
{
  int dapFd = DAPInputFd();
  if (gDAPEventSource || dapFd < 0)
    return false;
  Fl::add_fd(dapFd, FL_READ, [](FL_SOCKET, void *) { DAPHandleIdleRequests(); });
  gDAPEventSource = true;
  DAPHandleIdleRequests();    // requests that came while the program ran
  return true;
}

void RemoveDAPEventSource(void)
{
  Fl::remove_fd(DAPInputFd());
  gDAPEventSource = false;
}

} // namespace


void RunEventLoop(void)
{
  if (Fl::first_window() == nullptr && !TimersPending())
    return;
  fl_open_display();   // timeouts need it (macOS: the run loop), also without a window
  gEventLoopDepth = InterpreterDepth();
  bool added = AddDAPEventSource();
  // while a window is open, or calls are waiting (Host/Timers.h)
  // (Fl::wait() returns at once without a window; Fl::wait(time) waits for
  // the timeouts too. It runs the due ones first, then waits for the next:
  // so not longer than until the next call is due, to look again after it.)
  // Waiting calls keep a program without windows running; once it had
  // windows, it ends when they are closed (an app's timers may repeat).
  bool hadWindow = false;
  while (!DAPClientGone()) {
    if (Fl::first_window() != nullptr)
      hadWindow = true;
    else if (hadWindow || !TimersPending())
      break;
    Fl::wait(TimersPending() ? NextTimerDelay() : 1e20);
  }
  if (added)
    RemoveDAPEventSource();
  gEventLoopDepth = -1;
}


void RunModalEventLoop(bool (*inDone)(void *), void * inData)
{
  long outerDepth = gEventLoopDepth;
  gEventLoopDepth = InterpreterDepth();
  ++gModalLoops;
  bool added = AddDAPEventSource();
  while (!inDone(inData) && Fl::first_window() != nullptr && !DAPClientGone())
    Fl::wait();
  if (added)
    RemoveDAPEventSource();
  --gModalLoops;
  gEventLoopDepth = outerDepth;
}

#else

// No windows without FLTK: only the waiting calls (Host/Timers.h), each
// when it is due.
void RunEventLoop(void)
{
  gEventLoopDepth = InterpreterDepth();
  for (double delay; (delay = NextTimerDelay()) >= 0; ) {
    if (delay > 0)
      std::this_thread::sleep_for(std::chrono::duration<double>(delay));
    RunDueTimers();
  }
  gEventLoopDepth = -1;
}

void RunModalEventLoop(bool (*inDone)(void *), void * inData)
{
}

#endif
