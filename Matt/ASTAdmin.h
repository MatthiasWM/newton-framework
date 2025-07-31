
/*
 File:    Matt/ASTAdmin.h

 Matt's decompiler Abstract Syntax Tree.
 Administrative nodes.

 Written by:  Matt, 2025.
 */

#if !defined(__MATT_AST_ADMIN_H)
#define __MATT_AST_ADMIN_H 1

#include "Matt/AST.h"

#include <vector>


class ASTFirstNode : public ASTNode {
public:
  ASTFirstNode(Decompiler &d) : ASTNode(d) { }
  int provides() override { return kSpecialNode; }
  bool Resolved() override { return true; }
  void Print() override;
};


class ASTLastNode : public ASTNode {
public:
  ASTLastNode(Decompiler &d) : ASTNode(d) { }
  int provides() override { return kSpecialNode; }
  bool Resolved() override { return true; }
  void Print() override;
};


class ASTBytecodeNode : public ASTNode {
protected:
  int a_ = 0;
  int b_ = 0;
  void PrintPathExpr(ASTNode *inNode);
public:
  ASTBytecodeNode(Decompiler &d, int pc, int a, int b)
  : ASTNode(d, pc), a_(a), b_(b) { }
  int b() { return b_; }
  bool Resolved() override { return false; }
  void Print() override;
};


class AST_Consume1 : public ASTBytecodeNode {
protected:
  ASTNode *in_ { nullptr };
public:
  AST_Consume1(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  int consumes() override { return 1; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override;
  bool Resolved() override { return (in_ != nullptr); }
  void PrintChildren();
  void Print() override;
};


class AST_Consume2 : public ASTBytecodeNode {
protected:
  ASTNode *in1_ { nullptr };
  ASTNode *in2_ { nullptr };
public:
  AST_Consume2(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  int consumes() override { return 2; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override;
  bool Resolved() override { return (in1_ != nullptr) && (in2_ != nullptr); }
  void PrintChildren();
  void Print() override;
};


class AST_ConsumeN : public ASTBytecodeNode {
protected:
  int numIns_;
  std::vector<ASTNode *> ins_;
public:
  AST_ConsumeN(Decompiler &d, int pc, int a, int b, int n)
  : ASTBytecodeNode(d, pc, a, b), numIns_(n) { }
  int provides() override { if (ins_.empty()) return kProvidesUnknown; else return 1; }
  int consumes() override { return numIns_; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override;
  bool Resolved() override { return (ins_.size() == (size_t)numIns_); }
  void PrintChildren();
  void Print() override;
  void PrintResolvedCall(int nArgs);
};


#endif  /* __MATT_AST_ADMIN_H */
