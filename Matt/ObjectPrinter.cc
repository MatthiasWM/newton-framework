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


// TODO: recognize special binaries (reals, symbols, etc.) and prints them
// TODO: nicely and only falls back to this if nothing else fits.
/**
 \brief Print a binary in the MakeBinaryFromHex format.
 */
void ObjectPrinter::PrintBinary(RefArg ref) {
  assert(IsBinary(ref));
  if (IsSymbol(ref)) {
    return PrintSymbol(ref);
  } else if (IsReal(ref)) {
    return PrintReal(ref);
  } else if (IsString(ref)) {
    return PrintString(ref);
  }
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
    // Scientific, don't change it
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
  Trailer(); Print("]");
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
  Trailer(); Print("}");
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

/**
 \brief Print a function using the decompiler.
 */
void ObjectPrinter::PrintFunction(Ref ref, int indent)
{
  if (optionDecompile_) {
    mDecompile(ref, *this, debugAST_);
  } else {
    PrintFrame(ref);
  }
}

// TODO: notice special arrays (pathExpr) and print them elsewhere
/**
 \brief Print the array and all the slot in it.
 */
void ObjectPrinter::PrintArray(RefArg ref) {
  assert(IsArray(ref));
  RefVar klass = ClassOf(ref);
  Print("[");
  StartList(",", TextLength(ref));
  if (IsSymbol(klass) && (SymbolCompare(klass, SYMA(array)) != 0)) {
    Tag(); PrintSymbol(klass); Print(":");
  }
  int n = Length(ref);
  for (int i = 0; i < n; ++i) {
    RefVar slot = GetArraySlot(ref, i);
    Item(); PrintRef(slot); ItemDone();
  }
  Trailer(); Print("]");
  EndList();
  ItemDone();
}

// TODO: notice special frames (functions) and print them elsewhere
/**
 \brief Print the frame and all the slot in it.
 */
void ObjectPrinter::PrintFrame(RefArg ref) {
  assert(IsFrame(ref));
  Print("{");
  StartList(",", TextLength(ref));
  CObjectIterator iter(ref, false);
  for ( ; !iter.done(); iter.next()) {
    Item();
    PrintTag(iter.tag());
    Print(": ");
    PrintRef(iter.value());
    ItemDone();
  }
  Trailer(); Print("}");
  EndList();
  ItemDone();
}

/**
 \brief Print anything that is not a Frame, Array, or Binary
 */
void ObjectPrinter::PrintImmed(RefArg ref) {
  assert( !ISREALPTR(ref) );
  if (ref == NILREF)
    PrintNil(ref);
  else if (ref == TRUEREF)
    PrintTrue(ref);
  else if (IsInt(ref))
    PrintInteger(ref);
  else if (IsChar(ref))
    PrintCharacter(ref);
  else if (IsMagicPtr(ref))
    PrintRefConst(ref);
  else
    assert(0);
}

/**
 \brief Print a ref and everything that comes under it.
 If the map is set, ignoreMap is false, and the map entry of the ref needs
 special attention, only refer to it by its label.
 */
void ObjectPrinter::PrintRef(RefArg ref, bool ignoreMap)
{
  if (!ISREALPTR(ref)) {
    PrintImmed(ref);
    return;
  }
  if (ignoreMap == false) {
    Node &nd = map[ref];
    if (nd.EarlyPrint()) {
      Print(nd.label_);
      return;
    }
  }
  if (IsArray(ref))
    PrintArray(ref);
  else if (IsFrame(ref))
    PrintFrame(ref);
  else if (IsBinary(ref))
    PrintBinary(ref);
  else
    assert(0);
}

/* TODO: TextLength must take dependencies into account and start counting over at the length of the dependence label. */
/**
 \brief Get the print length of an immediate or a symbol, ot the cached length of an object.
 \arg[in] ref calculate the length of this
 \arg[in] sameSym if both ref and sameSym are the same symbol, return 0;
 */
int ObjectPrinter::TextLength(RefArg ref, RefArg sameSym) {
  if (ISREALPTR(ref)) {
    return map[ref].length_;
  }
  if (ref == NILREF) {
    return 3;
  } else if (ref == TRUEREF) {
    return 4;
  } else if (IsInt(ref)) {
    int v = RefToInt(ref);
    int sign = (v < 0) ? 1 : 0;
    // TODO: printout range for "%e" is (absValue < 1e-4 || absValue >= 1e+7)
    // after that it's "%.9f" minus trailing 0's
    if (v > 0) {
      return std::floor(std::log10(std::abs(v))) + 1 + sign;
    } else {
      return 1 + sign;
    }
  } else if (IsMagicPtr(ref)) {
    int v = (int)RVALUE(ref);
    if (v == 0) return 2;
    return std::floor(std::log10(std::abs(v))) + 2;
  } else if (IsChar(ref)) {
    return 2;
  } else {
    assert(0);
    return 3;
  }
}

/**
 \brief Print ref dependents, recursive part.
 Walk down the tree. If we find a dependence, print that one first (starting a
 new recursion). If not, continue in this recursion further down and along
 the tree.
 */
void ObjectPrinter::PrintDependents(RefArg ref)
{
  if (ISREALPTR(ref)) {
    if (IsArray(ref) || IsFrame(ref)) {
      FOREACH(ref, slot); {
        if (ISREALPTR(slot)) {
          if (map[slot].EarlyPrint()) {
            PrintPartialTree(slot);
          } else {
            PrintDependents(slot);
          }
        }
      } END_FOREACH;
    }
  }
}

/**
 \brief Recurse into dependents, then print the start of a tree branch.

 The recursion ensures that all dependent branches and binaries are printed
 first. Only then is this branch printed.
 */
void ObjectPrinter::PrintPartialTree(RefArg ref)
{
  // Immediates of all types can't be the head of a tree.
  if (!ISREALPTR(ref))
    return;

  Node &nd = map[ref];
  // The branch was already printed. We are done.
  if (nd.printed_)
    return;
  // Make sure that all dependents are printed first.
  PrintDependents(ref);
  // If this is true, we have a circular dependency.
  // TODO: enable a check if that ever occurs, and if it does, write some code for it.
  if (nd.printed_) return;
  nd.printed_ = true;
  // Print the label header, the the branch itself
  // TODO: surely there is more information that we can print
  // TODO: do we implement the Printer formatting here?
  Printf("%s := ", nd.label_.c_str());
  // TODO: Do we need a flag that tells us if to stop on refs that have a map entry?
  PrintRef(ref, true);
  ItemDone();
}

/**
 \brief Walk the tree and annotate the accumulated text length for all entries.
 */
void ObjectPrinter::BuildRefMapLength(RefArg ref)
{
  // Not needed.
  if (!ISREALPTR(ref))
    return;

  // Now handle slotted objects and binaries.
  Node &nd = map[ref];
  if (nd.visited_) return;

  nd.visited_ = true;
  nd.length_ = true;
  if (IsArray(ref) || IsFrame(ref)) {
    FOREACH(ref, slot) {
      BuildRefMapLength(slot);
      if (ISREALPTR(slot) && map[slot].EarlyPrint())
        nd.length_ += map[slot].label_.length();
      else
        nd.length_ += TextLength(slot);
    } END_FOREACH;
    nd.length_ += ::Length(ref) * 2 + 3;
    if (IsArray(ref)) nd.length_ += TextLength(ClassOf(ref), SYMA(array)) + 2;
  } else if (IsBinary(ref)) {
    // TODO: check the various known binary types to get a better length
    // Last resort: MakeBinaryFromHex("", 'symbol)
    nd.length_ = Length(ref) + TextLength(ClassOf(ref), SYMA(binary)) + 24;
  } else {
    assert(0);
  }
}


/**
 \brief Walk the tree and build a map with annotations, recursive part.
 \note Here is one good place to find more expressive names for labels.
 */
void ObjectPrinter::BuildRefMapBranch(RefArg ref)
{
  // Immediates of all types need no map entry.
  if (!ISREALPTR(ref))
    return;

  // Now handle slotted objects and binaries.
  Node &nd = map[ref];
  if (nd.numRefs_ == 0) {
    // If we are here the first time, fill in the node.
    nd.numRefs_ = 1; // avoid endless recursions
    char buffer[32];
    snprintf(buffer, 32, "Ref_%d", labelSerialNo_++);
    nd.label_ = std::string(buffer);
    if (IsArray(ref) || IsFrame(ref)) {
      FOREACH(ref, slot) {
        BuildRefMapBranch(slot);
//        nd.length_ += TextLength(slot);
      } END_FOREACH;
      // TODO: if this is a function, we may need to change the text length
//      nd.length_ += ::Length(ref) * 2 + 3;
//      if (IsArray(ref)) nd.length_ += TextLength(ClassOf(ref), SYMA(array)) + 2;
    } else if (IsBinary(ref)) {
      // TODO: check the various known binary types to get a better length
      // Last resort: MakeBinaryFromHex("", 'symbol)
//      nd.length_ = Length(ref) + TextLength(ClassOf(ref), SYMA(binary)) + 24;
    } else {
      assert(0);
    }
  } else {
    // If this is already filled in, just let the node know that there are multiple refs.
    nd.numRefs_++;
  }
}

/**
 \brief Walk the tree and build a map with annotations on how to handle that branch during printing.

 This is the main entry function for building the Ref Map. Every entry that is
 a real pointer (Binary, Array, Frame) gets an entry in the map.
 */
void ObjectPrinter::BuildRefMap(RefArg ref)
{
  // Clear the map for a new tree
  map.clear();
  labelSerialNo_ = 0;
  // Recursively walk the object tree down every branch.
  BuildRefMapBranch(ref);
  // Walk the tree again to find the text length of mapped refs
  for (auto &nd: map) nd.second.visited_ = false;
  BuildRefMapLength(ref);
}


/**
 \brief Main entry point for users.

 Print the ref object tree as NewtonScript source code that is formatted
 to be human readable and can be recompiled into the same object tree using
 the built-in compiler.

 Objects that are referenced multiple times or interesting in some other way
 are printed first, generating a temporary label, and will be reassembled later.

 \todo Implement options.
 */
void ObjectPrinter::Print(RefArg ref)
{
  map.clear();
  DeepList(";\n"); SetIndent(0);
  if (IsArray(ref) || IsFrame(ref)) {
    BuildRefMap(ref);
    PrintPartialTree(ref);
  } else {
    PrintRef(ref);
  }
  Item(); Print("");
  EndList();
}

/**
 \brief Shortcut to print an object tree and decompile the functions inside.
 \todo Flags not yet managed.
 */
void ObjectPrinter::Decompile(RefArg ref)
{
  OptionDecompile(true);
  Print(ref);
}

/**
 \brief Shortcut to print the contents of an object tree as a package.
 \todo Flags are not set at all yet.
 */
void printPackage(RefArg package) {
  ObjectPrinter p(std::cout);
  p.Print(package);
}
