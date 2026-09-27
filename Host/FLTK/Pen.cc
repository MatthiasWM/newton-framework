/*
 File: Pen.cc

 Pen strokes. See Pen.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>

#include "Host/FLTK/Pen.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "ROMResources.h"

#include <algorithm>
#include <deque>

extern ULong GetTicks(void);   // ObjectSystem.cc

namespace nfl {

namespace {

// The last strokes, newest last: their units stay valid.
const size_t kKeptStrokes = 8;
std::deque<Stroke *> gStrokes;

enum { kFirstX, kFirstY, kLastX, kLastY, kFinalX, kFinalY, kFirstXY, kLastXY, kFinalXY };

Ref MakePoint(PenPoint inPoint)
{
  RefVar point(AllocateFrame());
  SetFrameSlot(point, SYMA(x), MAKEINT(inPoint.x));
  SetFrameSlot(point, SYMA(y), MAKEINT(inPoint.y));
  return point;
}

} // namespace


Stroke::Stroke(PenPoint inPoint)
: fDownTime(long(GetTicks()))
{
  fPoints.push_back(inPoint);
}


Stroke * Stroke::Begin(PenPoint inPoint)
{
  Stroke * stroke = new Stroke(inPoint);
  gStrokes.push_back(stroke);
  if (gStrokes.size() > kKeptStrokes) {
    delete gStrokes.front();
    gStrokes.pop_front();
  }
  return stroke;
}


Stroke * Stroke::Of(RefArg inUnit)
{
  if (ISINT(inUnit) || ISPTR(inUnit)) {   // an address Ref is an integer
    Stroke * stroke = ISINT(inUnit) ? static_cast<Stroke *>(RefToAddress(inUnit)) : nullptr;
    if (std::find(gStrokes.begin(), gStrokes.end(), stroke) != gStrokes.end())
      return stroke;
  }
  ThrowMsg("nil unit");
  return nullptr;
}


void Stroke::Add(PenPoint inPoint)
{
  if (!fDone && (inPoint.x != fPoints.back().x || inPoint.y != fPoints.back().y))
    fPoints.push_back(inPoint);
}


void Stroke::End(PenPoint inPoint)
{
  Add(inPoint);
  fDone = true;
  fUpTime = long(GetTicks());
}


void Stroke::Update() const
{
  if (!fDone)
    Fl::check();
}


Ref GetPoint(RefArg inSelector, RefArg inUnit)
{
  Stroke * stroke = Stroke::Of(inUnit);
  long selector = ISINT(inSelector) ? RINT(inSelector) : -1;
  if (selector != kFirstX && selector != kFirstY && selector != kFirstXY)
    stroke->Update();   // the first point doesn't change
  // last: the latest point so far; final: the last one (the same here)
  switch (selector) {
    case kFirstX:  return MAKEINT(stroke->First().x);
    case kFirstY:  return MAKEINT(stroke->First().y);
    case kLastX:
    case kFinalX:  return MAKEINT(stroke->Last().x);
    case kLastY:
    case kFinalY:  return MAKEINT(stroke->Last().y);
    case kFirstXY: return MakePoint(stroke->First());
    case kLastXY:
    case kFinalXY: return MakePoint(stroke->Last());
  }
  return MAKEINT(0);
}


Ref GetPointsArray(RefArg inUnit, bool inXY)
{
  Stroke * stroke = Stroke::Of(inUnit);
  stroke->Update();
  const std::vector<PenPoint> & points = stroke->Points();
  RefVar array(MakeArray(ArrayIndex(points.size() * 2)));
  for (size_t i = 0; i < points.size(); ++i) {
    // GetPointsArray: y, x (Battleship's map takes points[0] as the row)
    SetArraySlot(array, ArrayIndex(i * 2), MAKEINT(inXY ? points[i].x : points[i].y));
    SetArraySlot(array, ArrayIndex(i * 2 + 1), MAKEINT(inXY ? points[i].y : points[i].x));
  }
  return array;
}


Ref StrokeBounds(RefArg inUnit)
{
  Stroke * stroke = Stroke::Of(inUnit);
  stroke->Update();
  long left = stroke->First().x, right = left, top = stroke->First().y, bottom = top;
  for (const PenPoint & p : stroke->Points()) {
    left = std::min(left, p.x);
    right = std::max(right, p.x);
    top = std::min(top, p.y);
    bottom = std::max(bottom, p.y);
  }
  RefVar bounds(AllocateFrame());
  SetFrameSlot(bounds, SYMA(left), MAKEINT(left));
  SetFrameSlot(bounds, SYMA(top), MAKEINT(top));
  SetFrameSlot(bounds, SYMA(right), MAKEINT(right + 1));
  SetFrameSlot(bounds, SYMA(bottom), MAKEINT(bottom + 1));
  return bounds;
}


Ref StrokeDone(RefArg inUnit)
{
  Stroke * stroke = Stroke::Of(inUnit);
  stroke->Update();
  return MAKEBOOLEAN(stroke->Done());
}


Ref InkOff(RefArg inUnit, bool inOff)
{
  // newtc draws no ink yet: the stroke only remembers it
  Stroke::Of(inUnit)->Ink(!inOff);
  return TRUEREF;
}


Ref UnitTime(RefArg inUnit, bool inUp)
{
  Stroke * stroke = Stroke::Of(inUnit);
  return MAKEINT(inUp ? stroke->UpTime() : stroke->DownTime());
}

} // namespace nfl
