
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
// DONE: for...to...by...do...
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
    /* target 1     */  AST_JumpTarget *jt = nullptr;
    /* n statements */  ASTNode *firstStmt = nullptr;
    /* expr         */  int numStmt = 0;
    /* branch 1     */  // <-- you are here

    // -- Try this pattern
    ASTNode *iter = prev;
    if (b_ > pc_) break; // Jump must be backward
    numStmt = FindStatementsBwd(&iter, &firstStmt);
    if ( !(jt = ToBwd<AST_JumpTarget>(&iter, false)) )  break;
    if ((jt->Origin() != pc()) || (jt->pc() != b()))    break;

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

#pragma mark - For loop -

#pragma mark - AST_BC_IncrVar

void AST_BC_IncrVar::Print() {
  if (!Resolved()) return PrintNode(false);
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

AST_BC_BranchLoop::AST_BC_BranchLoop(Decompiler &d, int pc, int a, int b)
: AST_Bytecode(d, pc, a, b)
{ }

ASTNode *AST_BC_BranchLoop::Resolve(Pass pass)
{
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    int iter = -1, limit = -1, incr = -1;
    /* setvar iter  */  AST_BC_SetVar *setvar3 = nullptr;
    /* setvar limit */  AST_BC_SetVar *setvar2 = nullptr;
    /* setvar incr  */  AST_BC_SetVar *setvar1 = nullptr;
    /* getvar incr  */  AST_BC_GetVar *getvar3 = nullptr;
    /* getvar iter  */  AST_BC_GetVar *getvar2 = nullptr;
    /* branch 1     */  AST_BC_Branch *branch = nullptr;
    /* jumptarget 2 */  AST_JumpTarget *jt2 = nullptr;
    /* n statments  */  ASTNode *firstStmt = nullptr;
    /*              */  int numStmt = 0;
    /* incrvar iter */  AST_BC_IncrVar *incrVar = nullptr;
    /* jumptarget 1 */  AST_JumpTarget *jt1 = nullptr;
    /* getvar limit */  AST_BC_GetVar *getvar1 = nullptr;
    /* branchloop 2 */  // <-- you are here

    // -- Try the pattern. We must trace backwards.
    ASTNode *it = prev;
    if ( !(getvar1  = ToBwd<AST_BC_GetVar>(&it, true)) )    break;
    if ( !(jt1      = ToBwd<AST_JumpTarget>(&it, false)) )  break;
    if ( !(incrVar  = ToBwd<AST_BC_IncrVar>(&it, false)) )  break;
    numStmt = FindStatementsBwd(&it, &firstStmt);
    if ( !(jt2      = ToBwd<AST_JumpTarget>(&it, false)) )  break;
    if ( !(branch   = ToBwd<AST_BC_Branch>(&it, false)) )   break;
    if ( !(getvar2  = ToBwd<AST_BC_GetVar>(&it, true)) )    break;
    if ( !(getvar3  = ToBwd<AST_BC_GetVar>(&it, true)) )    break;
    if ( !(setvar1  = ToBwd<AST_BC_SetVar>(&it, true)) )    break;
    if ( !(setvar2  = ToBwd<AST_BC_SetVar>(&it, true)) )    break;
    if ( !(setvar3  = ToBwd<AST_BC_SetVar>(&it, true)) )    break;

    // Check us of variables
    iter = getvar2->b();  dec.useLocalAs(iter, Decompiler::Local::Use::iter);
    limit = getvar1->b(); dec.useLocalAs(limit, Decompiler::Local::Use::iter);
    incr = getvar3->b();  dec.useLocalAs(incr, Decompiler::Local::Use::iter);
    if (setvar1->b() != incr) break;
    if (setvar2->b() != limit) break;
    if (setvar3->b() != iter) break;

    // Check jump origins and destinations
    if ((jt2->Origin() != pc()) || (jt2->pc() != b())) break;
    if ((jt1->Origin() != branch->pc()) || (jt1->pc() != branch->b())) break;

    // -- The pattern matches. Replace everything with a AST_CF_ForLoop node
    AST_CF_ForLoop *forLoopNode = new AST_CF_ForLoop(dec, pc(), setvar3, setvar2->input(), setvar1->input());
    forLoopNode->moveToBody(firstStmt, numStmt);
    setvar1->Unlink();
    setvar2->Unlink();  // Note: link only the input to the sevar
    setvar3->Unlink();  // Note: we link the sevar node iteself
    getvar1->Unlink();
    getvar2->Unlink();
    getvar3->Unlink();
    incrVar->Unlink();
    branch->Unlink();
    jt1->Unlink();
    jt2->Unlink();
    ReplaceWith(forLoopNode);
    dec.numASTChanges++;
  } while (0);
  return next;
}

#pragma mark - Foreach loop -

#pragma mark - AST_BC_NewIter

/**
 \class AST_BC_NewIter
 \brief Start a foreach loop.
 ```
 object deeply -- iterator
 ```
 Creates an iterator for object. If object is a frame and deeply is non-nil,
 the iterator will follow _proto links in object. If object is not a frame or
 array, bad type error NotAFrameOrArray is thrown.

 This generates two locals for an array and three locals for a frame:
  - slot, |slot|iter|
  - slot, value, |slotvalue|iter|

 If we choose 'collect' instead of 'do', another local variable |slot|result|
 is added.

 The iterator is a slotted object with the following members:
  - 0: The tag of the current slot
  - 1: The value of the current slot
  - 3: If the second argument to new-iterator is true, the total number of
    slots that will be visited by the iterator
  - 5: The number of slots in object

 \see AST_BC_IterNext
 \see AST_BC_IterDone
 */

void AST_BC_NewIter::Print() {
  if (!Resolved()) return PrintNode(false);
}

ASTNode *AST_BC_NewIter::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  ASTNode *ret = nullptr;
  if ((ret = ResolveForeachSlotValueDo())) return ret;
  if ((ret = ResolveForeachValueCollect())) return ret;
  if ((ret = ResolveForeachSlotValueCollect())) return ret;
  return next;
}

ASTNode *AST_BC_NewIter::ResolveForeachSlotValueDo()
{
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    int slot = -1, value = -1, iter = -1;
    /* expr         */  ASTNode *slottedObj;
    /* pushconst    */  AST_BC_PushConst *deeplyNd;
    /* newiter      */  // <---- you are here
    /* setvar iter  */  AST_BC_SetVar *setvar1;
    /* branch 1     */  AST_BC_Branch *branch;
    /* jumptarget 2 */  AST_JumpTarget *jt2;
    /* setvar slot  */  AST_BC_SetVar *setvar2;
    /* | getvar       */  AST_BC_GetVar *slot_getvar;
    /* | pushconst    */  AST_BC_PushConst *slot_pushconst;
    /* | aref         */  AST_BC_ARef *slot_aref;
    /* | setvar       */  AST_BC_SetVar *slot_setvar;
    /* statements   */  ASTNode *firstStmt;
    /*              */  int numStmts;
    /* iternext     */  AST_BC_IterNext *iternext;
    /* jumptarget 1 */  AST_JumpTarget *jt1;
    /* iterdone     */  AST_BC_IterDone *iterdone;
    /* c.branch 2   */  AST_BC_BranchIfFalse *cbranch;
    /* pushconst nil*/  AST_BC_PushConst *cleanup1;
    /* setvar iter  */  AST_BC_SetVar *cleanup2;
    /* pop          */  AST_BC_Pop *cleanup3;

    // -- Try the pattern. We must trace backwards.
    // Walk backwards in the AST root
    ASTNode *it = prev;
    bool deeply = false;
    if ( !(deeplyNd = ToBwd<AST_BC_PushConst>(&it, false)) ) break;
    if (deeplyNd->b() == 2) deeply = false;
    else if (deeplyNd->b() == 26) deeply = true;
    else break;
    slottedObj = it; if (!slottedObj->Resolved()) break;
    // Now walk forwards
    it = next;
    if ( !(setvar1 = ToFwd<AST_BC_SetVar>(&it, false)) ) break;
    iter = setvar1->b();
    if ( !(branch = ToFwd<AST_BC_Branch>(&it, false)) ) break;
    if ( !(jt2 = ToFwd<AST_JumpTarget>(&it, false)) ) break;
    if ( !(setvar2 = ToFwd<AST_BC_SetVar>(&it, false)) ) break;
    value = setvar2->b();
    // If the original source code uses the 'slot' option, the following AST subtree must match:
    // The statement here is: `local slot := |slotvalue|iter|[0];`
    if ( (slot_setvar = dynamic_cast<AST_BC_SetVar*>(it)) ) {
      if ( (slot_aref = dynamic_cast<AST_BC_ARef*>(slot_setvar->input())) ) {
        slot_getvar = dynamic_cast<AST_BC_GetVar*>(slot_aref->input1());
        slot_pushconst = dynamic_cast<AST_BC_PushConst*>(slot_aref->input2());
        if ((slot_pushconst->b() == 0) && (slot_getvar->b() == iter)) {
          slot = slot_setvar->b();
          it = it->next;
        }
      }
    }
    if (slot == -1) slot_setvar = nullptr;
    // The remaining statements form the body
    numStmts = FindStatementsFwd(&it, &firstStmt);
    // Now find the rest of the 'foreach' pattern
    if ( !(iternext = ToFwd<AST_BC_IterNext>(&it, true)) ) break;
    if ( !(jt1 = ToFwd<AST_JumpTarget>(&it, false)) ) break;
    if ( !(iterdone = ToFwd<AST_BC_IterDone>(&it, false)) ) break;
    if ( !(cbranch = ToFwd<AST_BC_BranchIfFalse>(&it, false)) ) break;
    // cleanup
    if ( !(cleanup1 = ToFwd<AST_BC_PushConst>(&it, false)) ) break;
    if ( !(cleanup2 = ToFwd<AST_BC_SetVar>(&it, true)) ) break;
    if ( !(cleanup3 = ToFwd<AST_BC_Pop>(&it, false)) ) break;
    // TODO: we should still verify the two jump targets

    dec.useLocalAs(iter, Decompiler::Local::Use::iter);
    dec.useLocalAs(value, Decompiler::Local::Use::iter);
    if (slot != -1) dec.useLocalAs(slot, Decompiler::Local::Use::iter);

    // -- The pattern matches. Replace everything with a AST_CF_ForLoop node
    AST_CF_ForEachSlotValueDo *foreachNode =
      new AST_CF_ForEachSlotValueDo(dec, pc_, slottedObj, slot, value, deeply);
    foreachNode->moveToBody(firstStmt, numStmts);
    slottedObj->Unlink();
    deeplyNd->Unlink();
    setvar1->Unlink();
    branch->Unlink();
    jt2->Unlink();
    setvar2->Unlink();
    iternext->Unlink();
    jt1->Unlink();
    iterdone->Unlink();
    cbranch->Unlink();
    cleanup1->Unlink();
    cleanup2->Unlink();
    cleanup3->Unlink();
    if (slot_setvar) slot_setvar->Unlink();
    ReplaceWith(foreachNode);
    dec.numASTChanges++;

  } while (0);
  return nullptr;
}

ASTNode *AST_BC_NewIter::ResolveForeachValueCollect()
{
  // TODO: write me
  return nullptr;
}

ASTNode *AST_BC_NewIter::ResolveForeachSlotValueCollect()
{
  // TODO: write me
  return nullptr;
}

#pragma mark - AST_BC_IterNext

/**
 \class AST_BC_IterNext
 \brief Continue a foreach operation
 ```
 iterator --
 ```
 Pops a reference to an iterator from the stack and advances it to the next slot.
 */

void AST_BC_IterNext::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_IterDone

/**
 \class AST_BC_IterDone
 \brief Finalizes a 'foreach' statement.
 ```
 iterator -- done?
 ```
 Pops a reference to an iterator from the stack. If iterator is exhausted,
 pushes true onto the stack; otherwise, pushes nil onto the stack.
 */
void AST_BC_IterDone::Print() {
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

#pragma mark - AST_BC_SetLexScope

void AST_BC_SetLexScope::Print() {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_Call

ASTNode *AST_BC_Call::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  do {
    if (numIns_ != 3) break;
    auto nameNode = dynamic_cast<AST_BC_Push*>(prev); // AST_BC_Push is always resolved
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

