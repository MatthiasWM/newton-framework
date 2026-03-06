//
//  BookWriter.cc
//  Newton
//
//  Created by Matthias Melcher on 25.02.26.
//

// find /Users/matt/Azureus/unna2/books/ -name "*.pkg" -exec ./build/Xcode/Debug/newtc -pkg \{\} -ohtml "x" \;

// Not a book:
// '/Users/matt/Azureus/unna2/books/Maps/BostonSubway-TMap-12.pkg'

// Package reader crashes:
// '/Users/matt/Azureus/unna2/books/Maps/SMRTmap.pkg'
// '/Users/matt/Azureus/unna2/books/Newton/newtonmail.htm.pkg'


#include "BookWriter.h"

#include <string.h>
#include <arpa/inet.h>

#include "Symbols.h"
#include "Frames/Iterators.h"
#include "Matt/ObjectPrinter.h"
#include "Matt/PDFGen/pdfgen.h"

extern std::string currentFileName;
extern int writeBookToHTML(RefArg book, const std::string &filename);

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

// Contents
const char* known_book_contents_keys[] = {
  "data",         // string (text) or binary 'image (PICT resource?) or frame with NS bitmap image (b/w, gray, color) or NS forms
  "layout",       // integer, usually only one bit set
  "styles",       // array of pairs: first, number of chars, then style, ether an integer (bits), or a ref to a style (see version?)
  "viewJustify",  // integer, observing 1 or 2
  "viewFont",     // integer of style frame ref (see styles)
  "flags",        // integer flag bits, observed 4, 8, 16, 32
  "tabs",         // array of integers, tab position in pixels? Can be an empty array
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
  "family",       // Symbol:  espy, geneva, newyork,
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

int writePackageBookToHTML(RefArg pkg, const std::string &filename) {
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

  if (writeBookToHTML(book, filename))
    ret = 1;
  return ret;
}

int writeBookToHTML(RefArg book, const std::string &filename)
{
#if 0
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
    // {data: "Title Page", layout: 2048}
    if (checkForUnknownKeys("part:data:book:contents[#]:", text_block, known_book_contents_keys))
      ret = 1;

    Ref r = GetFrameSlot(text_block, MakeSymbol("FN"));
    if (!ISNIL(r)) {
//      if (!IsBinary(r)) {
          printf("---> "); PrintObject(r, 0); printf(" <----\n");
//      }
    }

    Ref data = GetFrameSlot(text_block, MakeSymbol("data"));
    if (IsString(data)) {
      //printf("%s\n", BinaryData(ASCIIString(data)));
    }
  }
#endif

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
  /*
  "Times-Roman",
  "Times-Bold",
  "Times-Italic",
  "Times-BoldItalic",
  "Helvetica",
  "Helvetica-Bold",
  "Helvetica-Oblique",
  "Helvetica-BoldOblique",
  "Courier",
  "Courier-Bold",
  "Courier-Oblique",
  "Courier-BoldOblique",
  "Symbol",
  "ZapfDingbats",
  */
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
    if (IsFrame(page)) {
      pdf_append_page(pdf);
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
          pdf_add_rectangle(pdf, nullptr, l, page_height-b, r-l, b-t, 0.5, PDF_BLACK);
          // item
          Ref it = GetFrameSlot(block, MakeSymbol("item"));
          if (IsFrame(it)) {
            // item.data (and much more)
            Ref data = GetFrameSlot(it, MakeSymbol("data"));
            if (IsString(data)) {
              char *text = BinaryData(ASCIIString(data));
              // dataOffset
              // dataLen
              //pdf_add_text(pdf, NULL, text, 12, l, t, PDF_BLACK);
              float font_height = 8;
              pdf_add_text_wrap(pdf, nullptr, text, font_height,
                                l, page_height-t-font_height*0.8, 0.0, PDF_BLACK, r-l,
                                PDF_ALIGN_LEFT, nullptr);
            }
          }
        }
      }
    }
  }

  pdf_save(pdf, "/Users/matt/dev/newton-framework/test.pdf");
  pdf_destroy(pdf);
  return 0;

}

