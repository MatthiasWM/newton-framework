
/*
 File:    Matt/ASTControlFlow.h

 Matt's decompiler Abstract Syntax Tree.
 Control Flow nodes that are created for script output.

 Written by:  Matt, 2025.
 */

#if !defined(__MATT_AST_CONTROLFLOWHELPER_H)
#define __MATT_AST_CONTROLFLOWHELPER_H 1

#include "Matt/ASTAdmin.h"

#include <vector>

namespace ast {

class JumpTarget : public Node {
  int origin_ { -1 }; // Initialize to impossible pc.
public:
  JumpTarget(Decompiler &d, int pc, int origin) : Node(d, pc), origin_(origin) { }
  const char *Class() override { return "JumpTarget"; }
  pattern::Tag tag() const override { return pattern::Tag::JumpTarget; }
  void Print(uint32_t flags = 0) override;
  void PrintNode(bool deep) override;
  int provides() override { return kJumpTarget; }
  int Origin() { return origin_; }
  /// Node can never be resolved, but will be removed if all origins were resolved
  bool Resolved() override { return false; }
};

/**
 \brief Wraps a NewtonScript compound expression (`begin stmt1; stmt2;
 value end`) used as a single inline value -- specifically, a `while`
 loop's or `a or b`'s own condition, when that condition is itself more
 than one bytecode instruction (e.g. `i := StrPos(...); i` -- a statement
 computing a value, immediately followed by reading it back as the actual
 boolean test). Built directly by BCBranchIfTrue::Resolve()
 (ASTControlFlow.cc), not by the pattern engine -- that method's own
 backward condition-consumption runs *before* the Builder chain for
 `while`/`or` even starts, and needs a single node reporting IsExpr()==true
 to hand back as `Input()`, the same as the far more common single-node
 case. Print() renders the whole chain inline (`begin ...; ... end`), never
 with PrintBodyChain()'s begin/end-suppression flags or PrintOnNewLine()'s
 line-wrapping -- this is a *value* embedded in a larger statement (`while
 <this> do ...`), not a printed body of its own.
 */
class CompoundExpr : public Node {
  Node *body_ = nullptr;
public:
  CompoundExpr(Decompiler &d, int pc, Node *body) : Node(d, pc), body_(body) { }
  const char *Class() override { return "CompoundExpr"; }
  int provides() override { return kProvidesOne; }
  bool Resolved() override { return true; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

class ControlBlock : public Node {
public:
  int provides_ = kProvidesNone;
  Node *body_;
public:
  ControlBlock(Decompiler &d, int pc, int inProvides);
  const char *Class() override { return "ControlBlock"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override = 0;
  int provides() override { return provides_; }
  bool Resolved() override { return true; }
};

class CFLoop : public ControlBlock {
public:
  CFLoop(Decompiler &d, int pc, int prov, Node *body);
  const char *Class() override { return "CFLoop"; }
  void Print(uint32_t flags = 0) override;
};

class CFWhile : public ControlBlock {
protected:
  Node *cond_ { nullptr };
public:
  CFWhile(Decompiler &d, int pc, int prov, Node *condition, Node *body);
  const char *Class() override { return "CFWhile"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

class CFRepeat : public ControlBlock {
protected:
  Node *cond_ { nullptr };
public:
  CFRepeat(Decompiler &d, int pc, int prov, Node *condition, Node *body);
  const char *Class() override { return "CFRepeat"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

/**
 \brief This node replaces an if/then/else pattern.
 \note In an if/then/else expression, if the else-branch is just pushing the
 `nil` constant, the else-branch need not be printed as a script.
 \note if this creates a short `if a then b else nil` expression, this may
 originally have been an `a and b` statement.
 \see BCBranchIfFalse
 */
class CFIfThen: public ControlBlock {
public:
  Node *cond_ { nullptr };
  Node *elseBody_ = nullptr;
public:
  CFIfThen(Decompiler &d, int pc, Node *condition, bool returnsAValue);
  const char *Class() override { return "CFIfThen"; }
  void PrintChildren(bool deep) override;
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
};

class CFOr: public ControlBlock {
public:
  Node *left_ = nullptr;
  Node *right_ = nullptr;
public:
  CFOr(Decompiler &d, int pc, Node *left, Node *right);
  const char *Class() override { return "CFOr"; }
  void PrintChildren(bool deep) override;
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
};

class CFBreak : public Node {
  Node *in_ = nullptr;
public:
  CFBreak(Decompiler &d, int origin, int target, Node *input);
  const char *Class() override { return "CFBreak"; }
  int provides() override { return kProvidesNone; }
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
};

class CFForLoop : public ControlBlock {
  Node *iter_ = nullptr;
  Node *limit_ = nullptr;
  Node *incr_ = nullptr;
public:
  CFForLoop(Decompiler &d, int pc, int prov, Node *iter, Node *limit, Node *incr, Node *body);
  const char *Class() override { return "CFForLoop"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

class CFForEachSlotValueDo : public ControlBlock {
  Node *object_ = nullptr;
  int slot_ = -1;
  int value_ = -1;
  bool deeply_ = false;
public:
  CFForEachSlotValueDo(Decompiler &d, int pc, int slot, int value, bool deeply, Node *obj, Node *body);
  const char *Class() override { return "CFForEachSlotDo"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

/**
 \brief `foreach [slot,] value [deeply] in object collect body end`.
 Unlike CFForEachSlotValueDo, `body` here is always a single expression
 (never a statement / multi-statement chain) -- it's whatever gets assigned
 into the result array on each iteration, not a free-form loop body -- so
 Print() never needs PrintOnNewLine()'s begin/end handling.
 \see BCNewIter::Resolve(), Matt/ASTControlFlowPatterns.cc (BuildForeachCollectPattern)
 */
class CFForEachSlotValueCollect : public ControlBlock {
  Node *object_ = nullptr;
  int slot_ = -1;
  int value_ = -1;
  bool deeply_ = false;
public:
  CFForEachSlotValueCollect(Decompiler &d, int pc, int slot, int value, bool deeply, Node *obj, Node *body);
  const char *Class() override { return "CFForEachSlotValueCollect"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

class ExceptionHandler : public JumpTarget {
protected:
  int excp_ = -1;
  Node *body_ = nullptr;
public:
  ExceptionHandler(Decompiler &d, int pc, int origin, int excp)
  : JumpTarget(d, pc, origin), excp_(excp) { }
  const char *Class() override { return "ExceptionHandler"; }
  pattern::Tag tag() const override { return pattern::Tag::ExceptionHandler; }
  void Body(Node *body) { body_ = body; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

class CFTry : public Node {
protected:
  int provides_ = kProvidesNone;
  Node *body_ = nullptr;
  std::vector<ExceptionHandler*> exList_;
public:
  CFTry(Decompiler &d, int pc, int provides, Node *first, Node *last);
  const char *Class() override { return "CFTry"; }
  bool Resolved() override { return true; }
  int provides() override { return provides_; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags) override;
};

}; // namespace ast;

#endif  /* __MATT_AST_CONTROLFLOWHELPER_H */
