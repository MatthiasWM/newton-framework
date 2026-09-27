/*
 File: ViewMethods.h

 The view methods that are natives (in the ROM's root view _proto, @287):
 NewtonScript calls them on a view frame, view:GlobalBox() etc. With FLTK
 (NEWTC_USES_FLTK) they act on the view's link (Host/FLTK/Links.h); without
 FLTK there are no views, and they are stubs: they say so and return nil.

   view:Open(), view:Close() -> the ROM's Open calls _Open (FOpenX); Close
       is FCloseX. Open the view (and its children), close it.
   view:GlobalBox(), view:LocalBox() -> the view's bounds on the display,
       or with its top left at 0, 0 ({left, top, right, bottom}).
   view:ChildViewFrames() -> the view frames of its open children.
   view:Hide(), view:Show() -> hide or show the view (it stays open).
   view:Dirty() -> draw the view again.

 Except Close and Dirty, they throw "nil view" for a view that isn't open
 (the ROM's FailGetView).
 */

#ifndef HOST_VIEWMETHODS_H
#define HOST_VIEWMETHODS_H

#include "Frames/Objects.h"

extern "C" {
Ref FOpenX(RefArg rcvr);
Ref FCloseX(RefArg rcvr);
Ref FGlobalBoxX(RefArg rcvr);
Ref FLocalBoxX(RefArg rcvr);
Ref FChildViewFramesX(RefArg rcvr);
Ref FHideX(RefArg rcvr);
Ref FShowX(RefArg rcvr);
Ref FDirtyX(RefArg rcvr);
}

#endif // HOST_VIEWMETHODS_H
