/*
 File:    NewtonPackagePrinter.cc

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

#include "Newton.h"

#include "Objects.h"
#include "ObjHeader.h"
#include "ROMResources.h"
#include "Symbols.h"
#include "Globals.h"
#include "Iterators.h"

#include <stdio.h>
#include <ios>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cassert>
#include <string>
#include <memory>
#include <vector>
#include <map>


// struct Node { value_t value; std::vector<std::shared_ptr<Node>> children; std::weak_ptr<Node> parent; };

// Run the graph and build a dependency tree.
// Every node has a list of children that the node references.
// Every node has a list of parents that reference the node
// There is also a flag for nodes that are always written separated
// Run the tree and find circular dependencies (somehow we need to resolve those last)
// Recurse down the tree: write everything that is marked or has multiple parents first
//   Write a node and all dependencies, only if it was already written write the reference instead
// Write the commands to resolve all cyclic dependencies


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

void AddObject(Ref obj);
void AddRef(Ref ref);
void PrintDependents(Ref ref);
void PrintPartialTree(Ref ref);


/**
 \brief Return true if this ref is managed by a node in the map.

 A node is created for every ref that can potentially be referenced multiple
 time or otherwise need special handling.

 This will create a node if none was created yet.
 */
bool HasNode(Ref ref)
{
  if (map.find(ref)!=map.end()) return true;
  if (IsReal(ref)) return false;
  if (ISREALPTR(ref) && !IsSymbol(ref)) {
    map.insert(std::make_pair(ref, std::make_shared<Node>(ref)));
    return true;
  }
  return false;
}

void PrintDependents(Ref ref)
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

void PrintIndent(int indent) {
  for (int i=0; i<indent; i++) printf("  ");
}

void PrintRef(Ref ref, int indent) {
  if (ISREALPTR(ref)) {
    if (IsFrame(ref)) {
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
    } else {
      PrintObject(ref, indent);
    }
  } else {
    PrintObject(ref, indent);
  }
}

void PrintPartialTree(Ref ref)
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



void AddObject(Ref ref)
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
          printf("Link 0x%016lx to 0x%016lx, %d\n", map[ref]->ref, map[slot]->ref, map[slot]->numParents);
        }
      }
    }
  }
}

void AddRef(Ref ref)
{
  if (HasNode(ref)) {
    if (!map[ref]->visited)
      AddObject(ref);
  } else {
    // Nothing to do.
  }
}

void SetNodeLabels() {
  for (auto &ndi: map) {
    Node *nd = ndi.second.get();
    if (nd->label.empty()) {
      char buffer[80];
      snprintf(buffer, 80, "Ref_0x%016lx", nd->ref);
      nd->label = buffer;
    }
  }
}

void BuildNodeTree(Ref package)
{
  AddRef(package);
  map[package]->special = true;
  SetNodeLabels();
}




void printPackage(Ref package)
{
  BuildNodeTree(package);
  map[package]->label = "package";
  printf("-----------------------\n");
  PrintPartialTree(package);
  printf("%s;\n", map[package]->label.c_str());

}
