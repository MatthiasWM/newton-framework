
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
  void Print(uint32_t flags = 0) override;
  void PrintNode(bool deep) override;
  int provides() override { return kJumpTarget; }
  int Origin() { return origin_; }
  /// Node can never be resolved, but will be removed if all origins were resolved
  bool Resolved() override { return false; }
};


class CodeBlock : public Node {
public:
  int provides_ = kProvidesNone;
  std::vector<Node*> body_;
  void moveToBody(Node *nd, int numNodes, std::vector<Node*> &body);
  void PrintBody(const std::string &prolog,
                 const std::string &separator,
                 const std::string &epilog,
                 std::vector<Node*> &body);
public:
  CodeBlock(Decompiler &d, int pc, int inProvides);
  const char *Class() override { return "CodeBlock"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
  void add(Node *nd);
  int size() { return (int)body_.size(); }
  Node *at(int ix) { return body_.at(ix); }
  void moveToBody(Node *nd, int numNodes) { moveToBody(nd, numNodes, body_); }
  int provides() override { return provides_; }
  bool Resolved() override { return true; }
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
  CFForEachSlotValueDo(Decompiler &d, int pc, Node *obj, int slot, int value, bool deeply);
  const char *Class() override { return "CFForEachSlotDo"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

}; // namespace ast;

#endif  /* __MATT_AST_CONTROLFLOWHELPER_H */
