/*
 File: PNG.h

 Newton bitmaps and icons from PNG files (plan 4.6, see PNG.cc).
 */

#ifndef MATT_PNG_H
#define MATT_PNG_H

#include "Objects.h"

/// MakeBitmapFromPNG(file): a bitmap frame {bounds, bits, mask, colorData};
/// file: a path, or {file: path, depth: 4, dither: true, ...}
Ref FMakeBitmapFromPNG(RefArg inRcvr, RefArg inFile);
/// MakeIconFromPNG(file): the same with a mask always
Ref FMakeIconFromPNG(RefArg inRcvr, RefArg inFile);

#endif // MATT_PNG_H
