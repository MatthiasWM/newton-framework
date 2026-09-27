/*
 File: Shapes.cc

 Shapes: making and measuring them. See Shapes.h.
 */

#include "Host/Shapes.h"

#include "Frames/Frames.h"
#include "Frames/Lookup.h"
#include "Frames/Globals.h"
#include "Frames/Iterators.h"
#include "ROMResources.h"
#include "Host/Pict.h"

#include <algorithm>
#include <cstring>

namespace shapes {

long GetShort(RefArg inBinary, long inOffset)
{
  const unsigned char * data = (const unsigned char *)BinaryData(inBinary);
  return short((data[inOffset] << 8) | data[inOffset + 1]);
}


void SetShort(RefArg inBinary, long inOffset, long inValue)
{
  unsigned char * data = (unsigned char *)BinaryData(inBinary);
  data[inOffset] = (unsigned char)(inValue >> 8);
  data[inOffset + 1] = (unsigned char)inValue;
}


Box GetBox(RefArg inBinary, long inOffset)
{
  Box box;
  box.top = GetShort(inBinary, inOffset);
  box.left = GetShort(inBinary, inOffset + 2);
  box.bottom = GetShort(inBinary, inOffset + 4);
  box.right = GetShort(inBinary, inOffset + 6);
  return box;
}


void SetBox(RefArg inBinary, const Box & inBox, long inOffset)
{
  SetShort(inBinary, inOffset, inBox.top);
  SetShort(inBinary, inOffset + 2, inBox.left);
  SetShort(inBinary, inOffset + 4, inBox.bottom);
  SetShort(inBinary, inOffset + 6, inBox.right);
}


bool IsPrimShape(RefArg inObj)
{
  if (!ISPTR(inObj))
    return false;
  RefVar cls(ClassOf(inObj));
  return EQ(cls, SYMA(rectangle)) || EQ(cls, SYMA(line)) || EQ(cls, SYMA(textBox))
      || EQ(cls, SYMA(ink)) || EQ(cls, SYMA(roundRectangle)) || EQ(cls, SYMA(oval))
      || EQ(cls, SYMA(bitmap)) || (EQ(cls, SYMA(picture)) && IsFrame(inObj))
      || EQ(cls, SYMA(polygon)) || EQ(cls, SYMA(wedge)) || EQ(cls, SYMA(region))
      || EQ(cls, SYMA(text));
}


bool IsStyleFrame(RefArg inObj)
{
  return IsFrame(inObj) && EQ(ClassOf(inObj), SYMA(frame));
}


namespace {

Box Union(const Box & a, const Box & b)
{
  if (a.bottom <= a.top || a.right <= a.left)
    return b;
  if (b.bottom <= b.top || b.right <= b.left)
    return a;
  return Box{std::min(a.top, b.top), std::min(a.left, b.left),
             std::max(a.bottom, b.bottom), std::max(a.right, b.right)};
}

Ref MakeBoxShape(RefArg inClass, const Box & inBox, long inSize = 8)
{
  RefVar shape(AllocateBinary(inClass, inSize));
  memset(BinaryData(shape), 0, inSize);
  SetBox(shape, inBox);
  return shape;
}

Box BoxOf(RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom)
{
  return Box{RINT(inTop), RINT(inLeft), RINT(inBottom), RINT(inRight)};
}

// a polygon's data from points (x, y pairs)
Ref PolygonData(RefArg inPoints)
{
  ArrayIndex count = Length(inPoints) / 2;
  long size = 12 + 4 * long(count);
  RefVar data(AllocateBinary(SYMA(polygonData), size));
  memset(BinaryData(data), 0, size);
  SetShort(data, 0, size);
  Box box{32767, 32767, -32768, -32768};
  for (ArrayIndex i = 0; i < count; ++i) {
    long x = RINT(GetArraySlot(inPoints, i * 2)), y = RINT(GetArraySlot(inPoints, i * 2 + 1));
    SetShort(data, 12 + i * 4, y);
    SetShort(data, 12 + i * 4 + 2, x);
    box = Box{std::min(box.top, y), std::min(box.left, x), std::max(box.bottom, y), std::max(box.right, x)};
  }
  if (count == 0)
    box = Box();
  SetBox(data, box, 4);
  return data;
}

// the bounds of a frame shape, as a binary (bitmap, picture, text)
Ref BoundsBinary(RefArg inShape)
{
  return GetProtoVariable(inShape, SYMA(bounds));
}

void Offset(RefArg ioShape, long dx, long dy)
{
  if (IsArray(ioShape)) {
    for (ArrayIndex i = 0, n = Length(ioShape); i < n; ++i) {
      RefVar item(GetArraySlot(ioShape, i));
      if (NOTNIL(item) && !IsStyleFrame(item))
        Offset(item, dx, dy);
    }
    return;
  }
  RefVar cls(ClassOf(ioShape));
  auto move = [dx, dy](RefArg binary, long offset) {
    Box b = GetBox(binary, offset);
    SetBox(binary, Box{b.top + dy, b.left + dx, b.bottom + dy, b.right + dx}, offset);
  };
  if (EQ(cls, SYMA(polygon))) {
    RefVar data(GetProtoVariable(ioShape, SYMA(data)));
    move(data, 4);
    for (long at = 12, size = Length(data); at + 4 <= size; at += 4) {
      SetShort(data, at, GetShort(data, at) + dy);
      SetShort(data, at + 2, GetShort(data, at + 2) + dx);
    }
  } else if (IsFrame(ioShape)) {
    RefVar bounds(BoundsBinary(ioShape));
    if (IsBinary(bounds))
      move(bounds, 0);
  } else if (IsBinary(ioShape) && Length(ioShape) >= 8)
    move(ioShape, 0);   // rectangle, oval, round rectangle, wedge, line
}

} // namespace


Box Bounds(RefArg inShape)
{
  if (IsArray(inShape)) {
    Box all;
    for (ArrayIndex i = 0, n = Length(inShape); i < n; ++i) {
      RefVar item(GetArraySlot(inShape, i));
      if (NOTNIL(item) && !IsStyleFrame(item))
        all = Union(all, Bounds(item));
    }
    return all;
  }
  RefVar cls(ClassOf(inShape));
  if (EQ(cls, SYMA(polygon))) {
    Box b = GetBox(GetProtoVariable(inShape, SYMA(data)), 4);
    b.bottom++;
    b.right++;
    return b;
  }
  if (IsFrame(inShape)) {
    RefVar bounds(BoundsBinary(inShape));
    if (IsBinary(bounds))
      return GetBox(bounds);
    ThrowErr(exGraf, -8804);
  }
  if (!IsPrimShape(inShape))
    ThrowErr(exGraf, -8804);
  Box b = GetBox(inShape);
  if (EQ(cls, SYMA(line))) {   // never empty
    if (b.bottom < b.top) std::swap(b.top, b.bottom);
    if (b.right < b.left) std::swap(b.left, b.right);
    if (b.bottom == b.top) b.bottom++;
    if (b.right == b.left) b.right++;
  }
  return b;
}

} // namespace shapes

using namespace shapes;

extern "C" {

Ref FMakeRect(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom)
{
  return MakeBoxShape(SYMA(rectangle), BoxOf(inLeft, inTop, inRight, inBottom));
}


Ref FMakeOval(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom)
{
  return MakeBoxShape(SYMA(oval), BoxOf(inLeft, inTop, inRight, inBottom));
}


Ref FMakeRoundRect(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom, RefArg inDiameter)
{
  RefVar shape(MakeBoxShape(SYMA(roundRectangle), BoxOf(inLeft, inTop, inRight, inBottom), 12));
  SetShort(shape, 8, RINT(inDiameter));
  SetShort(shape, 10, RINT(inDiameter));
  return shape;
}


Ref FMakeWedge(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom, RefArg inStart, RefArg inArc)
{
  RefVar shape(MakeBoxShape(SYMA(wedge), BoxOf(inLeft, inTop, inRight, inBottom), 12));
  SetShort(shape, 8, RINT(inStart));
  SetShort(shape, 10, RINT(inArc));
  return shape;
}


Ref FMakeLine(RefArg rcvr, RefArg inX1, RefArg inY1, RefArg inX2, RefArg inY2)
{
  // as the ROM: y1, x1, y2, x2 (the port had x and y swapped: B24)
  return MakeBoxShape(SYMA(line), Box{RINT(inY1), RINT(inX1), RINT(inY2), RINT(inX2)});
}


Ref FMakePolygon(RefArg rcvr, RefArg inPoints)
{
  RefVar shape(Clone(RA(canonicalPolygonShape)));
  SetFrameSlot(shape, SYMA(data), PolygonData(inPoints));
  return shape;
}


Ref FMakeText(RefArg rcvr, RefArg inString, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom)
{
  RefVar shape(Clone(RA(canonicalTextShape)));
  SetFrameSlot(shape, SYMA(bounds), MakeBoxShape(SYMA(boundsRect), BoxOf(inLeft, inTop, inRight, inBottom)));
  RefVar text(Clone(inString));
  SetClass(text, SYMA(textData));
  SetFrameSlot(shape, SYMA(data), text);
  return shape;
}


Ref FMakeShape(RefArg rcvr, RefArg inObject)
{
  if (IsFrame(inObject) && FrameHasSlot(inObject, SYMA(bits))) {
    // an icon: a bitmap shape of its bits, at its bounds
    RefVar shape(Clone(RA(canonicalBitmapShape)));
    RefVar bits(Clone(GetFrameSlot(inObject, SYMA(bits))));
    Box box;
    RefVar bounds(GetFrameSlot(inObject, SYMA(bounds)));
    if (IsFrame(bounds))
      box = Box{RINT(GetFrameSlot(bounds, SYMA(top))), RINT(GetFrameSlot(bounds, SYMA(left))),
                RINT(GetFrameSlot(bounds, SYMA(bottom))), RINT(GetFrameSlot(bounds, SYMA(right)))};
    else {   // the bitmap's own bounds, from its header
      Box b = GetBox(bits, 8);
      box = Box{0, 0, b.bottom - b.top, b.right - b.left};
    }
    SetFrameSlot(shape, SYMA(bounds), MakeBoxShape(SYMA(boundsRect), box));
    SetFrameSlot(shape, SYMA(data), bits);
    if (FrameHasSlot(inObject, SYMA(mask)))
      SetFrameSlot(shape, SYMA(mask), GetFrameSlot(inObject, SYMA(mask)));
    return shape;
  }
  if (IsBinary(inObject) && EQ(ClassOf(inObject), SYMA(picture))) {
    // a PICT: a picture shape, at the picture's frame
    int top, left, bottom, right;
    if (!pict::Frame((const unsigned char *)BinaryData(inObject), Length(inObject), &top, &left, &bottom, &right))
      ThrowErr(exGraf, -8804);
    RefVar shape(Clone(RA(canonicalPictureShape)));
    SetFrameSlot(shape, SYMA(bounds), MakeBoxShape(SYMA(boundsRect), Box{top, left, bottom, right}));
    SetFrameSlot(shape, SYMA(data), Clone(inObject));
    return shape;
  }
  if (IsPrimShape(inObject) || IsArray(inObject))
    return inObject;
  ThrowErr(exGraf, -8804);
  return NILREF;
}


Ref FMakePict(RefArg rcvr, RefArg inShapes, RefArg inStyle)
{
  // a PICT on a Newton; here the shapes with their style (see Shapes.h)
  RefVar pict(MakeArray(2));
  SetArraySlot(pict, 0, IsFrame(inStyle) ? Clone(inStyle) : AllocateFrame());
  SetArraySlot(pict, 1, inShapes);
  return pict;
}


Ref FOffsetShape(RefArg rcvr, RefArg ioShape, RefArg inDX, RefArg inDY)
{
  Offset(ioShape, RINT(inDX), RINT(inDY));
  return ioShape;
}


Ref FShapeBounds(RefArg rcvr, RefArg inShape)
{
  Box b = Bounds(inShape);
  RefVar bounds(AllocateFrame());
  SetFrameSlot(bounds, SYMA(left), MAKEINT(b.left));
  SetFrameSlot(bounds, SYMA(top), MAKEINT(b.top));
  SetFrameSlot(bounds, SYMA(right), MAKEINT(b.right));
  SetFrameSlot(bounds, SYMA(bottom), MAKEINT(b.bottom));
  return bounds;
}


Ref FIsPrimShape(RefArg rcvr, RefArg inObject)
{
  return MAKEBOOLEAN(IsPrimShape(inObject));
}


Ref FPointsToArray(RefArg rcvr, RefArg inPolygon)
{
  RefVar data(GetProtoVariable(inPolygon, SYMA(data)));
  ArrayIndex count = (Length(data) - 12) / 4;
  RefVar points(MakeArray(count * 2));
  for (ArrayIndex i = 0; i < count; ++i) {
    SetArraySlot(points, i * 2, MAKEINT(GetShort(data, 12 + i * 4 + 2)));
    SetArraySlot(points, i * 2 + 1, MAKEINT(GetShort(data, 12 + i * 4)));
  }
  return points;
}


Ref FArrayToPoints(RefArg rcvr, RefArg inArray)
{
  return PolygonData(inArray);
}

}
