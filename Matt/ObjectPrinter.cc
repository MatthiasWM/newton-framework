/*
 File:    ObjectPrinter.cc

 Prints a NewtonScript object tree that resembles a Package into a
 source file that can be recompiled into the same Package.

 Written by:  Matt, 2025.
 */

/*
 This class will take an NS Object tree and print it as a source code file,
 so it can be recompiled into the same object tree. It focuses and understanding
 and printing the contents of a package .pkg file.

 The main use for this module will be printing existing package files
 into a human readable and easily editable format. This will make it possible
 to understand nuances of NewtonOS and fix shortcomings in the packages.

 More importantly, it will eventually allow us to test every package on a
 simulated NewtonOS, and find issues in the simulation by allowing us to
 single-step through NewtonScript source code files.

 So far, we can read package files, convert them into a NewtonScript object
 tree, and write that tree back into the same package file. We can also compile
 hand-written NewtonScript files into such an Object tree, and then write a
 valid package.

 Goals:

 - Convert the tree into a crude text representation
   - understand and write all non-pointer refs
   - write slotted objects (Forms and Arrays)
   - write binary objects as MakeBinaryFromHex
   - write known binary objects in their native form (Real, String, ...)
 - Write objects with additional demands in separate blocks
   - mark everything that needs special handling
     - separate out the package definition and the app description and icons
     - separate out 'stepView' members so we can define individual Forms and Views
     - find objects with multiple references, so they get only written once
       but referenced as often as needed
   - find and order based on reference hierarchy (find circular dependencies)
   - find good labels
   - maybe generate a TOC at the top?
 - Improve text output by removing MakeBinaryFromHex wherever possible
   - write images as .png import commands
   - write sounds as .wav import commands
   - and the big one: decompile NS functions into readable NewtonScript
   - if we find ARM machine instructions, convert them back into assembler
 - Find common known patterns and write them, so they are easier to understand
   - NTK uses AddStepForm() and StepDeclare. Does it make sense to generate that code
   - Does it make sense to recreate NTK files from the source code? Layouts?
 - Will the compiler optimize multiple use of the same Symbol?
 - Will the compiler optimize Frame Maps?
 - Verify that all package-to-script-to-package conversion produce the same package

 */

#include "Matt/ObjectPrinter.h"

#include "Iterators.h"
#include "Matt/Decompiler.h"

#include <iostream>
#include <cctype>

extern bool IsPathExpr(RefArg inObj);

// struct Node { value_t value; std::vector<std::shared_ptr<Node>> children; std::weak_ptr<Node> parent; };

// Run the graph and build a dependency tree.
// Every node has a list of children that the node references.
// Every node has a list of parents that reference the node
// There is also a flag for nodes that are always written separated
// Run the tree and find circular dependencies (somehow we need to resolve those last)
// Recurse down the tree: write everything that is marked or has multiple parents first
//   Write a node and all dependencies, only if it was already written write the reference instead
// Write the commands to resolve all cyclic dependencies


/**
 \brief Return true if this ref is managed by a node in the map.

 A node is created for every ref that can potentially be referenced multiple
 time or otherwise need special handling.

 This will create a node if none was created yet.
 */
bool ObjectPrinter::HasNode(Ref ref)
{
  if (map.find(ref)!=map.end()) return true;
  if (IsReal(ref)) return false;
  if (ISREALPTR(ref) && !IsSymbol(ref)) {
    map.insert(std::make_pair(ref, std::make_shared<Node>(ref)));
    return true;
  }
  return false;
}

void ObjectPrinter::PrintDependents(Ref ref)
{
  //printf("PrintDependents: 0x%016llx\n", (uint64_t)ref);
  if (ISREALPTR(ref)) {
    ObjHeader *o = ObjectPtr(ref);
    if (ISSLOTTED(o)) {
      map[ref]->visited = true;
      SlottedObject *f = (SlottedObject*)o;
      uint32_t numSlots = (f->size - sizeof(ObjHeader)) / sizeof(Ref); // first slot is class or map!
      for (uint32_t i=1; i<numSlots; i++) {
        Ref slot = f->slot[i];
        if (HasNode(slot)) {
          if (map[slot]->IsSpecial()) {
            PrintPartialTree(slot);
          } else {
            PrintDependents(slot);
          }
        }
      }
    }
  }
}

void ObjectPrinter::PrintIndent(int indent) {
  for (int i=0; i<indent; i++) printf("  ");
}


/**
 \brief Print a frame that contains NewtonScript function.
 */
void ObjectPrinter::PrintFunction(Ref ref, int indent)
{
  if (optionDecompile_) {
    mDecompile(ref, *this, debugAST_);
  } else {
    printf("{\n");
    bool first = true;
    CObjectIterator iter(ref, false);
    for ( ; !iter.done(); iter.next()) {
      if (!first) printf(",\n");
      if (first) first = false;
      PrintIndent(indent+1);
      Ref tag = iter.tag();
      if (IsSymbol(tag))
        printf("%s", SymbolName(tag));
      else
        PrintObject(iter.tag(), 0);
      printf(": ");
      Ref slot = iter.value();
      if (HasNode(slot) && map[slot]->IsSpecial()) {
        printf("%s", map[slot]->label.c_str());
      } else {
        PrintRef(slot, indent+1);
      }
    }
    if (!first) printf("\n");
    PrintIndent(indent); printf("}");
  }
}


// TODO: Sooooo, if this is a literal *and* a slotted object, the '[' or '{'
// must be preceded with a tick ('), *and* all symbols inside the object
// must *not* be preceded with a tick, aso it's either:
// a := '[test, toast, [butter]]; or a := ['test, 'toast, ['butter]];
// The first is generated at compile time, the second at run time, resulting
// in the same object at the end (but at very different execution speeds).
// In NewtonScriptProgramLanguage.pdf, see 'object' vs. 'constructor'
//
// TODO: Ok, well, to get this perfect, we need to look at
// Frames/Compiler/y.tab.c , generated from Frames/Compiler/NewtonScript.y
// which gives us not only the entire syntax, but also hints at what
// bytecodes and objects make up a nodes in the syntax tree. If we want to do
// this right, we implement everything based on that source.
//
void ObjectPrinter::PrintRef(Ref ref, int indent, bool symbolTick) {
  if (ISREALPTR(ref)) {
    if (IsFunction(ref)) {
      PrintFunction(ref, indent);
    } else if (IsFrame(ref)) {
      printf("{\n");
      bool first = true;
      CObjectIterator iter(ref, false);
      for ( ; !iter.done(); iter.next()) {
        if (!first) printf(",\n");
        if (first) first = false;
        PrintIndent(indent+1);
        Ref tag = iter.tag();
        if (IsSymbol(tag))
          printf("%s", SymbolName(tag));
        else
          PrintObject(iter.tag(), 0);
        printf(": ");
        Ref slot = iter.value();
        if (HasNode(slot) && map[slot]->IsSpecial()) {
          printf("%s", map[slot]->label.c_str());
        } else {
          PrintRef(slot, indent+1);
        }
      }
      if (!first) printf("\n");
      PrintIndent(indent); printf("}");
    } else if (IsArray(ref)) {
      printf("[\n");
      Ref tag = ClassOf(ref);
      if (IsSymbol(tag) && SymbolCompare(tag, SYMA(array)) != 0) {
        PrintIndent(indent+1);
        printf("%s:\n", SymbolName(tag));
      }
      bool first = true;
      CObjectIterator iter(ref, false);
      for ( ; !iter.done(); iter.next()) {
        if (!first) printf(",\n");
        if (first) first = false;
        PrintIndent(indent+1);
        Ref slot = iter.value();
        if (HasNode(slot) && map[slot]->IsSpecial()) {
          printf("%s\n", map[slot]->label.c_str());
        } else {
          PrintRef(slot, indent+1);
        }
      }
      if (!first) printf("\n");
      PrintIndent(indent); printf("]");
    } else if ((symbolTick == false) && IsSymbol(ref)) {
      printf("%s", SymbolName(ref));
    } else {
      PrintObject(ref, indent);
    }
  } else {
    PrintObject(ref, indent);
  }
}

void ObjectPrinter::PrintPartialTree(Ref ref)
{
  if (HasNode(ref)) {
    Node *nd = map[ref].get();
    if (nd->printed) return;
    PrintDependents(ref);
    if (nd->printed) return;
    nd->printed = true;
    printf("%s := \n", nd->label.c_str());
    PrintRef(ref, 0);
    printf(";\n\n");
  }
}



void ObjectPrinter::AddObject(Ref ref)
{
  map[ref]->visited = true;
  if (ISREALPTR(ref)) {
    ObjHeader *o = ObjectPtr(ref);
    if (ISSLOTTED(o)) {
      map[ref]->visited = true;
      SlottedObject *f = (SlottedObject*)o;
      uint32_t numSlots = (f->size - sizeof(ObjHeader)) / sizeof(Ref); // first slot is class or map!
      for (uint32_t i=1; i<numSlots; i++) {
        Ref slot = f->slot[i];
        AddRef(slot);
        if (HasNode(slot)) {
          map[ref]->children.push_back(map[slot]);
          map[slot]->numParents++;
          //printf("Link 0x%016lx to 0x%016lx, %d\n", map[ref]->ref, map[slot]->ref, map[slot]->numParents);
        }
      }
    }
  }
}

void ObjectPrinter::AddRef(Ref ref)
{
  if (HasNode(ref)) {
    if (!map[ref]->visited)
      AddObject(ref);
  } else {
    // Nothing to do.
  }
}

void ObjectPrinter::SetNodeLabels() {
  for (auto &ndi: map) {
    Node *nd = ndi.second.get();
    if (nd->label.empty()) {
      char buffer[80];
      snprintf(buffer, 80, "Ref_0x%016lx", nd->ref);
      nd->label = buffer;
    }
  }
}

void ObjectPrinter::BuildNodeTree(Ref package)
{
  AddRef(package);
  map[package]->special = true;
  SetNodeLabels();
}

void ObjectPrinter::TestPrint(Ref package)
{
  BuildNodeTree(package);
  map[package]->label = "package";
  printf("-----------------------\n");
  PrintPartialTree(package);
  printf("%s;\n", map[package]->label.c_str());

}

void printPackage(Ref package) {
  ObjectPrinter p(std::cout);
  p.TestPrint(package);
}

void ObjectPrinter::Print(RefArg ref)
{
  map.clear();
  if (IsArray(ref) || IsFrame(ref)) {
    BuildNodeTree(ref);
    PrintPartialTree(ref);
  } else {
    PrintRef(ref, 0);
  }
  puts("");
}

void ObjectPrinter::Decompile(RefArg ref)
{
#if 1
  OptionDecompile(true);
  Print(ref);
#else

  DeepList(";");

  Tag(); Printer::Print("x := [");
  DeepList(",");

  Tag(); Printer::Print("array:");
  Tag(); Printer::Print("array:");
  Item(); Printer::Print("dings");
  Tag(); Printer::Print("array:");
  Item(); Printer::Print("dings");
  Item(); Printer::Print("dings");

  EndList();
  Item(); Printer::Print("]");

  Item(); Printer::Print("b := ..."); ItemDone();
  EmptyLines(2); Printer::Print("\n");

  EndList();


#endif
}

// -----------------------------------------------------------------------------

void ObjectPrinter::PrintBinary(RefArg ref) {
  assert(IsBinary(ref));
  Print("MakeBinaryFromHex(\"");
  uint8_t *data = (uint8_t*)BinaryData(ref);
  int n = Length(ref);
  for (int i = 0; i < n; ++i) {
    Printf("%02X", data[i]);
  }
  Print("\", ");
  PrintSymbol(ClassOf(ref));
  Print(")");
}

void ObjectPrinter::PrintFunction(RefArg ref) {
  assert(0);
}

// plain symbol: { { alpha | '_' } [ { alpha | digit | '_' } ]*
// piped symbol: ‘|’ [ { symbol-character | \ { ‘|’ | \ } ]* ‘|’ }
// symbol-character: <any ASCII character with code 32–127 except '|' or '\'>
void ObjectPrinter::PrintSymbol(RefArg ref) {
  Print("'");
  PrintTag(ref);
}

void ObjectPrinter::PrintTag(RefArg ref)
{
  std::function isSymStart = [](char c) -> bool { return (c > 31) && (c < 127) && (std::isalpha(c) || (c =='_')); };
  std::function isSymCont = [](char c) -> bool { return (c > 31) && (c < 127) && (std::isalnum(c) || (c =='_')); };
  assert(IsSymbol(ref));
  const char *sym = SymbolName(ref);
  bool piped = true;
  const char *s = sym;
  // Scan every character in the symbol to see if the symbol must be put between '|'s.
  if (isSymStart(*s++)) {
    char c = *s++;
    while (c) {
      if (!isSymCont(c)) break;
      c = *s++;
    }
    if (c == 0) piped = false;
  }
  if (piped) {
    Print("|");
    Print(sym);
    Print("|");
  } else {
    Print(sym);
  }
}

// Print the NIL constant
void ObjectPrinter::PrintNil(RefArg ref) {
  assert(ref == NILREF);
  Print("nil");
}

// Print the True constant
void ObjectPrinter::PrintTrue(RefArg ref) {
  assert(ref == TRUEREF);
  Print("true");
}

// A UTF-16 based text string
// '"' [ char | '\\' | '\"' | '\n' | '\t' | '\uXXXX\u' ]* '"'
void ObjectPrinter::PrintString(RefArg ref) {
  assert(IsString(ref));
  char buf[32];
  UniChar c, *s = (UniChar *)BinaryData(ref);
  int n = Length(ref)/sizeof(UniChar) - 1;
  Print("\"");
  for ( ; n > 0; --n) {
    c = *s++;
    if (c == '\\') {
      strcpy(buf, "\\\\");
    } else if (c >= 32 && c < 127) {
      buf[0] = (char)c; buf[1] = 0;
    } else if (c == '\n') {
      strcpy(buf, "\\n");
    } else if (c == '\t') {
      strcpy(buf, "\\t");
    } else if (c < 32) {
      snprintf(buf, 31, "\\%02X", c);
    } else {
      snprintf(buf, 31, "\\u%04X\\u", c);
    }
    Print(buf);
  }
  Print("\"");
}

// A UTF-16 character (X= capitalized hex digit)
// '$' [ character | `\\` | '\n' | '\t' | `\XX' | '\uXXXX` ]
void ObjectPrinter::PrintCharacter(RefArg ref) {
  assert(IsChar(ref));
  char buf[32];
  UniChar c = RefToUniChar(ref);
  if (c == '\\') {
    strcpy(buf, "$\\\\");
  } else if (c >= 32 && c < 127) {
    snprintf(buf, 31, "$%c", c);
  } else if (c == '\n') {
    strcpy(buf, "$\\n");
  } else if (c == '\t') {
    strcpy(buf, "$\\t");
  } else if (c < 32) {
    snprintf(buf, 21, "$\\%02X", c);
  } else {
    snprintf(buf, 21, "$\\u%04X", c);
  }
  return Print(buf);
}

// kTokenConst is true, nil, a string, a character
void ObjectPrinter::PrintTokenConst(RefArg ref) {
  if (ref == NILREF)
    return PrintNil(ref);
  else if (ref == TRUEREF)
    return PrintTrue(ref);
  else if (IsString(ref))
    return PrintString(ref);
  else if (IsChar(ref))
    return PrintCharacter(ref);
  assert(0);
}

// Print an integer (signed, hex '0x' notation is allowed
void ObjectPrinter::PrintInteger(RefArg ref) {
  assert(IsInt(ref));
  char buf[48];
  snprintf(buf, 47, "%d", RefToInt(ref));
  return Print(buf);
}

// Print a floating point "double"
//
void ObjectPrinter::PrintReal(RefArg ref) {
  assert(IsReal(ref));
  char buf[48];
  double v = CDouble(ref);
  double absValue = std::abs(v);
  if ((absValue != 0.0 && (absValue < 1e-4 || absValue >= 1e+7))) {
    // Scientific, don;t change it
    snprintf(buf, 47, "%e", v);
  } else {
    // Remove trailing zeros after the first character after the decimal point
    snprintf(buf, 47, "%.9f", v);
    int n = (int)strlen(buf) - 1;
    for ( ; n>0; --n) {
      if (buf[n-1] == '.') break;
      if (buf[n] != '0') break;
      buf[n] = 0;
    }
  }
  Print(buf);
}

// path_expr
// kTokenSymbol | path_expr '.' kTokenSymbol
// Path Expressions can also be a single integer or a single symbol.
void ObjectPrinter::PrintPathExpr(RefArg ref) {
  assert(IsPathExpr(ref));
  if (IsInt(ref))
    return PrintInteger(ref);
  else if (IsSymbol(ref))
    return PrintSymbol(ref);
  else {
    int n = Length(ref);
    for (int i = 0; i < n; ++i) {
      RefVar slot = GetArraySlot(ref, i);
      PrintTag(slot);
      if (i < n-1) Print(".");
    }
  }
}

// sexpr_star
// '[' { kTokenSymbol ':' } [ SExpr ',']* ']'
void ObjectPrinter::PrintSExprArray(RefArg ref) {
  assert(IsArray(ref));
  RefVar klass = ClassOf(ref);
  Print("[");
  DeepList(",");
  if (IsSymbol(klass) && (SymbolCompare(klass, SYMA(array)) != 0)) {
    Tag(); PrintSymbol(klass); Print(":");
  }
  int n = Length(ref);
  for (int i = 0; i < n; ++i) {
    RefVar slot = GetArraySlot(ref, i);
    Item(); PrintSExpr(slot); ItemDone();
  }
  Finalize(); Print("]");
  EndList();
  ItemDone();
}

// sexpr_frame_slot_star
// '{' [ kTokenSymbol ':' SExpr ',' ]* '}'
void ObjectPrinter::PrintSExprFrame(RefArg ref) {
  assert(IsFrame(ref));
  Print("{");
  DeepList(",");
  CObjectIterator iter(ref, false);
  for ( ; !iter.done(); iter.next()) {
    Item();
    PrintTag(iter.tag());
    Print(": ");
    PrintSExpr(iter.value());
    ItemDone();
  }
  Finalize(); Print("}");
  EndList();
  ItemDone();
}

//  "sexpr : kTokenConst",
//  "sexpr : kTokenInteger",
//  "sexpr : '-' kTokenInteger",
//  "sexpr : kTokenReal",
//  "sexpr : '-' kTokenReal",
//  "sexpr : kTokenRefConst",
//  "sexpr : path_expr",
//  "sexpr : '[' sexpr_star ']'",
//  "sexpr : '[' kTokenSymbol ':' sexpr_star ']'",
//  "sexpr : '{' sexpr_frame_slot_star '}'",
void ObjectPrinter::PrintSExpr(RefArg ref)
{
  if ((ref==NILREF) || (ref==TRUEREF) || IsString(ref) || IsChar(ref))
    return PrintTokenConst(ref);
  else if (IsInt(ref))
    return PrintInteger(ref);
  else if (IsReal(ref))
    return PrintReal(ref);
  else if (IsMagicPtr(ref))
    return PrintRefConst(ref);
  else if (IsPathExpr(ref))
    return PrintPathExpr(ref);
//  else if (IsFunction(ref))   // check before frame
//    return PrintFunction(ref);
  else if (IsArray(ref))
    return PrintSExprArray(ref);
  else if (IsFrame(ref))
    return PrintSExprFrame(ref);
  else if (IsBinary(ref))
    return PrintBinary(ref);
  // TODO: handle binaries that can be created programmatically, but are not
  // TODO: part of the Grammar (functions, for example...)
  assert(0);
}

// A Magic Value @32
// '@' [ integer ]
void ObjectPrinter::PrintRefConst(RefArg ref)
{
  assert(IsMagicPtr(ref));
  Printer::Print("@");
  Printer::Print((int)RVALUE(ref));
}

//  "constant : kTokenConst",
//  "constant : kTokenInteger",
//  "constant : kTokenReal",
//  "constant : '\\'' sexpr",
//  "constant : kTokenRefConst",
void ObjectPrinter::PrintConstant(RefArg ref)
{
  if ((ref==NILREF) || (ref==TRUEREF) || IsString(ref) || IsChar(ref))
    return PrintTokenConst(ref);
  else if (IsInt(ref))
    return PrintInteger(ref);
  else if (IsReal(ref))
    return PrintReal(ref);
  else if (IsMagicPtr(ref))
    return PrintRefConst(ref);
  else {
    Print("'"); PrintSExpr(ref); return;
  }
}

void ObjectPrinter::PrintRef(RefArg ref)
{
//  PrintConstant(ref);
  Print(ref);
/*
  if ((ref==NILREF) || (ref==TRUEREF) || IsString(ref) || IsChar(ref))
    return PrintTokenConst(ref);
  else if (IsInt(ref))
    return PrintInteger(ref);
  else if (IsReal(ref))
    return PrintReal(ref);
  else if (IsMagicPtr(ref))
    return PrintRefConst(ref);
  else if (IsPathExpr(ref))     // is that correct?
    return PrintPathExpr(ref);
  else if (IsFunction(ref))     // check before frame
    return PrintFunction(ref);
  else if (IsArray(ref))
    return PrintArray(ref);
  else if (IsFrame(ref))
    return PrintFrame(ref);
  else if (IsBinary(ref))
    return PrintBinary(ref);
 */
}

