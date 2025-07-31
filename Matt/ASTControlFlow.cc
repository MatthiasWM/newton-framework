
/*
 File:    Matt/ASTControlFlow.h

 Matt's decompiler Abstract Syntax Tree.
 Control Flow nodes.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTControlFlow.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/Decompiler.h"
#include "Matt/ObjectPrinter.h"

#pragma mark - ASTJumpTarget

void ASTJumpTarget::PrintNode(bool deep) {
  ASTNode::PrintNode(deep);
  dec.p.Printf("from %d", origin_);
}

#pragma mark - ASTLoop

void ASTLoop::Print() {
  if (!Resolved()) return PrintNode(false);
  if (body_.size() > 1) {
    dec.p.Printf("loop begin");
    dec.p.DeepList(";");
    for (auto &nd: body_) {
      dec.p.Item();
      nd->Print();
      dec.p.ItemDone();
    }
    dec.p.Trailer(); dec.p.Printf("end");
    dec.p.EndList();
  } else if (body_.size() == 1) {
    dec.p.Printf("loop "); // loop only one instruction forever (could be an if...break)
    dec.p.DeepList(";");
    dec.p.Item();
    body_[0]->Print();
    dec.p.EndList();
  } else {
    dec.p.Printf("loop nil"); // special case, loops forever
  }
};

#pragma mark - AST_Branch

ASTNode *AST_Branch::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  do {
    // `loop`: JumpTarget A; n*stmt; branch A;
    if (b_ > pc_) break;
    int numStmt = 0;
    ASTNode *nd = prev;
    while (nd->IsStatement()) { numStmt++; nd = nd->prev; }
    ASTJumpTarget *jt = dynamic_cast<ASTJumpTarget*>(nd);
    if (!jt) break;;
    if (jt->Origin() != pc_) break;

    // It's a loop! Build a new node.
    ASTLoop *loop = new ASTLoop(dec, pc_);
    nd = jt->next;
    delete jt->Unlink();
    for (int i=numStmt; i>0; --i) {
      ASTNode *nx = nd->next;
      loop->add(nd);
      nd->Unlink();
      nd = nx;
    }
    this->ReplaceWith(loop);
    dec.numASTChanges++;
    return loop;
  } while (0);
  return next;
}

void AST_Branch::Print() {
  if (!Resolved()) return PrintNode(false);
}


#pragma mark - ASTWhileDo

void ASTWhileDo::PrintChildren(bool deep) {
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  if (cond_) cond_->PrintNode(deep);
}

void ASTWhileDo::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("while "); cond_->Print(); dec.p.Printf(" do");
  if (body_.size() > 1) {
    dec.p.Printf(" begin");
    dec.p.DeepList(";");
    for (auto &nd: body_) {
      dec.p.Item();
      nd->Print();
      dec.p.ItemDone();
    }
    dec.p.Trailer(); dec.p.Printf("end");
    dec.p.EndList();
  } else if (body_.size() == 1) {
    // loop only one instruction forever (could be an if...break)
    dec.p.DeepList(";");
    dec.p.Item();
    body_[0]->Print();
    dec.p.ItemDone();
    dec.p.EndList();
  } else {
    dec.p.Printf("nil"); // special case, loops forever
  }
};

#pragma mark - ASTRepeatUntil

void ASTRepeatUntil::PrintChildren(bool deep) {
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  if (cond_) cond_->PrintNode(deep);
}

void ASTRepeatUntil::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("repeat");
  if (body_.size() > 1) {
    dec.p.Printf(" begin");
    dec.p.DeepList(";");
    for (auto &nd: body_) {
      dec.p.Item();
      nd->Print();
      dec.p.ItemDone();
    }
    dec.p.Trailer(); dec.p.Printf("end");
  } else if (body_.size() == 1) {
    // loop only one instruction forever (could be an if...break)
    dec.p.DeepList(";");
    dec.p.Item();
    body_[0]->Print();
    dec.p.ItemDone();
    dec.p.Trailer();
  } else {
    dec.p.DeepList(";");
    dec.p.Item();
    dec.p.Printf("nil"); // special case, loops forever
    dec.p.Trailer();
  }
  dec.p.Printf("until "); cond_->Print();
  dec.p.EndList();
};

#pragma mark - AST_BranchIfTrue

ASTNode *AST_BranchIfTrue::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  // TODO: used in "or"
  // `while...do...` Branch A; Target B; n*stmt; Target A; expr; BranchIfTrue B; PushNIL;
  do {
    // We expect that we jump backwards
    if (b_ > pc_) break;
    int numStmts = 0;
    ASTNode *nd = prev;
    // Next line must push NIL on the stack
    //      if (!next->IsNIL()) break;
    // Previous must be an expression
    if (!nd->IsExpr()) break;
    nd = nd->prev;
    // Now we want a jump target, check the origin when we know where the loop starts
    ASTJumpTarget *jt2 = dynamic_cast<ASTJumpTarget*>(nd);
    if (!jt2) break;
    nd = nd->prev;
    // Skip over any number of statements
    while (nd->IsStatement()) { numStmts++; nd = nd->prev; }
    ASTNode *stmts = nd->next;
    // We must find the jump target for this branch node now
    ASTJumpTarget *jt1 = dynamic_cast<ASTJumpTarget*>(nd);
    if (!jt1 || (jt1->Origin() != this->pc_)) break;
    nd = nd->prev;
    // Finally, we expect an unconditional jump to jt2
    AST_Branch *branch = dynamic_cast<AST_Branch*>(nd);
    if (!branch || (branch->b() != jt2->pc())) break;
    if (jt2->Origin() != branch->pc()) break;

    // We did it. This is a while...do... construct!
    ASTWhileDo *wd = new ASTWhileDo(dec, pc(), prev->Unlink());
    delete branch->Unlink();
    delete jt1->Unlink();
    delete jt2->Unlink();
    nd = stmts;
    for (int i=numStmts; i>0; --i) {
      ASTNode *nx = nd->next;
      wd->add(nd);
      nd->Unlink();
      nd = nx;
    }
    this->ReplaceWith(wd);
    dec.numASTChanges++;
    return wd;

  } while (0);
  return next;
}

void AST_BranchIfTrue::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - ASTIfThenElseNode

void ASTIfThenElseNode::Print() {
  if (!Resolved()) return PrintNode(false);
  bool needBeginEnd = ((ifBranch_.size() > 1) || (elseBranch_.size() > 1));
  // >> if (condition) the begin
  dec.p.Print("if ");
  int pp = dec.precedence; dec.precedence = 0;
  cond_->Print();
  dec.precedence = pp;
  dec.p.Print(" then");
  if (needBeginEnd) dec.p.Printf(" begin");
  // >>   if-Branch
  dec.p.DeepList(";");
  for (auto &nd: ifBranch_) {
    dec.p.Item(); nd->Print(); dec.p.ItemDone();
  }
  if (elseBranch_.empty()) {
    if (needBeginEnd) { dec.p.Trailer(); dec.p.Printf("end"); }
    dec.p.EndList();
  }
  if (!elseBranch_.empty()) {
    // >> end else if
    dec.p.Trailer();
    if (needBeginEnd) dec.p.Printf("end else begin"); else dec.p.Printf("else");
    dec.p.EndList();
    // >>   else-Branch
    dec.p.DeepList(";");
    for (auto &nd: elseBranch_) {
      dec.p.Item(); nd->Print(); dec.p.ItemDone();
    }
    if (needBeginEnd) { dec.p.Trailer(); dec.p.Printf("end"); }
    dec.p.EndList();
  }
}

void ASTIfThenElseNode::moveToBody(ASTNode *nd, int numNodes, std::vector<ASTNode*> &body)
{
  for (int i = 0; i < numNodes; ++i) {
    ASTNode *nx = nd->next;
    nd->Unlink();
    body.push_back(nd);
    nd = nx;
  }
}

// TODO: better debug output needed?
//  } else {
//    if (dec.output == Print::deep) {
//      dec.p.Item(); printHeader(); dec.p.Printf("%3d: ASTIfThenElseNode: if ###", pc_);
//      dec.p.DeepList(); cond_->Print(); dec.p.EndList();
//
//      dec.p.Item(); printHeader(); dec.p.Printf("%3d: ASTIfThenElseNode: then ###", pc_);
//      dec.p.DeepList();
//      for (auto *nd: ifBranch_) nd->Print();
//      dec.p.EndList();
//
//      dec.p.Item(); printHeader(); dec.p.Printf("%3d: ASTIfThenElseNode: else ###", pc_);
//      dec.p.DeepList();
//      for (auto *nd: elseBranch_) nd->Print();
//      dec.p.EndList();
//    }
//    dec.p.Item();
//    printHeader();
//    dec.p.Printf("%3d: ASTIfThenElseNode %s- %zu %zu ###", pc_,
//                 returnsAValue_ ? "Expression " : "",
//                 ifBranch_.size(), elseBranch_.size());
//  }
//}

#pragma mark - AST_BranchIfFalse

/**
 \brief Find patterns around a branch-if-false instruction and resolve them.

 This instruction can be the start of an if...then...else... construct. There
 are three different patterns: if-expr-then-expr-else-expr, if-expr-then-stmt,
 and if-expr-then-stmt-else-stmt.

 If a matching pattern is found, branch commands and jump targets are removed
 and this node is replaced with a ASTIfThenElseNode, holding the instructions
 inside the 'then' and 'else' branch.
 */
ASTNode *AST_BranchIfFalse::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  // ---- Check for the if...then...else... pattern
  do { // If any of the pattern checks fail, we can escape using 'break'.
    // -- Store the result of our exploration here
    ASTNode *it = next;
    ASTNode *firstIfStmt = nullptr;
    ASTNode *firstElseStmt = nullptr;
    ASTJumpTarget *jt1 = nullptr;
    ASTJumpTarget *jt2 = nullptr;
    AST_Branch *bi2 = nullptr;
    int numIf = 0;
    int numElse = 0;
    bool returnsAValue = false;
    bool hasElse = false;

    // -- Try this pattern
    /*          */  if (pc() > b()) break;  // Jump must be forward
    /* expr     */  if (!prev->IsExpr()) break;
    /* branch   */  // <-- you are here
    /* n-stmts  */  numIf = FindStatementsFwd(&it, &firstIfStmt); // 0 statements ok
    /* [expr]   */  if (it->IsExpr()) { returnsAValue = true; it = it->next; }
    /*          */  if ((numIf == 0) && !returnsAValue) break; // we have to have at least one statement or expression
    /* [branch] */  if ((bi2 = dynamic_cast<AST_Branch*>(it))) { hasElse = true; it = it->next; }
    /* target   */  if ((jt1 = dynamic_cast<ASTJumpTarget*>(it))) it = it->next; else break;
    /*          */  if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;
    /*          */  if (hasElse) {
    /* n-stmts  */    numElse = FindStatementsFwd(&it, &firstElseStmt);
    /* [expr]   */    if (returnsAValue) { if (it->IsExpr()) it = it->next; else break; }
    /* target   */    if (!(jt2 = dynamic_cast<ASTJumpTarget*>(it))) break;
    /*          */    if ((jt2->Origin() != bi2->pc()) || (jt2->pc() != bi2->b())) break;
    /*          */  }

    // -- The pattern matches. Replace everything with a ASTIfThenElseNode
    ASTIfThenElseNode *ite = new ASTIfThenElseNode(dec, pc_, returnsAValue);
    ite->setCond(prev->Unlink());
    ite->moveToIfBody(firstIfStmt, numIf + returnsAValue);
    delete jt1->Unlink();
    if (hasElse) {
      ite->moveToElseBody(firstElseStmt, numElse + returnsAValue);
      delete bi2->Unlink();
      delete jt2->Unlink();
    }
    ReplaceWith(ite);
    dec.numASTChanges++;
    return ite;
  } while (0);
  return next;
}

void AST_BranchIfFalse::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_Return

void AST_Return::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("return ");
  in_->Print();
}

#pragma mark - AST_Break

ASTNode *AST_Break::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  if (in_) { // It's already resolved. Remove the jump target from the AST.
    ASTNode *nd = next;
    while (nd) {
      ASTJumpTarget *jt = dynamic_cast<ASTJumpTarget*>(nd);
      if (jt && (jt->Origin() == pc_) && (jt->pc() == b_)) {
        delete nd->Unlink();
        dec.numASTChanges++;
        break;
      }
      nd = nd->next;
    }
  }
  return next;
}

void AST_Break::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("break");
  if (!in_->IsNIL()) {
    dec.p.Printf(" ");
    in_->Print();
  }
  dec.p.Item();
}

#pragma mark - AST_PopHandlers

void AST_PopHandlers::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_SetLexScope

void AST_SetLexScope::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_IncrVar

void AST_IncrVar::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_IterNext

void AST_IterNext::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_IterDone

void AST_IterDone::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_NewIter

void AST_NewIter::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_Call

ASTNode *AST_Call::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  do {
    if (numIns_ != 3) break;
    auto nameNode = dynamic_cast<AST_Push*>(prev);
    if (!nameNode) break;
    if (!prev->prev->IsExpr() || !prev->prev->prev->IsExpr()) break;
    RefVar sym = dec.GetLiteral(nameNode->b());
    if (!::IsSymbol(sym)) break;
    const char *name = SymbolName(sym);
    if (!name) break;
    AST_BinaryOperator *op = nullptr;
    if (strcmp(name, "<<")==0) {
      op = new AST_BinaryOperator(dec, pc_, a_, b_, "<<", 8);
    } else if (strcmp(name, ">>")==0) {
      op = new AST_BinaryOperator(dec, pc_, a_, b_, ">>", 8);
    } else if (strcasecmp(name, "mod")==0) {
      op = new AST_BinaryOperator(dec, pc_, a_, b_, "mod", 7);
    }
    if (op) {
      delete prev->Unlink();
      this->ReplaceWith(op);
      dec.numASTChanges++;
      return op->Resolve(pass);
    }
  } while (0);
  return AST_ConsumeN::Resolve(pass);
}

void AST_Call::Print() {
  if (!Resolved()) return PrintNode(false);
  PrintResolvedCall(numIns_-1);
}

#pragma mark - AST_Invoke

void AST_Invoke::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("call ");
  ins_[numIns_-1]->Print();
  dec.p.Printf(" with (");
  dec.p.StartList(",");
  for (int i=0; i<numIns_-1; i++) {
    dec.p.Item(); ins_[i]->Print(); dec.p.ItemDone();
  }
  dec.p.Trailer(); dec.p.Printf(")");
  dec.p.EndList();
}

#pragma mark - AST_Send

void AST_Send::Print() {
  if (!Resolved()) return PrintNode(false);
  ins_[numIns_-2]->Print();   // Print the receiver
  dec.p.Print(":");           // Print the operator
  if (ifDefined_) dec.p.Printf("?");
  PrintResolvedCall(numIns_-2);
}

#pragma mark - AST_Resend

void AST_Resend::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Print("inherited:");        // Print the receiver and operator
  if (ifDefined_) dec.p.Printf("?");
  PrintResolvedCall(numIns_-1);
}

#pragma mark - AST_NewHandler

void AST_NewHandler::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BranchLoop

void AST_BranchLoop::PrintChildren(bool deep) {
  if (incr_) incr_->PrintNode(deep);
  if (index_) index_->PrintNode(deep);
  if (limit_) limit_->PrintNode(deep);
}

void AST_BranchLoop::Print() {
  if (!Resolved()) return PrintNode(false);
}
