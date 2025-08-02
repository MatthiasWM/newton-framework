
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
  void Print() override;
  void PrintNode(bool deep) override;
  int provides() override { return kJumpTarget; }
  int Origin() { return origin_; }
  /// Node can never be resolved, but will be removed if all origins were resolved
  bool Resolved() override { return false; }
};


class AST_CodeBlock : public ASTNode {
protected:
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
  void add(ASTNode *nd) { body_.push_back(nd); }
  void moveToBody(ASTNode *nd, int numNodes) { moveToBody(nd, numNodes, body_); }
  int provides() override { return provides_; }
  bool Resolved() override { return true; }
};

class AST_CF_Loop : public AST_CodeBlock {
public:
  AST_CF_Loop(Decompiler &d, int pc);
  const char *Class() override { return "AST_CF_Loop"; }
  void Print() override;
};

class AST_CF_While : public AST_CodeBlock {
protected:
  ASTNode *cond_ { nullptr };
public:
  AST_CF_While(Decompiler &d, int pc, ASTNode *condition);
  const char *Class() override { return "AST_CF_While"; }
  void PrintChildren(bool deep) override;
  void Print() override;
};

class AST_CF_Repeat : public AST_CodeBlock {
protected:
  ASTNode *cond_ { nullptr };
public:
  AST_CF_Repeat(Decompiler &d, int pc, ASTNode *condition);
  const char *Class() override { return "AST_CF_Repeat"; }
  void PrintChildren(bool deep) override;
  void Print() override;
};

/**
 \brief This node replaces an if/then/else pattern.
 \note In an if/then/else expression, if the else-branch is just pushing the
 `nil` constant, the else-branch need not be printed as a script.
 \note if this creates a short `if a then b else nil` expression, this may
 originally have been an `a and b` statement.
 \see AST_BC_BranchIfFalse
 */
class AST_CF_IfThen: public AST_CodeBlock {
  ASTNode *cond_ { nullptr };
  std::vector<ASTNode*> elseBody_;
public:
  AST_CF_IfThen(Decompiler &d, int pc, ASTNode *condition, bool returnsAValue);
  const char *Class() override { return "AST_CF_IfThen"; }
  void PrintChildren(bool deep) override;
  bool Resolved() override { return true; }
  void Print() override;

  void moveToIfBody(ASTNode *nd, int numNodes) { moveToBody(nd, numNodes, body_); }
  void moveToElseBody(ASTNode *nd, int numNodes) { moveToBody(nd, numNodes, elseBody_); }

};

class AST_CF_Break : public ASTNode {
  ASTNode *in_ = nullptr;
public:
  AST_CF_Break(Decompiler &d, int origin, int target, ASTNode *input);
  const char *Class() override { return "AST_CF_Break"; }
  int provides() override { return kProvidesNone; }
  bool Resolved() override { return true; }
  void Print() override;
};

class AST_CF_ForLoop : public AST_CodeBlock {
  ASTNode *iter_ = nullptr;
  ASTNode *limit_ = nullptr;
  ASTNode *incr_ = nullptr;
public:
  AST_CF_ForLoop(Decompiler &d, int pc, ASTNode *iter, ASTNode *limit, ASTNode *incr);
  const char *Class() override { return "AST_CF_ForLoop"; }
  void PrintChildren(bool deep) override;
  void Print() override;
};

class AST_CF_ForEachSlotDo : public AST_CodeBlock {
  ASTNode *iter_ = nullptr;
  ASTNode *limit_ = nullptr;
  ASTNode *incr_ = nullptr;
public:
  AST_CF_ForEachSlotDo(Decompiler &d, int pc, ASTNode *iter, ASTNode *limit, ASTNode *incr);
  const char *Class() override { return "AST_CF_ForEachSlotDo"; }
  void PrintChildren(bool deep) override;
  void Print() override;
};


#endif  /* __MATT_AST_CONTROLFLOWHELPER_H */
