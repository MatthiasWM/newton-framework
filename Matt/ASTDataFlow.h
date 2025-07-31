
/*
 File:    Matt/ASTDataFlow.h

 Matt's decompiler Abstract Syntax Tree.
 Data Flow nodes.

 Written by:  Matt, 2025.
 */

#if !defined(__MATT_AST_DATAFLOW_H)
#define __MATT_AST_DATAFLOW_H 1

#include "Matt/ASTAdmin.h"

// (A=3): -- literal
class AST_Push : public ASTBytecodeNode {
public:
  AST_Push(Decompiler &d, int pc, int a, int b) : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_Push"; }
  int provides() override { return 1; }
  bool IsSymbol() override;
  bool Resolved() override { return true; }
  void Print() override;
};

// (A=4, B=signed): -- value
class AST_PushConst : public ASTBytecodeNode {
public:
  AST_PushConst(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_PushConst"; }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print() override;
  bool IsNIL() override;
};

// (A=0, B=3):  -- RCVR
class AST_PushSelf : public ASTBytecodeNode {
public:
  AST_PushSelf(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_PushSelf"; }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print() override;
};

// (A=14): -- value
// Performs a variable lookup. The B field is the zero-based index in the
// literals array of a symbol (here called name) naming the variable.
class AST_FindVar : public ASTBytecodeNode {
public:
  AST_FindVar(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_FindVar"; }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print() override;
};

// (A=15): -- value
class AST_GetVar : public ASTBytecodeNode {
public:
  AST_GetVar(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_GetVar"; }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print() override;
};

// (A=0, B=0): value --
class AST_Pop : public AST_Consume1 {
public:
  AST_Pop(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_Pop"; }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  ASTNode *Resolve(Pass pass) override;
  void Print() override;
};

// (A=0, B=1): x -- x x
class AST_Dup : public AST_Consume1 {
public:
  AST_Dup(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_Dup"; }
  // TODO: provides(2) does not match any consumers!
  // NOTE: we must find an actual use case and the corresponding source code
  // NOTE: it may make sense to split this into a dup1 and dup2 node?!
  int provides() override { if (Resolved()) return 2; else return kProvidesUnknown; }
  void Print() override;
};


// (A=20): value --
class AST_SetVar : public AST_Consume1 {
public:
  AST_SetVar(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_SetVar"; }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print() override;
};

// (A=21): value --
class AST_FindAndSetVar : public AST_Consume1 {
public:
  AST_FindAndSetVar(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_FindAndSetVar"; }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print() override;
};

// (A=24, B=5): value -- value
class AST_Not : public AST_Consume1 {
public:
  AST_Not(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_Not"; }
  void Print() override;
};

// (A=24, B=18): object -- length
class AST_Length : public AST_Consume1 {
public:
  AST_Length(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_Length"; }
  void Print() override;
};

// (A=24, B=19): object -- clone
class AST_Clone : public AST_Consume1 {
public:
  AST_Clone(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_Clone"; }
  void Print() override;
};

// (A=24, B=22): array -- string
class AST_Stringer : public AST_Consume1 {
public:
  AST_Stringer(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_Stringer"; }
  void Print() override;
};

// (A=24, B=24): object -- class
class AST_ClassOf : public AST_Consume1 {
public:
  AST_ClassOf(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_ClassOf"; }
  void Print() override;
};

// (A=24, B=16): num -- result
class AST_BitNot : public AST_Consume1 {
public:
  AST_BitNot(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  const char *Class() override { return "AST_BitNot"; }
  void Print() override;
};

class AST_BinaryExpression : public AST_Consume2 {
public:
  AST_BinaryExpression(Decompiler &d, int pc, int a, int b)
  : AST_Consume2(d, pc, a, b) { }
  const char *Class() override { return "AST_BinaryExpression"; }
  ASTNode *Resolve(Pass pass) override;
};

// num1 num2 -- result
// bAnd, bOr
class AST_BinaryFunction : public AST_BinaryExpression {
protected:
  const char *func_ { nullptr };
public:
  AST_BinaryFunction(Decompiler &d, int pc, int a, int b, const char *func)
  : AST_BinaryExpression(d, pc, a, b), func_(func) { }
  const char *Class() override { return "AST_BinaryFunction"; }
  void Print() override;
};

// num1 num2 -- result
// +, -, *, /, div, =, <>, <, <=, >, >=
class AST_BinaryOperator : public AST_BinaryExpression {
protected:
  const char *op_ { nullptr };
  int precedence_ { 0 };
public:
  AST_BinaryOperator(Decompiler &d, int pc, int a, int b, const char *op, int precedence)
  : AST_BinaryExpression(d, pc, a, b), op_(op), precedence_(precedence)
  { }
  const char *Class() override { return "AST_BinaryOperator"; }
  void Print() override;
};

// (A=17, B=0xFFFF): size class -- array
class AST_NewArray : public AST_Consume2 {
public:
  AST_NewArray(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  const char *Class() override { return "AST_NewArray"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print() override;
};

// (A=18): object pathExpr -- value
// Retrieves the value corresponding to pathExpr in the frame or array object.
// The B field may be zero or one.
// The value corresponding to a path expression is the value that would be
// found by doing array and frame accesses corresponding to each element of
// the path expression. Integers represent array accesses to the given array
// index; symbols represent frame accesses to the given frame slot.
class AST_GetPath : public AST_Consume2 {
public:
  AST_GetPath(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  const char *Class() override { return "AST_GetPath"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print() override;
};

// (A=24, B=2): object index -- element
class AST_ARef : public AST_Consume2 {
public:
  AST_ARef(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  const char *Class() override { return "AST_ARef"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print() override;
};

// (A=24, B=20): object class -- object
class AST_SetClass : public AST_Consume2 {
public:
  AST_SetClass(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  const char *Class() override { return "AST_SetClass"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print() override;
};

// (A=24, B=21): array object -- object
class AST_AddArraySlot : public AST_Consume2 {
public:
  AST_AddArraySlot(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  const char *Class() override { return "AST_AddArraySlot"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print() override;
};

// (A=24, B=23): object pathExpr -- result
class AST_HasPath : public AST_Consume2 {
public:
  AST_HasPath(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  const char *Class() override { return "AST_HasPath"; }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print() override;
};

// (A=19)
//    B=0: object pathExpr value --
//    B=1: object pathExpr value -- value
// Sets the value corresponding to pathExpr in the frame or array object
// to value.The B field may be zero or one. The object, path, and value are
// popped from the stack. If the B field is one, the value is
// pushed back onto the stack.
class AST_SetPath : public ASTBytecodeNode {
protected:
  ASTNode *object_ { nullptr };
  ASTNode *path_ { nullptr };
  ASTNode *value_ { nullptr };
public:
  AST_SetPath(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_SetPath"; }
  void PrintChildren(bool deep);
  int provides() override;
  int consumes() override { return 3; }
  ASTNode *Resolve(Pass pass) override;
  bool Resolved() override { return (object_ != nullptr) && (path_ != nullptr) && (value_ != nullptr); }
  void Print() override;
};

// (A=24, B=3): object index element -- element
class AST_SetARef : public ASTBytecodeNode {
protected:
  ASTNode *object_ { nullptr };
  ASTNode *index_ { nullptr };
  ASTNode *element_ { nullptr };
public:
  AST_SetARef(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  const char *Class() override { return "AST_SetARef"; }
  void PrintChildren(bool deep);
  int provides() override { if (object_ && index_ && element_) return kProvidesNone; else return kProvidesUnknown; }
  int consumes() override { return 3; }
  ASTNode *Resolve(Pass pass) override;
  bool Resolved() override { return (object_ != nullptr) && (index_ != nullptr) && (element_ != nullptr); }
  void Print() override;
};

// (A=16, B=numVal) val1 val2 ... valN map -- frame
// Makes a frame and fills in its slots using values from the stack. The B
// field contains the number of slot values on the stack. The slot values are
// on the stack in index order, followed by the map to use for the frame.
// The slot values and map are removed from the stack, and a reference to the
// newly-allocated frame is pushed onto the stack. The B field may contain a
// number less than the number of slots in the frame, in which
// case the remaining slots at the end of the frame are set to nil.
class AST_MakeFrame : public AST_ConsumeN {
public:
  AST_MakeFrame(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "AST_MakeFrame"; }
  void Print() override;
};


// (A=17, B=numVal):  val1 val2 ... valN class -- array): arg1 arg2 ... argN name -- result
// Makes an array and fills in its slots using values from the stack.
// The B field contains the size of the array. An array of that size and class
// class is allocated. The values for the array slots, on the stack in index
// order, are copied into the slots of the array. The values are removed from
// the stack, and a reference to the array is pushed onto the stack.
// \see AST_NewArray
class AST_MakeArray : public AST_ConsumeN {
public:
  AST_MakeArray(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  const char *Class() override { return "AST_MakeArray"; }
  void Print() override;
};




#endif  /* __MATT_AST_DATAFLOW_H */
