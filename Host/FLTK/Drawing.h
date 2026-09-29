/*
 File: Drawing.h

 Drawing shapes (Host/Shapes.h) on views: DrawShape(shape, style),
 DoDrawing(method, args), viewDrawScript; and into bitmaps (MakeBitmap):
 DrawIntoBitmap, ViewIntoBitmap.

 On a Newton, a script draws straight onto the screen: the pixels stay
 until the view system draws that part again. Here each view can have a
 canvas: an offscreen image (Fl_Image_Surface, at the screen's
 resolution) that gets what the scripts draw, and a mask of the pixels
 they drew. The widget draws itself and its children as always, then the
 canvas on top, with the mask as its transparency (as on a Newton, what a
 script draws covers the view's children). A
 change on the Newton side (Dirty, SetValue, Hide, Show, a child moving or
 opening or closing, a hilite) drops the canvases of the view and the
 views it is in (Link::DropCanvas), as the view system would draw over
 the pixels.

 A viewDrawScript runs when the widget draws (after its own drawing, before
 its children); what it draws goes directly onto the widget. It runs also
 while another script runs (a redraw in a nested event loop), as NewtonOS
 draws views there too.

 Style: penSize, penPattern, fillPattern (vfWhite ... vfBlack, or an 8 by
 8 pattern, drawn as its gray), transferMode (copy, or, xor; the others:
 not yet, drawn as copy), transform ([dx, dy]; scaling: not yet), font,
 justification.

 XOR inverts what is under a shape (a Newton's XOR with black): the shape
 in white in Quartz's difference blend mode (|white - D| = 1 - D; macOS
 only: FLTK has no blend mode yet, other platforms stop at an #error in
 BlendInvert()). On the canvas, XOR shapes go to a layer of their own (the
 invert layer: drawn twice, a shape is gone), drawn last in that mode. A style frame in a list of shapes applies to the
 shapes after it in the list.
 */

#ifndef HOST_FLTK_DRAWING_H
#define HOST_FLTK_DRAWING_H

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl_Widget.H>
#include <FL/Fl_Image.H>

#include "Frames/Objects.h"

#include <functional>

namespace nfl {

class Link;

/** view:DrawShape(shape, style) */
Ref DrawShape(RefArg inContext, RefArg inShape, RefArg inStyle);

/** view:DrawXBitmap(bounds, bitmap, index, mode): the index-th cell of a
    strip of bounds-sized cells (a clock's hands, digits, ...) at bounds.
    mode: as a style's transferMode (nil: copy). */
Ref DrawXBitmap(RefArg inContext, RefArg inBounds, RefArg inBitmap, RefArg inIndex, RefArg inMode);

/** An image (a Newton bitmap, a picture) at x, y, its pixels blocks of
    the screen's (not smoothed when scaled). */
void DrawPixels(Fl_Image * inImage, int x, int y);

/** What follows inverts what is under it, drawn in white (XOR with black):
    the difference blend mode (macOS; see Drawing.cc). */
void BlendInvert(bool inOn);

/** Invert what inDraw draws in x, y, w, h (called with the area's top left,
    in black): for text, which the blend mode doesn't reach (CoreText). */
void DrawInverted(int x, int y, int w, int h, const std::function<void(int, int)> & inDraw);

/** DrawIntoBitmap(shape, style, bitmap): the shape drawn into a bitmap
    (MakeBitmap's; an icon), as on a view; 1 bit deep, grays as the
    Newton's patterns. */
Ref DrawIntoBitmap(RefArg inShape, RefArg inStyle, RefArg inBitmap);

/** view:ViewIntoBitmap(srcRect, destRect, bitmap): the view as it is on
    the screen (with its children and what scripts drew), its srcRect
    (nil, as the ROM's: its outer bounds, frame and shadow too) into the bitmap's destRect (nil, as the ROM's: the
    source's size, at the bitmap's top left), scaled if they differ. Nothing if the view isn't open. */
Ref ViewIntoBitmap(RefArg inView, RefArg inSource, RefArg inDest, RefArg inBitmap);

/** view:DoDrawing(method, args): view:method(args...), drawing on the view. */
Ref DoDrawing(RefArg inContext, RefArg inMethod, RefArg inArgs);

/** For a view widget's draw(), at the end (after its children): what
    scripts drew on the view, if anything. */
void DrawOverlay(Fl_Widget * inWidget);

/** For a view widget's draw(), after its own drawing: the view's
    viewDrawScript, if it has one. */
void RunDrawScript(Fl_Widget * inWidget);

} // namespace nfl

#endif // HOST_FLTK_DRAWING_H
