/*
 File:    NewtonPackagePrinter.h

 Prints a NewtonScript object tree that resembles a Package into a
 source file that can be recompiled into the same Package.

 Written by:  Matt, 2025.
 */

#if !defined(__NEWTONPACKAGEPRINTER_H)
#define __NEWTONPACKAGEPRINTER_H 1

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


class NewtonPackagePrinter {
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

  NewtonPackagePrinter() = default;
  bool HasNode(Ref ref);
  void PrintDependents(Ref ref);
  void PrintIndent(int indent);
  void PrintFunction(Ref ref, int indent);
  void PrintRef(Ref ref, int indent);
  void PrintPartialTree(Ref ref);
  void AddObject(Ref ref);
  void AddRef(Ref ref);
  void SetNodeLabels();
  void BuildNodeTree(Ref package);
  void TestPrint(Ref package);
};


void printPackage(Ref package);

#endif  /* __NEWTONPACKAGEPRINTER_H */
