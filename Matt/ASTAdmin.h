
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

namespace ast {

class INVALID_Node : public Node {
public:
  INVALID_Node(Decompiler &d, int pc, int a, int b)
  : Node(d, pc, a, b) { }
  const char *Class() override { return "INVALID_Node"; }
  bool Resolved() override { return false; }
};


class FirstNode : public Node {
public:
  FirstNode(Decompiler &d) : Node(d) { }
  const char *Class() override { return "FirstNode"; }
  int provides() override { return kSpecialNode; }
  void Print(uint32_t flags = 0) override { }
  bool Resolved() override { return true; }
};


class LastNode : public Node {
public:
  LastNode(Decompiler &d) : Node(d) { }
  const char *Class() override { return "LastNode"; }
  int provides() override { return kSpecialNode; }
  void Print(uint32_t flags = 0) override { }
  bool Resolved() override { return true; }
};


class Bytecode : public Node {
protected:
  void PrintPathExpr(Node *inNode);
public:
  Bytecode(Decompiler &d, int pc, int a, int b)
  : Node(d, pc, a, b) { }
  const char *Class() override { return "Bytecode"; }
  bool Resolved() override { return false; }
};


class Consume1 : public Bytecode {
protected:
  Node *in_ { nullptr };
public:
  Consume1(Decompiler &d, int pc, int a, int b)
  : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "Consume1"; }
  void PrintChildren(bool deep) override;
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  int consumes() override { return 1; }
  Node *Input() { return in_; }
  Node *Resolve(Pass pass) override;
  bool Resolved() override { return (in_ != nullptr); }
};


class Consume2 : public Bytecode {
protected:
  Node *in1_ { nullptr };
  Node *in2_ { nullptr };
public:
  Consume2(Decompiler &d, int pc, int a, int b)
  : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "Consume2"; }
  void PrintChildren(bool deep) override;
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  int consumes() override { return 2; }
  Node *Resolve(Pass pass) override;
  bool Resolved() override { return (in1_ != nullptr) && (in2_ != nullptr); }
  Node *input1() { return in1_; }
  Node *input2() { return in2_; }
};


class ConsumeN : public Bytecode {
protected:
  int numIns_;
  std::vector<Node *> ins_;
public:
  ConsumeN(Decompiler &d, int pc, int a, int b, int n)
  : Bytecode(d, pc, a, b), numIns_(n) { }
  const char *Class() override { return "ConsumeN"; }
  void PrintChildren(bool deep) override;
  int provides() override { if (ins_.empty()) return kProvidesUnknown; else return 1; }
  int consumes() override { return numIns_; }
  Node *Resolve(Pass pass) override;
  bool Resolved() override { return (ins_.size() == (size_t)numIns_); }
  void PrintResolvedCall(int nArgs);
};

}; // namespace ast

#endif  /* __MATT_AST_ADMIN_H */
