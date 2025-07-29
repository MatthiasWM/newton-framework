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
  bool optionDecompile_ { false };
  bool debugAST_ { false };

public:
  class Node {
  public:
    Ref ref = NILREF;
    std::string label;
    std::vector<std::shared_ptr<Node>> children;
    //std::vector<std::weak_ptr<Node>> parents;  // Maybe numParents is enough?
    int numParents = 0;
    bool special = false;
    bool printed = false;
    bool visited = false;
    int tag = 0; // find cyclic dependency
    Node(Ref r) : ref(r) { }
    bool IsSpecial() { return special || (numParents > 1); }
  };
  std::map<Ref, std::shared_ptr<Node>> map;

  bool HasNode(Ref ref);
  void PrintDependents(Ref ref);
  void PrintIndent(int indent);
  void PrintFunction(Ref ref, int indent);
  void PrintRef(Ref ref, int indent, bool symbolTick = true);
  void PrintPartialTree(Ref ref);
  void AddObject(Ref ref);
  void AddRef(Ref ref);
  void SetNodeLabels();
  void BuildNodeTree(Ref package);
  void TestPrint(Ref package);

  ObjectPrinter(std::ostream &oStream) : Printer(oStream) { }
  void Print(RefArg ref);
  void Print(const std::string &token) { Printer::Print(token); }
  void Decompile(RefArg ref);

  void OptionDecompile(bool v) { optionDecompile_ = v;}
  void DebugAST(bool v) { debugAST_ = v;}

  // --- Print Refs following Grammar Rules:

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
  void PrintRef(RefArg ref);

};


void printPackage(Ref package);

#endif // MATT_OBJECT_PRINTER
