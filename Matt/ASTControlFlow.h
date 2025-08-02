
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

class AST_Branch : public ASTBytecodeNode {
public:
  AST_Branch(Decompiler &d, int pc, int a, int b) : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_Branch"; }
  int provides() override { return kBranch; }
  bool Resolved() override { return false; }
  ASTNode *ResolveLoop();
  ASTNode *ResolveBreak();
  ASTNode *Resolve(Pass pass) override;
  void Print() override;
};

class AST_BranchIfTrue : public AST_Consume1 {
public:
  AST_BranchIfTrue(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BranchIfTrue"; }
  int provides() override { if (in_) return kBranchIfTrue; else return kProvidesUnknown; }
  ASTNode *Resolve(Pass pass) override;
  void Print() override;
};

class AST_BranchIfFalse : public AST_Consume1 {
public:
  AST_BranchIfFalse(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BranchIfFalse"; }
  int provides() override { if (in_) return kBranchIfFalse; else return kProvidesUnknown; }
  ASTNode *ResolveIfTheElse();
  ASTNode *ResolveRepeatUntil();
  ASTNode *Resolve(Pass pass) override;
  bool Resolved() override { return false; }
  void Print() override;
};

class AST_Return : public AST_Consume1 {
public:
  AST_Return(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_Return"; }
  // Even though the return command leaves the function immediately,
  // technically it is an expression and leaves a value on the stack.
  // So `a := 3 + return 4;` is a valid statement. It compiles, but just never runs.
  int provides() override { return Resolved() ? 1 : kProvidesUnknown; }
  void Print() override;
};

// (A=0, B=4): func -- closure
class AST_SetLexScope : public AST_Consume1 {
public:
  AST_SetLexScope(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_SetLexScope"; }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print() override;
};

// (A=22) addend -- addend value
class AST_IncrVar : public AST_Consume1 {
public:
  AST_IncrVar(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_IncrVar"; }
  int provides() override { if (in_) return 2; else return kProvidesUnknown; }
  void Print() override;
};

// (A=0, B=5) iterator --
class AST_IterNext : public AST_Consume1 {
public:
  AST_IterNext(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_IterNext"; }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print() override;
};

// (A=0, B=6) iterator -- done
class AST_IterDone : public AST_Consume1 {
public:
  AST_IterDone(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_IterDone"; }
  int provides() override { if (in_) return 1; else return kProvidesUnknown; }
  void Print() override;
};

// (A=24, B=17): object deeply -- iterator
class AST_NewIter : public AST_Consume2 {
public:
  AST_NewIter(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  const char *Class() override { return "AST_NewIter"; }
  bool Resolved() override { return false; }
  void Print() override;
};

// (A=23) incr index limit --
class AST_BranchLoop : public ASTBytecodeNode {
protected:
  ASTNode *incr_ { nullptr };
  ASTNode *index_ { nullptr };
  ASTNode *limit_ { nullptr };
public:
  AST_BranchLoop(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_BranchLoop"; }
  void PrintChildren(bool deep) override;
  int provides() override { if (incr_ && index_ && limit_) return kProvidesNone; else return kProvidesUnknown; }
  int consumes() override { return 3; }
  bool Resolved() override { return (incr_ != nullptr) && (index_ != nullptr) && (limit_ != nullptr); }
  void Print() override;
};

#pragma mark - Exceptions -

// (A=25): sym1 pc1 sym2 pc2 ... symN pcN --
class AST_NewHandler : public AST_ConsumeN {
public:
  AST_NewHandler(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b*2) { }
  const char *Class() override { return "AST_NewHandler"; }
  void Print() override;
};

// (A=0, B=7): --
class AST_PopHandlers : public ASTBytecodeNode {
public:
  AST_PopHandlers(Decompiler &d, int pc, int a, int b) : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_PopHandlers"; }
  int provides() override { return kProvidesNone; }
  // Don't know yet
  bool Resolved() override { return false; }
  void Print() override;
};

#pragma mark - Calls -

// (A=5): arg1 arg2 ... argN name -- result
// Calls a global function. The function arguments (if any) are on the stack
// in left-to-right order, followed by a symbol giving the name of the function
// to call. The B field contains the number of arguments on the stack.
class AST_Call : public AST_ConsumeN {
public:
  AST_Call(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "AST_Call"; }
  // The following special functions (reserved words) need to be written "a op b"
  // mod, <<, >>, (note the precedence! 7, 8, 8)
  // HasVar(a) can also be written as "a exists", but both are legal
  // TODO: we should probably do this at creation time!
  ASTNode *Resolve(Pass pass) override;
  void Print() override;
};

// (A=6): arg1 arg2 ... argN func -- result
// call ... with (...)
// Performs a function call. The function arguments (if any) are on the stack
// in left-to-right order, followed by the function object to call. The B field
// contains the number of arguments on the stack.
class AST_Invoke : public AST_ConsumeN {
public:
  AST_Invoke(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "AST_Invoke"; }
  void Print() override;
};

// (A=7): arg1 arg2 ... argN receiver name -- result
// Performs a message send or a conditional send (send if defined).
// The function arguments (if any) are on the stack in left-to-right order,
// followed by the message name (a symbol), followed by the receiver. The B
// field contains the number of arguments on the stack.
// \note the order of name and receiver is flipped in the original documentation!
class AST_Send : public AST_ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  AST_Send(Decompiler &d, int pc, int a, int b, bool ifDefined)
  : AST_ConsumeN(d, pc, a, b, b+2), ifDefined_(ifDefined) { }
  const char *Class() override { return "AST_Send"; }
  void Print() override;
};

// (A=9): arg1 arg2 ... argN name -- result
// inherited:Print(3);
// Performs an inherited message send. The function arguments (if any) are on
// the stack in left-to-right order, followed by the message name (a symbol).
// The B field contains the number of arguments on the stack.
class AST_Resend : public AST_ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  AST_Resend(Decompiler &d, int pc, int a, int b, bool ifDefined)
  : AST_ConsumeN(d, pc, a, b, b+1), ifDefined_(ifDefined) { }
  const char *Class() override { return "AST_Resend"; }
  void Print() override;
};




#endif  /* __MATT_AST_CONTROLFLOW_H */
