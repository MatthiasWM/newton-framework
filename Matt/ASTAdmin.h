
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


class AST_INVALID_Node : public ASTNode {
public:
  AST_INVALID_Node(Decompiler &d, int pc, int a, int b)
  : ASTNode(d, pc, a, b) { }
  const char *Class() override { return "AST_INVALID_Node"; }
  bool Resolved() override { return false; }
};


class AST_FirstNode : public ASTNode {
public:
  AST_FirstNode(Decompiler &d) : ASTNode(d) { }
  const char *Class() override { return "AST_FirstNode"; }
  int provides() override { return kSpecialNode; }
  void Print(uint32_t flags = 0) override { }
  bool Resolved() override { return true; }
};


class AST_LastNode : public ASTNode {
public:
  AST_LastNode(Decompiler &d) : ASTNode(d) { }
  const char *Class() override { return "AST_LastNode"; }
  int provides() override { return kSpecialNode; }
  void Print(uint32_t flags = 0) override { }
  bool Resolved() override { return true; }
};


class AST_Bytecode : public ASTNode {
protected:
  void PrintPathExpr(ASTNode *inNode);
public:
  AST_Bytecode(Decompiler &d, int pc, int a, int b)
  : ASTNode(d, pc, a, b) { }
  const char *Class() override { return "AST_Bytecode"; }
  bool Resolved() override { return false; }
};


class AST_Consume1 : public AST_Bytecode {
protected:
  ASTNode *in_ { nullptr };
public:
  AST_Consume1(Decompiler &d, int pc, int a, int b)
  : AST_Bytecode(d, pc, a, b) { }
  const char *Class() override { return "AST_Consume1"; }
  void PrintChildren(bool deep) override;
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  int consumes() override { return 1; }
  ASTNode *Resolve(Pass pass) override;
  bool Resolved() override { return (in_ != nullptr); }
};


class AST_Consume2 : public AST_Bytecode {
protected:
  ASTNode *in1_ { nullptr };
  ASTNode *in2_ { nullptr };
public:
  AST_Consume2(Decompiler &d, int pc, int a, int b)
  : AST_Bytecode(d, pc, a, b) { }
  const char *Class() override { return "AST_Consume2"; }
  void PrintChildren(bool deep) override;
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  int consumes() override { return 2; }
  ASTNode *Resolve(Pass pass) override;
  bool Resolved() override { return (in1_ != nullptr) && (in2_ != nullptr); }
  ASTNode *input1() { return in1_; }
  ASTNode *input2() { return in2_; }
};


class AST_ConsumeN : public AST_Bytecode {
protected:
  int numIns_;
  std::vector<ASTNode *> ins_;
public:
  AST_ConsumeN(Decompiler &d, int pc, int a, int b, int n)
  : AST_Bytecode(d, pc, a, b), numIns_(n) { }
  const char *Class() override { return "AST_ConsumeN"; }
  void PrintChildren(bool deep) override;
  int provides() override { if (ins_.empty()) return kProvidesUnknown; else return 1; }
  int consumes() override { return numIns_; }
  ASTNode *Resolve(Pass pass) override;
  bool Resolved() override { return (ins_.size() == (size_t)numIns_); }
  void PrintResolvedCall(int nArgs);
};


#endif  /* __MATT_AST_ADMIN_H */
