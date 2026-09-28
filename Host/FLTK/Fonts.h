/*
 File: Fonts.h

 Newton's fonts in FLTK. A font spec (a viewFont, a style's font) is a font
 frame ({family, face, size}: the ROM's are espy 9, 12, 14, 18, plain, bold
 and underlined) or an integer (family, size and face bits: tsFamilyMask,
 tsSizeMask, tsFaceMask). The four families, and what newtc uses for them
 until the fonts are embedded (on macOS; elsewhere FLTK's Helvetica and
 Times):

   System       tsSystem 0, 'espy          Espy Sans (bitmaps: a Geneva)
                                           Geneva; bold, italic: Verdana
   Fancy        tsFancy 1, 'newYork        New York
                                           Times New Roman (macOS keeps New
                                           York for its own UI: CoreText
                                           gives Times for the name)
   Simple       tsSimple 2, 'geneva        Geneva
                                           Geneva; bold, italic: Verdana
   Handwriting  tsHandwriting 3,           Casual (the HWFont)
                'handwriting               Apple Casual

 Geneva and Verdana Bold measure as Espy Sans does (its bitmaps, from a Mac
 with the font: "Turn Speed" in 10 is 55 pixels, bold 63; Geneva 10 55,
 Verdana Bold 10 64). Geneva has no bold or italic face on macOS, Apple
 Casual only its regular one (FLTK can't make one up). Apple Casual and
 Verdana Bold are drawn bigger than asked (FontFromSpec): they are smaller
 than the Newton's Casual and Espy Sans Bold.

 Each family is four FLTK fonts, as FLTK's own: plain, bold, italic, bold
 italic (NewtonFont(family, face)). The other face bits (underline,
 outline, super- and subscript): not yet.
 */

#ifndef HOST_FLTK_FONTS_H
#define HOST_FLTK_FONTS_H

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Enumerations.H>

#include "Frames/Objects.h"

namespace nfl {

/** The families' fonts, from FLTK's first free one on; each is four: plain,
    bold, italic, bold italic. */
enum : int {
  FONT_SYSTEM = FL_FREE_FONT,
  FONT_FANCY = FONT_SYSTEM + 4,
  FONT_SIMPLE = FONT_SYSTEM + 8,
  FONT_HANDWRITING = FONT_SYSTEM + 12,
  LAST_FONT = FONT_SYSTEM + 16
};

/** Register the fonts with FLTK (only the first call does; NewtonFont()
    calls it). */
void RegisterFonts();

/** The FLTK font for a family (tsSystem ... tsHandwriting; others: System)
    and face bits (bold 1, italic 2; the others ignored). */
Fl_Font NewtonFont(long inFamily, long inFace);

/** The FLTK font and size for a font spec (a font frame or an integer; nil:
    System 12): the size to draw and measure text with, which is bigger
    than the spec's for Handwriting (Apple Casual is smaller than the
    Newton's Casual: 14 for 10) and for System and Simple bold (Verdana
    Bold is smaller than Espy Sans Bold: 10 for 9). inAsRequested: the
    spec's size, for the
    font's height, ascent and descent (FontHeight(), ...: a Newton script
    gets the size it asked for). */
void FontFromSpec(RefArg inSpec, Fl_Font * outFont, Fl_Fontsize * outSize, bool inAsRequested = false);

} // namespace nfl

#endif // HOST_FLTK_FONTS_H
