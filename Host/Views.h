/*
 File: Views.h

 Newton views, the part that doesn't depend on FLTK: view frames
 ("contexts"). A view frame is what NewtonScript sees of a view: its _proto
 is the template (or a stationery's form), its _parent the parent view's
 frame, and its viewCObject slot refers to the native view while the view
 is open (nil before; see Matt/CLAUDE.md, "Design"). Opening, drawing, and
 events come with the link classes (Host/FLTK/).

 NewtonScript:
   BuildContext(template) -> a view frame for the template, a child of the
       root view, not open (no native view yet). Installing a form part
       keeps the app's base view made this way: GetRoot().(appSymbol).
   view:Open(), view:Close() -> the ROM's view methods (in the root view's
       _proto) call the natives FOpenX (_Open) and FCloseX (Close), which
       open and close the view with FLTK (Host/FLTK/Links.h). Without FLTK
       they are stubs: they say so and return nil.
 */

#ifndef HOST_VIEWS_H
#define HOST_VIEWS_H

#include "Frames/Objects.h"

/** A view frame for inTemplate, with the root view as its parent; nil for
    a template that isn't visible (vVisible off) unless inEvenIfHidden.
    As CView::buildContext (Views/View.cc) and the ROM. Throws for a
    template without a view class. */
Ref BuildViewContext(RefArg inTemplate, bool inEvenIfHidden);

extern "C" Ref FBuildContext(RefArg rcvr, RefArg inTemplate);
extern "C" Ref FOpenX(RefArg rcvr);
extern "C" Ref FCloseX(RefArg rcvr);

#endif // HOST_VIEWS_H
