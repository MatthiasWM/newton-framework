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
   view:Parent() -> the view's parent view frame (_parent; also without
       FLTK).
   GetViewFlags(view) (FGetFlags) -> the open view's viewFlags as they are
       now (vVisible off while hidden: Visible(view) uses it); 0 if it
       isn't open.
   view:Dirty() -> draw the view again.
   view:Drag(unit, bounds) -> the view follows the pen until it comes up.
   view:DrawShape(shape, style), view:DoDrawing(method, args) -> draw on
       the view (Host/FLTK/Drawing.h).
   DoPopup(items, bounds or left, top, context) -> a popup menu
       (Host/FLTK/Popup.h); nil.
   GetView(view) -> the view frame if it is open, else nil.
   SetValue(view, slot, value) -> nil: sets the slot, and an open view
       shows it (text, viewBounds, else it is drawn again).
   view:Hilite(on) -> draw the view hilited or not.
   view:TrackHilite(unit) -> in a viewClickScript: hilite the view while
       the pen is down inside it; true if it came up inside.
   view:ModalDialog() -> open the view as a modal window; returns when it
       is closed (events run their scripts meanwhile).
   view:SetupIdle(milliseconds) -> viewIdleScript() after that long, and
       again after the milliseconds it returns, until it returns nil.

 Except Close, Dirty and Hilite (which do nothing), they throw "nil view"
 for a view that isn't open (the ROM's FailGetView).
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
Ref FDragX(RefArg rcvr, RefArg inUnit, RefArg inBounds);
Ref FDrawShape(RefArg rcvr, RefArg inShape, RefArg inStyle);
Ref FDoPopup(RefArg rcvr, RefArg inItems, RefArg inWhere, RefArg inTop, RefArg inContext);
Ref FGetView(RefArg rcvr, RefArg inView);
Ref FDoDrawing(RefArg rcvr, RefArg inMethod, RefArg inArgs);
/** RefreshViews(): draw the views that need it now (not when the script
    is done). */
Ref FRefreshViews(RefArg rcvr);
Ref FDrawXBitmap(RefArg rcvr, RefArg inBounds, RefArg inBitmap, RefArg inIndex, RefArg inMode);
Ref FParentX(RefArg rcvr);
Ref FSetValue(RefArg rcvr, RefArg inView, RefArg inTag, RefArg inValue);
Ref FGetFlags(RefArg rcvr, RefArg inView);
Ref FHiliteX(RefArg rcvr, RefArg inOn);
Ref FTrackHiliteX(RefArg rcvr, RefArg inUnit);
Ref FModalDialog(RefArg rcvr);
Ref FSetupIdleX(RefArg rcvr, RefArg inMilliseconds);
}

#endif // HOST_VIEWMETHODS_H
