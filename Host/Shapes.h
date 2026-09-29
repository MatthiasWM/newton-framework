/*
 File: Shapes.h

 Shapes: what NewtonScript draws (DrawShape, DoDrawing; the drawing is in
 Host/FLTK/Drawing.h). A shape is an object the ROM's functions make, in
 the ROM's formats, so that shapes from packages (made by NTK) and shapes
 made here are the same. Numbers in binaries are 16-bit big-endian; a
 Rect is top, left, bottom, right.

   'rectangle, 'oval: a binary, the Rect
   'roundRectangle: the Rect, then the corners' width and height
   'wedge: the Rect, then the start angle and the arc angle
   'line: the Rect as the two points (top, left = y1, x1; bottom, right =
       y2, x2), as the ROM's MakeLine
   'polygon: a frame (canonicalPolygonShape); data: 'polygonData, its size
       in bytes, 0, the bounds (a Rect), then the points (y, x)
   'text: a frame (canonicalTextShape); bounds: a 'boundsRect; data: the
       string ('textData)
   'bitmap: a frame (canonicalBitmapShape); bounds: a 'boundsRect; data:
       a copy of the icon's bits (a Newton bitmap); mask. MakeBitmap's (as
       the ROM's): data is 'pixels, a PixelMap (28 bytes: the offset of
       the rows, rowBytes, 0, the bounds, flags (kPixMapOffset,
       version 2, the depth), the resolution (72, 72), 0), then the rows;
       white, 1 bit deep (other depths: not yet, 1 bit)
   'picture: a frame (canonicalPictureShape); bounds: a 'boundsRect (the
       picture's frame); data: the PICT (Host/Pict.h)
   an array: shapes, and style frames that apply to the shapes after them

 NewtonScript: MakeRect, MakeOval, MakeRoundRect(l, t, r, b, diameter),
 MakeWedge(l, t, r, b, start, arc), MakeLine(x1, y1, x2, y2),
 MakePolygon([x, y, ...]), MakeText(string, l, t, r, b), MakeShape(icon
 or polygon or picture), MakeBitmap(width, height, options), MakePict(shapes, style), OffsetShape(shape, dx,
 dy), ShapeBounds(shape), IsPrimShape(obj), PointsToArray(polygon),
 ArrayToPoints(array).

 MakePict records into a PICT on a Newton; here it gives the shapes with
 their style, [style, shapes], which draws the same.
 */

#ifndef HOST_SHAPES_H
#define HOST_SHAPES_H

#include "Frames/Objects.h"

namespace shapes {

struct Box { long top = 0, left = 0, bottom = 0, right = 0; };

/** A 16-bit big-endian number at inOffset of a binary. */
long GetShort(RefArg inBinary, long inOffset);
void SetShort(RefArg inBinary, long inOffset, long inValue);

/** The Rect at inOffset of a binary. */
Box GetBox(RefArg inBinary, long inOffset = 0);
void SetBox(RefArg inBinary, const Box & inBox, long inOffset = 0);

/** A shape's bounds (as ShapeBounds). Throws for what isn't a shape. */
Box Bounds(RefArg inShape);

bool IsPrimShape(RefArg inObj);
bool IsStyleFrame(RefArg inObj);

/** Where a bitmap's pixels are: rows of rowBytes bytes (the leftmost pixel
    in the high bit), 1 bit deep. */
struct PixelsInfo { long offset = 0; int width = 0, height = 0, rowBytes = 0; };

/** A bitmap's pixels: of a bitmap shape (its data: 'pixels as MakeBitmap
    makes it, or an icon's bits), an icon ({bits, bounds}), or bits alone
    (a binary: 4 bytes, rowBytes, 2 bytes, the bounds, the rows). The
    binary goes to outBinary. False if it is none of these, or not 1 bit
    deep. */
bool GetPixels(RefArg inImage, RefVar & outBinary, PixelsInfo * outInfo);

} // namespace shapes

extern "C" {
Ref FMakeRect(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom);
Ref FMakeOval(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom);
Ref FMakeRoundRect(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom, RefArg inDiameter);
Ref FMakeWedge(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom, RefArg inStart, RefArg inArc);
Ref FMakeLine(RefArg rcvr, RefArg inX1, RefArg inY1, RefArg inX2, RefArg inY2);
Ref FMakePolygon(RefArg rcvr, RefArg inPoints);
Ref FMakeText(RefArg rcvr, RefArg inString, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom);
Ref FMakeShape(RefArg rcvr, RefArg inObject);
Ref FMakeBitmap(RefArg rcvr, RefArg inWidth, RefArg inHeight, RefArg inOptions);
Ref FMakePict(RefArg rcvr, RefArg inShapes, RefArg inStyle);
Ref FOffsetShape(RefArg rcvr, RefArg ioShape, RefArg inDX, RefArg inDY);
Ref FShapeBounds(RefArg rcvr, RefArg inShape);
Ref FIsPrimShape(RefArg rcvr, RefArg inObject);
Ref FPointsToArray(RefArg rcvr, RefArg inPolygon);
Ref FArrayToPoints(RefArg rcvr, RefArg inArray);
}

#endif // HOST_SHAPES_H
