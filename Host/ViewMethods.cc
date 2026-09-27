/*
 File: ViewMethods.cc

 The view methods that are natives. See ViewMethods.h.
 */

#include "Host/ViewMethods.h"
#include "Utilities/Unimplemented.h"
#if NEWTC_USES_FLTK
#include "Host/FLTK/Links.h"
#endif

extern "C" {

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
Ref FHiliteX(RefArg rcvr, RefArg inOn)        { return nfl::HiliteView(rcvr, inOn); }
Ref FTrackHiliteX(RefArg rcvr, RefArg inUnit) { return nfl::TrackHilite(rcvr, inUnit); }

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
NS_STUB(FHiliteX, RefArg rcvr, RefArg inOn)
NS_STUB(FTrackHiliteX, RefArg rcvr, RefArg inUnit)

#endif

}
