/*
 File: Root.cc

 The root view. See Root.h.
 */

#include "Host/Root.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"
#include "Frames/Funcs.h"
#include "Frames/Globals.h"

#include <string>

namespace {

Ref gRoot = NILREF;   // a GC root (AddGCRoot), made on first use

// A NewtonScript function object for a C function (see newtc.cc).
Ref MakeCFunction(void * inFunction, int inNumArgs)
{
  RefVar fn(AllocateFrame());
  SetFrameSlot(fn, MakeSymbol("class"), kPlainCFunctionClass);
  SetFrameSlot(fn, MakeSymbol("function"), (Ref)inFunction);
  SetFrameSlot(fn, MakeSymbol("numargs"), MAKEINT(inNumArgs));
  return fn;
}

std::string Text(RefArg inObj)
{
  return IsString(inObj) ? UTF8FromString(inObj) : std::string();
}

// root:Notify(level, title, message): NewtonOS shows an alert (level: how
// important, e.g. 3 kNotifyAlert). newtc prints it.
Ref Notify(RefArg rcvr, RefArg inLevel, RefArg inTitle, RefArg inMessage)
{
  std::string title = Text(inTitle);
  std::string message = Text(inMessage);
  REPprintf("%s%s%s\n", title.c_str(), title.empty() ? "" : ": ", message.c_str());
  return NILREF;
}

} // namespace


Ref RootView(void)
{
  if (ISNIL(gRoot)) {
    AddGCRoot(&gRoot);
    RefVar root(AllocateFrame());
    SetFrameSlot(root, MakeSymbol("Notify"), MakeCFunction((void *)Notify, 3));
    gRoot = root;
  }
  return gRoot;
}


Ref FGetRoot(RefArg rcvr)
{
  return RootView();
}
