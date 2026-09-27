/*
 File: ViewMethods.cc

 The view methods that are natives. See ViewMethods.h.
 */

#include "Host/ViewMethods.h"
#include "Utilities/Unimplemented.h"
#include "Frames/Frames.h"
#include "Frames/Lookup.h"
#include "Frames/Globals.h"
#include "ROMResources.h"
#if NEWTC_USES_FLTK
#include "Host/FLTK/Links.h"
#endif

extern "C" {

// As the ROM's: the _parent slot, open or not.
Ref FParentX(RefArg rcvr)
{
  return GetProtoVariable(rcvr, SYMA(_parent));
}

// The slot is set with or without FLTK; an open view shows it.
Ref FSetValue(RefArg rcvr, RefArg inView, RefArg inTag, RefArg inValue)
{
  SetFrameSlot(inView, inTag, inValue);
#if NEWTC_USES_FLTK
  nfl::ValueChanged(inView, inTag);
#endif
  return NILREF;
}

#if NEWTC_USES_FLTK

// The ROM sends close, hide and show as commands that run when the events
// are handled; newtc does them at once.
Ref FOpenX(RefArg rcvr)            { return nfl::OpenView(rcvr); }
Ref FCloseX(RefArg rcvr)           { return nfl::CloseView(rcvr); }
Ref FGlobalBoxX(RefArg rcvr)       { return nfl::GlobalBox(rcvr); }
Ref FLocalBoxX(RefArg rcvr)        { return nfl::LocalBox(rcvr); }
Ref FChildViewFramesX(RefArg rcvr) { return nfl::ChildViewFrames(rcvr); }
Ref FHideX(RefArg rcvr)            { return nfl::HideView(rcvr); }
Ref FShowX(RefArg rcvr)            { return nfl::ShowView(rcvr); }
Ref FDirtyX(RefArg rcvr)           { return nfl::DirtyView(rcvr); }
Ref FDragX(RefArg rcvr, RefArg inUnit, RefArg inBounds) { return nfl::DragView(rcvr, inUnit, inBounds); }
Ref FGetFlags(RefArg rcvr, RefArg inView) { return nfl::ViewFlags(inView); }
Ref FHiliteX(RefArg rcvr, RefArg inOn)        { return nfl::HiliteView(rcvr, inOn); }
Ref FTrackHiliteX(RefArg rcvr, RefArg inUnit) { return nfl::TrackHilite(rcvr, inUnit); }
Ref FModalDialog(RefArg rcvr)                 { return nfl::ModalDialog(rcvr); }
Ref FSetupIdleX(RefArg rcvr, RefArg inMilliseconds) { return nfl::SetupIdle(rcvr, inMilliseconds); }

#else

// No views without FLTK (NEWTC_USES_FLTK): the stubs say so.
NS_STUB(FOpenX, RefArg rcvr)
NS_STUB(FCloseX, RefArg rcvr)
NS_STUB(FGlobalBoxX, RefArg rcvr)
NS_STUB(FLocalBoxX, RefArg rcvr)
NS_STUB(FChildViewFramesX, RefArg rcvr)
NS_STUB(FHideX, RefArg rcvr)
NS_STUB(FShowX, RefArg rcvr)
NS_STUB(FDirtyX, RefArg rcvr)
NS_STUB(FDragX, RefArg rcvr, RefArg inUnit, RefArg inBounds)
// no views open without FLTK
Ref FGetFlags(RefArg rcvr, RefArg inView) { return MAKEINT(0); }
NS_STUB(FHiliteX, RefArg rcvr, RefArg inOn)
NS_STUB(FTrackHiliteX, RefArg rcvr, RefArg inUnit)
NS_STUB(FModalDialog, RefArg rcvr)
NS_STUB(FSetupIdleX, RefArg rcvr, RefArg inMilliseconds)

#endif

}
