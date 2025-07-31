
/*
 File:    Matt/AST.cc

 Abstract Syntax Tree for the NewtonScript decompiler

 Written by:  Matt, 2025.
 */

#include "Matt/AST.h"
#include "Matt/ASTAdmin.h"
#include "Matt/Decompiler.h"

/*
    ASTNode --+-- ASTFirstNode
              +-- ASTLastNode
              +-- ASTByteCodeNode --+-- ASTConsume1
                                    +-- ASTConsume2
                                    +-- ASTConsumeN


 */

/**
 \brief Call PrintNode() on all dependent of this node.
 Override this for derived classes with new dependents. The caller already
 takes care of indent, and the children will take care of line breaks.
 */
void ASTNode::PrintChildren(bool deep)
{
}

/**
 \brief Print information about this node for debugging.
 To expand this, override this method, call the original, and the just append
 more text by calling dec.p.Print* functions.
 */
void ASTNode::PrintNode(bool deep)
{
  if (deep) {
    dec.p.DeepList();
    PrintChildren(deep);
    dec.p.EndList();
  }
  dec.p.Item();
  dec.p.Printf("##### [P:%2d] pc=%3d: %s a=%d, b=%d", provides(), pc_, Class(), a_, b_);
}

/** Unlink all nodes, starting with this, up to last */
void ASTNode::UnlinkRange(ASTNode *last) {
  ASTNode *nd = this;
  for (;;) {
    ASTNode *nx = nd; nd = nx->next; nx->Unlink();
    if ((nx == last) || (nd == nullptr)) break;
  }
}

/** Replace this node with another node in the linked list. */
void ASTNode::ReplaceWith(ASTNode *nd) {
  prev->next = nd; next->prev = nd;
  nd->prev = prev; nd->next = next;
  prev = next = nullptr;
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
ASTNode *ASTNode::Resolve(Pass pass)
{
  return next;
}

/**
 \brief Collect all Statement style nodes next in the list.
 \param[inout] crsr start at this node, walk down 'next', and return the first
    node that is not a statement.
 \param[out] first returns the first node that was a statement, or the initial crsr if there was none
 \return the number of statements found
 */
int ASTNode::FindStatementsFwd(ASTNode **crsr, ASTNode **start)
{
  ASTNode *nd = *crsr;
  if (start) *start = nd;
  int n = 0;
  while (nd->IsStatement()) {
    n++;
    nd = nd->next;
  }
  *crsr = nd;
  return n;
}
