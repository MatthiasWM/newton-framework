
/*
 File:    Matt/AST.h

 Matt's decompiler Abstract Syntax Tree

 Written by:  Matt, 2025.
 */

#if !defined(__MATT_AST_H)
#define __MATT_AST_H 1

#include <tuple>


class Decompiler;

constexpr int kProvidesNone = 0;      // The node is defined enough to know that there is nothing on the stack
constexpr int kProvidesOne = 1;
constexpr int kProvidesUnknown = -1;  // We don't know yet how many values will be on the stack
constexpr int kSpecialNode = -2;      // First or Last node. Stop searching.
constexpr int kJumpTarget = -3;
constexpr int kBranch = -4;
constexpr int kBranchIfFalse = -5;
constexpr int kBranchIfTrue = -6;

using Direction =enum {
  kFwd = 1,
  kBwd = -1
};

class ASTNode
{
protected:
  Decompiler &dec;  // Quick access to the decompiler state and the ObjectPrinter (dec.p.)
  int pc_ = -1;
  int a_ = 0;
  int b_ = 0;

public:
  // ---- Type declarations used within nodes
  // Resolve nodes following this priority.
  enum class Pass {
    DataFlow,
    // CombineStatments,
    ControlFlow,
    // Exceptions
  };

  // ---- constructors and destructor
  ASTNode(Decompiler &d) : dec(d) { }
  ASTNode(Decompiler &d, int pc, int a=0, int b=0) : dec(d), pc_(pc), a_(a), b_(b) { }
  virtual ~ASTNode() = default;

  // ---- debugging
  virtual const char *Class() { return "ASTNode"; }
  virtual void PrintChildren(bool deep);
  virtual void PrintNode(bool deep);

  // ---- setter and getter
  int pc() { return pc_; }
  int a() { return a_; }
  int b() { return b_; }

  // ---- virtual methods that can be overridden by derived classes
  // -- Resolve the AST
  virtual int provides() { return kProvidesUnknown; }
  virtual int consumes() { return 0; } // Never called
  virtual ASTNode *Resolve(Pass pass);
  /** Return true if we know everything there is to know about this node. */
  virtual bool Resolved() = 0;
  // -- Print the result
  /** Print the node, either for debugging or for the final script reconstruction. */
  virtual void Print() { PrintNode(false); }
  // -- Helpers
  /** Return true if all this node does is put a NIL on the stack */
  virtual bool IsNIL() { return false; }
  /** Return true if the node returns a symbol (does not catch all cases!) */
  virtual bool IsSymbol() { return false; }
  /** Return if the node pushes a single value on the stack. */
  bool IsExpr() { return (provides() == 1); }
  /** Return if the node pushes no value on the stack and is not a control node. */
  bool IsStatement() { return (provides() == 0); }

  // ---- Helpers for finding patterns in the AST
  template <class T>
  static T *ToFwd(ASTNode **iter, bool mustBeResolved) {
    ASTNode *nd = *iter;
    if (!nd || (mustBeResolved && !nd->Resolved())) return nullptr;
    T *ret = dynamic_cast<T*>(nd);
    if (ret) *iter = nd->next;
    return ret;
  }
  template <class T>
  static T *ToBwd(ASTNode **iter, bool mustBeResolved) {
    ASTNode *nd = *iter;
    if (!nd || (mustBeResolved && !nd->Resolved())) return nullptr;
    T *ret = dynamic_cast<T*>(nd);
    if (ret) *iter = nd->prev;
    return ret;
  }

  // ---- Helpers for the doubly linked list
  ASTNode *prev { nullptr };
  ASTNode *next { nullptr };
  /** Remove this node from the linked list. Don;t use this for First and Last. */
  ASTNode *Unlink() { prev->next = next; next->prev = prev; prev = next = nullptr; return this; }
  void UnlinkRange(ASTNode *last);
  void ReplaceWith(ASTNode *nd);
  int FindStatementsFwd(ASTNode **crsr, ASTNode **start);
  int FindStatementsBwd(ASTNode **crsr, ASTNode **start);
  void DeleteJumpTarget(int origin, int target);
};


#endif  /* __MATT_AST_H */
