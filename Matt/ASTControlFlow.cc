
/*
 File:    Matt/ASTControlFlow.h

 Matt's decompiler Abstract Syntax Tree.
 Control Flow nodes based on bytecodes.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTControlFlow.h"
#include "Matt/ASTControlFlowHelper.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/Decompiler.h"
#include "Matt/ObjectPrinter.h"

// DONE: if...then...else...
// DONE: loop...
// DONE: repeat...until...
// DONE: while...do...
// DONE: break
// TODO: for...to...by...do...
// TODO: foreach...slot...in...do...
// TODO: foreach...slot,value...in...do...
// TODO: foreach...deeply in...do...
// TODO: foreach...in...collect...
// TODO: foreach...deeply in...collect...
// TODO: try...onexception...do...

#pragma mark - conditions and loops -

#pragma mark - AST_BC_Branch

/**
 \brief Try to resolve this bytecode as part of a 'loop' construct.
 A loop is simply an unconditional jump backwards. It can be interrupted
 with a 'break' or 'return' statement. The decompiler assumes that the
 bytecode is correct and does not check for break or return.
 \return the next node if this is a 'loop', or nullptr if no match was found.
 */
ASTNode *AST_BC_Branch::ResolveLoop()
{
  // ---- Check for the loop... pattern
  do {
    // -- Store the result of our exploration here
    /* target 1 */  AST_JumpTarget *jt = nullptr;
    /* n-stmts  */  ASTNode *firstStmt = nullptr;
    /* expr     */  int numStmt = 0;
    /* branch 1 */  // <-- you are here

    // -- Try this pattern
    ASTNode *it = prev;
    if (b_ > pc_) break; // Jump must be backward
    numStmt = FindStatementsBwd(&it, &firstStmt);
    if (!(jt = dynamic_cast<AST_JumpTarget*>(it))) break;
    if ((jt->Origin() != pc()) || (jt->pc() != b())) break;

    // -- The pattern matches. Replace everything with a AST_CF_Loop node
    // It's a loop! Build a new node.
    AST_CF_Loop *loop = new AST_CF_Loop(dec, pc_);
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
 The AST_CF_Break will take care of the jump target when resolved.
 \return the next node if this is a 'break', or nullptr if no match was found.
 */
ASTNode *AST_BC_Branch::ResolveBreak()
{
  do {
    // -- Check the pattern
    if (b_ < pc_) break;
    if (!prev->IsExpr()) break;
    if (!dynamic_cast<AST_BC_Pop*>(next)) break;
    // -- It applies. Replace the instructions and remove the jump target.
    AST_CF_Break *breakNode = new AST_CF_Break(dec, pc(), b(), prev->Unlink());
    delete next->Unlink();
    DeleteJumpTarget(pc(), b());
    ReplaceWith(breakNode);
    dec.numASTChanges++;
    return breakNode->next;
  } while (0);
  return nullptr;
}

ASTNode *AST_BC_Branch::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;
  ASTNode *nextNode = nullptr;
  if ((nextNode = ResolveLoop())) return nextNode;
  if ((nextNode = ResolveBreak())) return nextNode;
  return next;
}

void AST_BC_Branch::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_BranchIfTrue

/**
 \class AST_BC_BranchIfTrue
 \brief A conditional jump.
 A value is popped from the stack. If it is nil, execution continues with the
 next instruction. Otherwise, PC is set to the B field value.
 */
ASTNode *AST_BC_BranchIfTrue::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  // TODO: also used in "or", but how do we know which was used?
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    /* branch 2 */  AST_BC_Branch *branch2 = nullptr;
    /* target 1 */  AST_JumpTarget *jt1 = nullptr;
    /* n-stmts  */  ASTNode *stmts = nullptr;
    /*          */  int numStmts = 0;
    /* target 2 */  AST_JumpTarget *jt2 = nullptr;
    /* expr     */  ASTNode *cond = nullptr;
    /* c.brch 1 */  // <-- you are here

    // -- Try the pattern
    if (b_ > pc_) break;  // jump backwards
    ASTNode *it = prev;
    if (it->IsExpr()) { cond = it; it = it->prev; } else break;
    if ((jt2 = dynamic_cast<AST_JumpTarget*>(it))) it = it->prev; else break;
    numStmts = FindStatementsBwd(&it, &stmts);
    if ((jt1 = dynamic_cast<AST_JumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;
    if (!(branch2 = dynamic_cast<AST_BC_Branch*>(it))) break;
    if ((branch2->b() != jt2->pc()) || (branch2->pc() != jt2->Origin())) break;

    // -- The pattern matches. Replace everything with a AST_CF_Loop node
    // We did it. This is a while...do... construct!
    AST_CF_While *wd = new AST_CF_While(dec, pc(), cond->Unlink());
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

void AST_BC_BranchIfTrue::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_BranchIfFalse

/**
 \class AST_BC_BranchIfFalse
 \brief Based on this node, find the pattern of an if/then or if/then/else structure in the AST.

 This class checks for three different pattern, generating one of three possible variations
 of the AST_CF_IfThen. If one of the pattern matches, the new AST_CF_IfThen
 will replace all other code involved.

 Pattern one is a simple if/then statement:
 - BranchIfFalse B, n*statement, Target B

 The second pattern adds and 'else' branch:
 - BranchIfFalse A, n*statement, Branch B, Target A, n*statement, Target B

 A third pattern generates an expression instead of a statement, laving a ref on the stack.
 This pattern exists only as if/then/else. An missing 'else' branch in the source
 creates an 'else' branch that pushes 'nil':
 - BranchIfFalse A, n*statement, expr, Branch B, Target A, n*statement, expr, Target B

 \note if...then...else... creates the same bytecode as *and*. `a and b` generates
 `if a then b else nil`.
 \note `if not...` generates "not" and "BranchIfFalse" and is not optimized into "BranchIfTrue".
 \note BranchIfTrue is used to generated an `or` operation.
 \note A `break` command is not allowed in the branches unless the *if* stament
 is inside an other loop.
 \see AST_CF_IfThen
 */

/**
 \brief Find patterns around a branch-if-false instruction and resolve them.

 This instruction can be the start of an if...then...else... construct. There
 are three different patterns: if-expr-then-expr-else-expr, if-expr-then-stmt,
 and if-expr-then-stmt-else-stmt.

 If a matching pattern is found, branch commands and jump targets are removed
 and this node is replaced with a AST_CF_IfThen, holding the instructions
 inside the 'then' and 'else' branch.
 */
ASTNode *AST_BC_BranchIfFalse::ResolveIfTheElse() {
  // ---- Check for the if...then...else... pattern
  do { // If any of the pattern checks fail, we can escape using 'break'.
    // -- Store the result of our exploration here
    ASTNode *it = next;
    ASTNode *firstIfStmt = nullptr;
    ASTNode *firstElseStmt = nullptr;
    AST_JumpTarget *jt1 = nullptr;
    AST_JumpTarget *jt2 = nullptr;
    AST_BC_Branch *bi2 = nullptr;
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
    /* [branch] */  if ((bi2 = dynamic_cast<AST_BC_Branch*>(it))) { hasElse = true; it = it->next; }
    /* target   */  if ((jt1 = dynamic_cast<AST_JumpTarget*>(it))) it = it->next; else break;
    /*          */  if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;
    /*          */  if (hasElse) {
    /* n-stmts  */    numElse = FindStatementsFwd(&it, &firstElseStmt);
    /* [expr]   */    if (returnsAValue) { if (it->IsExpr()) it = it->next; else break; }
    /* target   */    if (!(jt2 = dynamic_cast<AST_JumpTarget*>(it))) break;
    /*          */    if ((jt2->Origin() != bi2->pc()) || (jt2->pc() != bi2->b())) break;
    /*          */  }

    // -- The pattern matches. Replace everything with a AST_CF_IfThen
    AST_CF_IfThen *ite = new AST_CF_IfThen(dec, pc_, prev->Unlink(), returnsAValue);
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

ASTNode *AST_BC_BranchIfFalse::ResolveRepeatUntil() {
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    /* target 1 */  AST_JumpTarget *jt1 = nullptr;
    /* n-stmts  */  ASTNode *stmts = nullptr;
    /*          */  int numStmts = 0;
    /* expr     */  ASTNode *cond = nullptr;
    /* c.brch 1 */  // <-- you are here

    // -- Try the pattern
    if (b_ > pc_) break;  // jump backwards
    ASTNode *it = prev;
    if (it->IsExpr()) { cond = it; it = it->prev; } else break;
    numStmts = FindStatementsBwd(&it, &stmts);
    if ((jt1 = dynamic_cast<AST_JumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;

    // -- The pattern matches. Replace everything with a AST_CF_Loop node
    // We did it. This is a while...do... construct!
    AST_CF_Repeat *ru = new AST_CF_Repeat(dec, pc(), cond->Unlink());
    delete jt1->Unlink();
    ru->moveToBody(stmts, numStmts);
    ReplaceWith(ru);
    dec.numASTChanges++;
    return ru;
  } while (0);
  return nullptr;
}

ASTNode *AST_BC_BranchIfFalse::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;
  ASTNode *nextNode = nullptr;
  if ((nextNode = ResolveIfTheElse())) return nextNode;
  if ((nextNode = ResolveRepeatUntil())) return nextNode;
  return next;
}

void AST_BC_BranchIfFalse::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_Return

/**
 \class AST_BC_Return
 \brief Return from this function.
 \todo the very last return probably doesn't need to be printed. It's actually
    a bug in the newt-framework compiler. NTK does not generate the extra return bytecode
 \todo return NIL is implied if there is no return statement in the source code
 \todo handle implied return values nicely, so we don't generate "return a := b;"
 */

void AST_BC_Return::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("return ");
  in_->Print();
}

#pragma mark - AST_BC_SetLexScope

void AST_BC_SetLexScope::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_IncrVar

void AST_BC_IncrVar::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_IterNext

void AST_BC_IterNext::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_IterDone

void AST_BC_IterDone::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_NewIter

/* This is the start of a foreach loop. These are the patterns:
  (not sure yet what code the "deeply" keyword generates, maybe changes the iterator type?)

 foreach...slot...in...do...
    locals 5:slot, 6:|slot|iter|
    AST_BC_NewIter a=24, b=17
    AST_BC_SetVar a=20, b=6
    AST_BC_Branch 1
  AST_JumpTarget 2
    AST_BC_SetVar a=20, b=5
    n * Statements
    AST_BC_IterNext a=0, b=5
  AST_JumpTarget 1
    AST_BC_IterDone a=0, b=6
    AST_BC_BranchIfFalse 2
    AST_BC_PushConst a=4, b=2
    AST_BC_SetVar a=20, b=6

 foreach...slot,value...in...do...
    locals 5:slot, 6:value, 7:|slotvalue|iter|
    AST_BC_NewIter a=24, b=17
    AST_BC_SetVar a=20, b=7
    AST_BC_Branch 1
  AST_JumpTarget 2
    AST_BC_SetVar a=20, b=6
    AST_BC_SetVar a=20, b=5
    n * Statements
    AST_BC_IterNext a=0, b=5
  AST_JumpTarget 1
    AST_BC_IterDone a=0, b=6
    AST_BC_BranchIfFalse 2
    AST_BC_PushConst a=4, b=2
    AST_BC_SetVar a=20, b=7

 foreach...slot...in...collect...
    locals 5:slot, 6:|slot|iter|, 7:|slot|index|, 8:|slot|result|
 ##### [P:-1] pc=  2: AST_BC_NewIter a=24, b=17
 ##### [P:-1] pc=  5: AST_BC_SetVar a=20, b=6
 ##### [P: 0] pc= 15: AST_BC_SetVar a=20, b=8
 ##### [P: 0] pc= 19: AST_BC_SetVar a=20, b=7
 ##### [P:-4] pc= 22: AST_BC_Branch a=11, b=48
 ##### [P:-3] pc= 25: AST_JumpTarget a=0, b=0 from 50
 ##### [P: 0] pc= 28: AST_BC_SetVar a=20, b=5
 ##### [P: 0] pc= 38: AST_BC_SetARef a=24, b=3
 ##### [P:-1] pc= 39: AST_BC_Pop a=0, b=0
 ##### [P: 2] pc= 41: AST_BC_IncrVar a=22, b=7
 ##### [P:-1] pc= 44: AST_BC_Pop a=0, b=0
 ##### [P:-1] pc= 45: AST_BC_Pop a=0, b=0
 ##### [P: 0] pc= 47: AST_BC_IterNext a=0, b=5
 ##### [P:-3] pc= 48: AST_JumpTarget a=0, b=0 from 22
 ##### [P: 1] pc= 49: AST_BC_IterDone a=0, b=6
 ##### [P:-1] pc= 50: AST_BC_BranchIfFalse a=13, b=25
 ##### [P:-4] pc= 53: AST_BC_Branch a=11, b=61
 ##### [P:-1] pc= 56: AST_BC_SetVar a=20, b=8
 ##### [P:-1] pc= 59: AST_BC_Pop a=0, b=0
 ##### [P:-1] pc= 60: AST_BC_Pop a=0, b=0
 ##### [P:-3] pc= 61: AST_JumpTarget a=0, b=0 from 53
 ##### [P: 1] pc= 61: AST_BC_GetVar a=15, b=8
 ##### [P: 0] pc= 65: AST_BC_SetVar a=20, b=8
 ##### [P: 0] pc= 69: AST_BC_SetVar a=20, b=6


 */

void AST_BC_NewIter::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - Exceptions -

#pragma mark - AST_BC_NewHandler

/*
 This is the start of a 'try' block. The pattern is:
 try:
 n * AST_BC_Push, AST_BC_PushConst Tx
 AST_BC_NewHandler(n)
 try_body:
 Instructions
 AST_BC_PopHandlers
 AST_BC_Branch 1
 onexception x:
 AST_JumpTarget Tx
 AST_BC_Branch 2
 end_handlers:
 AST_BC_PopHandlers
 end_try
 AST_JumpTarget 1
 */

void AST_BC_NewHandler::Print() {
  if (!Resolved()) return PrintNode(false);
}


#pragma mark - AST_BC_PopHandlers

void AST_BC_PopHandlers::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - Calls -

#pragma mark - AST_BC_Call

ASTNode *AST_BC_Call::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  do {
    if (numIns_ != 3) break;
    auto nameNode = dynamic_cast<AST_BC_Push*>(prev);
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

void AST_BC_Call::Print() {
  if (!Resolved()) return PrintNode(false);
  PrintResolvedCall(numIns_-1);
}

#pragma mark - AST_BC_Invoke

void AST_BC_Invoke::Print() {
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

#pragma mark - AST_BC_Send

void AST_BC_Send::Print() {
  if (!Resolved()) return PrintNode(false);
  ins_[numIns_-2]->Print();   // Print the receiver
  dec.p.Print(":");           // Print the operator
  if (ifDefined_) dec.p.Printf("?");
  PrintResolvedCall(numIns_-2);
}

#pragma mark - AST_BC_Resend

void AST_BC_Resend::Print() {
  if (!Resolved()) return PrintNode(false);
  dec.p.Print("inherited:");        // Print the receiver and operator
  if (ifDefined_) dec.p.Printf("?");
  PrintResolvedCall(numIns_-1);
}

#pragma mark - AST_BC_BranchLoop

/*
 This indicates the end of a 'for' loop. The pattern is:
    local 5:i, 6:|i|limit|, 7:|i|incr|
    AST_BC_SetVar b=5
    AST_BC_SetVar b=6
    AST_BC_SetVar b=7
    AST_BC_GetVar b=7
    AST_BC_GetVar b=5
    AST_BC_Branch 1
    AST_JumpTarget 2
    Statements
    AST_BC_IncrVar a=22, b=5
    AST_JumpTarget 1
    AST_BC_GetVar a=15, b=6
    AST_BC_BranchLoop 2
 */

void AST_BC_BranchLoop::PrintChildren(bool deep) {
  if (incr_) incr_->PrintNode(deep);
  if (index_) index_->PrintNode(deep);
  if (limit_) limit_->PrintNode(deep);
}

void AST_BC_BranchLoop::Print() {
  if (!Resolved()) return PrintNode(false);
}
