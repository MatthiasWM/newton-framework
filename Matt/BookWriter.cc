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
  // part [
  //   type: "book"
  //   data {
  //     book {
  //       version, isbn, title, shortTitle, copyright, author, publisher,
  //       data { }
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
  //         {nColumns: 1, column: [{width: 12, type: 0}]}, etc.
  //       ]
  //       rendering: [
  //         { pageSize: { }
  //           contents: [ [ 1, 2, 3, 5, ...] ]
  //           pages [
  //             { template: ->template
  //               blocks: [
  //                 { bounds: { }, item: ->Item, dataOffset, dataLen }
  //               ]
  //       ]
}
