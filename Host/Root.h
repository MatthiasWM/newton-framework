/*
 File: Root.h

 The root view: the frame GetRoot() returns, the parent of every app's base
 view (installing a package keeps the app's base view in it, as
 root.(appSymbol); see Matt/CLAUDE.md, "How a Newton installs and opens a
 package").

 NewtonOS's root view has much more (the Extras drawer, the notification
 system, memory kept aside so it can still tell the user when memory runs
 out, ...). newtc's grows only with what programs need: so far a writable
 frame with Notify(). It has no window of its own: window-like views get one
 FLTK window each (Host/FLTK/).

 Not FLTK-specific, so newtc without FLTK has a root view, too (-dap installs
 packages into it).

 NewtonScript:
   GetRoot() -> the root view frame (the same one every time)
   GetRoot():Notify(level, title, message) -> nil
       tell the user something (the ROM uses it for errors while installing,
       ...): newtc prints "title: message" (the Debug Console with -dap).
 */

#ifndef HOST_ROOT_H
#define HOST_ROOT_H

#include "Frames/Objects.h"

/** The root view's frame (made on first use; a GC root). */
Ref RootView(void);

extern "C" Ref FGetRoot(RefArg rcvr);

#endif // HOST_ROOT_H
