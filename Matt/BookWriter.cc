//
//  BookWriter.cc
//  Newton
//
//  Created by Matthias Melcher on 25.02.26.
//


  // The Book Package format is not too difficult to understand. "Contents"
  // holds all text and image data. "Rendering" lays out that content on pages.
  // "Styles" describes fonts and text styles. "Hints" is used for text search
  // optimization. "Browsers" are used for a clcikable table of contents, and
  // "Templates" can generate multi-colun page layouts.

  // Note: packages can hold book-like contents without actually being of type
  // "book". In contrast, "Books" can contain NewtonScript objects and views and
  // receive messages. They can be as complex as a full Newton application.

  // Note: the "Book" format uses Classic Mac PICT format V1 and V2 for images
  // and graphics. The PICT format itself is highly complex and can contain
  // vector graphics and text layouts besides pixel maps. It is not within
  // scope to implement a full PICT parser, but we may at some point support
  // a subset of the PICT format.

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
  //         nil, true, binary
  //           The "hints" array is used for text search optimization. It
  //           contains bitmap-based fingerprints of the text content that
  //           allow for fast text searching without having to scan through
  //           the entire text (see Hints.h and Hints.cc).
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

  // find unna/books/ -name "*.pkg"
  // Not all of the packages are actually books. Not all packages can be read by
  // the current package parser.


#include "BookWriter.h"
#include "Symbols.h"
#include "Frames/Iterators.h"
#include "Matt/ObjectPrinter.h"
#include "Matt/PDFGen/pdfgen.h"

#include <string.h>
#include <arpa/inet.h>

#include <vector>


// The following code ist only used once to generate the font data tables in pdfgen.c
// Call with: write_font_data("/Users/matt/dev/fontdata.txt");
#if 0
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
#endif

extern void PrintObjectAux(Ref obj, int indent, int depth);

extern std::string currentFileName;
extern int writeBookToPDF(RefArg book, const std::string &filename);

constexpr uint8_t kNoBreak = 0;
constexpr uint8_t kLineBreak = 1;
constexpr uint8_t kWordBreak = 2;
constexpr uint8_t kTabBreak = 3;

constexpr Ref test = MakeSymbol("Test");

uint8_t current_page_font = 0;

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
  "data",         // string (text) or binary 'image (PICT resource?) or 'picture or frame with NS bitmap image (b/w, gray, color) or NS forms
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
  nullptr
};

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
 Convert a Newton Unicode character into MacRoman encoding.
 Unsupported characters return 255.
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

/**
 Convert a Unicode code point to UTF-8 encoding.
 This is a simple implementation that handles code points in the Basic
 Multilingual Plane (BMP) up to U+FFFF.
 */
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

/**
 Write a book package to a PDF file.
 Returns 0 on success, 1 on failure.
 The package must be of type "book" and follow the expected structure,
 otherwise an error is printed and 1 is returned.
 */
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


/**
 Convert an integrer of frame based style information into a font index and a font height.
 */
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

struct PictRect {
  uint16_t top, left, bottom, right;
  void read(uint16_t* d) {
    top = htons(d[0]);
    left = htons(d[1]);
    bottom = htons(d[2]);
    right = htons(d[3]);
  }
};

struct PictLoc {
  uint16_t v, h;
  void read(uint16_t* d) {
    v = htons(d[0]);
    h = htons(d[1]);
  }
};


//int pictToImage(RefArg text_block, RefArg data) {
int pictToPdf(struct pdf_doc *pdf, const PictRect& br, RefArg block, RefArg data)
{
  pdf_add_rectangle(pdf, nullptr, br.left, br.top, br.right-br.left, br.bottom-br.top, 1.0f, PDF_BLACK);
  return 0;
#if 0
  static int picnum = 0;
  char buf[255];
  sprintf(buf, "/Users/matt/dev/newton-framework/p%03d.pict", picnum++);

  // Data is a binary block that contains an image in PICT1 or PICT2 format
  uint8_t* pict = (uint8_t*)BinaryData(data);
  uint16_t* pictw = (uint16_t*)BinaryData(data);
  int pict_size = Length(data);

  FILE *f = fopen(buf, "wb");
  fwrite(pict, pict_size, 1, f);
  fclose(f);

  PictRect bounds;
  bounds.read(pictw+1);
  PictRect clip = bounds;
  PictRect r = bounds;
  // FIXME: use pnLoc and txLoc to store line drawing position and text position
  PictLoc pnLoc { 0, 0 };   // Pen Location
  PictLoc pnLoc2 { 0, 0 };  // Secondary Pen Location
  PictLoc txLoc { 0, 0 };   // Text Location
  int8_t dh, dv;
  uint8_t udh, udv;
  uint16_t txSize = 14;

  if ((pict[11]==0x11) && (pict[12]==0x01)) { // 0x00 0x11 0x01
    fprintf(stderr, "Data is in PICT format version 1\n");
  } else if ((htons(pictw[5])==0x0011) && (htons(pictw[6])==0x02ff)) { // 0x0011 0x02ff
    fprintf(stderr, "Data is in PICT format version 2\n");
    // $0C00
    // 24 byte header subversion, reserved, hres(2), vres(2), src_rect(4), reserved
    int ix = 7+12;
    while (ix < pict_size/2) {
      uint16_t op = htons(pictw[ix]);
      uint16_t sz = 0;
      fprintf(stderr, "Op: 0x%04x: ", op);
      switch (op) {
        case 0x0000: fprintf(stderr, "nop\n"); ix++; break; // nop
        case 0x0001: // clipping region
          fprintf(stderr, "clip region ");
          sz = htons(pictw[ix+1]);
          r.read(pictw+ix+2);
          fprintf(stderr, " @%d, %d, w:%d, h:%d\n", r.left, r.top, r.right-r.left, r.bottom-r.top);
          ix += 1+sz/2;
          break;
        case 0x0003: fprintf(stderr, "TxFont\n"); ix += 2; break; // TxFont, 2 bytes
        case 0x0004: fprintf(stderr, "TxFace\n"); ix += 2; break; // TxFace, 1 bytes (?!)
        case 0x0007: fprintf(stderr, "PenSize\n"); ix += 3; break; // Pen Size, 4 bytes
        case 0x0009: fprintf(stderr, "PenPattern\n"); ix += 5; break; // Pen Pattern, 8 bytes
        case 0x000a: fprintf(stderr, "FillPattern\n"); ix += 5; break; // Fill Pattern, 8 bytes
        case 0x000d: fprintf(stderr, "TextSize\n");
          txSize = htons(pictw[ix+1]);
          ix += 2; break; // Text Size, 2 bytes
        case 0x0015: fprintf(stderr, "PnLocHFrac\n");
          // actual_h = pnLoc.h + (PnLocHFrac / 65536.0)
          ix += 2; break; // PnLocHFrac, Fractional Pen Position, 2 bytes
          // case 0x001e: // DefHilite  Use default highlight color; no data; set highlight to default (from low memory)
        case 0x0022: fprintf(stderr, "ShortLine: "); // Short Line: 00.22.vv.vv.hh.hh.dh.dv
          pnLoc2.read(pictw+ix+1);
          dh = (int8_t)pict[ix*2+6];
          dv = (int8_t)pict[ix*2+7];
          fprintf(stderr, "%d %d rel %d %d (%d %d)\n", pnLoc2.h, pnLoc2.v, dh, dv, pnLoc.h, pnLoc.v);
          pnLoc.h = pnLoc2.h + dh;
          pnLoc.v = pnLoc2.v + dv;
          pdf_add_line(pdf, nullptr, br.left+pnLoc2.h, br.top-pnLoc2.v, br.left+pnLoc.h, br.top-pnLoc.v, 1.0f, PDF_RED);
          ix += 4;
          break;
        case 0x0023: fprintf(stderr, "ShortLineFrom "); // Short Line From 00.23.dh.dv
          dh = (int8_t)pict[ix*2+2];
          dv = (int8_t)pict[ix*2+3];
          pnLoc2 = pnLoc;
          pnLoc.h += dh;
          pnLoc.v += dv;
          fprintf(stderr, "%d %d rel %d %d (%d %d)\n", pnLoc2.h, pnLoc2.v, dh, dv, pnLoc.h, pnLoc.v);
          pdf_add_line(pdf, nullptr, br.left+pnLoc2.h, br.top-pnLoc2.v, br.left+pnLoc.h, br.top-pnLoc.v, 1.0f, PDF_BLUE);
          ix += 2;
          break;
        case 0x0028: fprintf(stderr, "LongText "); // long text (point, count, text)
          txLoc.read(pictw+ix+1);
          sz = pict[ix*2+6];
          // TODO: rewrite this to use MacRoman encoding
          memcpy(buf, pict+ix*2+7, sz); buf[sz] = 0;
          fprintf(stderr, " @%d, %d: \"%s\"(%d)\n", txLoc.h, txLoc.v, buf, sz);
          pdf_add_text(pdf, nullptr, buf, txSize, br.left+txLoc.h, br.top-txLoc.v, PDF_BLACK);
          ix += 4 + sz/2;
          break;
        case 0x002a: { fprintf(stderr, "DVText "); //  DVText  dv (0..255), count (0..255), text
          uint8_t dv = pict[ix*2+2];
          sz = pict[ix*2+3];
          txLoc.v += dv;
          memcpy(buf, pict+ix*2+4, sz); buf[sz] = 0;
          fprintf(stderr, " @%d, %d: \"%s\"(%d)\n", txLoc.h, txLoc.v, buf, sz);
          pdf_add_text(pdf, nullptr, buf, txSize, br.left+txLoc.h, br.top-txLoc.v, PDF_BLACK);
          ix += 2 + (sz+1)/2;
          } break;
        case 0x002b: { fprintf(stderr, "DHDVText "); // DHDVText 00.2b.dh.dv.sz.tt.tt.tt...
          uint8_t dh = pict[ix*2+2];
          uint8_t dv = pict[ix*2+3];
          sz = pict[ix*2+4];
          txLoc.h += dh;
          txLoc.v += dv;
          // TODO: rewrite this to use MacRoman encoding
          memcpy(buf, pict+ix*2+5, sz); buf[sz] = 0;
          fprintf(stderr, " @%d, %d: \"%s\"(%d)\n", txLoc.h, txLoc.v, buf, sz);
          pdf_add_text(pdf, nullptr, buf, txSize, br.left+txLoc.h, br.top-txLoc.v, PDF_BLACK);
          ix += 3 + sz/2;
          } break;
        case 0x002c: fprintf(stderr, "SetFont\n"); sz = htons(pictw[ix+1]); ix += 3+sz/2; break; // Set Font, 5 + name length
        case 0x002e: fprintf(stderr, "GlyphStats\n"); sz = htons(pictw[ix+1]); ix += 2+sz/2; break; // Glyph Stats, 8 bytes
        case 0x0030: fprintf(stderr, "FrameRect ");
          r.read(pictw+1);
          fprintf(stderr, " @%d, %d, w:%d, h:%d\n", r.left, r.top, r.right-r.left, r.bottom-r.top);
          pdf_add_rectangle(pdf, nullptr, br.left+r.left, br.top-r.bottom, r.right-r.left, r.bottom-r.top, 1.0f, PDF_BLACK);
          ix += 5; break; // Frame Rect, 8 bytes
        case 0x0031: fprintf(stderr, "FillRect ");
          r.read(pictw+1);
          fprintf(stderr, " @%d, %d, w:%d, h:%d\n", r.left, r.top, r.right-r.left, r.bottom-r.top);
          pdf_add_filled_rectangle(pdf, nullptr, br.left+r.left, br.top-r.bottom, r.right-r.left, r.bottom-r.top, 1.0f, 0xcccccc, PDF_BLACK);
          ix += 5; break; // Fill Rect, 8 bytes
        case 0x0081: fprintf(stderr, "paintRegion "); // paintRegion
          sz = htons(pictw[ix+1]);
          r.read(pictw+ix+2);
          fprintf(stderr, " @%d, %d, w:%d, h:%d\n", r.left, r.top, r.right-r.left, r.bottom-r.top);
          pdf_add_rectangle(pdf, nullptr, br.left+r.left, br.top-r.bottom, r.right-r.left, r.bottom-r.top, 1.0f, PDF_GREEN);
          ix += 1+sz/2;
          break;
        case 0x0084: fprintf(stderr, "fillRegion "); // fillRegion
          sz = htons(pictw[ix+1]);
          r.read(pictw+ix+2);
          fprintf(stderr, " @%d, %d, w:%d, h:%d\n", r.left, r.top, r.right-r.left, r.bottom-r.top);
          pdf_add_rectangle(pdf, nullptr, br.left+r.left, br.top-r.bottom, r.right-r.left, r.bottom-r.top, 1.0f, 0x888800);
          ix += 1+sz/2;
          break;
        case 0x00a1: fprintf(stderr, "LongComment %d\n", htons(pictw[ix+1])); sz = htons(pictw[ix+2]); ix += 3+sz/2; break; // long comment (kind, size, data...)
        case 0x00a0: fprintf(stderr, "ShortComment %d\n", htons(pictw[ix+1])); ix+=2; break; // short comment
        case 0x00ff: fprintf(stderr, "END\n"); // end of format
          ix = 0x7fffffff; break;
        default: fprintf(stderr, "Unknown op code\n");
          ix = 0x7fffffff; break;
      }
    }
  } else {
    fprintf(stderr, "Not a supported PICT format (0x%02x%02x)\n", pict[11], pict[12]);
    return 1;
  }
  return 0;
#endif
}


/**
 A segements is a sequence of characters with the same font and size, that can be drawn together.
 Text is split into segments when font size or style changes, orwhen a
 line break or tab is encountered.
 */
struct Segment {
  std::string text;
  uint8_t font, size;
  float ascent, descent;
  // TODO: "leading" is the distance between lines. We just make an assumtion
  // somewhere in the code below (currently 1.2 * (ascent + descent) ).
  float left, right;

  void draw(struct pdf_doc *pdf, float xorigin, float ybaseline) {
    if (!text.empty()) {
      if (font != current_page_font) {
        pdf_set_font(pdf, pdf_font[font].font_name);
        current_page_font = font;
      }
      pdf_add_text(pdf, nullptr, text.c_str(), size, xorigin+left, ybaseline, PDF_BLACK);
    }
  }
};

/**
 A line is a sequence of segments that have the same vertical baseline position.
 Lines are split when a newline is encountered, or when the line overflows the
 available width.
 */
struct Line {
  std::vector<struct Segment*> segments;
  float left, top, right;
  float max_ascent, max_descent;

  void clear() {
    for (auto &v: segments) delete v;
    segments.clear();
    left = top = right = 0.0f;
    max_ascent = max_descent = 0.0;
  }
  void preset(float line_left, float line_top) {
    clear();
    left = right = line_left;
    top = line_top;
  }
  float draw(struct pdf_doc *pdf, float xorigin, float yorigin) {
    for (auto &v: segments) v->draw(pdf, xorigin+left, yorigin-top-max_ascent);
    return max_ascent + max_descent;
  }
};

/**
 A text block is rendered by layoing out a sequnce of characters into lines and
 segments, and then drawing those segments at the right position on the page.
 */
void renderTextBlockToPDF(struct pdf_doc* pdf, RefArg block, RefArg item, RefArg data, int l, int t, int b, int r, float page_height) {
  int text_len = Length(data) / 2;
  int data_offset = 0;
  int data_len = text_len;
  Ref ref = GetFrameSlot(block, MakeSymbol("dataOffset"));
  if (ISINT(ref)) data_offset = (int)RVALUE(ref);
  ref = GetFrameSlot(block, MakeSymbol("dataLen"));
  if (ISINT(ref)) data_len = (int)RVALUE(ref);
  if (data_offset > text_len) data_offset = text_len;
  if (data_offset + data_len > text_len) data_len = text_len - data_offset;

  auto lettersRef = GetFrameSlot(item, MakeSymbol("_letters"));
  auto letters = static_cast<std::vector<Letter>*>(RefToAddress(lettersRef));

  auto tabsRef = GetFrameSlot(item, MakeSymbol("_tabs"));
  std::vector<int>* tabs = nullptr;
  if (NOTNIL(tabsRef)) {
    tabs = static_cast<std::vector<int>*>(RefToAddress(tabsRef));
  }

  int len = (int)letters->size();
  if (data_offset > len) data_offset = len;
  if (data_offset + data_len > len) data_len = len - data_offset;

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
      line.right = std::max(line.right, seg->right);
//                  printf("Adding segment: \"%s\"\n", seg->text.c_str());
      data_start = seg_start = seg_next;
    }
    line.draw(pdf, l, page_height-t);
    yy += 1.2f * (line.max_ascent + line.max_descent);
  }
  // Block layout:
  // Box, alignment, template (dual column, triple column)
  // text style, text segment styles (array)
}

/**
  Convert the UTF16 text and styles in a text block into an array of Letter
  structs that can be easily rendered.
  One text block can be rendered multiple times with different dataOffset and
  dataLen. This function prepares the text once for easy C++ style access by
  the page and block rendering code.
 */
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

  // If there is a tabs array, copy all tabs into a C++ vector.
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
    } else if (ISNIL(data)) {
//      fprintf(stderr, "Contents block without data entry\n");
//      ret = 1;
    } else if ((IsBinary(data)) && (IsInstance(data, MakeSymbol("picture")))) {
//      fprintf(stderr, "PICT image format\n");
//      pictToImage(text_block, data);
//      ret = 1;
    } else if (IsFrame(data)) {
      if (FrameHasSlot(data, MakeSymbol("colordata"))) {
        // bounds{}, colordata{ colortable, bitdepth, cbits, bits }
        // '/Users/matt/Azureus/unna2/books/Maps/venice2.pkg'
        // Huge scrollable grayscale(?) maps
//        fprintf(stderr, "uncompressed Newton image format\n");
//        printf("---> "); PrintObjectAux(data, 0, 0); printf(" <----\n");
//        ret = 1;
      } else if (FrameHasSlot(data, MakeSymbol("bits"))) {
        // Faulty: bounds, bits
        // only in '/Users/matt/Azureus/unna2/books/Travel_Geography/Citibank Locations/NYCATMS.pkg'
//        fprintf(stderr, "Simple Newton Image\n");
//        printf("---> "); PrintObject(data, 0); printf(" <----\n");
//        ret = 1;
      } else if (FrameHasSlot(data, MakeSymbol("_proto"))) {
//        fprintf(stderr, "NewtonScript form or rect or other proto\n");
//        ret = 1;
      } else if (FrameHasSlot(data, MakeSymbol("stepChildren"))) {
//        fprintf(stderr, "NewtonScript form\n");
//        ret = 1;
      } else {
        // '/Users/matt/Azureus/unna2/books/Computers/JargonFile/jargon3.pkg':
        // {_proto: @160, viewFormat: vfFillDkGray} (a black rectangle)
//        fprintf(stderr, "Contents block with unknown data FRAME\n");
//        printf("---> "); PrintObject(data, 0); printf(" <----\n");
//        ret = 1;
      }
    } else {
//      fprintf(stderr, "Contents block with unknown data entry\n");
//      printf("---> "); PrintObject(data, 0); printf(" <----\n");
//      ret = 1;
    }
  }
  return ret;
}

/**
 Create a PDF file by rendering all pages in the "rendering" slot.
 */
int writeBookToPDF(RefArg book, const std::string &filename)
{
  int ret = 0;

//  if (prepareContents(book)) return 1;
  if (prepareContents(book)) ret = 1;

  // Our central concern is the "rendering" slot which lists all pages.
  Ref rendering_array = GetFrameSlot(book, MakeSymbol("rendering"));
  if (ISNIL(rendering_array)) { return 0; }
  if (!IsArray(rendering_array)) {
    return 1;
  }

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
    .creator = "My software",   // TODO: use real metadata from the book
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

  // The "rendering" slot contains an array of page formats. There can be
  // different rendering formats for different page sizes. For now, we just
  // take the first one we find and ignore the rest.
  Ref pages = GetFrameSlot(rendering, MakeSymbol("pages"));
  if (!IsArray(pages)) {
    fprintf(stderr, "No pages found!\n");
    return 1;
  }
  // One rendering format has basic page size information and holds an array
  // of pages. Let's loop through all pages.
  for (int i=0; i<Length(pages); ++i) {
    Ref page = GetArraySlot(pages, i);
    pdf_append_page(pdf);
    // A page is described using a Frame, usually containing a template for
    // the page layout and an array of text and graphics blocks.
    if (IsFrame(page)) {
      Ref blocks = GetFrameSlot(page, MakeSymbol("blocks"));
      if (IsArray(blocks)) {
        // Loop through all blocks on this page.
        for (int j=0; j<Length(blocks); ++j) {
          Ref block = GetArraySlot(blocks, j);
          // The bounds entry describes the block location and size on the page.
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
          // The "Item" entry is a reference to the data within the block. Note
          // that multiple blocks can reference the same data. "dataOffset" and
          // "dataLen" are used for text to specify only a subset of the data
          // to be rendered in this block.
          Ref item = GetFrameSlot(block, MakeSymbol("item"));
          if (IsFrame(item)) {
            // item.data (and much more)
            Ref data = GetFrameSlot(item, MakeSymbol("data"));
            if (IsString(data)) {
              renderTextBlockToPDF(pdf, block, item, data, l, t, b, r, page_height);
            } else if ((IsBinary(data)) && (IsInstance(data, MakeSymbol("picture")))) {
              pictToPdf(pdf, PictRect(page_height-t, l, page_height-b, r), item, data);
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

