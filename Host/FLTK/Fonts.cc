/*
 File: Fonts.cc

 Newton's fonts in FLTK. See Fonts.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl.H>

#include "Host/FLTK/Fonts.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "ROMResources.h"
#include "ViewFlags.h"

namespace nfl {

namespace {

// The fonts of each family, as FLTK's: plain, bold, italic, bold italic
// (macOS PostScript names).
const char * const kFontNames[4][4] = {
  { "Geneva", "Verdana-Bold", "Verdana-Italic", "Verdana-BoldItalic" },   // System (Espy Sans)
  { "TimesNewRomanPSMT", "TimesNewRomanPS-BoldMT",                          // Fancy (New York)
    "TimesNewRomanPS-ItalicMT", "TimesNewRomanPS-BoldItalicMT" },
  { "Geneva", "Verdana-Bold", "Verdana-Italic", "Verdana-BoldItalic" },   // Simple (Geneva)
  { "AppleCasual", "AppleCasual", "AppleCasual", "AppleCasual" },          // Handwriting (Casual)
};

// How much bigger a family's font is drawn than its size (in tenths), for
// plain, bold, italic, bold italic: Espy Sans Bold is bigger than Verdana
// Bold of the same size (its bitmaps: bold 9 "Difficulty" 50 wide, capitals
// 7 high; Verdana Bold 9: 46 and 6.6, 10: 51 and 7.3); the Newton's Casual
// is bigger than Apple Casual (Screenshot1: "Medium", "Yes", "Normal" in
// Casual 10 are 45, 19 and 41 pixels wide; Apple Casual 14: 44, 20, 43).
const int kSizeScale[4][4] = {
  { 10, 11, 10, 11 },   // System: Geneva, Verdana Bold
  { 10, 10, 10, 10 },   // Fancy
  { 10, 11, 10, 11 },   // Simple: as System
  { 14, 14, 14, 14 },   // Handwriting
};

// The family of a font frame's family symbol ('espy ...; others: System).
long FamilyOf(RefArg inName)
{
  if (EQ(inName, SYMA(newYork)))
    return tsFancy;
  if (EQ(inName, SYMA(geneva)))
    return tsSimple;
  if (EQ(inName, SYMA(handwriting)))
    return tsHandwriting;
  return tsSystem;
}

long IntSlot(RefArg inFrame, const char * inSlot)
{
  Ref value = GetFrameSlot(inFrame, MakeSymbol(inSlot));
  return ISINT(value) ? RINT(value) : 0;
}

} // namespace


void RegisterFonts()
{
  static bool registered = false;
  if (registered)
    return;
  registered = true;
  for (int family = 0; family < 4; ++family)
    for (int face = 0; face < 4; ++face) {
      Fl_Font font = Fl_Font(FONT_SYSTEM + 4 * family + face);
#if defined(__APPLE__)
      Fl::set_font(font, kFontNames[family][face]);
#else
      // no Newton fonts here yet: FLTK's (Times for Fancy, else Helvetica)
      Fl::set_font(font, Fl_Font((family == tsFancy ? FL_TIMES : FL_HELVETICA) + face));
#endif
    }
}


Fl_Font NewtonFont(long inFamily, long inFace)
{
  RegisterFonts();
  if (inFamily < tsSystem || inFamily > tsHandwriting)
    inFamily = tsSystem;
  return Fl_Font(FONT_SYSTEM + 4 * inFamily + (inFace & (kBoldFace | kItalicFace)));
}


void FontFromSpec(RefArg inSpec, Fl_Font * outFont, Fl_Fontsize * outSize, bool inAsRequested)
{
  long family = tsSystem, face = 0, size = 12;
  if (IsFrame(inSpec)) {
    family = FamilyOf(GetFrameSlot(inSpec, SYMA(family)));
    face = IntSlot(inSpec, "face");
    if (long s = IntSlot(inSpec, "size"))
      size = s;
  } else if (ISINT(inSpec)) {
    long spec = RINT(inSpec);
    family = (spec & tsFamilyMask) >> tsFamilyShift;
    face = (spec & tsFaceMask) >> tsFaceShift;
    if (long s = (spec & tsSizeMask) >> tsSizeShift)
      size = s;
  }
  *outFont = NewtonFont(family, face);
  if (family < tsSystem || family > tsHandwriting)
    family = tsSystem;
  *outSize = Fl_Fontsize(inAsRequested ? size : (size * kSizeScale[family][face & 3] + 5) / 10);
}

} // namespace nfl
