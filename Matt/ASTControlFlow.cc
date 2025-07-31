
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

#pragma mark -

void ASTJumpTarget::Print() {
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: ASTJumpTarget from %d ###", pc_, origin_);
}

#pragma mark -

void ASTLoop::Print() {
  if ((dec.output == Print::script) && Resolved()) {
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
  } else {
    if (dec.output == Print::deep) {
      dec.p.DeepList();
      for (auto &nd: body_) nd->Print();
      dec.p.EndList();
    }
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: ASTLoop ###", pc_);
  }
};

#pragma mark -

// (A=11): --
auto AST_Branch::ResolveControlFlow() -> std::tuple<bool, ASTNode*> {
  if (Resolved()) return { false, next };
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
    return { true, loop };
  } while(0);
  return { false, next };
}

void AST_Branch::Print() {
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_Branch pc:%d ###", pc_, b_);
}


#pragma mark -

void ASTWhileDo::PrintChildren() {
  dec.p.DeepList();
  for (auto &nd: body_) nd->Print();
  dec.p.EndList();
}

void ASTWhileDo::Print() {
  if ((dec.output == Print::script) && Resolved()) {
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
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: ASTWhileDo ###", pc_);
  }
};

#pragma mark -

void ASTRepeatUntil::PrintChildren() {
  dec.p.DeepList();
  for (auto &nd: body_) nd->Print();
  dec.p.EndList();
}

void ASTRepeatUntil::Print() {
  if ((dec.output == Print::script) && Resolved()) {
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
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: ASTRepeatUntil ###", pc_);
  }
};

#pragma mark -

auto AST_BranchIfTrue::ResolveControlFlow() -> std::tuple<bool, ASTNode*> {
  if (Resolved()) return { false, next };
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
    return { true, wd };

  } while (0);
  return { false, next };
}

void AST_BranchIfTrue::Print() {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_BranchIfTrue pc:%d ###", pc_, b_);
}

#pragma mark -

void ASTIfThenElseNode::Print() {
  if ((dec.output == Print::script) && Resolved()) {
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
  } else {
    if (dec.output == Print::deep) {
      dec.p.Item(); printHeader(); dec.p.Printf("%3d: ASTIfThenElseNode: if ###", pc_);
      dec.p.DeepList(); cond_->Print(); dec.p.EndList();

      dec.p.Item(); printHeader(); dec.p.Printf("%3d: ASTIfThenElseNode: then ###", pc_);
      dec.p.DeepList();
      for (auto *nd: ifBranch_) nd->Print();
      dec.p.EndList();

      dec.p.Item(); printHeader(); dec.p.Printf("%3d: ASTIfThenElseNode: else ###", pc_);
      dec.p.DeepList();
      for (auto *nd: elseBranch_) nd->Print();
      dec.p.EndList();
    }
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: ASTIfThenElseNode %s- %zu %zu ###", pc_,
                 returnsAValue_ ? "Expression " : "",
                 ifBranch_.size(), elseBranch_.size());
  }
}

#pragma mark -

auto AST_BranchIfFalse::ResolveControlFlow() -> std::tuple<bool, ASTNode*> {
  auto [changed, nextNode] = ResolveForwardBranch();
  if (changed) return { changed, nextNode };
  return ResolveBackwardBranch();
}

auto AST_BranchIfFalse::ResolveBackwardBranch() -> std::tuple<bool, ASTNode*> {
  if (Resolved()) return { false, next };
  return { false, next };
}

auto AST_BranchIfFalse::ResolveForwardBranch() -> std::tuple<bool, ASTNode*> {
  if (Resolved()) return { false, next };
  // Track if the if/then/else returns a value
  bool returnsAValue = false;
  ASTJumpTarget *jt1 = nullptr;
  ASTJumpTarget *jt2 = nullptr;
  int numIf = 0;
  int numElse = 0;
  int jt2origin = pc_;

  // Step 1: Make sure the condition input is resolved
  if (!in_) return AST_Consume1::ResolveDataFlow(); // TODO: control flow?

  // Step 2: Start pattern matching for if/then/else structure
  ASTNode *nd = next;

  // Step 3: Count statements in the 'if' branch (nodes that provide nothing)
  while (nd->IsStatement()) {
    numIf++;
    nd = nd->next;
  }

  // Step 4: Check if the next node is an expression (provides a value)
  // If so, this is an if/then/else expression, not just a statement
  if (nd->IsExpr()) {
    returnsAValue = true;
    numIf++;
    nd = nd->next;
  }

  // Step 5: Check for the presence of an 'else' branch
  if (nd->provides() == kBranch) {
    // Save the branch's program counter
    int branch_pc = nd->pc();
    nd = nd->next;

    // Step 6: The next node must be a jump target for the 'if' branch
    if (nd->provides() != kJumpTarget) return { false, next };
    jt1 = static_cast<ASTJumpTarget*>(nd);
    if (jt1->Origin() != pc_) return { false, next };
    nd = nd->next;

    // Step 7: Count statements in the 'else' branch
    while (nd->provides() == kProvidesNone) {
      numElse++;
      nd = nd->next;
    }

    // Step 8: If this is an expression, check for a single value in the else branch
    if (returnsAValue) {
      if (!nd->IsExpr()) return { false, next };
      numElse++;
      nd = nd->next;
    }

    // Step 9: The next node must be a jump target for the unconditional branch
    if (nd->provides() != kJumpTarget) return { false, next };
    jt2 = static_cast<ASTJumpTarget*>(nd);
    if (jt2->Origin() != branch_pc) return { false, next };
    jt2origin = branch_pc;
  } else if (nd->provides() == kJumpTarget) {
    // Step 10: Handle the case with no else branch (simple if/then)
    jt2 = static_cast<ASTJumpTarget*>(nd);
    if (jt2->Origin() != pc_) return { false, next };
  } else {
    // Pattern does not match any known if/then/else structure
    return { false, next };
  }

  // Step 11: Build the ASTIfThenElseNode and replace the matched nodes
  ASTIfThenElseNode *ite = new ASTIfThenElseNode(dec, pc_, returnsAValue);
  ite->setCond(in_);
  // Add all nodes from the 'if' branch
  for (int i=0; i<numIf; i++) {
    ite->addIf(next);
    next->Unlink();
  }
  if (numElse > 0) {
    // Remove branch and jump target nodes before the else branch
    next->Unlink(); // branch
    next->Unlink(); // jump target
    // Add all nodes from the 'else' branch
    for (int i=0; i<numElse; i++) {
      ite->addElse(next);
      next->Unlink();
    }
  }
  // Step 12: Remove the jump target, it's no longer needed and in the way now
  delete jt2->Unlink(); // jump target

  // Step 13: Replace this node with the new if/then/else node
  ReplaceWith(ite);
  return { true, ite };
}

void AST_BranchIfFalse::Print() {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_BranchIfFalse pc:%d ###", pc_, b_);
}

#pragma mark -

void AST_Return::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Printf("return ");
    in_->Print();
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_Return ###", pc_);
  }
}

#pragma mark -

auto AST_Break::ResolveDataFlow() -> std::tuple<bool, ASTNode*> {
  if (Resolved()) return { false, next };
  auto [changed, nextNode] = AST_Consume1::ResolveDataFlow();
  if (in_) { // It's resolved. Remove the jump target from the AST.
    ASTNode *nd = next;
    while (nd) {
      ASTJumpTarget *jt = dynamic_cast<ASTJumpTarget*>(nd);
      if (jt && (jt->Origin() == pc_) && (jt->pc() == b_)) {
        delete nd->Unlink();
        break;
      }
      nd = nd->next;
    }
  }
  return { changed, nextNode };
}

void AST_Break::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Printf("break");
    if (!in_->IsNIL()) {
      dec.p.Printf(" ");
      in_->Print();
    }
    dec.p.Item();
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_Break target=%d ###", pc_, b_);
  }
}

#pragma mark -

void AST_PopHandlers::Print() {
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_PopHandlers ###", pc_);
}

#pragma mark -

void AST_SetLexScope::Print() {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_SetLexScope ###", pc_);
}

#pragma mark -

void AST_IncrVar::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_IncrVar local[%d] ###", pc_, b_);
  //    }
}

#pragma mark -

void AST_IterNext::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_IterNext ###", pc_);
  //    }
}

#pragma mark -

void AST_IterDone::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_IterDone ###", pc_);
  //    }
}

#pragma mark -

void AST_NewIter::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_NewIter ###", pc_);
  //    }
}

#pragma mark -

auto AST_Call::ResolveDataFlow() -> std::tuple<bool, ASTNode*> {
  if (Resolved()) return { false, next };
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
      return op->ResolveDataFlow();
    }
  } while (0);
  return AST_ConsumeN::ResolveDataFlow();
}

void AST_Call::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    PrintResolvedCall(numIns_-1);
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_Call n=%d ###", pc_, b_);
  }
}

#pragma mark -

void AST_Invoke::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Printf("call ");
    ins_[numIns_-1]->Print();
    dec.p.Printf(" with (");
    dec.p.StartList(",");
    for (int i=0; i<numIns_-1; i++) {
      dec.p.Item(); ins_[i]->Print(); dec.p.ItemDone();
    }
    dec.p.Trailer(); dec.p.Printf(")");
    dec.p.EndList();
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_Invoke n=%d ###", pc_, numIns_);
  }
}

#pragma mark -

void AST_Send::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    ins_[numIns_-2]->Print();   // Print the receiver
    dec.p.Print(":");           // Print the operator
    if (ifDefined_) dec.p.Printf("?");
    PrintResolvedCall(numIns_-2);
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    if (ifDefined_)
      dec.p.Printf("%3d: AST_Send if defined n=%d ###", pc_, numIns_);
    else
      dec.p.Printf("%3d: AST_Send n=%d ###", pc_, numIns_);
  }
}

#pragma mark -

void AST_Resend::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Print("inherited:");        // Print the receiver and operator
    if (ifDefined_) dec.p.Printf("?");
    PrintResolvedCall(numIns_-1);
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    if (ifDefined_)
      dec.p.Printf("%3d: AST_Resend if defined n=%d ###", pc_, numIns_);
    else
      dec.p.Printf("%3d: AST_Resend n=%d ###", pc_, numIns_);
  }
}

#pragma mark -

void AST_NewHandler::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_NewHandler n=%d ###", pc_, numIns_);
  //    }
}

#pragma mark -

void AST_BranchLoop::PrintChildren() {
  dec.p.DeepList();
  if (incr_) incr_->Print();
  if (index_) index_->Print();
  if (limit_) limit_->Print();
  dec.p.EndList();
}

void AST_BranchLoop::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_BranchLoop ###", pc_);
  //    }
}
