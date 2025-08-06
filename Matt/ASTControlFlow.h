
/*
 File:    Matt/ASTControlFlow.h

 Matt's decompiler Abstract Syntax Tree.
 Control Flow nodes based on bytecodes.

 Written by:  Matt, 2025.
 */

#if !defined(__MATT_AST_CONTROLFLOW_H)
#define __MATT_AST_CONTROLFLOW_H 1

#include "Matt/ASTAdmin.h"

#include <vector>

namespace ast {

#pragma mark - conditions and loops -

class BCBranch : public Bytecode {
public:
  BCBranch(Decompiler &d, int pc, int a, int b) : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "BCBranch"; }
  int provides() override { return kBranch; }
  bool Resolved() override { return false; }
  Node *ResolveLoop();
  Node *ResolveBreak();
  Node *Resolve(Pass pass) override;
  void Print(uint32_t flags = 0) override;
};

class BCBranchIfTrue : public Consume1 {
public:
  BCBranchIfTrue(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCBranchIfTrue"; }
  int provides() override { return kProvidesUnknown; }
  int extracted(BCBranch *branch2, Node *&it);

  Node *ResolveWhileDo();
  Node *ResolveOr();
  Node *Resolve(Pass pass) override;
  bool Resolved() override { return false; }
  void Print(uint32_t flags = 0) override;
};

class BCBranchIfFalse : public Consume1 {
public:
  BCBranchIfFalse(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCBranchIfFalse"; }
  int provides() override { return kProvidesUnknown; }
  Node *ResolveIfTheElse();
  Node *ResolveRepeatUntil();
  Node *Resolve(Pass pass) override;
  bool Resolved() override { return false; }
  void Print(uint32_t flags = 0) override;
};

class BCReturn : public Consume1 {
public:
  BCReturn(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCReturn"; }
  // Even though the return command leaves the function immediately,
  // technically it is an expression and leaves a value on the stack.
  // So `a := 3 + return 4;` is a valid statement. It compiles, but just never runs.
  int provides() override { return Resolved() ? 1 : kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

#pragma mark - For loop -

// (A=22) addend -- addend value
class BCIncrVar : public Consume1 {
public:
  BCIncrVar(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCIncrVar"; }
  int provides() override { if (in_) return 2; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=23) incr index limit --
class BCBranchLoop : public Bytecode {
public:
  BCBranchLoop(Decompiler &d, int pc, int a, int b);
  const char *Class() override { return "BCBranchLoop"; }
  int provides() override { return kProvidesUnknown; }
  int consumes() override { return 3; }
  bool Resolved() override { return false; }
  Node *Resolve(Pass pass) override;
};

#pragma mark - Foreach loop -

// (A=24, B=17): object deeply -- iterator
class BCNewIter : public Consume2 {
public:
  BCNewIter(Decompiler &d, int pc, int a, int b) : Consume2(d, pc, a, b) { }
  const char *Class() override { return "BCNewIter"; }
  int provides() override { return kProvidesUnknown; }
  bool Resolved() override { return false; }
  Node *Resolve(Pass pass) override;
  Node *ResolveForeachSlotValueDo();
  Node *ResolveForeachSlotValueCollect();
  void Print(uint32_t flags = 0) override;
};

// (A=0, B=5) iterator --
class BCIterNext : public Consume1 {
public:
  BCIterNext(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCIterNext"; }
  int provides() override { return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=0, B=6) iterator -- done
class BCIterDone : public Consume1 {
public:
  BCIterDone(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCIterDone"; }
  int provides() override { return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

#pragma mark - Exceptions -

// (A=25): sym1 pc1 sym2 pc2 ... symN pcN --
class BCNewHandler : public ConsumeN {
public:
  BCNewHandler(Decompiler &d, int pc, int a, int b)
  : ConsumeN(d, pc, a, b, b*2) { }
  const char *Class() override { return "BCNewHandler"; }
  int provides() override { return kNewHandler; }
  void Print(uint32_t flags = 0) override;
  Node *Resolve(Pass pass) override;
  bool Resolved() override { return false; }
};

// (A=0, B=7): --
class BCPopHandlers : public Bytecode {
public:
  BCPopHandlers(Decompiler &d, int pc, int a, int b) : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "BCPopHandlers"; }
  int provides() override { return kPopHandlers; }
  bool Resolved() override { return false; }
  void Print(uint32_t flags = 0) override;
};

#pragma mark - Calls -

// (A=0, B=4): func -- closure
class BCSetLexScope : public Consume1 {
public:
  BCSetLexScope(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCSetLexScope"; }
  int provides() override { if (in_) return kProvidesOne; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=5): arg1 arg2 ... argN name -- result
// Calls a global function. The function arguments (if any) are on the stack
// in left-to-right order, followed by a symbol giving the name of the function
// to call. The B field contains the number of arguments on the stack.
class BCCall : public ConsumeN {
public:
  BCCall(Decompiler &d, int pc, int a, int b)
  : ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "BCCall"; }
  Node *Resolve(Pass pass) override;
  void Print(uint32_t flags = 0) override;
};

// (A=6): arg1 arg2 ... argN func -- result
// call ... with (...)
// Performs a function call. The function arguments (if any) are on the stack
// in left-to-right order, followed by the function object to call. The B field
// contains the number of arguments on the stack.
class BCInvoke : public ConsumeN {
public:
  BCInvoke(Decompiler &d, int pc, int a, int b)
  : ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "BCInvoke"; }
  void Print(uint32_t flags = 0) override;
};

// (A=7): arg1 arg2 ... argN receiver name -- result
// Performs a message send or a conditional send (send if defined).
// The function arguments (if any) are on the stack in left-to-right order,
// followed by the message name (a symbol), followed by the receiver. The B
// field contains the number of arguments on the stack.
// \note the order of name and receiver is flipped in the original documentation!
class BCSend : public ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  BCSend(Decompiler &d, int pc, int a, int b, bool ifDefined)
  : ConsumeN(d, pc, a, b, b+2), ifDefined_(ifDefined) { }
  const char *Class() override { return "BCSend"; }
  void Print(uint32_t flags = 0) override;
};

// (A=9): arg1 arg2 ... argN name -- result
// inherited:Print(3);
// Performs an inherited message send. The function arguments (if any) are on
// the stack in left-to-right order, followed by the message name (a symbol).
// The B field contains the number of arguments on the stack.
class BCResend : public ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  BCResend(Decompiler &d, int pc, int a, int b, bool ifDefined)
  : ConsumeN(d, pc, a, b, b+1), ifDefined_(ifDefined) { }
  const char *Class() override { return "BCResend"; }
  void Print(uint32_t flags = 0) override;
};


}; // namespace ast;

#endif  /* __MATT_AST_CONTROLFLOW_H */
