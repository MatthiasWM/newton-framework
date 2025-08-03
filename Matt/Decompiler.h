
/*
 File:    MattsDecompiler.h

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#if !defined(__MATTSDECOMPILER_H)
#define __MATTSDECOMPILER_H 1

#include "Matt/ObjectPrinter.h"

class ObjectPrinter;
class ASTNode;
class AST_JumpTarget;
class AST_Bytecode;

enum class Print { bytecode, deep, script };

class Decompiler
{
  friend ASTNode;

public:
  ObjectPrinter &p;
  int numASTChanges = 0; // While resolving, this number will increase whenever a node is resolved
  class Local {
  public:
    enum class Use { undefined, system, arg, local, loop, iter, limit };
    Ref ref;
    Use use = Use::undefined;
  };

protected:
  int nos_ = 0;
  int numArgs_ = 0;
  int numLocals_ = 0;

  std::vector<Local> locals_;
  //RefVar locals_;         ///< An array of the symbols in argFrame:
  ///< _nextArgFrame, _parent, _implementor, parameters, locals
  int numLiterals_ = 0;
  RefVar literals_;       // Array of Refs
  ASTNode *first_ { nullptr };
  ASTNode *last_ { nullptr };
  std::map<int /* destination pc*/, std::map<int /* origin pc */, AST_JumpTarget*>> targetMap_;
  bool debugAST_ { false };

  void AddToTargets(int target, int origin);
  ASTNode *Append(ASTNode *lastNode, ASTNode *newNode);
  AST_Bytecode *NewBytecodeNode(int pc, int a, int b);

public:
  Decompiler(ObjectPrinter &printer) : p( printer ) { }
  Ref GetLiteral(int i) { return GetArraySlot(literals_, i); }
  ObjectPrinter *Printer() { return &p; }
  void DebugAST(bool v) { debugAST_ = v;}
  void printAST(const char *label);
  void printASTRoot();
  void printSource();
  void printLiteralAsTag(int ix) {
    p.PrintTag(GetArraySlot(literals_, ix));
  }
  void printLiteral(int ix) {
    p.PrintRef(GetArraySlot(literals_, ix));
  }
  bool localUsedAs(int ix, Local::Use sameUse) { return (locals_[ix].use == sameUse); }
  void useLocalAs(int ix, Local::Use newUse) { locals_[ix].use = newUse; }
  void printLocal(int ix) {
    p.PrintTag(locals_[ix].ref);
  }
  void decompile(Ref ref);
  void printPathExpr(RefArg pathExpr);
  bool compressAST();
  void solve();
  void generateAST(Ref instructions);

  // Print state:
  Print output { Print::bytecode };
  int precedence { 0 }; // During printout, store the precedence of the current operation
};

NewtonErr mDecompile(Ref ref, ObjectPrinter &printer, bool debugAST);


#endif  /* __MATTSDECOMPILER_H */
