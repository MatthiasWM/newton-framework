
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
constexpr int kProvidesUnknown = -1;  // We don't know yet how many values will be on the stack
constexpr int kSpecialNode = -2;      // First or Last node. Stop searching.
constexpr int kJumpTarget = -3;
constexpr int kBranch = -4;
constexpr int kBranchIfFalse = -5;
constexpr int kBranchIfTrue = -6;

class ASTNode {
protected:
  Decompiler &dec;  // Quick access to the decompiler state and the ObjectPrinter (dec.p.)
  int pc_ = -1;
public:

  ASTNode(Decompiler &d) : dec(d) { }
  ASTNode(Decompiler &d, int pc) : dec(d), pc_(pc) { }
  ASTNode *prev { nullptr };
  ASTNode *next { nullptr };
  int pc() { return pc_; }
  virtual ~ASTNode() = default;
  virtual int provides() { return kProvidesUnknown; }
  virtual int consumes() { return 0; } // Never called
  void printHeader();

  /** Remove this node from the linked list. Don;t use this for First and Last. */
  ASTNode *Unlink() { prev->next = next; next->prev = prev; prev = next = nullptr; return this; }
  void UnlinkRange(ASTNode *last);
  void ReplaceWith(ASTNode *nd);

  /** Return true if all this node does is put a NIL on the stack */
  virtual bool IsNIL() { return false; }
  /** Return true if the node returns a symbol (does not catch all cases!) */
  virtual bool IsSymbol() { return false; }
  /** Return if the node pushes a single value on the stack. */
  bool IsExpr() { return (provides() == 1); }
  /** Return if the node pushes no value on the stack and is not a control node. */
  bool IsStatement() { return (provides() == 0); }

  /** Resolve all data flow patterns.
   \return true if the call changed the AST
   \return the next node in the list
   */
  virtual auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> { return {false, next}; }
  /** Resolve all control flow patterns.
   \return true if the call changed the AST
   \return the next node in the list
   */
  virtual auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> { return { false, next }; }
  /** Return true if we know everything there is to know about this node. */
  virtual bool Resolved() = 0;

  /** Print the node, either for debugging or for the final script reconstruction. */
  virtual void Print() = 0;
};


#endif  /* __MATT_AST_H */
