/*
 File:    ObjectPrinter.h

 Prints a NewtonScript object tree that resembles a Package into a
 source file that can be recompiled into the same Package.

 Written by:  Matt, 2025.
 */

#ifndef MATT_OBJECT_PRINTER
#define MATT_OBJECT_PRINTER 1

#include "Matt/Printer.h"

#include "Newton.h"

#include "Objects.h"
#include "ObjHeader.h"
#include "ROMResources.h"
#include "Symbols.h"
#include "Globals.h"

#include <cassert>
#include <string>
#include <memory>
#include <map>
#include <vector>


class ObjectPrinter : public Printer
{
  int labelSerialNo_ { 0 };
  bool optionDecompile_ { false };
  bool optionPackage_ { false };
  bool debugASTProgress_ { false };
  bool debugAST_ { false };
  bool debugBC_ { false };

public:

  enum class Option {
    Decompile,        // Recognize and decompile function frames
    Package,          // Make a package printout easier to read: recognize 'stepChildren
    DebugASTProgress, // Print the abstract syntax tree as we decompile functions
    DebugAST,         // Print the final abstract syntax tree after decompilation
    DebugBC,          // Print the Bytecode of functions
  };

  // Nodes cache information for objects.
  // - If objects are referenced multiple times, they must be printed first.
  // - Some objects should be printed first for clarity, so mark that here.
  // - Some objects have a known name, so store that here.
  // - Before we print an array or frame, we must know length, so we can decide
  //   if we print it in-line or one line per element.
  class Node {
  public:
    std::string label_;
    int length_ { 0 };
    int numRefs_ { 0 };
    bool printed_ { false };
    bool visited_ { false };
    bool suppressEarlyPrint_ { false };
    bool forceEarlyPrint_ { false };
    bool EarlyPrint() { return forceEarlyPrint_ || ((numRefs_ > 1) && !suppressEarlyPrint_); }
  };
  std::map<Ref, Node> map;

  int TextLength(RefArg ref, RefArg sameSym = NILREF);
  void BuildRefMapLength(RefArg ref);
  void BuildRefMapBranch(RefArg ref);
  void BuildRefMap(RefArg ref);

  void OptionDecompile(bool v) { optionDecompile_ = v;}
  void DebugAST(bool v) { debugAST_ = v;}
  bool DebugAST() { return debugAST_; }
  void DebugBC(bool v) { debugBC_ = v;}
  bool DebugBC() { return debugBC_; }

#pragma mark - updated stuff

  ObjectPrinter(std::ostream &oStream) : Printer(oStream) { }

  void PrintFunction(RefArg ref);
  void PrintBinary(RefArg ref);
  void PrintSymbol(RefArg ref);
  void PrintTag(RefArg ref);
  void PrintNil(RefArg ref);
  void PrintTrue(RefArg ref);
  void PrintString(RefArg ref);
  void PrintCharacter(RefArg ref);
  void PrintTokenConst(RefArg ref);
  void PrintInteger(RefArg ref);
  void PrintReal(RefArg ref);
  void PrintPathExpr(RefArg ref);
  void PrintSExprArray(RefArg ref);
  void PrintSExprFrame(RefArg ref);
  void PrintSExpr(RefArg ref);
  void PrintRefConst(RefArg ref);
  void PrintConstant(RefArg ref);
  void PrintArray(RefArg ref);
  void PrintFrame(RefArg ref);
  
  void PrintImmed(RefArg ref);

  void PrintRef(RefArg ref, bool ignoreMap = false);

  void PrintDependents(RefArg ref);
  void PrintPartialTree(RefArg ref);

  void Print(const std::string &token) { Printer::Print(token); }
  void Print(RefArg ref);
  void Decompile(RefArg ref);
};


void printPackage(RefArg package);

#endif // MATT_OBJECT_PRINTER
