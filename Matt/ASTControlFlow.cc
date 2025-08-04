
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
// DONE: loop...break...
// DONE: repeat...break...until...
// DONE: while...do...break...
// DONE: for...to...by...do...break...
// DONE: foreach...slot...in...do...break...
// DONE: foreach...slot,value...in...do...break...
// DONE: foreach...deeply in...do...break...
// TODO: foreach...in...collect...
// TODO: try...onexception...do...
// TODO: call a function inside a function
// TODO: and
// TODO: or

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
    /* expr         */  ASTNode *body = nullptr;
    /* branch 1     */  // <-- you are here

    // -- Try this pattern
    ASTNode *iter = prev;
    if (b_ > pc_) break; // Jump must be backward
    if (iter->IsStatement()) { body = iter; iter = iter->prev; } else break;
    if ( !(jt = ToBwd<AST_JumpTarget>(&iter, false)) )  break;
    if ((jt->Origin() != pc()) || (jt->pc() != b()))    break;

    // -- The pattern matches. Replace everything with a AST_CF_Loop node
    // Check for a trailing "break targets"
    HandleBreakTargets(jt, iter, false);
    // It's a loop! Build a new node.
    AST_CF_Loop *loop = new AST_CF_Loop(dec, pc_, kProvidesOne, body->Unlink());
    jt->Unlink();
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
    next->Unlink();
    // Don't delete the jump target! Let the loops take care of that.
    ReplaceWith(breakNode);
    dec.numASTChanges++;
    return breakNode->next;
  } while (0);
  return nullptr;
}

ASTNode *AST_BC_Branch::Resolve(Pass pass)
{
  if (pass == Pass::DataFlow) {
    // If this resolves to a 'break', it behaves like a data flow element
    ASTNode *nextNode = ResolveBreak();
    if (nextNode) return nextNode;
  }
  if (pass == Pass::ControlFlow) {
    // If this resolves to 'loop', it's part of the control flow
    ASTNode *nextNode = ResolveLoop();
    if (nextNode) return nextNode;
  }
  return next;
}

void AST_BC_Branch::Print(uint32_t flags) {
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
  if (pass == Pass::DataFlow) {
    if (!in_ && prev->IsExpr()) {
      in_ = prev;
      prev->Unlink();
      dec.numASTChanges++;
      return next;
    } else {
      return next;
    }
  }
  if ((pass == Pass::ControlFlow) && (in_)) {
    ASTNode *nextNode = nullptr;
    if ((nextNode = ResolveWhileDo())) return nextNode;
  }
  return next;
}

ASTNode *AST_BC_BranchIfTrue::ResolveWhileDo()
{
  // TODO: also used in "or", but how do we know which was used?
  // FIXME: WhileDo is actually an expression that returns either nil, or
  //    whatever was pushed on the stack by a 'break' inside the loop (as is
  //    probably true for all other loops). So the pattern does not end in
  //    "BranchIfTrue", but is followed by a "PushConst nil" and the jump a
  //    jump target for every 'break' inside the loop, followed by a consumer.
  // NOTE: if there is no 'break' statement, push_nil and the consumer will
  //    be compressed into a CodeBlock (but the result is always nil anyway).
  //    If there is no consumer, there will be a "pop", and the last two
  //    instructions can be ignored.
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    /* branch 2 */  AST_BC_Branch *branch2 = nullptr;
    /* target 1 */  AST_JumpTarget *jt1 = nullptr;
    /* n-stmts  */  ASTNode *body = nullptr;
    /* target 2 */  AST_JumpTarget *jt2 = nullptr;
    /* expr     */  // Already in this->in_
    /* c.brch 1 */  // <-- you are here
    /* push nil */
    /* break targets */
    /* consumer */

    // -- Try the pattern
    ASTNode *it = prev;
    if (b_ > pc_) break;  // jump backwards
    if (!in_) break;
    if ((jt2 = dynamic_cast<AST_JumpTarget*>(it))) it = it->prev; else break;
    if (it->IsStatement()) { body = it; it = it->prev; } else break;
    if ((jt1 = dynamic_cast<AST_JumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;
    if (!(branch2 = dynamic_cast<AST_BC_Branch*>(it))) break;
    if ((branch2->b() != jt2->pc()) || (branch2->pc() != jt2->Origin())) break;
    // A useless "push-const nil, pop" was already removed in AST_BC_Pop::Resolve()
    // If there are break targets, they will be removed below.

    // -- The pattern matches. Replace everything with a AST_CF_Loop node
    // Check for a trailing "push-nil, break targets, consumer"
    int prov = HandleBreakTargets(branch2, it, true);
    // Now create our while...do node:
    AST_CF_While *wd = new AST_CF_While(dec, pc(), prov, in_, body->Unlink());
    delete branch2->Unlink();
    delete jt1->Unlink();
    delete jt2->Unlink();
    ReplaceWith(wd);
    dec.numASTChanges++;
    return wd;
  } while (0);
  return next;
}

void AST_BC_BranchIfTrue::Print(uint32_t flags) {
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
    ASTNode *ifStmt = nullptr;
    ASTNode *elseStmt = nullptr;
    AST_JumpTarget *jt1 = nullptr;
    AST_JumpTarget *jt2 = nullptr;
    AST_BC_Branch *bi2 = nullptr;
    bool returnsAValue = false;
    bool hasElse = false;

    // -- Try this pattern
    /*          */  if (pc() > b()) break;  // Jump must be forward
    /* expr     */  if (!in_) break;
    /* branch.f */  // <-- you are here
    /* n-stmts  */  if (it->IsStatement()) { ifStmt = it; it = it->next; }
    /* [expr]   */  else if (it->IsExpr()) { ifStmt = it; returnsAValue = true; it = it->next; }
    /*          */  else break;
    /* [branch] */  if ((bi2 = dynamic_cast<AST_BC_Branch*>(it))) { hasElse = true; it = it->next; }
    /* target   */  if ((jt1 = dynamic_cast<AST_JumpTarget*>(it))) it = it->next; else break;
    /*          */  if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;
    /*          */  if (hasElse) {
    /* n-stmts  */    if (!returnsAValue && it->IsStatement()) { elseStmt = it; it = it->next; }
    /* [expr]   */    else if (returnsAValue && it->IsExpr()) { elseStmt = it; it = it->next; }
    /*          */    else break;
    /* target   */    if (!(jt2 = dynamic_cast<AST_JumpTarget*>(it))) break;
    /*          */    if ((jt2->Origin() != bi2->pc()) || (jt2->pc() != bi2->b())) break;
    /*          */  }

    // -- The pattern matches. Replace everything with a AST_CF_IfThen
    AST_CF_IfThen *newNode = new AST_CF_IfThen(dec, pc_, in_, returnsAValue);
    newNode->body_ = ifStmt->Unlink();
    jt1->Unlink();
    if (hasElse) {
      newNode->elseBody_ = elseStmt->Unlink();
      bi2->Unlink();
      jt2->Unlink();
    }
    ReplaceWith(newNode);
    dec.numASTChanges++;
    return newNode;
  } while (0);
  return nullptr;
}

ASTNode *AST_BC_BranchIfFalse::ResolveRepeatUntil() {
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    /* target 1 */  AST_JumpTarget *jt1 = nullptr;
    /* n-stmts  */  ASTNode *body = nullptr;
    /* expr     */  // condition is in this->in_
    /* c.brch 1 */  // <-- you are here

    // -- Try the pattern
    ASTNode *it = prev;
    if (b_ > pc_) break;  // jump backwards
    if (!in_) break;
    if (it->IsStatement()) { body = it; it = it->prev; } else break;
    if ((jt1 = dynamic_cast<AST_JumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;

    // -- The pattern matches. Replace everything with a AST_CF_Loop node
    int prov = HandleBreakTargets(jt1, it, true);
    // We did it. This is a while...do... construct!
    AST_CF_Repeat *ru = new AST_CF_Repeat(dec, pc(), prov, in_, body->Unlink());
    jt1->Unlink();
    ReplaceWith(ru);
    dec.numASTChanges++;
    return ru;
  } while (0);
  return nullptr;
}

ASTNode *AST_BC_BranchIfFalse::Resolve(Pass pass)
{
  if (pass == Pass::DataFlow) {
    if (!in_ && prev->IsExpr()) {
      in_ = prev;
      prev->Unlink();
      dec.numASTChanges++;
      return next;
    } else {
      return next;
    }
  }
  if ((pass == Pass::ControlFlow) && (in_)) {
    ASTNode *nextNode = nullptr;
    if ((nextNode = ResolveIfTheElse())) return nextNode;
    if ((nextNode = ResolveRepeatUntil())) return nextNode;
  }
  return next;
}

void AST_BC_BranchIfFalse::Print(uint32_t flags) {
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

void AST_BC_Return::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("return ");
  in_->Print();
}

#pragma mark - For loop -

#pragma mark - AST_BC_IncrVar

void AST_BC_IncrVar::Print(uint32_t flags) {
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
  if (pass != Pass::ControlFlow) return next;
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
    /* n statments  */  ASTNode *body = nullptr;
    /* incrvar iter */  AST_BC_IncrVar *incrVar = nullptr;
    /* jumptarget 1 */  AST_JumpTarget *jt1 = nullptr;
    /* getvar limit */  AST_BC_GetVar *getvar1 = nullptr;
    /* branchloop 2 */  // <-- you are here

    // -- Try the pattern. We must trace backwards.
    ASTNode *it = prev;
    if ( !(getvar1  = ToBwd<AST_BC_GetVar>(&it, true)) )    break;
    if ( !(jt1      = ToBwd<AST_JumpTarget>(&it, false)) )  break;
    if ( !(incrVar  = ToBwd<AST_BC_IncrVar>(&it, false)) )  break;
    body = it; it = it->prev;
    if ( !(jt2      = ToBwd<AST_JumpTarget>(&it, false)) )  break;
    if ( !(branch   = ToBwd<AST_BC_Branch>(&it, false)) )   break;
    if ( !(getvar2  = ToBwd<AST_BC_GetVar>(&it, true)) )    break;
    // TODO: the remainder is in a CodeBlock
//    ##### ---> Body
//        ##### [P: 1] pc=  0: AST_BC_PushConst a=4, b=4
//      ##### [P: 0] pc=  1: AST_BC_SetVar a=20, b=5
//        ##### [P: 1] pc=  2: AST_BC_PushConst a=4, b=400
//      ##### [P: 0] pc=  5: AST_BC_SetVar a=20, b=6
//        ##### [P: 1] pc=  6: AST_BC_PushConst a=4, b=8
//      ##### [P: 0] pc=  9: AST_BC_SetVar a=20, b=7
//      ##### [P: 1] pc= 12: AST_BC_GetVar a=15, b=7
//      ##### <--- Body
//    ##### [P: 1] pc=  1: AST_CodeBlock a=0, b=0
    AST_CodeBlock *lead = dynamic_cast<AST_CodeBlock*>(it);
    if (!lead) break;
    int n = (int)lead->body_.size();
    if (n < 4) break;
    if ( !(getvar3  = dynamic_cast<AST_BC_GetVar*>(lead->body_[n-1])) ) break;
    if ( !(setvar1  = dynamic_cast<AST_BC_SetVar*>(lead->body_[n-2])) ) break;
    if ( !(setvar2  = dynamic_cast<AST_BC_SetVar*>(lead->body_[n-3])) ) break;
    if ( !(setvar3  = dynamic_cast<AST_BC_SetVar*>(lead->body_[n-4])) ) break;

    // Check use of variables
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
    int prov = HandleBreakTargets(branch, it, true);
    // Build our for loop node
    AST_CF_ForLoop *forLoopNode = new AST_CF_ForLoop(dec, pc(), prov, setvar3, setvar2->input(), setvar1->input());
    forLoopNode->body_ = body->Unlink();
    // remove from the "lead" code block
    n = n-4;
    lead->body_.resize(n);
    if (n == 0) lead->Unlink();
    // Unlink for the roor list
    getvar1->Unlink();
    getvar2->Unlink();
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

void AST_BC_NewIter::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

ASTNode *AST_BC_NewIter::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  ASTNode *ret = nullptr;
  if ((ret = ResolveForeachSlotValueDo())) return ret;
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
    /* setvar value */  AST_BC_SetVar *setvar2;
    /* setvar slot  */  AST_BC_SetVar *setvar3 = nullptr; // optional
    /* statements   */  AST_CodeBlock *body;
    /* iternext     */  AST_BC_IterNext *iternext;
    /* jumptarget 1 */  AST_JumpTarget *jt1;
    /* iterdone     */  AST_BC_IterDone *iterdone;
    /* c.branch 2   */  AST_BC_BranchIfFalse *cbranch;

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
    // The remaining header and the following body are in a single CodeBlock
    if ( !(body = ToFwd<AST_CodeBlock>(&it, false)) ) break;
    // The first node in body must set the 'value'
    if (body->body_.size() < 1) break;
    if ( !(setvar2 = dynamic_cast<AST_BC_SetVar*>(body->body_[0])) ) break;
    value = setvar2->b(); if (value != iter-1) break; // b = value
    // If we have a second setvar, and b = value-1, it's the 'slot' variable
    if (   (body->body_.size()  >= 2 )
        && ((setvar3 = dynamic_cast<AST_BC_SetVar*>(body->body_[1])))
        && (setvar3->b() == iter-2 )) { slot = setvar3->b(); };
    // The remaining statements in the code block form the body

    // Now find the rest of the 'foreach' pattern
    if ( !(iternext = ToFwd<AST_BC_IterNext>(&it, true)) ) break;
    if ( !(jt1 = ToFwd<AST_JumpTarget>(&it, false)) ) break;
    if ( !(iterdone = ToFwd<AST_BC_IterDone>(&it, false)) ) break;
    if ( !(cbranch = ToFwd<AST_BC_BranchIfFalse>(&it, false)) ) break;
    // TODO: we should still verify the two jump targets

    dec.useLocalAs(iter, Decompiler::Local::Use::iter);
    dec.useLocalAs(value, Decompiler::Local::Use::iter);
    if (slot != -1) dec.useLocalAs(slot, Decompiler::Local::Use::iter);

    // -- The pattern matches. Replace everything with a AST_CF_ForLoop node
    cbranch->HandleBreakTargets(jt2, it, true);
    // 'foreach' resets the iterator to nil for garbage collection.
    // It would probably be wise to check that before removing it here:
    it->Unlink(); // push-const nil, set-var |value|iter|
    AST_CF_ForEachSlotValueDo *foreachNode =
      new AST_CF_ForEachSlotValueDo(dec, pc_, slottedObj, slot, value, deeply);
    body->Unlink();
    body->body_.erase(body->body_.begin());
    if (slot != -1) body->body_.erase(body->body_.begin());
    foreachNode->body_ = body;
    slottedObj->Unlink();
    deeplyNd->Unlink();
    setvar1->Unlink();
    branch->Unlink();
    jt2->Unlink();
    iternext->Unlink();
    jt1->Unlink();
    iterdone->Unlink();
    cbranch->Unlink();
    ReplaceWith(foreachNode);
    dec.numASTChanges++;
    return foreachNode->next;
  } while (0);
  return nullptr;
}

ASTNode *AST_BC_NewIter::ResolveForeachSlotValueCollect()
{
// foreach slot value collect
//  5: slot: nil,
//  6: value: nil,
//  7: |slotvalue|iter|: nil,
//  8: |slotvalue|index|: nil,
//  9: |slotvalue|result|: nil

  // ##### [P: 1] pc=  8: push expr                           object
  // ##### [P: 1] pc=  9: AST_BC_PushConst a=4, b=2           deeply
  // ##### [P:-1] pc= 10: AST_BC_NewIter a=24, b=17           foreach
  // ##### [P:-1] pc= 13: AST_BC_SetVar a=20, b=7             iter
  //   ##### ---> Body
  //         ##### [P: 1] pc= 16: AST_BC_GetVar a=15, b=7     iter
  //         ##### [P: 1] pc= 19: AST_BC_PushConst a=4, b=20  5
  //       ##### [P: 1] pc= 22: AST_BC_ARef a=24, b=2         iter[5]
  //       ##### [P: 1] pc= 23: AST_BC_Push a=3, b=0          'array
  //     ##### [P: 1] pc= 24: AST_BC_NewArray a=17, b=65535   new array[5]
  //   ##### [P: 0] pc= 27: AST_BC_SetVar a=20, b=9           result = new array[5]
  //     ##### [P: 1] pc= 30: AST_BC_PushConst a=4, b=0       0
  //   ##### [P: 0] pc= 31: AST_BC_SetVar a=20, b=8           index
  //   ##### <--- Body
  // ##### [P: 0] pc= 27: AST_CodeBlock a=0, b=0
  // ##### [P:-4] pc= 34: AST_BC_Branch a=11, b=79            -> check for end
  // ##### [P:-3] pc= 37: AST_JumpTarget a=0, b=0 from 83     <- next round
  //     ##### ---> Body
  //         ##### [P: 1] pc= 37: AST_BC_GetVar a=15, b=7     iter
  //         ##### [P: 1] pc= 40: AST_BC_PushConst a=4, b=4   1
  //       ##### [P: 1] pc= 41: AST_BC_ARef a=24, b=2         iter[1]
  //     ##### [P: 0] pc= 42: AST_BC_SetVar a=20, b=6         value := iter[1]
  //         ##### [P: 1] pc= 43: AST_BC_GetVar a=15, b=7     iter
  //         ##### [P: 1] pc= 46: AST_BC_PushConst a=4, b=0   0
  //       ##### [P: 1] pc= 47: AST_BC_ARef a=24, b=2         iter[0]
  //     ##### [P: 0] pc= 48: AST_BC_SetVar a=20, b=5         slot
  //     ##### [P: 1] pc= 49: AST_BC_GetVar a=15, b=9         result ERR -v
  //     ##### <--- Body
  //   ##### [P: 1] pc= 42: AST_CodeBlock a=0, b=0
  //   ##### [P: 1] pc= 52: AST_BC_GetVar a=15, b=8           index ERR -v
  //     ##### ---> Body
  //         ##### [P: 1] pc= 55: AST_BC_GetVar a=15, b=5     slot
  //         ##### [P: 1] pc= 56: AST_BC_Push a=3, b=1
  //       ##### [P: 1] pc= 57: AST_BC_Call a=5, b=1
  //     ##### [P: 0] pc= 58: AST_BC_Pop a=0, b=0
  //     ##### [P: 0] pc= 62: AST_CF_Break a=0, b=89
  //     ##### [P: 1] pc= 66: AST_BC_GetVar a=15, b=6
  //     ##### <--- Body
  //   ##### [P: 1] pc= 58: AST_CodeBlock a=0, b=0
  // ##### [P: 0] pc= 67: AST_BC_SetARef a=24, b=3  --- ERROR -^^: this should have consumed 3!
  // ##### [P:-1] pc= 68: AST_BC_Pop a=0, b=0
  //   ##### [P: 1] pc= 69: AST_BC_PushConst a=4, b=4
  // ##### [P: 2] pc= 70: AST_BC_IncrVar a=22, b=8
  // ##### [P:-1] pc= 73: AST_BC_Pop a=0, b=0
  // ##### [P:-1] pc= 74: AST_BC_Pop a=0, b=0
  //   ##### [P: 1] pc= 75: AST_BC_GetVar a=15, b=7
  // ##### [P:-1] pc= 78: AST_BC_IterNext a=0, b=5
  // ##### [P:-3] pc= 79: AST_JumpTarget a=0, b=0 from 34
  //   ##### [P: 1] pc= 79: AST_BC_GetVar a=15, b=7
  // ##### [P:-1] pc= 82: AST_BC_IterDone a=0, b=6
  // ##### [P:-1] pc= 83: AST_BC_BranchIfFalse a=13, b=37
  // ##### [P:-4] pc= 86: AST_BC_Branch a=11, b=94
  // ##### [P:-3] pc= 89: AST_JumpTarget a=0, b=0 from 62
  // ##### [P:-1] pc= 89: AST_BC_SetVar a=20, b=9
  // ##### [P:-1] pc= 92: AST_BC_Pop a=0, b=0
  // ##### [P:-1] pc= 93: AST_BC_Pop a=0, b=0
  // ##### [P:-3] pc= 94: AST_JumpTarget a=0, b=0 from 86
  // ##### [P: 1] pc= 94: AST_BC_GetVar a=15, b=9
  //   ##### ---> Body
  //     ##### [P: 1] pc= 97: AST_BC_PushConst a=4, b=2
  //   ##### [P: 0] pc= 98: AST_BC_SetVar a=20, b=9
  //     ##### [P: 1] pc=101: AST_BC_PushConst a=4, b=2
  //   ##### [P: 0] pc=102: AST_BC_SetVar a=20, b=7
  //   ##### <--- Body
  // ##### [P: 0] pc= 98: AST_CodeBlock a=0, b=0
  // ##### [P:-1] pc=105: AST_BC_FindAndSetVar a=21, b=2
  //   ##### ---> Body
  //       ##### [P: 1] pc=106: AST_BC_FindVar a=14, b=2
  //       ##### [P: 1] pc=107: AST_BC_Push a=3, b=1
  //     ##### [P: 1] pc=108: AST_BC_Call a=5, b=1
  //   ##### [P: 0] pc=109: AST_BC_Pop a=0, b=0
  //     ##### [P: 1] pc=110: AST_BC_PushConst a=4, b=4
  //   ##### [P: 1] pc=111: AST_BC_Return a=0, b=2
  //   ##### <--- Body
  // ##### [P: 1] pc=109: AST_CodeBlock a=0, b=0
  // ##### [P:-2] pc= -1: AST_LastNode a=-1, b=-1


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

void AST_BC_IterNext::Print(uint32_t flags) {
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
void AST_BC_IterDone::Print(uint32_t flags) {
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

void AST_BC_NewHandler::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}


#pragma mark - AST_BC_PopHandlers

void AST_BC_PopHandlers::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - Calls -

#pragma mark - AST_BC_SetLexScope

void AST_BC_SetLexScope::Print(uint32_t flags) {
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

void AST_BC_Call::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  PrintResolvedCall(numIns_-1);
}

#pragma mark - AST_BC_Invoke

void AST_BC_Invoke::Print(uint32_t flags) {
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

void AST_BC_Send::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  ins_[numIns_-2]->Print();   // Print the receiver
  dec.p.Print(":");           // Print the operator
  if (ifDefined_) dec.p.Printf("?");
  PrintResolvedCall(numIns_-2);
}

#pragma mark - AST_BC_Resend

void AST_BC_Resend::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Print("inherited:");        // Print the receiver and operator
  if (ifDefined_) dec.p.Printf("?");
  PrintResolvedCall(numIns_-1);
}

