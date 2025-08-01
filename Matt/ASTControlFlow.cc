
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

/**
 \brief Print jump targets that have not been resolved and are still in the AST.
 */
void ASTJumpTarget::Print()
{
  PrintNode(true);
}

void ASTJumpTarget::PrintNode(bool deep)
{
  ASTNode::PrintNode(deep);
  dec.p.Printf(" from %d", origin_);
}

#pragma mark - ASTCodeBlock

/**
 \class ASTCodeBlock
 \brief A node that holds a block of statements, possibly followed by an expression.
 This is the base for control flow nodes.
*/

/**
 \brief Constructor called by derived classes.
 \param[in] d back link to the decompiler
 \param[in] pc original position in bytecode
 \param[in] inProvides sets the value that will be returned by Provides()
 */
ASTCodeBlock::ASTCodeBlock(Decompiler &d, int pc, int inProvides)
: ASTNode(d, pc),
  provides_(inProvides)
{ }

/**
 \brief Remove nodes from the AST root an add them as dependencies to this node.
 \param[in] nd start with this node
 \param[in] numNodes number of nodes to move
 \param[in] append them to this list
 */
void ASTCodeBlock::moveToBody(ASTNode *nd, int numNodes, std::vector<ASTNode*> &body)
{
  for (int i = 0; i < numNodes; ++i) {
    ASTNode *nx = nd->next;
    nd->Unlink();
    body.push_back(nd);
    nd = nx;
  }
}

/**
 \brief Print the body nodes of a code block.
 */
void ASTCodeBlock::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Body");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

/**
 \brief Print a typical code block body.
 */
void ASTCodeBlock::PrintBody(const std::string &prolog,
                             const std::string &separator,
                             const std::string &epilog,
                             std::vector<ASTNode*> &body)
{
  dec.p.Print(prolog);
  dec.p.DeepList(separator);
  for (auto &nd: body) {
    dec.p.Item();
    nd->Print();
    dec.p.ItemDone();
  }
  if (!epilog.empty()) {
    dec.p.Trailer();
    dec.p.Print(epilog);
  }
  dec.p.EndList();

}

#pragma mark - ASTLoop

/**
 \class ASTLoop
 \brief Holds the code block of a 'loop' instruction.
 This node is created by resolving another node pattern.
 It is alway marked as resolved.
 */

/**
 \brief Create a new node for a 'loop' instruction.
 The node returns a single value and is marked Resolved.
 */
ASTLoop::ASTLoop(Decompiler &d, int pc)
: ASTCodeBlock(d, pc, kProvidesOne)
{ }

/**
 \brief Print the source code for 'loop'.
 */
void ASTLoop::Print() {
  if (!Resolved()) return PrintNode(false);
  if (body_.size() > 1) {
    PrintBody("loop begin", ";", "end", body_);
  } else if (body_.size() == 1) {
    PrintBody("loop", ";", "", body_);
  } else {
    dec.p.Print("loop nil"); // special case, loops forever
  }
};

#pragma mark - AST_Branch

/**
 \brief Try to resolve this bytecode as part of a 'loop' construct.
 A loop is simply an unconditional jump backwards. It can be interrupted
 with a 'break' or 'return' statement. The decompiler assumes that the
 bytecode is correct and does not check for break or return.
 \return the next node if this is a 'loop', or nullptr if no match was found.
 */
ASTNode *AST_Branch::ResolveLoop()
{
  // ---- Check for the loop... pattern
  do {
    // -- Store the result of our exploration here
    /* target 1 */  ASTJumpTarget *jt = nullptr;
    /* n-stmts  */  ASTNode *firstStmt = nullptr;
    /* expr     */  int numStmt = 0;
    /* branch 1 */  // <-- you are here

    // -- Try this pattern
    ASTNode *it = prev;
    if (b_ > pc_) break; // Jump must be backward
    numStmt = FindStatementsBwd(&it, &firstStmt);
    if (!(jt = dynamic_cast<ASTJumpTarget*>(it))) break;
    if ((jt->Origin() != pc()) || (jt->pc() != b())) break;

    // -- The pattern matches. Replace everything with a ASTLoop node
    // It's a loop! Build a new node.
    ASTLoop *loop = new ASTLoop(dec, pc_);
    delete jt->Unlink();
    loop->moveToBody(firstStmt, numStmt);
    this->ReplaceWith(loop);
    dec.numASTChanges++;
    return loop;
  } while (0);
  return nullptr;
}

/**
 \brief Try to resolve this bytecode as part of a 'break' instruction.
 If the sequence is 'branch; pop;', the pop can never be reached
 because there is no jump target between them.
 Lucky for us, break operations are by definition expressions, so
 the pop is needed, which makes this a reliable way to find a
 break instruction.
 The AST_Break will take care of the jump target when resolved.
 \return the next node if this is a 'break', or nullptr if no match was found.
 */
ASTNode *AST_Branch::ResolveBreak()
{
  do {
    // -- Check the pattern
    if (b_ < pc_) break;
    if (!prev->IsExpr()) break;
    if (!dynamic_cast<AST_Pop*>(next)) break;
    // -- It applies. Replace the instructions and remove the jump target.
    AST_Break *breakNode = new AST_Break(dec, pc(), b(), prev->Unlink());
    delete next->Unlink();
    DeleteJumpTarget(pc(), b());
    ReplaceWith(breakNode);
    dec.numASTChanges++;
    return breakNode->next;
  } while (0);
  return nullptr;
}

ASTNode *AST_Branch::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;
  ASTNode *nextNode = nullptr;
  if ((nextNode = ResolveLoop())) return nextNode;
  if ((nextNode = ResolveBreak())) return nextNode;
  return next;
}

void AST_Branch::Print() {
  if (!Resolved()) return PrintNode(false);
}


#pragma mark - ASTWhileDo

ASTWhileDo::ASTWhileDo(Decompiler &d, int pc, ASTNode *condition)
: ASTCodeBlock(d, pc, kProvidesNone), cond_(condition)
{ }

void ASTWhileDo::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> while");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> do");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Body");
}

void ASTWhileDo::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("while "); cond_->Print();
  if (body_.size() > 1) {
    PrintBody(" do begin", ";", "end", body_);
  } else if (body_.size() == 1) {
    PrintBody(" do", ";", "", body_);
  } else {
    dec.p.Printf(" do nil"); // special case, loops forever
  }
};

#pragma mark - ASTRepeatUntil

ASTRepeatUntil::ASTRepeatUntil(Decompiler &d, int pc, ASTNode *condition)
: ASTCodeBlock(d, pc, kProvidesNone), cond_(condition)
{ }

void ASTRepeatUntil::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> Repeat");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> Until");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- Condition");
}

void ASTRepeatUntil::Print() {
  if (!Resolved()) return PrintNode(false);

  dec.p.Printf("repeat");
  dec.p.DeepList(";");
  for (auto &nd: body_) {
    dec.p.Item();
    nd->Print();
    dec.p.ItemDone();
  }
  dec.p.Trailer(); dec.p.Printf("until "); cond_->Print();
  dec.p.EndList();
};

#pragma mark - AST_BranchIfTrue

ASTNode *AST_BranchIfTrue::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  // TODO: also used in "or", but how do we know which was used?
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    /* branch 2 */  AST_Branch *branch2 = nullptr;
    /* target 1 */  ASTJumpTarget *jt1 = nullptr;
    /* n-stmts  */  ASTNode *stmts = nullptr;
    /*          */  int numStmts = 0;
    /* target 2 */  ASTJumpTarget *jt2 = nullptr;
    /* expr     */  ASTNode *cond = nullptr;
    /* c.brch 1 */  // <-- you are here

    // -- Try the pattern
    if (b_ > pc_) break;  // jump backwards
    ASTNode *it = prev;
    if (it->IsExpr()) { cond = it; it = it->prev; } else break;
    if ((jt2 = dynamic_cast<ASTJumpTarget*>(it))) it = it->prev; else break;
    numStmts = FindStatementsBwd(&it, &stmts);
    if ((jt1 = dynamic_cast<ASTJumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;
    if (!(branch2 = dynamic_cast<AST_Branch*>(it))) break;
    if ((branch2->b() != jt2->pc()) || (branch2->pc() != jt2->Origin())) break;

    // -- The pattern matches. Replace everything with a ASTLoop node
    // We did it. This is a while...do... construct!
    ASTWhileDo *wd = new ASTWhileDo(dec, pc(), cond->Unlink());
    delete branch2->Unlink();
    delete jt1->Unlink();
    delete jt2->Unlink();
    wd->moveToBody(stmts, numStmts);
    ReplaceWith(wd);
    dec.numASTChanges++;
    return wd;
  } while (0);
  return next;
}

void AST_BranchIfTrue::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - ASTIfThenElseNode

ASTIfThenElseNode::ASTIfThenElseNode(Decompiler &d, int pc, ASTNode *condition, bool returnsAValue)
: ASTCodeBlock(d, pc, returnsAValue ? kProvidesOne : kProvidesNone), cond_(condition)
{ }

void ASTIfThenElseNode::PrintChildren(bool deep) {
  dec.p.Tag(); dec.p.Print("##### ---> If Condition");
  if (cond_) cond_->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> If Body ");
  for (auto &nd: body_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--> Else Body ");
  for (auto &nd: elseBody_) if (nd) nd->PrintNode(deep);
  dec.p.Tag(); dec.p.Print("##### <--- If Done");
}

void ASTIfThenElseNode::Print() {
  if (!Resolved()) return PrintNode(false);
  bool needBeginEnd = ((body_.size() > 1) || (elseBody_.size() > 1));
  // >> if (condition) the begin
  dec.p.Print("if ");
  int pp = dec.precedence; dec.precedence = 0;
  cond_->Print();
  dec.precedence = pp;
  dec.p.Print(" then");
  if (needBeginEnd) dec.p.Printf(" begin");
  // >>   if-Branch
  dec.p.DeepList(";");
  for (auto &nd: body_) {
    dec.p.Item(); nd->Print(); dec.p.ItemDone();
  }
  if (elseBody_.empty()) {
    if (needBeginEnd) { dec.p.Trailer(); dec.p.Printf("end"); }
    dec.p.EndList();
  }
  if (!elseBody_.empty()) {
    // >> end else if
    dec.p.Trailer();
    if (needBeginEnd) dec.p.Printf("end else begin"); else dec.p.Printf("else");
    dec.p.EndList();
    // >>   else-Branch
    dec.p.DeepList(";");
    for (auto &nd: elseBody_) {
      dec.p.Item(); nd->Print(); dec.p.ItemDone();
    }
    if (needBeginEnd) { dec.p.Trailer(); dec.p.Printf("end"); }
    dec.p.EndList();
  }
}

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
ASTNode *AST_BranchIfFalse::ResolveIfTheElse() {
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
    /* branch.f */  // <-- you are here
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
    ASTIfThenElseNode *ite = new ASTIfThenElseNode(dec, pc_, prev->Unlink(), returnsAValue);
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
  return nullptr;
}

ASTNode *AST_BranchIfFalse::ResolveRepeatUntil() {
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    /* target 1 */  ASTJumpTarget *jt1 = nullptr;
    /* n-stmts  */  ASTNode *stmts = nullptr;
    /*          */  int numStmts = 0;
    /* expr     */  ASTNode *cond = nullptr;
    /* c.brch 1 */  // <-- you are here

    // -- Try the pattern
    if (b_ > pc_) break;  // jump backwards
    ASTNode *it = prev;
    if (it->IsExpr()) { cond = it; it = it->prev; } else break;
    numStmts = FindStatementsBwd(&it, &stmts);
    if ((jt1 = dynamic_cast<ASTJumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;

    // -- The pattern matches. Replace everything with a ASTLoop node
    // We did it. This is a while...do... construct!
    ASTRepeatUntil *ru = new ASTRepeatUntil(dec, pc(), cond->Unlink());
    delete jt1->Unlink();
    ru->moveToBody(stmts, numStmts);
    ReplaceWith(ru);
    dec.numASTChanges++;
    return ru;
  } while (0);
  return nullptr;
}

ASTNode *AST_BranchIfFalse::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;
  ASTNode *nextNode = nullptr;
  if ((nextNode = ResolveIfTheElse())) return nextNode;
  if ((nextNode = ResolveRepeatUntil())) return nextNode;
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

/**
 \brief This node writes out a 'break' instruction.
 */
AST_Break::AST_Break(Decompiler &d, int origin, int target, ASTNode *input)
: ASTNode(d, origin, 0, target), in_(input)
{ }

/**
 \brief Print the 'break' instruction.
 'Break' takes an expression, but if that is 'nil', it's not written out in the source code.
 */
void AST_Break::Print() {
  dec.p.Printf("break");
  if (!in_->IsNIL()) {
    dec.p.Printf(" ");
    in_->Print();
  }
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
