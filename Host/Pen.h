/*
 File: Pen.h

 Global functions for pen strokes ("units", the argument of
 viewClickScript): with FLTK they read the strokes in Host/FLTK/Pen.h;
 without FLTK there are no strokes, and they are stubs.

   GetPoint(selector, unit), GetPointsArray(unit), GetPointsArrayXY(unit),
   StrokeBounds(unit), StrokeDone(unit), InkOff(unit), InkOffUnHobbled(unit),
   InkOn(unit), GetUnitStartTime(unit), GetUnitEndTime(unit),
   GetUnitDownTime(unit), GetUnitUpTime(unit)
 */

#ifndef HOST_PEN_H
#define HOST_PEN_H

#include "Frames/Objects.h"

extern "C" {
Ref FGetPoint(RefArg rcvr, RefArg inSelector, RefArg inUnit);
Ref FGetPointsArray(RefArg rcvr, RefArg inUnit);
Ref FGetPointsArrayXY(RefArg rcvr, RefArg inUnit);
Ref FStrokeBounds(RefArg rcvr, RefArg inUnit);
Ref FStrokeDone(RefArg rcvr, RefArg inUnit);
Ref FInkOff(RefArg rcvr, RefArg inUnit);
Ref FInkOffUnHobbled(RefArg rcvr, RefArg inUnit);
Ref FInkOn(RefArg rcvr, RefArg inUnit);
Ref FGetUnitStartTime(RefArg rcvr, RefArg inUnit);
Ref FGetUnitEndTime(RefArg rcvr, RefArg inUnit);
Ref FGetUnitDownTime(RefArg rcvr, RefArg inUnit);
Ref FGetUnitUpTime(RefArg rcvr, RefArg inUnit);
}

#endif // HOST_PEN_H
