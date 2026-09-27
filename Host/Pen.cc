/*
 File: Pen.cc

 Global functions for pen strokes. See Pen.h.
 */

#include "Host/Pen.h"
#include "Utilities/Unimplemented.h"
#if NEWTC_USES_FLTK
#include "Host/FLTK/Pen.h"
#endif

extern "C" {

#if NEWTC_USES_FLTK

Ref FGetPoint(RefArg rcvr, RefArg inSelector, RefArg inUnit) { return nfl::GetPoint(inSelector, inUnit); }
Ref FGetPointsArray(RefArg rcvr, RefArg inUnit)   { return nfl::GetPointsArray(inUnit, false); }
Ref FGetPointsArrayXY(RefArg rcvr, RefArg inUnit) { return nfl::GetPointsArray(inUnit, true); }
Ref FStrokeBounds(RefArg rcvr, RefArg inUnit)     { return nfl::StrokeBounds(inUnit); }
Ref FStrokeDone(RefArg rcvr, RefArg inUnit)       { return nfl::StrokeDone(inUnit); }
Ref FInkOff(RefArg rcvr, RefArg inUnit)           { return nfl::InkOff(inUnit, true); }
Ref FInkOffUnHobbled(RefArg rcvr, RefArg inUnit)  { return nfl::InkOff(inUnit, true); }
Ref FInkOn(RefArg rcvr, RefArg inUnit)            { return nfl::InkOff(inUnit, false); }
// a unit is one stroke: it starts at pen down and ends at pen up
Ref FGetUnitStartTime(RefArg rcvr, RefArg inUnit) { return nfl::UnitTime(inUnit, false); }
Ref FGetUnitEndTime(RefArg rcvr, RefArg inUnit)   { return nfl::UnitTime(inUnit, true); }
Ref FGetUnitDownTime(RefArg rcvr, RefArg inUnit)  { return nfl::UnitTime(inUnit, false); }
Ref FGetUnitUpTime(RefArg rcvr, RefArg inUnit)    { return nfl::UnitTime(inUnit, true); }

#else

// No strokes without FLTK
NS_STUB(FGetPoint, RefArg rcvr, RefArg inSelector, RefArg inUnit)
NS_STUB(FGetPointsArray, RefArg rcvr, RefArg inUnit)
NS_STUB(FGetPointsArrayXY, RefArg rcvr, RefArg inUnit)
NS_STUB(FStrokeBounds, RefArg rcvr, RefArg inUnit)
NS_STUB(FStrokeDone, RefArg rcvr, RefArg inUnit)
NS_STUB(FInkOff, RefArg rcvr, RefArg inUnit)
NS_STUB(FInkOffUnHobbled, RefArg rcvr, RefArg inUnit)
NS_STUB(FInkOn, RefArg rcvr, RefArg inUnit)
NS_STUB(FGetUnitStartTime, RefArg rcvr, RefArg inUnit)
NS_STUB(FGetUnitEndTime, RefArg rcvr, RefArg inUnit)
NS_STUB(FGetUnitDownTime, RefArg rcvr, RefArg inUnit)
NS_STUB(FGetUnitUpTime, RefArg rcvr, RefArg inUnit)

#endif

}
