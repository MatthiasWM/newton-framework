
/*
 File:    Matt/ASTDataFlow.h

 Matt's decompiler Abstract Syntax Tree.
 Data Flow nodes.

 Written by:  Matt, 2025.
 */

#if !defined(__MATT_AST_DATAFLOW_H)
#define __MATT_AST_DATAFLOW_H 1

#include "Matt/ASTAdmin.h"

namespace ast {

// (A=3): -- literal
class BCPush : public Bytecode {
public:
  BCPush(Decompiler &d, int pc, int a, int b) : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "BCPush"; }
  int provides() override { return 1; }
  bool IsSymbol() override;
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
};

// (A=4, B=signed): -- value
class BCPushConst : public Bytecode {
public:
  BCPushConst(Decompiler &d, int pc, int a, int b)
  : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "BCPushConst"; }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
  bool IsNIL() override;
};

// (A=0, B=3):  -- RCVR
class BCPushSelf : public Bytecode {
public:
  BCPushSelf(Decompiler &d, int pc, int a, int b)
  : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "BCPushSelf"; }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
};

// (A=14): -- value
// Performs a variable lookup. The B field is the zero-based index in the
// literals array of a symbol (here called name) naming the variable.
class BCFindVar : public Bytecode {
public:
  BCFindVar(Decompiler &d, int pc, int a, int b)
  : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "BCFindVar"; }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
};

// (A=15): -- value
class BCGetVar : public Bytecode {
public:
  BCGetVar(Decompiler &d, int pc, int a, int b)
  : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "BCGetVar"; }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(uint32_t flags = 0) override;
};

// (A=0, B=0): value --
class BCPop : public Consume1 {
  using super = Consume1;
public:
  BCPop(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCPop"; }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  Node *Resolve(Pass pass) override;
  void Print(uint32_t flags = 0) override;
};

// (A=0, B=1): x -- x x
class BCDup : public Consume1 {
public:
  BCDup(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCDup"; }
  // TODO: provides(2) does not match any consumers!
  // NOTE: we must find an actual use case and the corresponding source code
  // NOTE: it may make sense to split this into a dup1 and dup2 node?!
  int provides() override { if (Resolved()) return 2; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};


// (A=20): value --
class BCSetVar : public Consume1 {
public:
  BCSetVar(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCSetVar"; }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
  Node *input() { return in_; }
};

// (A=21): value --
class BCFindAndSetVar : public Consume1 {
public:
  BCFindAndSetVar(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCFindAndSetVar"; }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=5): value -- value
class BCNot : public Consume1 {
public:
  BCNot(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCNot"; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=18): object -- length
class BCLength : public Consume1 {
public:
  BCLength(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCLength"; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=19): object -- clone
class BCClone : public Consume1 {
public:
  BCClone(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCClone"; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=22): array -- string
class BCStringer : public Consume1 {
public:
  BCStringer(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCStringer"; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=24): object -- class
class BCClassOf : public Consume1 {
public:
  BCClassOf(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCClassOf"; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=16): num -- result
class BCBitNot : public Consume1 {
public:
  BCBitNot(Decompiler &d, int pc, int a, int b) : Consume1(d, pc, a, b) { }
  const char *Class() override { return "BCBitNot"; }
  void Print(uint32_t flags = 0) override;
};

class BinaryExpression : public Consume2 {
public:
  BinaryExpression(Decompiler &d, int pc, int a, int b)
  : Consume2(d, pc, a, b) { }
  const char *Class() override { return "BinaryExpression"; }
  Node *Resolve(Pass pass) override;
};

// num1 num2 -- result
// bAnd, bOr
class BinaryFunction : public BinaryExpression {
protected:
  const char *func_ { nullptr };
public:
  BinaryFunction(Decompiler &d, int pc, int a, int b, const char *func)
  : BinaryExpression(d, pc, a, b), func_(func) { }
  const char *Class() override { return "BinaryFunction"; }
  void Print(uint32_t flags = 0) override;
};

// num1 num2 -- result
// +, -, *, /, div, =, <>, <, <=, >, >=
class BinaryOperator : public BinaryExpression {
protected:
  const char *op_ { nullptr };
  int precedence_ { 0 };
public:
  BinaryOperator(Decompiler &d, int pc, int a, int b, const char *op, int precedence)
  : BinaryExpression(d, pc, a, b), op_(op), precedence_(precedence)
  { }
  const char *Class() override { return "BinaryOperator"; }
  void Print(uint32_t flags = 0) override;
};

// (A=17, B=0xFFFF): size class -- array
class BCNewArray : public Consume2 {
public:
  BCNewArray(Decompiler &d, int pc, int a, int b) : Consume2(d, pc, a, b) { }
  const char *Class() override { return "BCNewArray"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=18): object pathExpr -- value
// Retrieves the value corresponding to pathExpr in the frame or array object.
// The B field may be zero or one.
// The value corresponding to a path expression is the value that would be
// found by doing array and frame accesses corresponding to each element of
// the path expression. Integers represent array accesses to the given array
// index; symbols represent frame accesses to the given frame slot.
class BCGetPath : public Consume2 {
public:
  BCGetPath(Decompiler &d, int pc, int a, int b) : Consume2(d, pc, a, b) { }
  const char *Class() override { return "BCGetPath"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=2): object index -- element
class BCARef : public Consume2 {
public:
  BCARef(Decompiler &d, int pc, int a, int b) : Consume2(d, pc, a, b) { }
  const char *Class() override { return "BCARef"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=20): object class -- object
class BCSetClass : public Consume2 {
public:
  BCSetClass(Decompiler &d, int pc, int a, int b) : Consume2(d, pc, a, b) { }
  const char *Class() override { return "BCSetClass"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=21): array object -- object
class BCAddArraySlot : public Consume2 {
public:
  BCAddArraySlot(Decompiler &d, int pc, int a, int b) : Consume2(d, pc, a, b) { }
  const char *Class() override { return "BCAddArraySlot"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=23): object pathExpr -- result
class BCHasPath : public Consume2 {
public:
  BCHasPath(Decompiler &d, int pc, int a, int b) : Consume2(d, pc, a, b) { }
  const char *Class() override { return "BCHasPath"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(uint32_t flags = 0) override;
};

// (A=19)
//    B=0: object pathExpr value --
//    B=1: object pathExpr value -- value
// Sets the value corresponding to pathExpr in the frame or array object
// to value.The B field may be zero or one. The object, path, and value are
// popped from the stack. If the B field is one, the value is
// pushed back onto the stack.
class SetPath : public Bytecode {
protected:
  Node *object_ { nullptr };
  Node *path_ { nullptr };
  Node *value_ { nullptr };
public:
  SetPath(Decompiler &d, int pc, int a, int b)
  : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "SetPath"; }
  void PrintChildren(bool deep) override;
  int provides() override;
  int consumes() override { return 3; }
  Node *Resolve(Pass pass) override;
  bool Resolved() override { return (object_ != nullptr) && (path_ != nullptr) && (value_ != nullptr); }
  void Print(uint32_t flags = 0) override;
};

// (A=24, B=3): object index element -- element
class BCSetARef : public Bytecode {
protected:
  Node *object_ { nullptr };
  Node *index_ { nullptr };
  Node *element_ { nullptr };
public:
  BCSetARef(Decompiler &d, int pc, int a, int b)
  : Bytecode(d, pc, a, b) { }
  const char *Class() override { return "BCSetARef"; }
  void PrintChildren(bool deep) override;
  int provides() override { if (object_ && index_ && element_) return kProvidesOne; else return kProvidesUnknown; }
  int consumes() override { return 3; }
  Node *Resolve(Pass pass) override;
  bool Resolved() override { return (object_ != nullptr) && (index_ != nullptr) && (element_ != nullptr); }
  void Print(uint32_t flags = 0) override;
};

// (A=16, B=numVal) val1 val2 ... valN map -- frame
// Makes a frame and fills in its slots using values from the stack. The B
// field contains the number of slot values on the stack. The slot values are
// on the stack in index order, followed by the map to use for the frame.
// The slot values and map are removed from the stack, and a reference to the
// newly-allocated frame is pushed onto the stack. The B field may contain a
// number less than the number of slots in the frame, in which
// case the remaining slots at the end of the frame are set to nil.
class BCMakeFrame : public ConsumeN {
public:
  BCMakeFrame(Decompiler &d, int pc, int a, int b)
  : ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "BCMakeFrame"; }
  void Print(uint32_t flags = 0) override;
};


// (A=17, B=numVal):  val1 val2 ... valN class -- array): arg1 arg2 ... argN name -- result
// Makes an array and fills in its slots using values from the stack.
// The B field contains the size of the array. An array of that size and class
// class is allocated. The values for the array slots, on the stack in index
// order, are copied into the slots of the array. The values are removed from
// the stack, and a reference to the array is pushed onto the stack.
// \see BCNewArray
class BCMakeArray : public ConsumeN {
public:
  BCMakeArray(Decompiler &d, int pc, int a, int b)
  : ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "BCMakeArray"; }
  void Print(uint32_t flags = 0) override;
};


}; // namespace ast;

#endif  /* __MATT_AST_DATAFLOW_H */
