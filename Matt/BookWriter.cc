//
//  BookWriter.cc
//  Newton
//
//  Created by Matthias Melcher on 25.02.26.
//

#include "BookWriter.h"

#include <arpa/inet.h>
//
//#include "Newton.h"
//#include "Objects.h"
//#include "NewtonPackage.h"
//#include "Matt/PackageWriter.h"
//#include "PackageTypes.h"
//#include "ObjHeader.h"
//#include "Ref32.h"
//#include "ROMResources.h"
#include "Symbols.h"


extern void writeBookToHTML(RefArg book, const std::string &filename);

void writePackageBookToHTML(RefArg pkg, const std::string &filename) {
  if (!IsFrame(pkg)) {
    fprintf(stderr, "Not a package, not a Frame!\n");
    return;
  }
  Ref signature = GetFrameSlot(pkg, MakeSymbol("signature"));
  if (!IsSymbol(signature)) {
    fprintf(stderr, "Not a package, no signature!\n");
    return;
  }
  if (SymbolCompare(signature, MakeSymbol("package0")) != 0) {
    fprintf(stderr, "Not a supported package, not 'package0'!\n");
    return;
  }
  Ref part_array = GetFrameSlot(pkg, MakeSymbol("part"));
  if (!IsArray(part_array)) {
    fprintf(stderr, "Not a supported package, part list not found!\n");
    return;
  }
  Ref part0 = GetArraySlot(part_array, 0);
  if (!IsFrame(part0)) {
    fprintf(stderr, "Not a supported package, part 0 not found!\n");
    return;
  }
  Ref type = GetFrameSlot(part0, MakeSymbol("type"));
  if (!IsString(type)) {
    fprintf(stderr, "Not a supported package, part 0 type not found!\n");
    return;
  }
  if (strcmp( BinaryData(ASCIIString(type)), "book") != 0) {
    fprintf(stderr, "Not a supported package, part 0 is not of type \"book\"!\n");
    return;
  }
  Ref data = GetFrameSlot(part0, MakeSymbol("data"));
  if (!IsFrame(data)) {
    fprintf(stderr, "Can't read book, part 0 data not found!\n");
    return;
  }
  Ref book = GetFrameSlot(data, MakeSymbol("book"));
  if (!IsFrame(book)) {
    fprintf(stderr, "Can't read book, part 0 book data not found!\n");
    return;
  }
  writeBookToHTML(book, filename);
}

void writeBookToHTML(RefArg book, const std::string &filename)
{
  Ref contents_array = GetFrameSlot(book, MakeSymbol("contents"));
  if (!IsArray(contents_array)) {
    fprintf(stderr, "Can't read book, no contents found!\n");
    return;
  }
  for (int i=0; i<Length(contents_array); ++i) {
    Ref text_block = GetArraySlot(contents_array, i);
    if (!IsFrame(text_block)) {
      fprintf(stderr, "Can't contents block %d, expected Frame!\n", i);
    }
    // {data: "Title Page", layout: 2048}
    Ref data = GetFrameSlot(text_block, MakeSymbol("data"));
    if (IsString(data)) {
      printf("%s\n", BinaryData(ASCIIString(data)));
    }
  }

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
}
