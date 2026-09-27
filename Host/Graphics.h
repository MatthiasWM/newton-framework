/*
 File: Graphics.h

 Global functions for rectangles and text measurement (natives in the ROM):

   SetBounds(left, top, right, bottom) -> {left, top, right, bottom}
   RelBounds(left, top, width, height) -> the same, from a size
   OffsetRect(rect, deltaH, deltaV) -> rect, moved (the frame itself)
   StrFontWidth(string, fontSpec) -> the string's width in pixels in that
       font (with FLTK: Host/FLTK/Links.h; without, a stub)
   FontHeight(fontSpec) -> a line's height in pixels (the same)
   FontAscent, FontDescent, FontLeading(fontSpec) -> its parts (no
       leading in FLTK: 0)
 */

#ifndef HOST_GRAPHICS_H
#define HOST_GRAPHICS_H

#include "Frames/Objects.h"

extern "C" {
Ref FSetBounds(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inRight, RefArg inBottom);
Ref FOffsetRect(RefArg rcvr, RefArg ioRect, RefArg inDeltaH, RefArg inDeltaV);
Ref FRelBounds(RefArg rcvr, RefArg inLeft, RefArg inTop, RefArg inWidth, RefArg inHeight);
Ref FStrFontWidth(RefArg rcvr, RefArg inString, RefArg inFontSpec);
Ref FFontHeight(RefArg rcvr, RefArg inFontSpec);
Ref FFontAscent(RefArg rcvr, RefArg inFontSpec);
Ref FFontDescent(RefArg rcvr, RefArg inFontSpec);
Ref FFontLeading(RefArg rcvr, RefArg inFontSpec);
}

#endif // HOST_GRAPHICS_H
