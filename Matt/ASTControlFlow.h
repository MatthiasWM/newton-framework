
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

#pragma mark - conditions and loops -

class AST_BC_Branch : public AST_Bytecode {
public:
  AST_BC_Branch(Decompiler &d, int pc, int a, int b) : AST_Bytecode(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_Branch"; }
  int provides() override { return kBranch; }
  bool Resolved() override { return false; }
  ASTNode *ResolveLoop();
  ASTNode *ResolveBreak();
  ASTNode *Resolve(Pass pass) override;
  void Print(uint32_t flags = 0) override;
};

class AST_BC_BranchIfTrue : public AST_Consume1 {
public:
  AST_BC_BranchIfTrue(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_BranchIfTrue"; }
  int provides() override { return kProvidesUnknown; }
  int extracted(AST_BC_Branch *branch2, ASTNode *&it);
  
  ASTNode *ResolveWhileDo();
  ASTNode *Resolve(Pass pass) override;
  bool Resolved() override { return false; }
  void Print(uint32_t flags = 0) override;
};

class AST_BC_BranchIfFalse : public AST_Consume1 {
public:
  AST_BC_BranchIfFalse(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_BranchIfFalse"; }
  int provides() override { return kProvidesUnknown; }
  ASTNode *ResolveIfTheElse();
  ASTNode *ResolveRepeatUntil();
  ASTNode *Resolve(Pass pass) override;
  bool Resolved() override { return false; }
  void Print(uint32_t flags = 0) override;
};

class AST_BC_Return : public AST_Consume1 {
public:
  AST_BC_Return(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_Return"; }
  // Even though the return command leaves the function immediately,
  // technically it is an expression and leaves a value on the stack.
  // So `a := 3 + return 4;` is a valid statement. It compiles, but just never runs.
  int provides() override { return Resolved() ? 1 : kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

#pragma mark - For loop -

// (A=22) addend -- addend value
class AST_BC_IncrVar : public AST_Consume1 {
public:
  AST_BC_IncrVar(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_IncrVar"; }
  int provides() override { if (in_) return 2; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=23) incr index limit --
class AST_BC_BranchLoop : public AST_Bytecode {
public:
  AST_BC_BranchLoop(Decompiler &d, int pc, int a, int b);
  const char *Class() override { return "AST_BC_BranchLoop"; }
  int provides() override { return kProvidesUnknown; }
  int consumes() override { return 3; }
  bool Resolved() override { return false; }
  ASTNode *Resolve(Pass pass) override;
};

#pragma mark - Foreach loop -

// (A=24, B=17): object deeply -- iterator
class AST_BC_NewIter : public AST_Consume2 {
public:
  AST_BC_NewIter(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_NewIter"; }
  int provides() override { return kProvidesUnknown; }
  bool Resolved() override { return false; }
  ASTNode *Resolve(Pass pass) override;
  ASTNode *ResolveForeachSlotValueDo();
  ASTNode *ResolveForeachSlotValueCollect();
  void Print(uint32_t flags = 0) override;
};

// (A=0, B=5) iterator --
class AST_BC_IterNext : public AST_Consume1 {
public:
  AST_BC_IterNext(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_IterNext"; }
  int provides() override { return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=0, B=6) iterator -- done
class AST_BC_IterDone : public AST_Consume1 {
public:
  AST_BC_IterDone(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_IterDone"; }
  int provides() override { return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

#pragma mark - Exceptions -

// (A=25): sym1 pc1 sym2 pc2 ... symN pcN --
class AST_BC_NewHandler : public AST_ConsumeN {
public:
  AST_BC_NewHandler(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b*2) { }
  const char *Class() override { return "AST_BC_NewHandler"; }
  void Print(uint32_t flags = 0) override;
};

// (A=0, B=7): --
class AST_BC_PopHandlers : public AST_Bytecode {
public:
  AST_BC_PopHandlers(Decompiler &d, int pc, int a, int b) : AST_Bytecode(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_PopHandlers"; }
  int provides() override { return kProvidesNone; }
  // Don't know yet
  bool Resolved() override { return false; }
  void Print(uint32_t flags = 0) override;
};

#pragma mark - Calls -

// (A=0, B=4): func -- closure
class AST_BC_SetLexScope : public AST_Consume1 {
public:
  AST_BC_SetLexScope(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BC_SetLexScope"; }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=5): arg1 arg2 ... argN name -- result
// Calls a global function. The function arguments (if any) are on the stack
// in left-to-right order, followed by a symbol giving the name of the function
// to call. The B field contains the number of arguments on the stack.
class AST_BC_Call : public AST_ConsumeN {
public:
  AST_BC_Call(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "AST_BC_Call"; }
  // The following special functions (reserved words) need to be written "a op b"
  // mod, <<, >>, (note the precedence! 7, 8, 8)
  // HasVar(a) can also be written as "a exists", but both are legal
  // TODO: we should probably do this at creation time!
  ASTNode *Resolve(Pass pass) override;
  void Print(uint32_t flags = 0) override;
};

// (A=6): arg1 arg2 ... argN func -- result
// call ... with (...)
// Performs a function call. The function arguments (if any) are on the stack
// in left-to-right order, followed by the function object to call. The B field
// contains the number of arguments on the stack.
class AST_BC_Invoke : public AST_ConsumeN {
public:
  AST_BC_Invoke(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "AST_BC_Invoke"; }
  void Print(uint32_t flags = 0) override;
};

// (A=7): arg1 arg2 ... argN receiver name -- result
// Performs a message send or a conditional send (send if defined).
// The function arguments (if any) are on the stack in left-to-right order,
// followed by the message name (a symbol), followed by the receiver. The B
// field contains the number of arguments on the stack.
// \note the order of name and receiver is flipped in the original documentation!
class AST_BC_Send : public AST_ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  AST_BC_Send(Decompiler &d, int pc, int a, int b, bool ifDefined)
  : AST_ConsumeN(d, pc, a, b, b+2), ifDefined_(ifDefined) { }
  const char *Class() override { return "AST_BC_Send"; }
  void Print(uint32_t flags = 0) override;
};

// (A=9): arg1 arg2 ... argN name -- result
// inherited:Print(3);
// Performs an inherited message send. The function arguments (if any) are on
// the stack in left-to-right order, followed by the message name (a symbol).
// The B field contains the number of arguments on the stack.
class AST_BC_Resend : public AST_ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  AST_BC_Resend(Decompiler &d, int pc, int a, int b, bool ifDefined)
  : AST_ConsumeN(d, pc, a, b, b+1), ifDefined_(ifDefined) { }
  const char *Class() override { return "AST_BC_Resend"; }
  void Print(uint32_t flags = 0) override;
};




#endif  /* __MATT_AST_CONTROLFLOW_H */
