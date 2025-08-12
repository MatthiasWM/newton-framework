
/*
 File:    Matt/AST.cc

 Abstract Syntax Tree for the NewtonScript decompiler

 Written by:  Matt, 2025.
 */

#include "Matt/AST.h"
#include "Matt/ASTAdmin.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/ASTControlFlow.h"
#include "Matt/ASTControlFlowHelper.h"
#include "Matt/Decompiler.h"

using namespace ast;

/*
    Node --+-- FirstNode
              +-- LastNode
              +-- ASTByteCodeNode --+-- ASTConsume1
              |                     +-- ASTConsume2
              |                     +-- ASTConsumeN
              +-- CodeBlock ----+-- CFLoop
              |                     +-- CFIfThen


 */

/**
 \brief Call PrintNode() on all dependent of this node.
 Override this for derived classes with new dependents. The caller already
 takes care of indent, and the children will take care of line breaks.
 */
void Node::PrintChildren(bool deep)
{
}

/**
 \brief Print information about this node for debugging.
 To expand this, override this method, call the original, and the just append
 more text by calling dec.p.Print* functions.
 */
void Node::PrintNode(bool deep)
{
  if (deep) {
    dec.p.DeepList();
    PrintChildren(deep);
    dec.p.EndList();
  }
  dec.p.Item();
  dec.p.Printf("##### [P:%2d] pc=%3d: %s a=%d, b=%d", provides(), pc_, Class(), a_, b_);
}

/**
 \brief Print the next statement on a new line, handle 'begin' and 'end' nicely
 */
void Node::PrintOnNewLine(uint32_t flags)
{
  if (IsMultiStatement()) {
    Print(flags);
  } else {
    dec.p.DeepList(";");
    Print(flags);
    dec.p.EndList();
  }
}

/** Unlink all nodes, starting with this, up to last */
void Node::UnlinkRange(Node *last) {
  Node *nd = this;
  for (;;) {
    Node *nx = nd; nd = nx->next; nx->Unlink();
    if ((nx == last) || (nd == nullptr)) break;
  }
}

/** Replace this node with another node in the linked list. */
void Node::ReplaceWith(Node *nd) {
  prev->next = nd; next->prev = nd;
  nd->prev = prev; nd->next = next;
  prev = next = nullptr;
}

/** Insert nd before this node. */
void Node::InsertBefore(Node *nd) {
  nd->prev = prev; nd->next = this;
  prev->next = nd;
  prev = nd;
}

/**
 \brief Called by the decompiler, asking a node to find its purpose in the AST.
 Every node can evaluate neighboring nodes to find patterns. If a pattern is
 found, the node can reorganize the AST, store information, and mark itself
 resolved.

 A node that changes state to resolved must increment dec.numASTChanges.

 The AST is resolved in many rounds through several passes to find patterns that
 are evolving by resolving other nodes, and to avoid conflicts where patterns
 are similar. Resolve() must only react to one type of pass, but it will likely
 called many times.

 Once a node is resolved, it must no longer react to Resolve() and only return
 'next'. There is no support for partially resolved nodes. However a resolved
 node can be replaced with an unresolved node.

 \arg[in] pass
 \return the next node that should try to get resolved, or nullptr is there
    are no more nodes.

 \note This method may change the AST quite radically, removing and reparenting
 other nodes, and even unlinking and deleting itself.
 */
Node *Node::Resolve(Pass pass)
{
  return next;
}

/**
 \brief Collect all Statement style nodes next in the list.
 \param[inout] crsr start at this node, walk along 'next', and return the first
    node that is not a statement.
 \param[out] first returns the first node that was a statement, or the initial crsr if there was none
 \return the number of statements found
 */
int Node::FindStatementsFwd(Node **crsr, Node **start)
{
  Node *nd = *crsr;
  if (start) *start = nd;
  int n = 0;
  while (nd->IsStatement()) {
    n++;
    nd = nd->next;
  }
  *crsr = nd;
  return n;
}

/**
 \brief Collect all Statement style nodes prev in the list.
 \param[inout] crsr start at this node, walk along 'prev', and return the first
    node that is not a statement.
 \param[out] first returns the first node that was a statement, or the initial crsr if there was none
 \return the number of statements found
 */
int Node::FindStatementsBwd(Node **crsr, Node **start)
{
  Node *nd = *crsr;
  int n = 0;
  while (nd->IsStatement()) {
    n++;
    nd = nd->prev;
  }
  if (start) *start = nd->next;
  *crsr = nd;
  return n;
}


/**
 \brief Delete a jump target.
 */
void Node::DeleteJumpTarget(int origin, int target) {
  JumpTarget *jt = nullptr;
  if (pc() > target) { // search backward
    for (Node *it = this; it; it = it->prev) {
      jt = dynamic_cast<JumpTarget*>(it);
      if (jt && (jt->pc() == target) && (jt->Origin() == origin)) break;
    }
  } else { // search forward
    for (Node *it = this; it; it = it->next) {
      jt = dynamic_cast<JumpTarget*>(it);
      if (jt && (jt->pc() == target) && (jt->Origin() == origin)) break;
    }
  }
  if (jt) {
    delete jt->Unlink();
  } else {
    assert(0); // Tried to delete a jump target that does not exist!
  }
}

/**
 \brief When resolving conditional branches into loops, handle targets generated by break instructions.
 This method is called when a looping operation was found and the end of the
 loop needs to be cleaned up.
 \param[in] start jump target origins must be between start and it
 \param[inout] points after the jump instruction that jumps back to the start
    of the loop, and returns the first node after cleanup
 \param[in] findPushNil check if the first instruction is "push nil". If this
    is true, we
 */
int Node::HandleBreakTargets(Node *start, Node *&it, bool findPushNil) {
  int prov = kProvidesNone;
  BCPushConst *pushNil = dynamic_cast<BCPushConst*>(it);
  if ((pushNil && (pushNil->b() == NILREF)) || (findPushNil == false)) {
    if (findPushNil) it = it->next;
    bool breakFound = false;
    for (;;) {
      JumpTarget *jt = dynamic_cast<JumpTarget*>(it);
      if (jt && (jt->Origin() > start->pc()) && (jt->Origin() < it->pc())) {
        breakFound = true;
        it = jt->next;
        jt->Unlink();
      } else {
        break;
      }
    }
    if (breakFound) {
      if (findPushNil) pushNil->Unlink();
      prov = kProvidesOne;
    }
  }
  return prov;
}

/**
 \brief Create the resolved bytecode for a 'nil' statement.
 */
Node *Node::NewNil() {
  auto *pop = new BCPop(dec, pc(), 0, 0);
  pop->Input(new BCPushConst(dec, pc(), 4, 2));
  return pop;
}
