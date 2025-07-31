
/*
 File:    Matt/ASTControlFlow.h

 Matt's decompiler Abstract Syntax Tree.
 Control Flow nodes.

 Written by:  Matt, 2025.
 */

#if !defined(__MATT_AST_CONTROLFLOW_H)
#define __MATT_AST_CONTROLFLOW_H 1

#include "Matt/ASTAdmin.h"

#include <vector>


class ASTJumpTarget : public ASTNode {
  int origin_ { -1 }; // Initialize to impossible pc.
public:
  ASTJumpTarget(Decompiler &d, int pc, int origin) : ASTNode(d, pc), origin_(origin) { }
  int provides() override { return kJumpTarget; }
  int Origin() { return origin_; }
  /// Node can never be resolved, but will be removed if all origins were resolved
  bool Resolved() override { return false; }
  void Print() override;
};

/**
 \brief This node is used to replace a `loop` construct when detected by AST_Branch.
 */
class ASTLoop : public ASTNode {
protected:
  std::vector<ASTNode*> body_;
public:
  ASTLoop(Decompiler &d, int pc) : ASTNode(d, pc) { }
  void add(ASTNode *nd) { body_.push_back(nd); }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print() override;
};

// (A=11): --
class AST_Branch : public ASTBytecodeNode {
public:
  AST_Branch(Decompiler &d, int pc, int a, int b) : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { return kBranch; }
  bool Resolved() override { return false; }
  auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> override;
  void Print() override;
};

class ASTWhileDo : public ASTNode {
protected:
  ASTNode *cond_ { nullptr };
  std::vector<ASTNode*> body_;
public:
  ASTWhileDo(Decompiler &d, int pc, ASTNode *condition) : ASTNode(d, pc), cond_(condition) { }
  void add(ASTNode *nd) { body_.push_back(nd); }
  int provides() override { return kProvidesNone; }
  bool Resolved() override { return true; }
  void PrintChildren();
  void Print() override;
};

class ASTRepeatUntil : public ASTNode {
protected:
  ASTNode *cond_ { nullptr };
  std::vector<ASTNode*> body_;
public:
  ASTRepeatUntil(Decompiler &d, int pc, ASTNode *condition) : ASTNode(d, pc), cond_(condition) { }
  void add(ASTNode *nd) { body_.push_back(nd); }
  int provides() override { return kProvidesNone; }
  bool Resolved() override { return true; }
  void PrintChildren();
  void Print() override;
};


// (A=12): value --
// A value is popped from the stack. If it is nil, execution continues with the
// next instruction. Otherwise, PC is set to the B field value.
class AST_BranchIfTrue : public AST_Consume1 {
public:
  AST_BranchIfTrue(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kBranchIfTrue; else return kProvidesUnknown; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override { return {false, next}; }
  auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> override;
  void Print() override;
};

/**
 \brief This node replaces an if/then/else pattern.
 \note In an if/then/else expression, if the else-branch is just pushing the
 `nil` constant, the else-branch need not be printed as a script.
 \note if this creates a short `if a then b else nil` expression, this may
 originally have been an `a and b` statement.
 \see AST_BranchIfFalse
 */
class ASTIfThenElseNode: public ASTNode {
  ASTNode *cond_ { nullptr };
  std::vector<ASTNode*> ifBranch_;
  std::vector<ASTNode*> elseBranch_;
  bool returnsAValue_ { false };
public:
  ASTIfThenElseNode(Decompiler &d, int pc, bool returnsAValue) : ASTNode(d, pc), returnsAValue_(returnsAValue) { }
  void setCond(ASTNode *nd) { cond_ = nd; }
  void addIf(ASTNode *nd) { ifBranch_.push_back(nd); }
  void addElse(ASTNode *nd) { elseBranch_.push_back(nd); }
  int provides() override { return returnsAValue_ ? 1 : 0; }
  /// This node only exists if all nodes involved are resolved.
  bool Resolved() override { return true; }
  void Print() override;
};

// (A=13): value --
/**
 \brief Based on this node, find the pattern of an if/then or if/then/else structure in the AST.

 This class checks for three different pattern, generating one of three possible variations
 of the ASTIfThenElseNode. If one of the pattern matches, the new ASTIfThenElseNode
 will replace all other code involved.

 Pattern one is a simple if/then statement:
 - BranchIfFalse B, n*statement, Target B

 The second pattern adds and 'else' branch:
 - BranchIfFalse A, n*statement, Branch B, Target A, n*statement, Target B

 A third pattern generates an expression instead of a statement, laving a ref on the stack.
 This pattern exists only as if/then/else. An missing 'else' branch in the source
 creates an 'else' branch that pushes 'nil':
 - BranchIfFalse A, n*statement, expr, Branch B, Target A, n*statement, expr, Target B

 \note if...then...else... creates the same bytecode as *and*. `a and b` generates
 `if a then b else nil`.
 \note `if not...` generates "not" and "BranchIfFalse" and is not optimized into "BranchIfTrue".
 \note BranchIfTrue is used to generated an `or` operation.
 \note A `break` command is not allowed in the branches unless the *if* stament
 is inside an other loop.
 \see ASTIfThenElseNode
 */
class AST_BranchIfFalse : public AST_Consume1 {
public:
  AST_BranchIfFalse(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kBranchIfFalse; else return kProvidesUnknown; }
  auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> override;
  auto ResolveBackwardBranch() -> std::tuple<bool, ASTNode*>;
  auto ResolveForwardBranch() -> std::tuple<bool, ASTNode*>;
  bool Resolved() override { return false; }
  void Print() override;
};

// TODO: the very last return probably doesn't need to be printed
// It's actually a bug in the newt-framework compiler. NTK does not generate the extra return bytecode
// TODO: return NIL is implied if there is no return statement in the source code
// TODO: handle implied return values nicely, so we don't generate "return a := b;"
// (A=0, B=2): --
class AST_Return : public AST_Consume1 {
public:
  AST_Return(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  // Even though the return command leaves the function immediately,
  // technically it is an expression and leaves a value on the stack.
  // So `a := 3 + return 4;` is a valid statement. It compiles, but just never runs.
  int provides() override { return Resolved() ? 1 : kProvidesUnknown; }
  void Print() override;
};

class AST_Break : public AST_Consume1 {
public:
  AST_Break(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override;
  void Print() override;
};


// (A=0, B=7): --
class AST_PopHandlers : public ASTBytecodeNode {
public:
  AST_PopHandlers(Decompiler &d, int pc, int a, int b) : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { return kProvidesNone; }
  // Don't know yet
  bool Resolved() override { return false; }
  void Print() override;
};

// (A=0, B=4): func -- closure
class AST_SetLexScope : public AST_Consume1 {
public:
  AST_SetLexScope(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print() override;
};

// (A=22) addend -- addend value
class AST_IncrVar : public AST_Consume1 {
public:
  AST_IncrVar(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return 2; else return kProvidesUnknown; }
  void Print() override;
};

// (A=0, B=5) iterator --
class AST_IterNext : public AST_Consume1 {
public:
  AST_IterNext(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print() override;
};

// (A=0, B=6) iterator -- done
class AST_IterDone : public AST_Consume1 {
public:
  AST_IterDone(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return 1; else return kProvidesUnknown; }
  void Print() override;
};

// (A=24, B=17): object deeply -- iterator
class AST_NewIter : public AST_Consume2 {
public:
  AST_NewIter(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  bool Resolved() override { return false; }
  void Print() override;
};

// (A=5): arg1 arg2 ... argN name -- result
// Calls a global function. The function arguments (if any) are on the stack
// in left-to-right order, followed by a symbol giving the name of the function
// to call. The B field contains the number of arguments on the stack.
class AST_Call : public AST_ConsumeN {
public:
  AST_Call(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  // The following special functions (reserved words) need to be written "a op b"
  // mod, <<, >>, (note the precedence! 7, 8, 8)
  // HasVar(a) can also be written as "a exists", but both are legal
  // TODO: we should probably do this at creation time!
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override;
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
  void Print() override;
};

// (A=25): sym1 pc1 sym2 pc2 ... symN pcN --
class AST_NewHandler : public AST_ConsumeN {
public:
  AST_NewHandler(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b*2) { }
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
  int provides() override { if (incr_ && index_ && limit_) return kProvidesNone; else return kProvidesUnknown; }
  int consumes() override { return 3; }
  bool Resolved() override { return (incr_ != nullptr) && (index_ != nullptr) && (limit_ != nullptr); }
  void PrintChildren();
  void Print() override;
};




#endif  /* __MATT_AST_CONTROLFLOW_H */
