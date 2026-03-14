//
//  BookWriter.cc
//  Newton
//
//  Created by Matthias Melcher on 25.02.26.
//

// find /Users/matt/Azureus/unna2/books/ -name "*.pkg" -exec ./build/Xcode/Debug/newtc -pkg \{\} -opdf "x" \;

// Not a book:
// '/Users/matt/Azureus/unna2/books/Maps/BostonSubway-TMap-12.pkg'

// Package reader crashes:
// '/Users/matt/Azureus/unna2/books/Maps/SMRTmap.pkg'
// '/Users/matt/Azureus/unna2/books/Newton/newtonmail.htm.pkg'


#include "BookWriter.h"

#include <string.h>
#include <arpa/inet.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

// Return the human-readable full name of a CTFontRef as std::string
static std::string CTFontGetHumanReadableName(CTFontRef font) {
  if (!font) return std::string();
  CFStringRef cfName = CTFontCopyFullName(font);
  if (!cfName) return std::string();
  // Get length in a UTF-8 buffer
  CFIndex length = CFStringGetLength(cfName);
  CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::string result;
  result.resize(static_cast<size_t>(maxSize));
  if (CFStringGetCString(cfName, result.data(), maxSize, kCFStringEncodingUTF8)) {
    // Trim to actual C-string length
    result.resize(strlen(result.c_str()));
  } else {
    result.clear();
  }
  CFRelease(cfName);
  return result;
}

#include <vector>

#include "Symbols.h"
#include "Frames/Iterators.h"
#include "Matt/ObjectPrinter.h"
#include "Matt/PDFGen/pdfgen.h"

extern std::string currentFileName;
extern int writeBookToPDF(RefArg book, const std::string &filename);

constexpr uint8_t kNoBreak = 0;
constexpr uint8_t kLineBreak = 1;
constexpr uint8_t kWordBreak = 2;
constexpr uint8_t kTabBreak = 3;

typedef struct {
  uint16_t code;
  uint8_t font;
  uint8_t size;
  float width;
} Letter;

// Do we also want a "required key" list?
const char* known_book_keys[] = {
  "version",      // integer, 1 or 2
  "isbn",         // string, "Co9505..." and many others
  "title",        // string
  "shortTitle",   // string
  "copyright",    // string
  "author",       // string
  "publisher",    // string
  "data",         // documented: frame, creator data for scripts
  "contents",     // array of frames, see below
  "styles",       // array of frames, see below, duplicates possible
  "hints",        // array of nil, true, and binary blocks for faster word search
  "browsers",     // array of frames, not explored yet
  "templates",    // array of frames, not explored yet, templates for page and column layouts
  "rendering",    // array of frames, not explored much, pixel exact rendering of each page for one or more screens
  "icon",         // frame holding an image: mask, bits, bounds, usually a small (<32x32) bitmap with transparency
  "publicationDate", // integer
  "flags",        // integer, only value found is 64
  "appSymbol",    // symbol: often name of source html file, but not always
  "scripts",      // array of pairs: symbol ('buttonClickScript), followed by function or value ('copyProtection: 1)
  "keywords",     // string: space separated list of words
  nullptr
};

// Contents // TODO: compare to clParagraphView
const char* known_book_contents_keys[] = {
  "data",         // string (text) or binary 'image (PICT resource?) or frame with NS bitmap image (b/w, gray, color) or NS forms
  "layout",       // integer, usually only one bit set
  "styles",       // array of pairs: first, number of chars, then style, ether an integer (bits), or a ref to a style (see version?)
  "viewJustify",  // integer, observing vjRightH = 1 or vjCenterH = 2
  "viewFont",     // integer of style frame ref (see styles) (ROM_fontSystem10 := @87?), or tsBold := 0x00100000; tsItalic := 0x00200000; simpleFont12 := 0x00003002;
  "flags",        // integer flag bits, observed 4, 8, 16, 32, ?? vWidthGrowsWithText (1 << 2), etc.?
  "tabs",         // array of integers, tab position in pixels, relative to the left edge of the block. Up to 8, can be an empty array
  "look",         // integer, bits?
  "scripts",      // array of symbol and value pairs, symbol i.e. viewClickScript, value can be a Newton script block,
  "name",         // short string, sometimes chapter name, or headline?
  "src",          // string, original image url, rarely used
  "scale",        // real: probably images scale
  "hrefs",        // array of arrays: string url or anchor or both, followed by 0 to 6 integers
  "type",         // not used very often, always symbol 'form ('data then describes a widget form)
  "edgeWidth",    // rarely used, integer, with of line used in frames around this block (see flags? box, rounded?)
  "help",         // used twice, integer, 1 or 2: only in '/Users/matt/Azureus/unna2/books/Misc_Non-Fiction/MacWorldExpoSF96Guide.pkg'
  "FN",           // integer 1..34: only in '/Users/matt/Azureus/unna2/books/Computers/Cyberwar/Cyberwar.pkg'
  nullptr
};

// styles
const char* known_book_style_keys[] = {
  "family",       // Symbol:  espy->Helvetica, geneva->Helvetica, newyork->Times Roman, (Monaco->Courier)
                  // found: espy, newyork, geneva
                    //  "Currently, the Newton MessagePad supports only the bitmapped versions of
                    //  font families New York, Geneva, Espy Sans and Espy Sans Bold in sizes 9, 10,
                    //  12, 14, and 18 points."
  "face",         // Integer: 0, 1, 4, 1024, and more
  "size",         // Integer: 5, 9, 10, 12, 14, 16, 18, 24, 36
  nullptr
};

// browsers
const char* known_book_browsers_keys[] = {
  "name",         // String: "Contents" in all tested files
  "list",         // Array of entries: not explored entirely
// browser item
//  {item: {data: "Cunnigham park",
//    viewFont: tsSystem+tsSize(12)+tsPlain,
                  //  /* constant */ tsFamilyMask := 0x000003FF;
                  //  /* constant */ tsFamilyShift := 0;
                  //  /* constant */ tsSizeMask := 0x000FFC00;
                  //  /* constant */ tsSizeShift := 10;
                  //  /* constant */ tsFaceMask := 0x3FF00000;
                  //  /* constant */ tsFaceShift := 20;
                  //  /* constant */ tsSimple := 2 << tsFamilyShift;
                  //  /* constant */ tsFancy := 1 << tsFamilyShift;
                  //  func tsSize(num)((num) << tsSizeShift);
                  //  /* constant */ tsPlain := 0x00000000;
                  //  /* constant */ tsBold := 0x00100000;
                  //  /* constant */ tsItalic := 0x00200000;
                  //  /* constant */ tsUnderline := 0x00400000;
                  //  /* constant */ tsOutline := 0x00800000;
                  //  /* constant */ tsSuperScript := 0x08000000;
                  //  /* constant */ tsSubScript := 0x10000000;
                  //  /* constant */ tsUndefinedFace := 0x20000000;
                  //  /* constant */ userFont9 := 0x00002401;
                  //  /* constant */ userFont10 := 0x00002801;
                  //  /* constant */ userFont12 := 0x00003001;
                  //  /* constant */ userFont18 := 0x00004801;
//    layout: 38,
//    name: "MAP"
//    tabs: []},
//  level: 2},
  nullptr
};

// templates
const char* known_book_templates_keys[] = {
  "nColumns",     // Integer, 1, 2, or 3
  "column",       // Array with nColumns entries:  [ {width: 12, type: 0}, {...} ]
  "flags",        // Integer, observed 1, 8, 9
  "header",       // Ref to a Contents block
  "scripts",      // Array of symbol, value, symbol, value, ...
  "runTop",       // Ref to a Contents block
  "runBottom",    // Ref to a Contents block, can be images, text, an entire menu system, etc.
  nullptr
};

// rendering
const char* known_book_rendering_keys[] = {
  "pageSize",     // Box: {left: 0, top: 0, right: 240, bottom: 302}
  "contents",     // Array of array of incrementing integers, inner can be empty
  "pages",        // array of frames
  /* TODO: page description
   { template:
     blocks:
     [
       {
         bounds: [ int l, t, r, b]
         item: -> Contents
         dataOffset: integer for rendering partial data blocks
         dataLen: integer for rendering partial data blocks
       }, { ... }
     ]
   }
   */
  nullptr
};

/* Reserved slot names:

 bookmarking: rendered for creating a bookmark
 curRendering kioskDest scripts
 bookRef data layout type
 browser destPage look
 contentArea edgeWidth
 printing: bool, rendered for printing
 cuPage flags related
 */

int checkForUnknownKeys(const char* label, RefArg frame, const char* known_keys[]) {
  if (!IsFrame(frame)) {
    fprintf(stderr, "%s Not a frame!\n", label);
    return 1;
  }
  int ret = 0;
  CObjectIterator iter(frame, false);
  for ( ; !iter.done(); iter.next()) {
    const char* tag = SymbolName(iter.tag());
    const char** key = known_keys;
    for (; *key; ++key) {
      if (strcasecmp(*key, tag) == 0) break;
    }
    if (!*key) {
      fprintf(stderr, "%s Unknown key '%s'!\n", label, tag);
      ret = 1;
    }
  }
  return ret;
};


// Use this table to convert unicode to macRoman and back.
static uint16_t unicodeLUT[128] = {
  0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1,
  0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
  0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3,
  0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,
  0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,
  0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,
  0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211,
  0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x2126, 0x00E6, 0x00F8,
  0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB,
  0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,
  0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,
  0x00FF, 0x0178, 0x2044, 0x00A4, 0x2039, 0x203A, 0xFB01, 0xFB02,
  0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1,
  0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
  0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC,
  0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7 };

/**
 Convert a Newton Unicode character into MacRoman encoding. Unsupported
 characters return 255.
 Slow implementation. Could use a std::map.
 */
uint8_t unicode_to_macroman(uint16_t u) {
  if (u < 128)
    return (uint8_t)u;
  for (int i = 0; i < 128; ++i) {
    if (unicodeLUT[i] == u)
      return (uint8_t)(i + 128);
  }
  return 255; // Not representable in MacRoman
}

/**
 Convert a MacRoman encoded character into a UTF16 unicode character.
 */
uint16_t macroman_to_unicode(uint8_t u) {
  if (u < 128)
    return u;
  else
    return unicodeLUT[u-128];
}

std::string unicode_to_utf8(uint16_t u) {
    std::string out;
    if (u <= 0x7F) {
        // 1-byte UTF-8
        out.push_back(static_cast<char>(u));
    } else if (u <= 0x7FF) {
        // 2-byte UTF-8
        out.push_back(static_cast<char>(0xC0 | ((u >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (u & 0x3F)));
    } else {
        // 3-byte UTF-8 (for BMP, ignoring surrogates)
        out.push_back(static_cast<char>(0xE0 | ((u >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (u & 0x3F)));
    }
    return out;
}

int writePackageBookToPDF(RefArg pkg, const std::string &filename) {
//  printf("Reading '%s'\n", currentFileName.c_str());
  if (!IsFrame(pkg)) {
    fprintf(stderr, "Not a package, not a Frame!\n");
    return 1;
  }
  Ref signature = GetFrameSlot(pkg, MakeSymbol("signature"));
  if (!IsSymbol(signature)) {
    fprintf(stderr, "Not a package, no signature!\n");
    return 1;
  }
  if ( (SymbolCompare(signature, MakeSymbol("package0")) != 0) &&
       (SymbolCompare(signature, MakeSymbol("package1")) != 0) ) {
    fprintf(stderr, "Not a supported package, not 'package0 or 1'!\n");
    return 1;
  }
  Ref part_array = GetFrameSlot(pkg, MakeSymbol("part"));
  if (!IsArray(part_array)) {
    fprintf(stderr, "Not a supported package, part list not found!\n");
    return 1;
  }
  Ref part0 = GetArraySlot(part_array, 0);
  if (!IsFrame(part0)) {
    fprintf(stderr, "Not a supported package, part 0 not found!\n");
    return 1;
  }
  Ref type = GetFrameSlot(part0, MakeSymbol("type"));
  if (!IsString(type)) {
    fprintf(stderr, "Not a supported package, part 0 type not found!\n");
    return 1;
  }
  if (strcmp( BinaryData(ASCIIString(type)), "book") != 0) {
    fprintf(stderr, "Not a supported package, part 0 is not of type \"book\"!\n");
    return 1;
  }
  Ref data = GetFrameSlot(part0, MakeSymbol("data"));
  if (!IsFrame(data)) {
    fprintf(stderr, "Can't read book, part 0 data not found!\n");
    return 1;
  }
  Ref book = GetFrameSlot(data, MakeSymbol("book"));
  if (!IsFrame(book)) {
    fprintf(stderr, "Can't read book, part 0 book data not found!\n");
    return 1;
  }
  int ret = 0;
  if (checkForUnknownKeys("part:data:book:", book, known_book_keys))
    ret = 1;

  if (writeBookToPDF(book, filename))
    ret = 1;
  return ret;
}

void getStyle(Ref style, int* font, int* height) {
  if (ISINT(style)) {
    // family: 0x000003FF, size: 0x000FFC00 (>>10), face: 0x3FF00000; (>>20)
    // pain=00, bold=01, italic=02, underline=04, outline=08, super=80, sub=100
    auto f = RVALUE(style);
    // TODO: map to pdf fonts
  } else if (IsFrame(style)) {
    // { family, mask, size }
    Ref f = GetFrameSlot(style, MakeSymbol("family"));
    Ref m = GetFrameSlot(style, MakeSymbol("face"));
    Ref s = GetFrameSlot(style, MakeSymbol("size"));
  }
}

void set_font(RefArg r, int* font, int* height) {
  *font = 1;
  *height = 14;
  if (ISINT(r)) {
    int f = RefToInt(r);
    int family = f & 0x000003FF;
    // espy (system, 0), geneva (simple,, 2), newyork (fancy, 1), handwriting (casual, edit, 3)
    int size = (f & 0x000FFC00) >> 10;
    bool bold = (f & 0x00100000) != 0;
    bool italic = (f & 0x00200000) != 0;
    int face = 0;
    if (bold) face += 1;
    if (italic) face += 2;
    switch (family) {
      case 0: *font = 4 + face; break; // espy, system -> Courier(8) or Helvetica(4)?
      case 1: *font = 4 + face; break; // geneva, simple -> Helvetica
      case 2: *font = 0 + face; break; // newyork, fancy -> Times
      default: *font = 4 + face; break;
    }
    *height = size;
  } else if (IsFrame(r)) {
    int family = 4;
    auto familyRef = GetFrameSlot(r, MakeSymbol("family"));
    if (IsSymbol(familyRef)) {
      if (SymbolCompare(familyRef, MakeSymbol("espy"))==0) family = 4;          // espy, system -> Courier or Helvetica?
      else if (SymbolCompare(familyRef, MakeSymbol("geneva"))==0) family = 4;   // geneva, simple -> Helvetica
      else if (SymbolCompare(familyRef, MakeSymbol("newyork"))==0) family = 0;  // newyork, fancy -> Times
    }
    int face = 0;
    auto faceRef = GetFrameSlot(r, MakeSymbol("face"));
    if (ISINT(faceRef)) {
      if (RefToInt(faceRef) & 1) face += 1;
      if (RefToInt(faceRef) & 2) face += 2;
    }
    int size = 14;
    auto sizeRef = GetFrameSlot(r, MakeSymbol("size"));
    if (ISINT(sizeRef))
      size = RefToInt(sizeRef);
    *font = family + face;
    *height = size;
  }
}

int textBlockToLetters(RefArg text_block) {
  int i;

  // Extract the UTF16 string and styles
  Ref text = GetFrameSlot(text_block, MakeSymbol("data"));
  if (!IsString(text))
    return 1;

  Ref styles = GetFrameSlot(text_block, MakeSymbol("styles"));
  Ref viewFont = GetFrameSlot(text_block, MakeSymbol("viewFont"));

  int current_font = 0;
  int current_height = 14;
  if (NOTNIL(viewFont)) {
    set_font(viewFont, &current_font, &current_height);
  }
  int default_font = current_font;
  int default_height = current_height;

  int len = Length(text) / 2; // UTF16 = 2 bytes per char
  const uint16_t* chars = (const uint16_t*)BinaryData(text);
  if ((len > 0) && (chars[len-1] == 0)) len--;

  // Preset the letters array
  auto letters = new std::vector<Letter>(len);
  for (i=0; i<len; ++i) {
    letters->at(i) = { '@', (uint8_t)default_font, (uint8_t)default_height, 10.0f };
  }

  // If there is a styles array, use those values
  int p = 0;
  if (IsArray(styles)) {
    for (i = 0; i < Length(styles); i += 2) {
      Ref lenRef = GetArraySlot(styles, i);
      Ref fontRef = GetArraySlot(styles, i+1);
      set_font(fontRef, &current_font, &current_height);
      int slen = ISINT(lenRef) ? (int)RVALUE(lenRef) : 0;
      for (int j=0; j<slen; j++) {
        if (p >= len) break;
        letters->at(p).font = current_font;
        letters->at(p).size = current_height;
        p++;
      }
      if (p >= len) break;
    }
  }

  // Now copy all letters over and calculate the width of each character
  for (i=0; i<len; ++i) {
    Letter* ll = &letters->at(i);
    uint16_t ucc = chars[i];
    ll->code = ucc;
    uint8_t mrc = unicode_to_macroman(ucc);
    ll->width = pdf_font[ll->font].char_width[mrc] / 72.0f / 14.0f * ll->size;
    //printf("%c %d %d %6.2f\n", mrc, ll.font, ll.size, ll.width);
  }

  std::vector<int>* tabArray = nullptr;
  Ref tabs = GetFrameSlot(text_block, MakeSymbol("tabs"));
  if (IsArray(tabs)) {
    tabArray = new std::vector<int>;
    int n = Length(tabs);
    for (int i=0; i<n; i++) {
      auto t = GetArraySlot(tabs, i);
      if (ISINT(t))
        tabArray->push_back(RefToInt(t));
    }
  }

  auto o = ObjectPtr(text_block);
  o->flags &= ~ kObjReadOnly; // Hack the Object to make it writeable
  // TODO: "_letters" and "_tabs" are never deleted (use large binaries with custom deleter?)
  SetFrameSlot(text_block, MakeSymbol("_letters"), AddressToRef(letters));
  if (tabArray && !tabArray->empty()) {
    SetFrameSlot(text_block, MakeSymbol("_tabs"), AddressToRef(tabArray));
  }
  return 0;
}

int prepareContents(RefArg book) {
  Ref contents_array = GetFrameSlot(book, MakeSymbol("contents"));
  if (!IsArray(contents_array)) {
    fprintf(stderr, "Can't read book, no contents found!\n");
    return 1;
  }
  int ret = 0;
  for (int i=0; i<Length(contents_array); ++i) {
    Ref text_block = GetArraySlot(contents_array, i);
    if (!IsFrame(text_block)) {
      fprintf(stderr, "Can't read contents block %d, expected Frame!\n", i);
    }
    Ref data = GetFrameSlot(text_block, MakeSymbol("data"));
    if (IsString(data)) {
      textBlockToLetters(text_block);
    }
  }
  return ret;
}


void write_letter_widths(FILE* out, const char* fontName) {
  // Pseudocode for macOS (Core Text/Core Graphics)
  CFStringRef cfFontName = CFStringCreateWithCString(kCFAllocatorDefault, fontName, kCFStringEncodingUTF8);
  CTFontRef font = CTFontCreateWithName(cfFontName, 14, NULL);
  std::string name = CTFontGetHumanReadableName(font);
  float ascent = CTFontGetAscent(font);
  float descent = CTFontGetDescent(font);
  float leading = CTFontGetLeading(font);
  fprintf(out, "{\n\"%s\", // %s\n", fontName, name.c_str());
  fprintf(out, "%4d, %4d, %4d, // ascent, descent, line spacing\n",
          static_cast<int>(ascent*72.0f),
          static_cast<int>(descent*72.0f),
          static_cast<int>(leading*72.0f));
  fprintf(out, "{  // [0..255]\n");
  for (int c = 0; c < 256; ++c) {
    // Get the Unicode or glyph for MacRoman code 'c'
    // You must implement MacRomanToUnicode or use an existing lookup table!
    UniChar unicode = macroman_to_unicode(c);
    CGGlyph glyph;
    CTFontGetGlyphsForCharacters(font, &unicode, &glyph, 1);
    CGSize size;
    CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, &glyph, &size, 1);
    // Store width in your array as needed.
    fprintf(out, "%4d, ", static_cast<int>(size.width*72.0f));
    if ((c % 16) == 15) fprintf(out, "\n");
    // printf("Char '%s' is %g units wide\n", unicode_to_utf8(unicode).c_str(), size.width);
  }
  CFRelease(font);
  fprintf(out, "}\n},\n");
}

void write_font_data(const char* filename) {
  FILE* out = fopen(filename, "wb");
  if (!out) {
    fprintf(stderr, "Can't open file %s\n", filename);
    return;
  }
  write_letter_widths(out, "Times-Roman");
  write_letter_widths(out, "Times-Bold");
  write_letter_widths(out, "Times-Italic");
  write_letter_widths(out, "Times-BoldItalic");
  write_letter_widths(out, "Helvetica");
  write_letter_widths(out, "Helvetica-Bold");
  write_letter_widths(out, "Helvetica-Oblique");
  write_letter_widths(out, "Helvetica-BoldOblique");
  write_letter_widths(out, "Courier");
  write_letter_widths(out, "Courier-Bold");
  write_letter_widths(out, "Courier-Oblique");
  write_letter_widths(out, "Courier-BoldOblique");
  write_letter_widths(out, "Symbol");
  write_letter_widths(out, "ZapfDingbats");
  fclose(out);
}

uint8_t current_page_font = 0;

struct Segment {
  std::string text;
  uint8_t font, size;
  float ascent, descent;
  // TODO: leading?
  float left, right;

  void draw(struct pdf_doc *pdf, float xorigin, float ybaseline) {
    // y is negative!
//    pdf_add_rectangle(pdf, nullptr, xorigin+left, ybaseline-descent, right-left, ascent+descent, 1, PDF_RED);
//    pdf_add_line(pdf, nullptr, xorigin+left-4, ybaseline, xorigin+right+4, ybaseline, 1, PDF_BLUE);
    if (!text.empty()) {
      if (font != current_page_font) {
        pdf_set_font(pdf, pdf_font[font].font_name);
        current_page_font = font;
      }
      pdf_add_text(pdf, nullptr, text.c_str(), size, xorigin+left, ybaseline, PDF_BLACK);
//    } else {
//      pdf_add_text(pdf, nullptr, "\\n", size, xorigin+left, ybaseline, PDF_BLACK);
    }
  }
};

struct Line {
  std::vector<struct Segment*> segments;
  float left, top;
  float max_ascent, max_descent;

  void clear() {
    for (auto &v: segments) delete v;
    segments.clear();
    left = top = 0.0f;
    max_ascent = max_descent = 0.0;
  }
  void preset(float line_left, float line_top) {
    clear();
    left = line_left;
    top = line_top;
  }
  float draw(struct pdf_doc *pdf, float xorigin, float yorigin) {
    for (auto &v: segments) v->draw(pdf, xorigin+left, yorigin-top-max_ascent);
    return max_ascent + max_descent;
  }
};


int writeBookToPDF(RefArg book, const std::string &filename)
{
//  write_font_data("/Users/matt/dev/fontdata.txt");
//  return -1;

  if (prepareContents(book)) return 1;

#if 0
  Ref browsers_array = GetFrameSlot(book, MakeSymbol("browsers"));
  if (!IsArray(browsers_array)) {
    return 1;
  }
  int ret = 0;
  for (int i=0; i<Length(browsers_array); ++i) {
    Ref browser = GetArraySlot(browsers_array, i);

    if (IsFrame(browser)) {

      // {data: "Title Page", layout: 2048}
      if (checkForUnknownKeys("part:data:book:style[#]:", browser, known_book_browsers_keys))
        ret = 1;

      Ref r = GetFrameSlot(browser, MakeSymbol("list"));
      if (!ISNIL(r)) {
        //      if (!IsBinary(r)) {
        printf("---> "); PrintObject(r, 0); printf(" <----\n");
        //      }
      }
    } else if (IsInt(browser)) {
      printf("Style: 0x%08x\n", RefToInt(browser));
    } else {
      fprintf(stderr, "Can't read style %d, expected Frame!\n", i);
    }
  }
#endif

#if 0
  Ref rendering_array = GetFrameSlot(book, MakeSymbol("rendering"));
  if (ISNIL(rendering_array)) { return 0; }
  if (!IsArray(rendering_array)) {
    return 1;
  }
  int ret = 0;
  for (int i=0; i<Length(rendering_array); ++i) {
    Ref rendering = GetArraySlot(rendering_array, i);

    if (IsFrame(rendering)) {

      // {data: "Title Page", layout: 2048}
      if (checkForUnknownKeys("part:data:book:style[#]:", rendering, known_book_rendering_keys))
        ret = 1;
//      "nColumns",
//      "column",
//      "flags",
//      "header",
//      "scripts",
//      "runTop",
//      "runBottom",

      Ref r = GetFrameSlot(rendering, MakeSymbol("pages"));
      if (!ISNIL(r)) {
        //      if (!IsBinary(r)) {
        printf("---> "); PrintObject(r, 2); printf(" <----\n");
        //      }
      }
    } else if (IsInt(rendering)) {
      printf("Style: 0x%08x\n", RefToInt(rendering));
    } else {
      fprintf(stderr, "Can't read style %d, expected Frame!\n", i);
    }
  }

  return ret;
#endif

  // Primitive approach, let's just write the contents.

  // part [
  //   type: "book"
  //   data {
  //     book {
  //       version, isbn, title, shortTitle, copyright, author, publisher,
  //       data { } // arbitrary user data?
  //       contents [
  //         {
  //           data: "text",
  //           styles: [ // optional
  //             char count, styleRef, repeat
  //           ],
  //           viewFont: ref_to_style
  //           tabs: [0, 36, 72, ...],
  //           layout: 2048,
  //           flags: 16,
  //           viewJustify: 2
  //         }
  //       ]
  //       styles [
  //         {family: 'geneva, face: 0, size: 9}, ...
  //       ]
  //       hints [
  //         nil, true, binary // ?????
  // According to AI: The "hints" array is used for text search optimization. It contains bitmap-based fingerprints of the text content that allow for fast text searching without having to scan through the entire text.
  /*
Technical Implementation (see Hints.h and Hints.cc):
From the code in Hints.h and Hints.cc, I can see that:

TextHint Structure: Each hint is a 64-bit bitmap (two 32-bit integers)
Hash-based: Words in the text are processed through character quadgrams (4-character sequences) that are hashed into bit positions
Bloom Filter Pattern: This implements a type of Bloom filter - a probabilistic data structure that can tell you "definitely not present" or "possibly present"
How It Works
When text is stored, the system generates hints by:

Taking 4-character sliding windows from each word
Hashing these quadgrams to bit positions (0-63)
Setting the corresponding bits in the 64-bit hint bitmap
When searching, the system:

Generates the same hint pattern for the search term
Compares it against stored hints using bitwise AND operations
If no bits match, the word is definitely not in that text block
If bits match, the word might be in that text block (requiring full search)
  */

  //       ]
  //       browsers [
  //         {
  //           name: "Contents"
  //           list: [
  //             {item: ->contents}
  //             {level: 2, item: ->contents}
  //           ]
  //       ]
  //       Templates: [
  //         // List for single column or multi column layout
  //         {nColumns: 1, column: [{width: 12, type: 0}]}, etc.
  //       ]
  //       rendering: [
  //         // We can offer multiple page formats which the user can choose and
  //         // which will change paging
  //         { pageSize: { }
  //           contents: [ [ 1, 2, 3, 5, ...] ]
  //           pages [
  //             { template: ->template
  //               blocks: [
  //                 { bounds: { }, item: ->Item, dataOffset, dataLen }
  //               ]
  //       ]


  // Our central concern is the "rendering" slot which lists all pages.
  Ref rendering_array = GetFrameSlot(book, MakeSymbol("rendering"));
  if (ISNIL(rendering_array)) { return 0; }
  if (!IsArray(rendering_array)) {
    return 1;
  }
  int ret = 0;

  // "rendering" can contain more than one book format. For now, we choose
  // the first one we find.
  Ref rendering = GetArraySlot(rendering_array, 0);
  if (ISNIL(rendering)) { return 0; }
  float page_width = PDF_A4_WIDTH / 2.0f; // default to DIN A5 portrait
  float page_height = PDF_A4_WIDTH;
  int page_top = 0;
  int page_left = 0;
  int page_bottom = page_height;
  int page_right = page_width;
  Ref page_size = GetFrameSlot(rendering, MakeSymbol("pageSize"));
  if (IsFrame(page_size)) {
    Ref t = GetFrameSlot(page_size, MakeSymbol("top"));
    if (ISINT(t)) page_top = (int)RVALUE(t);
    t = GetFrameSlot(page_size, MakeSymbol("left"));
    if (ISINT(t)) page_left = (int)RVALUE(t);
    t = GetFrameSlot(page_size, MakeSymbol("bottom"));
    if (ISINT(t)) page_bottom = (int)RVALUE(t);
    t = GetFrameSlot(page_size, MakeSymbol("right"));
    if (ISINT(t)) page_right = (int)RVALUE(t);
    page_width = page_right - page_left;
    page_height = page_bottom - page_top;
  }

  struct pdf_info info = {
    .creator = "My software",
    .producer = "My software",
    .title = "My document",
    .author = "My name",
    .subject = "My subject",
    .date = "Today"
  };
  // See PDF_MM_TO_POINT()
  struct pdf_doc *pdf = pdf_create(page_width, page_height, &info);
  pdf_set_font(pdf, "Times-Roman");
  current_page_font = 0;

//  pdf_append_page(pdf);
  // pdf_add_text(pdf, page, text, size, xoff, yoff, colour)
  // pdf_add_text_spacing(... , spacing, angle)
  // pdf_add_text_wrap(...)
//  pdf_add_text(pdf, NULL, "This is text", 12, 50, 20, PDF_BLACK);
//  pdf_add_text(pdf, NULL, "This is a second text", 12, 50, 40, PDF_BLACK);
//  pdf_add_line(pdf, NULL, 50, 24, 150, 24, 3, PDF_BLACK);


  Ref pages = GetFrameSlot(rendering, MakeSymbol("pages"));
  if (!IsArray(pages)) {
    fprintf(stderr, "No pages found!\n");
    return 1;
  }
  for (int i=0; i<Length(pages); ++i) {
    Ref page = GetArraySlot(pages, i);
    pdf_append_page(pdf);
    if (IsFrame(page)) {
      Ref blocks = GetFrameSlot(page, MakeSymbol("blocks"));
      if (IsArray(blocks)) {
        for (int j=0; j<Length(blocks); ++j) {
          Ref block = GetArraySlot(blocks, j);
          Ref bounds = GetFrameSlot(block, MakeSymbol("bounds"));
          int t = 0, l = 0, b = 0, r = 0;
          if (IsArray(bounds)) {
            Ref ref = GetArraySlot(bounds, 0);
            if (ISINT(ref)) l = (int)RVALUE(ref);
            ref = GetArraySlot(bounds, 1);
            if (ISINT(ref)) t = (int)RVALUE(ref);
            ref = GetArraySlot(bounds, 2);
            if (ISINT(ref)) r = (int)RVALUE(ref); else r = l;
            ref = GetArraySlot(bounds, 3);
            if (ISINT(ref)) b = (int)RVALUE(ref); else b = t;
          }
//          pdf_add_rectangle(pdf, nullptr, l, page_height-b, r-l, b-t, 0.5, 0xaaaaaa);
          // item
          Ref it = GetFrameSlot(block, MakeSymbol("item"));
          if (IsFrame(it)) {
            // item.data (and much more)
            Ref data = GetFrameSlot(it, MakeSymbol("data"));
            if (IsString(data)) {
              int text_len = Length(data) / 2;
              int data_offset = 0;
              int data_len = text_len;
              Ref ref = GetFrameSlot(block, MakeSymbol("dataOffset"));
              if (ISINT(ref)) data_offset = (int)RVALUE(ref);
              ref = GetFrameSlot(block, MakeSymbol("dataLen"));
              if (ISINT(ref)) data_len = (int)RVALUE(ref);
              if (data_offset > text_len) data_offset = text_len;
              if (data_offset + data_len > text_len) data_len = text_len - data_offset;

#if 0
              char *text = BinaryData(ASCIIString(Substring(data, data_offset, data_len)));
              float font_height = 12;
              pdf_add_text_wrap(pdf, nullptr, text, font_height,
                                l, page_height-t-font_height*0.8, 0.0, PDF_BLACK, r-l,
                                PDF_ALIGN_LEFT, nullptr);
#else
              auto lettersRef = GetFrameSlot(it, MakeSymbol("_letters"));
              auto letters = static_cast<std::vector<Letter>*>(RefToAddress(lettersRef));

              auto tabsRef = GetFrameSlot(it, MakeSymbol("_tabs"));
              std::vector<int>* tabs = nullptr;
              if (NOTNIL(tabsRef)) {
                tabs = static_cast<std::vector<int>*>(RefToAddress(tabsRef));
              }

              int len = (int)letters->size();
              if (data_offset > len) data_offset = len;
              if (data_offset + data_len > len) data_len = len - data_offset;
#if 1
              struct Line line;
              int data_start = data_offset;
              int data_end = data_offset + data_len;
              float yy = 0.0f;
              // loop through as many lines as it takes
//              printf("Create page\n");
              while (data_start < data_end) {
                bool line_done = false;
                float xx = 0.0f;
                line.preset(xx, yy);
//                printf("Starting line at %g, %g\n", xx, yy);
                // create all segments needed to draw a line
                while ((data_start < data_end) && !line_done) {
                  struct Segment* seg = new Segment;
                  int seg_start = data_start;
                  int seg_end = seg_start;
                  int seg_break = seg_start;
                  int seg_break_right = xx;
                  int seg_next = seg_end;
                  Letter* ll = &letters->at(seg_start);
                  seg->size = ll->size;
                  seg->font = ll->font;
                  const pdf_font_data* ft = pdf_font + ll->font;
                  seg->ascent = ft->ascent / 72.0f / 14.0f * ll->size;
                  seg->descent = ft->descent / 72.0f / 14.0f * ll->size;;
                  seg->left = seg->right = xx;
                  // find the longest sequence that is not interrupter by tabs, line endings, or line overflow
                  while (seg_end < data_end) {
                    ll = &letters->at(seg_end);
                    if ((ll->code == '\r') || (ll->code == '\n')) {
                      line_done = true;
                      seg_next = seg_end + 1;
                      break;
                    }
                    if ((ll->font != seg->font) || (ll->size != seg->size)) {
                      xx = seg->right;
                      break;
                    }
                    if (ll->code == ' ') {
                      seg_break = seg_end;
                      seg_break_right = seg->right;
                    }
                    // TODO: tabs
                    if (ll->code == '\t') {
                      if (tabs) {
                        int ix = 0, n = (int)tabs->size();
                        for (ix = 0; ix < n; ++ix) {
                          if ((*tabs)[ix] > seg->right) break;
                        }
                        int se = seg_end+1;
                        while ((se < data_end) && (letters->at(se).code == '\t')) {
                          ix++;
                          se++;
                        }
                        if (ix >= n) {
                          xx = r-l;
                        } else {
                          xx = (*tabs)[ix];
                        }
                        seg_next = se;
                      } else {
                        // default tab stop of 0.5"
                        int t = seg->right + 37;
                        int d = t/36;
                        xx = d * 36;
                        int se = seg_end+1;
                        while ((se < data_end) && (letters->at(se).code == '\t')) {
                          xx += 36;
                          se++;
                        }
                        seg_next = se;
                      }
                      if (xx >= r-l) {
                        line_done = true;
                        xx = 0;
                      }
                      break;
                    }
                    if (ll->code >= ' ') {
                      seg->right += ft->char_width[unicode_to_macroman(ll->code)] / 72.0f / 14.0f * ll->size;
                      if (seg->right > r-l) {
                        if ((seg_break == data_start) && (xx == 0)) {
                          // if there was no word break yet, break at the character to avoid an endless loop
                          seg_next = seg_end;
                        } else {
                          // use a previous word break to break the line there
                          seg_end = seg_break;
                          seg->right = seg_break_right;
                          seg_next = seg_end + 1; // assumes this was a word break and we skip the space between words
                        }
                        line_done = true;
                        break;
                      }
                    }
                    seg_end++;
                    seg_next = seg_end;
                  }
                  // copy the characters into the segment as utf8
                  for (int ci=seg_start; ci < seg_end; ci++) {
                    ll = &letters->at(ci);
                    seg->text += unicode_to_utf8(ll->code);
                  }
                  // add the segment to the line
                  line.segments.push_back(seg);
                  line.max_ascent = std::max(line.max_ascent, seg->ascent);
                  line.max_descent = std::max(line.max_descent, seg->descent);
//                  printf("Adding segment: \"%s\"\n", seg->text.c_str());
                  data_start = seg_start = seg_next;
                }
                line.draw(pdf, l, page_height-t);
                yy += 1.3f * (line.max_ascent + line.max_descent);
              }
#endif
#if 0
              int i = 0;
              int xoff = l;
              int yoff = t + 10;
              std::string segText;
              float segSize = 12.0f;
              for (i=data_offset; i<data_offset+data_len; i++) {
                Letter* ll = &letters->at(i);
                if ((ll.code == '\r') || (ll.code == '\n')) {
                  pdf_add_text(pdf, nullptr, segText.c_str(), segSize,
                               xoff, page_height-yoff, PDF_BLACK);
                  yoff += 12;
                  segText.clear();
                } else {
                  segText += unicode_to_utf8(ll.code);
                }
              }
#endif
#endif
              // Block layout:
              // Box, alignment, template (dual column, triple column)
              // Text, dataOffset, dataLen
              // text style, text segment styles (array)
              // tabs
              // special characters: tab, CR/NL, space, soft hyphen?
            }
          }
        }
      }
    }
  }

  pdf_save(pdf, filename.c_str());
  pdf_destroy(pdf);
  return 0;

}

