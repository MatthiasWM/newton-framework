
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


class ASTJumpTarget : public ASTNode {
  int origin_ { -1 }; // Initialize to impossible pc.
public:
  ASTJumpTarget(Decompiler &d, int pc, int origin) : ASTNode(d, pc), origin_(origin) { }
  const char *Class() override { return "ASTJumpTarget"; }
  void Print() override;
  void PrintNode(bool deep) override;
  int provides() override { return kJumpTarget; }
  int Origin() { return origin_; }
  /// Node can never be resolved, but will be removed if all origins were resolved
  bool Resolved() override { return false; }
};


class ASTCodeBlock : public ASTNode {
protected:
  int provides_ = kProvidesNone;
  std::vector<ASTNode*> body_;
  void moveToBody(ASTNode *nd, int numNodes, std::vector<ASTNode*> &body);
  void PrintBody(const std::string &prolog,
                 const std::string &separator,
                 const std::string &epilog,
                 std::vector<ASTNode*> &body);
public:
  ASTCodeBlock(Decompiler &d, int pc, int inProvides);
  const char *Class() override { return "ASTCodeBlock"; }
  void PrintChildren(bool deep) override;
  void add(ASTNode *nd) { body_.push_back(nd); }
  void moveToBody(ASTNode *nd, int numNodes) { moveToBody(nd, numNodes, body_); }
  int provides() override { return provides_; }
  bool Resolved() override { return true; }
};

class ASTLoop : public ASTCodeBlock {
public:
  ASTLoop(Decompiler &d, int pc);
  const char *Class() override { return "ASTLoop"; }
  void Print() override;
};

class ASTWhileDo : public ASTCodeBlock {
protected:
  ASTNode *cond_ { nullptr };
public:
  ASTWhileDo(Decompiler &d, int pc, ASTNode *condition);
  const char *Class() override { return "ASTWhileDo"; }
  void PrintChildren(bool deep) override;
  void Print() override;
};

class ASTRepeatUntil : public ASTCodeBlock {
protected:
  ASTNode *cond_ { nullptr };
public:
  ASTRepeatUntil(Decompiler &d, int pc, ASTNode *condition);
  const char *Class() override { return "ASTRepeatUntil"; }
  void PrintChildren(bool deep) override;
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
class ASTIfThenElseNode: public ASTCodeBlock {
  ASTNode *cond_ { nullptr };
  std::vector<ASTNode*> elseBody_;
public:
  ASTIfThenElseNode(Decompiler &d, int pc, ASTNode *condition, bool returnsAValue);
  const char *Class() override { return "ASTIfThenElseNode"; }
  void PrintChildren(bool deep) override;
  bool Resolved() override { return true; }
  void Print() override;

  void moveToIfBody(ASTNode *nd, int numNodes) { moveToBody(nd, numNodes, body_); }
  void moveToElseBody(ASTNode *nd, int numNodes) { moveToBody(nd, numNodes, elseBody_); }

};

class AST_Break : public ASTNode {
  ASTNode *in_ = nullptr;
public:
  AST_Break(Decompiler &d, int origin, int target, ASTNode *input);
  const char *Class() override { return "AST_Break"; }
  int provides() override { return kProvidesNone; }
  bool Resolved() override { return true; }
  void Print() override;
};

#endif  /* __MATT_AST_CONTROLFLOWHELPER_H */
