
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


class AST_JumpTarget : public ASTNode {
  int origin_ { -1 }; // Initialize to impossible pc.
public:
  AST_JumpTarget(Decompiler &d, int pc, int origin) : ASTNode(d, pc), origin_(origin) { }
  const char *Class() override { return "AST_JumpTarget"; }
  void Print(uint32_t flags = 0) override;
  void PrintNode(bool deep) override;
  int provides() override { return kJumpTarget; }
  int Origin() { return origin_; }
  /// Node can never be resolved, but will be removed if all origins were resolved
  bool Resolved() override { return false; }
};


class AST_CodeBlock : public ASTNode {
public:
  int provides_ = kProvidesNone;
  std::vector<ASTNode*> body_;
  void moveToBody(ASTNode *nd, int numNodes, std::vector<ASTNode*> &body);
  void PrintBody(const std::string &prolog,
                 const std::string &separator,
                 const std::string &epilog,
                 std::vector<ASTNode*> &body);
public:
  AST_CodeBlock(Decompiler &d, int pc, int inProvides);
  const char *Class() override { return "AST_CodeBlock"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
  void add(ASTNode *nd);
  void moveToBody(ASTNode *nd, int numNodes) { moveToBody(nd, numNodes, body_); }
  int provides() override { return provides_; }
  bool Resolved() override { return true; }
};

class AST_ControlBlock : public ASTNode {
public:
  int provides_ = kProvidesNone;
  ASTNode *body_;
public:
  AST_ControlBlock(Decompiler &d, int pc, int inProvides);
  const char *Class() override { return "AST_ControlBlock"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override = 0;
  int provides() override { return provides_; }
  bool Resolved() override { return true; }
};

class AST_CF_Loop : public AST_ControlBlock {
public:
  AST_CF_Loop(Decompiler &d, int pc, int prov, ASTNode *body);
  const char *Class() override { return "AST_CF_Loop"; }
  void Print(uint32_t flags = 0) override;
};

class AST_CF_While : public AST_ControlBlock {
protected:
  ASTNode *cond_ { nullptr };
public:
  AST_CF_While(Decompiler &d, int pc, int prov, ASTNode *condition, ASTNode *body);
  const char *Class() override { return "AST_CF_While"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

class AST_CF_Repeat : public AST_ControlBlock {
protected:
  ASTNode *cond_ { nullptr };
public:
  AST_CF_Repeat(Decompiler &d, int pc, int prov, ASTNode *condition, ASTNode *body);
  const char *Class() override { return "AST_CF_Repeat"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

/**
 \brief This node replaces an if/then/else pattern.
 \note In an if/then/else expression, if the else-branch is just pushing the
 `nil` constant, the else-branch need not be printed as a script.
 \note if this creates a short `if a then b else nil` expression, this may
 originally have been an `a and b` statement.
 \see AST_BC_BranchIfFalse
 */
class AST_CF_IfThen: public AST_ControlBlock {
public:
  ASTNode *cond_ { nullptr };
  ASTNode *elseBody_ = nullptr;
public:
  AST_CF_IfThen(Decompiler &d, int pc, ASTNode *condition, bool returnsAValue);
  const char *Class() override { return "AST_CF_IfThen"; }
  void PrintChildren(bool deep) override;
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
};

class AST_CF_Break : public ASTNode {
  ASTNode *in_ = nullptr;
public:
  AST_CF_Break(Decompiler &d, int origin, int target, ASTNode *input);
  const char *Class() override { return "AST_CF_Break"; }
  int provides() override { return kProvidesNone; }
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
};

class AST_CF_ForLoop : public AST_ControlBlock {
  ASTNode *iter_ = nullptr;
  ASTNode *limit_ = nullptr;
  ASTNode *incr_ = nullptr;
public:
  AST_CF_ForLoop(Decompiler &d, int pc, int prov, ASTNode *iter, ASTNode *limit, ASTNode *incr);
  const char *Class() override { return "AST_CF_ForLoop"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};

class AST_CF_ForEachSlotValueDo : public AST_ControlBlock {
  ASTNode *object_ = nullptr;
  int slot_ = -1;
  int value_ = -1;
  bool deeply_ = false;
public:
  AST_CF_ForEachSlotValueDo(Decompiler &d, int pc, ASTNode *obj, int slot, int value, bool deeply);
  const char *Class() override { return "AST_CF_ForEachSlotDo"; }
  void PrintChildren(bool deep) override;
  void Print(uint32_t flags = 0) override;
};


#endif  /* __MATT_AST_CONTROLFLOWHELPER_H */
