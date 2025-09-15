
/*
 File:    MattsDecompiler.h

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#if !defined(__MATTSDECOMPILER_H)
#define __MATTSDECOMPILER_H 1

#include "Matt/ObjectPrinter.h"

class ObjectPrinter;

namespace ast {
class Node;
class JumpTarget;
class Bytecode;
};

enum class Print { bytecode, deep, script };

// Precedence Table:
// TODO: How exactly do we treat the if-then-else statement?
// (depending on left-to-right or right-to-left eval, we may increment by 1 internally)
using Precedence = uint8_t;
constexpr Precedence kPrecedenceAssign       =  10;  //   0: assign ':=', right to left
constexpr Precedence kPrecedenceAndOr        =  20;  //   1: and, or
constexpr Precedence kPrecedenceLogicNot     =  30;  //   2: not
constexpr Precedence kPrecedenceCompare      =  40;  //   3: comparisons (<, >, =, <>, <=, >=)
constexpr Precedence kkPrecedenceExists      =  50;  //   4: "exists"
constexpr Precedence kPrecedenceStringer     =  60;  //   5: stringer '&', '&&'
constexpr Precedence kPrecedenceAddSub       =  70;  //   6: add, subtract
constexpr Precedence kPrecedenceMulDiv       =  80;  //   7: divide, div, multiply, mod
constexpr Precedence kPrecedenceShift        =  90;  //   8: left shift, right shift '<<', '>>'
constexpr Precedence kPrecedenceUnaryMinus   = 100;  //   9: unary minus '-'
constexpr Precedence kPrecedenceArrayElement = 110;  //   10: array element '[]'
constexpr Precedence kPrecedenceSend         = 120;  //   11: send, conditional send ':', ':?'
constexpr Precedence kPrecedenceSlotAccess   = 130;  //   12: slot access '.'

class Decompiler
{
  friend ast::Node;

public:
  ObjectPrinter &p;
  int numASTChanges = 0; // While resolving, this number will increase whenever a node is resolved
  class Local {
  public:
    enum class Use { undefined, system, arg, local, loop, iter, limit, noted, notedArg };
    Ref ref;
    Use use = Use::undefined;
  };

protected:
  int numArgs_ = 0;
  int numLocals_ = 0;

  std::vector<Local> locals_;
  typedef struct { int arg; int local; } NotedArg;
  std::vector<NotedArg> notedArgs_;
  //RefVar locals_;         ///< An array of the symbols in argFrame:
  ///< _nextArgFrame, _parent, _implementor, parameters, locals
  int numLiterals_ = 0;
  RefVar literals_;       // Array of Refs
  ast::Node *first_ { nullptr };
  ast::Node *last_ { nullptr };
  std::map<int /* destination pc*/, std::map<int /* origin pc */, ast::JumpTarget*>> targetMap_;

  void AddToTargets(int target, int origin, int excp=-1);
  ast::Node *Append(ast::Node *lastNode, ast::Node *newNode);
  ast::Bytecode *NewBytecodeNode(int pc, int a, int b);

public:
  Decompiler(ObjectPrinter &printer) : p( printer ) { }
  Ref GetLiteral(int i) { return GetArraySlot(literals_, i); }
  ObjectPrinter *Printer() { return &p; }
  void printAST(const char *label);
  void printASTRoot();
  void printSource(bool isNative);
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
  Precedence precedence { kPrecedenceAssign }; // During printout, store the precedence of the current operation
};

NewtonErr mDecompile(Ref ref, ObjectPrinter &printer, bool isNative, bool debugAST, bool debugBC);


#endif  /* __MATTSDECOMPILER_H */
