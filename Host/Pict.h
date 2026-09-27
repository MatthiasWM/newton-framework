/*
 File: Pict.h

 QuickDraw pictures (PICT, class 'picture): the ROM's and packages' are
 version 1 and hold bitmaps (BitsRect, PackBitsRect), as far as newtc has
 seen (the ROM's four, Battleship's ships). DecodePict() draws those into
 a 1-bit bitmap in the Newton's layout: rows of rowBytes bytes, the
 leftmost pixel in the high bit, 1 black. Host/FLTK/RomImages and
 Host/FLTK/Images/extract_rom_images.py decode the same subset.
 */

#ifndef HOST_PICT_H
#define HOST_PICT_H

#include <cstddef>
#include <vector>

namespace pict {

/** The picture's frame (its bounds): top, left, bottom, right. */
bool Frame(const unsigned char * inData, size_t inSize, int * outTop, int * outLeft, int * outBottom, int * outRight);

/** The picture as a bitmap of its frame's size (see above). False if it
    isn't a version 1 picture of bitmaps. */
bool Decode(const unsigned char * inData, size_t inSize, int * outWidth, int * outHeight, int * outRowBytes,
            std::vector<unsigned char> * outBits);

} // namespace pict

#endif // HOST_PICT_H
