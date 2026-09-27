/*
 File: Pen.h

 Pen strokes (the mouse, while its button is down): what NewtonOS calls a
 unit and passes to viewClickScript(unit). A unit is an address Ref to the
 stroke (AddressToRef), as in the ROM (UnitFromRef). The stroke's points are
 in Newton display coordinates (as GlobalBox), from pen down to pen up; a
 view's link adds them as its widget gets the pen events (Links.cc,
 Link::HandlePen).

 A script can keep a unit after its viewClickScript: the last strokes stay
 (kKeptStrokes); an older unit is "nil unit", as a unit the ROM doesn't
 know.

 NewtonScript (Host/Pen.h has the natives):
   GetPoint(selector, unit): firstX 0, firstY 1, lastX 2, lastY 3, finalX 4,
       finalY 5; firstXY 6, lastXY 7, finalXY 8 give a point {x, y}.
   GetPointsArray(unit): [y, x, y, x, ...]; GetPointsArrayXY(unit): [x, y, ...]
   StrokeBounds(unit), StrokeDone(unit), InkOff(unit), InkOn(unit),
   GetUnitDownTime(unit), GetUnitUpTime(unit), GetUnitStartTime(unit),
   GetUnitEndTime(unit) (ticks).
 While a stroke goes on, asking about it lets FLTK handle waiting events
 first (Fl::check()): on a Newton the tablet adds points meanwhile, and a
 script may wait for StrokeDone(unit).
 */

#ifndef HOST_FLTK_PEN_H
#define HOST_FLTK_PEN_H

#include "Frames/Objects.h"

#include <vector>

namespace nfl {

/** A point on the Newton display. */
struct PenPoint
{
  long x = 0, y = 0;
};

class Stroke
{
public:
  /** A new stroke from pen down at inPoint. */
  static Stroke * Begin(PenPoint inPoint);
  /** The stroke of a unit, or throw "nil unit". */
  static Stroke * Of(RefArg inUnit);

  Ref Unit() { return AddressToRef(this); }
  void Add(PenPoint inPoint);
  void End(PenPoint inPoint);

  bool Done() const { return fDone; }
  const std::vector<PenPoint> & Points() const { return fPoints; }
  PenPoint First() const { return fPoints.front(); }
  PenPoint Last() const { return fPoints.back(); }
  long DownTime() const { return fDownTime; }
  long UpTime() const { return fUpTime; }
  bool Ink() const { return fInk; }
  void Ink(bool inOn) { fInk = inOn; }

  /** Let the events come that add points (see above). */
  void Update() const;

private:
  explicit Stroke(PenPoint inPoint);
  std::vector<PenPoint> fPoints;
  long fDownTime, fUpTime = 0;
  bool fDone = false;
  bool fInk = true;
};

/** The natives (Host/Pen.h calls them). */
Ref GetPoint(RefArg inSelector, RefArg inUnit);
Ref GetPointsArray(RefArg inUnit, bool inXY);
Ref StrokeBounds(RefArg inUnit);
Ref StrokeDone(RefArg inUnit);
Ref InkOff(RefArg inUnit, bool inOff);
Ref UnitTime(RefArg inUnit, bool inUp);

} // namespace nfl

#endif // HOST_FLTK_PEN_H
