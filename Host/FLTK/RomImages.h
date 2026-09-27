/*
 File: RomImages.h

 The ROM's graphics as images for FLTK. Host/FLTK/Images/ holds them as PNG
 files, lifted from the ROM by extract_rom_images.py: every magic pointer
 that is a picture (a QuickDraw PICT: all of them are bitmaps) or a bitmap
 frame (an icon), named rom_NNNN[_name].png after its index (@13 ->
 rom_0013.png). cmake/EmbedImages.cmake compiles them into newtc.

 A PNG can be replaced by a better one, of any resolution and depth (8-bit
 gray, with alpha or not): newtc draws it at the ROM object's size (a
 PICT's frame, an icon's bounds; Fl_Image::scale()), so one twice as big
 is sharp on a Retina screen. extract_rom_images.py leaves existing PNGs
 alone (unless --force).
 */

#ifndef HOST_FLTK_ROMIMAGES_H
#define HOST_FLTK_ROMIMAGES_H

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl_Image.H>

namespace nfl {

/** One embedded PNG (the table is generated). */
struct RomImageData
{
  int magic;                   // the magic pointer index
  const char * name;           // its file name
  const unsigned char * png;
  int size;
};

extern const RomImageData kRomImages[];
extern const int kRomImageCount;

/** The image of the ROM object with this magic pointer index, or nullptr
    if newtc has none. inInverted: black and white swapped (a hilited
    view); transparent stays transparent. Made once and kept: don't delete
    it. */
Fl_Image * RomImage(long inMagicIndex, bool inInverted = false);

} // namespace nfl

#endif // HOST_FLTK_ROMIMAGES_H
