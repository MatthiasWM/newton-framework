/*
 File: Graphics.cc

 Global functions for rectangles and text measurement. See Graphics.h.
 */

#include "Host/Graphics.h"
#include "Utilities/Unimplemented.h"
#if NEWTC_USES_FLTK
#include "Host/FLTK/Links.h"
#endif

#include "Frames/Frames.h"
#include "Frames/Lookup.h"
#include "Frames/Globals.h"
#include "ROMResources.h"

extern "C" {

Ref FSetBounds(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom)
{
  RefVar bounds(AllocateFrame());
  SetFrameSlot(bounds, SYMA(left), inLeft);
  SetFrameSlot(bounds, SYMA(top), inTop);
  SetFrameSlot(bounds, SYMA(right), inRight);
  SetFrameSlot(bounds, SYMA(bottom), inBottom);
  return bounds;
}


Ref FOffsetRect(RefArg rcvr, RefArg ioRect, RefArg inDeltaH, RefArg inDeltaV)
{
  long dh = RINT(inDeltaH), dv = RINT(inDeltaV);
  SetFrameSlot(ioRect, SYMA(left), MAKEINT(RINT(GetFrameSlot(ioRect, SYMA(left))) + dh));
  SetFrameSlot(ioRect, SYMA(right), MAKEINT(RINT(GetFrameSlot(ioRect, SYMA(right))) + dh));
  SetFrameSlot(ioRect, SYMA(top), MAKEINT(RINT(GetFrameSlot(ioRect, SYMA(top))) + dv));
  SetFrameSlot(ioRect, SYMA(bottom), MAKEINT(RINT(GetFrameSlot(ioRect, SYMA(bottom))) + dv));
  return ioRect;
}


#if NEWTC_USES_FLTK
Ref FStrFontWidth(RefArg rcvr, RefArg inString, RefArg inFontSpec)
{
  return nfl::StrFontWidth(inString, inFontSpec);
}
#else
NS_STUB(FStrFontWidth, RefArg rcvr, RefArg inString, RefArg inFontSpec)
#endif

}
