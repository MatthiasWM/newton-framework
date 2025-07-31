
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


void ASTNode::printHeader() {
  dec.p.Printf("###[%2d] ", provides());
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

