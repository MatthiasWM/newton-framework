/*
 File: EventLoop.cc

 The host's event loop (FLTK) and NewtonScript. See EventLoop.h.
 */

#if NEWTC_USES_FLTK
// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>
#endif

#include "Matt/EventLoop.h"
#include "Matt/DAP.h"

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

long InterpreterDepth(void)
{
  return STACKINDEX(gInterpreter->ctrlStack);
}

} // namespace


Ref SendEventMessage(RefArg inReceiver, RefArg inMessage, RefArg inArgs)
{
  if (gEventLoopDepth < 0 || InterpreterDepth() != gEventLoopDepth)
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
  DAPLeaveScript();
  return result;
}


#if NEWTC_USES_FLTK

void RunEventLoop(void)
{
  if (Fl::first_window() == nullptr)
    return;
  gEventLoopDepth = InterpreterDepth();
  int dapFd = DAPInputFd();
  if (dapFd >= 0) {
    Fl::add_fd(dapFd, FL_READ, [](FL_SOCKET, void *) { DAPHandleIdleRequests(); });
    DAPHandleIdleRequests();    // requests that came while the program ran
  }
  while (Fl::first_window() != nullptr && !DAPClientGone())
    Fl::wait();
  if (dapFd >= 0)
    Fl::remove_fd(dapFd);
  gEventLoopDepth = -1;
}

#else

void RunEventLoop(void)
{
}

#endif
