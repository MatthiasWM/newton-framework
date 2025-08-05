
/*
 File:    Matt/ASTControlFlow.h

 Matt's decompiler Abstract Syntax Tree.
 Control Flow nodes based on bytecodes.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTControlFlow.h"
#include "Matt/ASTControlFlowHelper.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/ASTMacros.h"

#include "Matt/Decompiler.h"
#include "Matt/ObjectPrinter.h"

using namespace ast;

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

#pragma mark - BCBranch

/**
 \brief Try to resolve this bytecode as part of a 'loop' construct.
 A loop is simply an unconditional jump backwards. It can be interrupted
 with a 'break' or 'return' statement. The decompiler assumes that the
 bytecode is correct and does not check for break or return.
 \return the next node if this is a 'loop', or nullptr if no match was found.
 */
Node *BCBranch::ResolveLoop()
{
  // ---- Check for the loop... pattern
  do {
    // -- Store the result of our exploration here
    /* target 1     */  JumpTarget *jt = nullptr;
    /* expr         */  Node *body = nullptr;
    /* branch 1     */  // <-- you are here

    // -- Try this pattern
    Node *iter = prev;
    if (b_ > pc_) break; // Jump must be backward
    if (iter->IsStatement()) { body = iter; iter = iter->prev; }
    if ( !(jt = ToBwd<JumpTarget>(&iter, false)) )  break;
    if ((jt->Origin() != pc()) || (jt->pc() != b()))    break;

    // -- The pattern matches. Replace everything with a CFLoop node
    // Check for a trailing "break targets"
    HandleBreakTargets(jt, iter = next, false);
    // It's a loop! Build a new node.
    if (body) body->Unlink(); else body = NewNil();
    CFLoop *loop = new CFLoop(dec, pc_, kProvidesOne, body);
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
 The CFBreak will take care of the jump target when resolved.
 \return the next node if this is a 'break', or nullptr if no match was found.
 */
Node *BCBranch::ResolveBreak()
{
  do {
    // -- Check the pattern
    if (b_ < pc_) break;
    if (!prev->IsExpr()) break;
    if (!dynamic_cast<BCPop*>(next)) break;
    // -- It applies. Replace the instructions and remove the jump target.
    CFBreak *breakNode = new CFBreak(dec, pc(), b(), prev->Unlink());
    next->Unlink();
    // Don't delete the jump target! Let the loops take care of that.
    ReplaceWith(breakNode);
    dec.numASTChanges++;
    return breakNode->next;
  } while (0);
  return nullptr;
}

Node *BCBranch::Resolve(Pass pass)
{
  if (pass == Pass::DataFlow) {
    // If this resolves to a 'break', it behaves like a data flow element
    Node *nextNode = ResolveBreak();
    if (nextNode) return nextNode;
  }
  if (pass == Pass::ControlFlow) {
    // If this resolves to 'loop', it's part of the control flow
    Node *nextNode = ResolveLoop();
    if (nextNode) return nextNode;
  }
  return next;
}

void BCBranch::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - BCBranchIfTrue

/**
 \class BCBranchIfTrue
 \brief A conditional jump.
 A value is popped from the stack. If it is nil, execution continues with the
 next instruction. Otherwise, PC is set to the B field value.
 */
Node *BCBranchIfTrue::Resolve(Pass pass)
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
    Node *nextNode = nullptr;
    if ((nextNode = ResolveWhileDo())) return nextNode;
  }
  return next;
}

Node *BCBranchIfTrue::ResolveWhileDo()
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
    /* branch 2 */  BCBranch *branch2 = nullptr;
    /* target 1 */  JumpTarget *jt1 = nullptr;
    /* n-stmts  */  Node *body = nullptr;
    /* target 2 */  JumpTarget *jt2 = nullptr;
    /* expr     */  // Already in this->in_
    /* c.brch 1 */  // <-- you are here
    /* push nil */
    /* break targets */
    /* consumer */

    // -- Try the pattern
    Node *it = prev;
    if (b_ > pc_) break;  // jump backwards
    if (!in_) break;
    if ((jt2 = dynamic_cast<JumpTarget*>(it))) it = it->prev; else break;
    if (it->IsStatement()) { body = it; it = it->prev; }
    if ((jt1 = dynamic_cast<JumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;
    if (!(branch2 = dynamic_cast<BCBranch*>(it))) break;
    if ((branch2->b() != jt2->pc()) || (branch2->pc() != jt2->Origin())) break;
    // A useless "push-const nil, pop" was already removed in BCPop::Resolve()
    // If there are break targets, they will be removed below.

    // -- The pattern matches. Replace everything with a CFLoop node
    // Check for a trailing "push-nil, break targets, consumer"
    int prov = HandleBreakTargets(branch2, it = next, true);
    // Now create our while...do node:
    if (body) body->Unlink(); else body = NewNil();
    CFWhile *wd = new CFWhile(dec, pc(), prov, in_, body);
    delete branch2->Unlink();
    delete jt1->Unlink();
    delete jt2->Unlink();
    ReplaceWith(wd);
    dec.numASTChanges++;
    return wd;
  } while (0);
  return next;
}

void BCBranchIfTrue::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - BCBranchIfFalse

/**
 \class BCBranchIfFalse
 \brief Based on this node, find the pattern of an if/then or if/then/else structure in the AST.

 This class checks for three different pattern, generating one of three possible variations
 of the CFIfThen. If one of the pattern matches, the new CFIfThen
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
 \see CFIfThen
 */

/**
 \brief Find patterns around a branch-if-false instruction and resolve them.

 This instruction can be the start of an if...then...else... construct. There
 are three different patterns: if-expr-then-expr-else-expr, if-expr-then-stmt,
 and if-expr-then-stmt-else-stmt.

 If a matching pattern is found, branch commands and jump targets are removed
 and this node is replaced with a CFIfThen, holding the instructions
 inside the 'then' and 'else' branch.
 */
Node *BCBranchIfFalse::ResolveIfTheElse() {
  // ---- Check for the if...then...else... pattern
  do { // If any of the pattern checks fail, we can escape using 'break'.
    // -- Store the result of our exploration here
    Node *it = next;
    Node *ifStmt = nullptr;
    Node *elseStmt = nullptr;
    JumpTarget *jt1 = nullptr;
    JumpTarget *jt2 = nullptr;
    BCBranch *bi2 = nullptr;
    bool returnsAValue = false;
    bool hasElse = false;

    // -- Try this pattern
    /*          */  if (pc() > b()) break;  // Jump must be forward
    /* expr     */  if (!in_) break;
    /* branch.f */  // <-- you are here
    /* n-stmts  */  if (it->IsStatement()) { ifStmt = it; it = it->next; }
    /* [expr]   */  else if (it->IsExpr()) { ifStmt = it; returnsAValue = true; it = it->next; }
    /*          */  else break;
    /* [branch] */  if ((bi2 = dynamic_cast<BCBranch*>(it))) { hasElse = true; it = it->next; }
    /* target   */  if ((jt1 = dynamic_cast<JumpTarget*>(it))) it = it->next; else break;
    /*          */  if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;
    /*          */  if (hasElse) {
    /* n-stmts  */    if (!returnsAValue && it->IsStatement()) { elseStmt = it; it = it->next; }
    /* [expr]   */    else if (returnsAValue && it->IsExpr()) { elseStmt = it; it = it->next; }
    /*          */    else break;
    /* target   */    if (!(jt2 = dynamic_cast<JumpTarget*>(it))) break;
    /*          */    if ((jt2->Origin() != bi2->pc()) || (jt2->pc() != bi2->b())) break;
    /*          */  }

    // -- The pattern matches. Replace everything with a CFIfThen
    CFIfThen *newNode = new CFIfThen(dec, pc_, in_, returnsAValue);
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

Node *BCBranchIfFalse::ResolveRepeatUntil() {
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    /* target 1 */  JumpTarget *jt1 = nullptr;
    /* n-stmts  */  Node *body = nullptr;
    /* expr     */  // condition is in this->in_
    /* c.brch 1 */  // <-- you are here

    // -- Try the pattern
    Node *it = prev;
    if (b_ > pc_) break;  // jump backwards
    if (!in_) break;
    if (it->IsStatement()) { body = it; it = it->prev; };
    if ((jt1 = dynamic_cast<JumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;

    // -- The pattern matches. Replace everything with a CFLoop node
    int prov = HandleBreakTargets(jt1, it = next, true);
    // We did it. This is a while...do... construct!
    if (body) body->Unlink(); else body = NewNil();
    CFRepeat *ru = new CFRepeat(dec, pc(), prov, in_, body);
    jt1->Unlink();
    ReplaceWith(ru);
    dec.numASTChanges++;
    return ru;
  } while (0);
  return nullptr;
}

Node *BCBranchIfFalse::Resolve(Pass pass)
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
    Node *nextNode = nullptr;
    if ((nextNode = ResolveIfTheElse())) return nextNode;
    if ((nextNode = ResolveRepeatUntil())) return nextNode;
  }
  return next;
}

void BCBranchIfFalse::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - BCReturn

/**
 \class BCReturn
 \brief Return from this function.
 \todo the very last return probably doesn't need to be printed. It's actually
    a bug in the newt-framework compiler. NTK does not generate the extra return bytecode
 \todo return NIL is implied if there is no return statement in the source code
 \todo handle implied return values nicely, so we don't generate "return a := b;"
 */

void BCReturn::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("return ");
  in_->Print();
}

#pragma mark - For loop -

#pragma mark - BCIncrVar

void BCIncrVar::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - BCBranchLoop

BCBranchLoop::BCBranchLoop(Decompiler &d, int pc, int a, int b)
: Bytecode(d, pc, a, b)
{ }

Node *BCBranchLoop::Resolve(Pass pass)
{
  if (pass != Pass::ControlFlow) return next;
  do {
    // ---- Try the for...to...by...do... pattern and take breaks into account.
    // We are at the end of the pattern, so walk backwards down the AST root.
    Node *it = prev;
    int iter = -1, limit = -1, incr = -1;
    //           ( BCBranchLoop, this )
    REQUIRED_NODE( BCGetVar,   getLimit, it, true  ) { it = it->prev; }
    REQUIRED_NODE( JumpTarget, jtTest,   it, false ) { it = it->prev; }
    REQUIRED_NODE( BCIncrVar,  incIter,  it, false ) { it = it->prev; }
    OPTIONAL_COND( Node, body, body->IsStatement(), it, false) { it = it->prev; }
    REQUIRED_NODE( JumpTarget, jtAgain,  it, false ) { it = it->prev; }
    REQUIRED_NODE( BCBranch,   brTest,   it, false ) { it = it->prev; }
    REQUIRED_NODE( BCGetVar,   getIter,  it, false ) { it = it->prev; }
    REQUIRED_NODE( CodeBlock,  start,    it, true  ) { it = it->prev; }
    int nInstr = start->size(); if (nInstr < 4) break;
    REQUIRED_NODE( BCGetVar,   getIncr,  start->at(nInstr-1), true);
    REQUIRED_NODE( BCSetVar,   setIncr,  start->at(nInstr-2), true);
    REQUIRED_NODE( BCSetVar,   setLimit, start->at(nInstr-3), true);
    REQUIRED_NODE( BCSetVar,   setIter,  start->at(nInstr-4), true);

    // The pattern is correct. Now check the use of locals
    iter  = setIter->b();  if (getIter->b() != iter)   break;
    limit = getLimit->b(); if (getLimit->b() != limit) break;
    incr  = setIncr->b();  if (getIncr->b() != incr)   break;

    // Locals are correct. Now check the jump instructions.
    if ((jtAgain->Origin() != pc()) || (jtAgain->pc() != b())) break;
    if ((jtTest->Origin() != brTest->pc()) || (jtTest->pc() != brTest->b())) break;

    // ---- If we reach all this way, the pattern matches.
    // Eval and unlink all the jump targets of break instructions inside the loop
    int prov = HandleBreakTargets(brTest, it = next, true);
    // Unlink all nodes in the pattern, so they can be relinked down the AST or later deleted
    getLimit->Unlink();
    jtTest->Unlink();
    incIter->Unlink();
    jtAgain->Unlink();
    brTest->Unlink();
    getIter->Unlink();
    start->body_.resize(nInstr - 4); // unlink the instructions at the end of the start code block
    if (start->size() == 0) start->Unlink();

    // Mark the locals with an alternative use, so they are not declared
    dec.useLocalAs(incr, Decompiler::Local::Use::iter);
    dec.useLocalAs(limit, Decompiler::Local::Use::iter);
    dec.useLocalAs(iter, Decompiler::Local::Use::iter);

    // Create a CFForLoop node that replaces the entire pattern
    if (body) body->Unlink(); else body = NewNil();
    CFForLoop *forLoopNode = new CFForLoop(dec, pc(), prov, setIter, setLimit->input(), setIncr->input(), body);
    ReplaceWith(forLoopNode);

    // Wrap things up
    dec.numASTChanges++;
    return forLoopNode->next;
  } while (0);
  return next;
}

#pragma mark - Foreach loop -

#pragma mark - BCNewIter

/**
 \class BCNewIter
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

 \see BCIterNext
 \see BCIterDone
 */

void BCNewIter::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

Node *BCNewIter::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  Node *ret = nullptr;
  if ((ret = ResolveForeachSlotValueDo())) return ret;
  if ((ret = ResolveForeachSlotValueCollect())) return ret;
  return next;
}

Node *BCNewIter::ResolveForeachSlotValueDo()
{
  do {
    // ---- Try the "foreach slot,value deeply in object do" pattern and take breaks into account.
    Node *it = prev;
    int slot = -1, value = -1, iter = -1;
    // Traverse back to evaluate the setup.
    REQUIRED_NODE( BCPushConst, deeplyConst, prev, true ) { it = it->prev; }
    REQUIRED_COND( Node, setObject, setObject->IsExpr(), it, true);
    // Travers forward to evaluate the rest of the pattern.
    it = next;
    REQUIRED_NODE( BCSetVar, setIter, it, false ) { it = it->next; iter = setIter->b(); }
    REQUIRED_NODE( BCBranch, brStart, it, false ) { it = it->next; }
    REQUIRED_NODE( JumpTarget, jtRepeat, it, false ) { it = it->next; }
    REQUIRED_NODE( CodeBlock, body, it, true ) { it = it->next; }
    // TODO: body can be missing if original is 'begin end'. Must replace with NIL.
    // TODO: also the two nodes below are now in the AST Root, and we must an add the matching unlink
    REQUIRED_COND( BCSetVar, setValue, setValue->b() == iter-1, body->at(0), true) {
      value = setValue->b();
    }
    OPTIONAL_NODE( BCSetVar, setSlot, body->at(1), true) {
      if (setSlot->b() == value-1) slot = setSlot->b();
    }
    REQUIRED_NODE( BCIterNext, iterNext, it, false ) { it = it->next; }
    REQUIRED_NODE( JumpTarget, jtStart, it, false ) { it = it->next; }
    REQUIRED_NODE( BCIterDone, iterDone, it, false ) { it = it->next; }
    REQUIRED_NODE( BCBranchIfFalse, brRepeat, it, false ) { it = it->next; }

    // The pattern is correct. Now check the jump instructions.
    if ((jtStart->Origin() != brStart->pc()) || (jtStart->pc() != brStart->b())) break;
    if ((jtRepeat->Origin() != brRepeat->pc()) || (jtRepeat->pc() != brRepeat->b())) break;

    // Find out if the original source code used 'deeply'
    bool deeply;
    if (deeplyConst->b() == NILREF) deeply = false;
    else if (deeplyConst->b() == TRUEREF) deeply = true;
    else break;

    // ---- If we reach all this way, the pattern matches.
    // Eval and unlink all the jump targets of break instructions inside the loop
    HandleBreakTargets(body, it, true);
    // Remove the command that clears the iterator for garbage collection
    it->Unlink();

    // If setObject is a CodeBlock, only use the last expression
    CodeBlock *objBlock = dynamic_cast<CodeBlock*>(setObject);
    Node *obj = nullptr;
    if (objBlock) {
      obj = objBlock->back();
      objBlock->pop_back();
      objBlock->UnlinkIfEmpty();
    } else {
      obj = setObject;
      setObject->Unlink();
    }

    // Unlink all nodes in the pattern, so they can be relinked down the AST or later deleted
    deeplyConst->Unlink();
    setIter->Unlink();
    brStart->Unlink();
    jtRepeat->Unlink();
    body->pop_front(); // unlink 'setValue'
    if (slot != -1) body->pop_front(); // unlink 'setSlot'
    body->Unlink();
    iterNext->Unlink();
    jtStart->Unlink();
    iterDone->Unlink();
    brRepeat->Unlink();

    // Mark the locals with an alternative use, so they are not declared
    if (slot != -1) dec.useLocalAs(slot, Decompiler::Local::Use::iter);
    dec.useLocalAs(value, Decompiler::Local::Use::iter);
    dec.useLocalAs(iter, Decompiler::Local::Use::iter);

    // Create a CFForEachSlotValueDo node that replaces the entire pattern
    CFForEachSlotValueDo *foreachNode =
      new CFForEachSlotValueDo(dec, pc_, slot, value, deeply, obj, body);
    ReplaceWith(foreachNode);

    // Wrap things up
    dec.numASTChanges++;
    return foreachNode->next;
  } while (0);
  return nullptr;
}

Node *BCNewIter::ResolveForeachSlotValueCollect()
{
// foreach slot value collect
//  5: slot: nil,
//  6: value: nil,
//  7: |slotvalue|iter|: nil,
//  8: |slotvalue|index|: nil,
//  9: |slotvalue|result|: nil
  do {
    // ---- Try the "foreach slot,value deeply in object do" pattern and take breaks into account.
    Node *it = prev;
    int slot = -1, value = -1, iter = -1, index = -1, result = -1;
    // Traverse back to evaluate the setup.
    REQUIRED_NODE( BCPushConst, deeplyConst, prev, true ) { it = it->prev; }
    REQUIRED_COND( Node, setObject, setObject->IsExpr(), it, true);
    // Travers forward to evaluate the rest of the pattern.
    it = next;
    REQUIRED_NODE( BCSetVar, setIter, it, false ) { it = it->next; iter = setIter->b(); }
    // The following block initializes the index and result for collecting data
    REQUIRED_NODE( CodeBlock, initCollect, it, false ) { it = it->next; iter = setIter->b(); }
    REQUIRED_NODE( BCSetVar, initResult, initCollect->at(0), true ) { result = initResult->b(); }
    REQUIRED_NODE( BCSetVar, initIndex, initCollect->at(1), true ) { index = initIndex->b(); }
    // Jump to the start of the loop
    REQUIRED_NODE( BCBranch, brStart, it, false ) { it = it->next; }
    REQUIRED_NODE( JumpTarget, jtRepeat, it, false ) { it = it->next; }
    // The following block contains the setup, the body, and the collector setting the 'result'
    REQUIRED_NODE( BCPop, bodyAndCollect, it, true ) { it = it->next; }
    // TODO: body can be missing if original is 'begin end'. Must replace with NIL.
    // TODO: the line above then returns a CodeBlock and the stuff below changes
    REQUIRED_NODE( BCSetARef, collect, bodyAndCollect->Input(), true );
    REQUIRED_NODE( CodeBlock, setup, collect->Object(), true );
    REQUIRED_NODE( BCSetVar, setValue, setup->at(0), true ) { value = setValue->b(); }
    OPTIONAL_NODE( BCSetVar, setSlot, setup->at(1), true ) { if (setSlot->b() == value-1) slot = setSlot->b(); }
    REQUIRED_COND( Node, body, body->IsExpr(), collect->Element(), true );
    // Count while collecting
    REQUIRED_NODE( BCIncrVar, incrIndex, it, true ) { it = it->next; }
    REQUIRED_NODE( BCPop, popIV0, it, false ) { it = it->next; }
    REQUIRED_NODE( BCPop, popIV1, it, false ) { it = it->next; }
    // Iterate through the slotted object
    REQUIRED_NODE( BCIterNext, iterNext, it, false ) { it = it->next; }
    REQUIRED_NODE( JumpTarget, jtStart, it, false ) { it = it->next; }
    REQUIRED_NODE( BCIterDone, iterDone, it, false ) { it = it->next; }
    REQUIRED_NODE( BCBranchIfFalse, brRepeat, it, false ) { it = it->next; }
    // Jump forward when done
    REQUIRED_NODE( BCBranch, skipCleanup, it, false ) { it = it->next; }
    // Skip all jump targets from 'break' instructions inside the loop
    for (;;) {
      REQUIRED_NODE( JumpTarget, jtBreak, it, false ) { it = it->next; }
    }
    // Set the result to whatever the 'break' instruction wants.
    OPTIONAL_NODE( BCSetVar, setResult2, it, false ) { it = it->next; }
    REQUIRED_NODE( BCPop, popR0, it, false ) { it = it->next; }
    REQUIRED_NODE( BCPop, popR1, it, false ) { it = it->next; }
    // Set the result
    REQUIRED_NODE( JumpTarget, jtCleanup, it, false ) { it = it->next; }
    REQUIRED_NODE( BCGetVar, getResult, it, false ) { it = it->next; }
    // Prepare result and iter for garbage collection
    REQUIRED_NODE( CodeBlock, prepareForGC, it, true ) { it = it->next; }

    // The pattern is correct. Now check the jump instructions.
    if ((jtStart->Origin() != brStart->pc()) || (jtStart->pc() != brStart->b())) break;
    if ((jtRepeat->Origin() != brRepeat->pc()) || (jtRepeat->pc() != brRepeat->b())) break;
    if ((jtCleanup->Origin() != skipCleanup->pc()) || (jtCleanup->pc() != skipCleanup->b())) break;

    // Find out if the original source code used 'deeply'
    bool deeply;
    if (deeplyConst->b() == NILREF) deeply = false;
    else if (deeplyConst->b() == TRUEREF) deeply = true;
    else break;

    // ---- If we reach all this way, the pattern matches.
    // Unlink everything between this and prepareForGC
    // Eval and unlink all the jump targets of break instructions inside the loop
//    HandleBreakTargets(body, it, true);


    // If setObject is a CodeBlock, only use the last expression
    CodeBlock *objBlock = dynamic_cast<CodeBlock*>(setObject);
    Node *obj = nullptr;
    if (objBlock) {
      obj = objBlock->back();
      objBlock->pop_back();
      objBlock->UnlinkIfEmpty();
    } else {
      obj = setObject;
      setObject->Unlink();
    }

    deeplyConst->Unlink();

    // Unlink everything from this to prepareForGC
    while (next && (next != prepareForGC)) next->Unlink();
    prepareForGC->Unlink();

    // Mark the locals with an alternative use, so they are not declared
    if (slot != -1) dec.useLocalAs(slot, Decompiler::Local::Use::iter);
    dec.useLocalAs(value, Decompiler::Local::Use::iter);
    dec.useLocalAs(iter, Decompiler::Local::Use::iter);
    dec.useLocalAs(index, Decompiler::Local::Use::iter);
    dec.useLocalAs(result, Decompiler::Local::Use::iter);

    // Create a CFForEachSlotValueDo node that replaces the entire pattern
    CFForEachSlotValueDo *foreachNode =
    new CFForEachSlotValueDo(dec, pc_, slot, value, deeply, obj, body);
    ReplaceWith(foreachNode);

    // Wrap things up
    dec.numASTChanges++;
    return foreachNode->next;
  } while (0);
  return nullptr;
}

#pragma mark - BCIterNext

/**
 \class BCIterNext
 \brief Continue a foreach operation
 ```
 iterator --
 ```
 Pops a reference to an iterator from the stack and advances it to the next slot.
 */

void BCIterNext::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - BCIterDone

/**
 \class BCIterDone
 \brief Finalizes a 'foreach' statement.
 ```
 iterator -- done?
 ```
 Pops a reference to an iterator from the stack. If iterator is exhausted,
 pushes true onto the stack; otherwise, pushes nil onto the stack.
 */
void BCIterDone::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - Exceptions -

#pragma mark - BCNewHandler

/* This is the start of a 'try' block. */

// BCNewHandler a b c
//   Statements
// PopHandlers
// Branch x
// Exception Handler a
//   CodeBlock
// Branch y
// Exception Handler b
//   CodeBlock
// Branch y
// Exception Handler c (last)
//   CodeBlock
// JumpTarget y
// PopHandlers
// JumpTarget x

void BCNewHandler::Print(uint32_t flags) {
  return PrintNode(false);
}

Node *BCNewHandler::Resolve(Pass pass)
{
  if ((pass == Pass::DataFlow) && (!ConsumeN::Resolved()))
    return ConsumeN::Resolve(pass);
  if ((pass != Pass::ControlFlow) || Resolved()) return next;
  do {
    // ---- Find the try...onexception...do... pattern
    int i;
    Node *it = next;
    REQUIRED_COND( Node, body, body->IsStatement(), it, true) { it = it->next; }
    REQUIRED_NODE( BCPopHandlers, bodyPop, it, false ) { it = it->next; }
    REQUIRED_NODE( BCBranch, brDone, it, false ) { it = it->next; }
    // Handle the 'onException...do...' pattern
    for (i=1; i<b_; i++) {
      REQUIRED_NODE( ExceptionHandler, handler, it, false ) { it = it->next; }
      REQUIRED_COND( Node, exBody, exBody->IsStatement(), it, true) { it = it->next; }
      REQUIRED_NODE( BCBranch, exDone, it, false ) { it = it->next; }
    }
    if (i != b_) break; // not enough handlers
    // Handle the last 'onException...do...' pattern
    REQUIRED_NODE( ExceptionHandler, handler, it, false ) { it = it->next; }
    REQUIRED_COND( Node, exBody, exBody->IsStatement(), it, true) { it = it->next; }
    // Handle the final cleanup
    REQUIRED_NODE( JumpTarget, jtExDone, it, false ) { it = it->next; }
    REQUIRED_NODE( BCPopHandlers, exPop, it, false ) { it = it->next; }
    REQUIRED_NODE( JumpTarget, jtDone, it, false ) { it = it->next; }

    // ---- The pattern is correct. Now make it printable.
    CFTry *exNode = new CFTry(dec, pc(), this, jtDone);
    ReplaceWith(exNode);
    dec.numASTChanges++;
    return exNode->next;
  } while (0);
  return next;
}

#pragma mark - BCPopHandlers

void BCPopHandlers::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - Calls -

#pragma mark - BCSetLexScope

void BCSetLexScope::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - BCCall

Node *BCCall::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  do {
    if (numIns_ != 3) break;
    auto nameNode = dynamic_cast<BCPush*>(prev); // BCPush is always resolved
    if (!nameNode) break;
    if (!prev->prev->IsExpr() || !prev->prev->prev->IsExpr()) break;
    RefVar sym = dec.GetLiteral(nameNode->b());
    if (!::IsSymbol(sym)) break;
    const char *name = SymbolName(sym);
    if (!name) break;
    BinaryOperator *op = nullptr;
    if (strcmp(name, "<<")==0) {
      op = new BinaryOperator(dec, pc_, a_, b_, "<<", 8);
    } else if (strcmp(name, ">>")==0) {
      op = new BinaryOperator(dec, pc_, a_, b_, ">>", 8);
    } else if (strcasecmp(name, "mod")==0) {
      op = new BinaryOperator(dec, pc_, a_, b_, "mod", 7);
    }
    if (op) {
      delete prev->Unlink();
      this->ReplaceWith(op);
      dec.numASTChanges++;
      return op->Resolve(pass);
    }
  } while (0);
  return ConsumeN::Resolve(pass);
}

void BCCall::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  PrintResolvedCall(numIns_-1);
}

#pragma mark - BCInvoke

void BCInvoke::Print(uint32_t flags) {
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

#pragma mark - BCSend

void BCSend::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  ins_[numIns_-2]->Print();   // Print the receiver
  dec.p.Print(":");           // Print the operator
  if (ifDefined_) dec.p.Printf("?");
  PrintResolvedCall(numIns_-2);
}

#pragma mark - BCResend

void BCResend::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Print("inherited:");        // Print the receiver and operator
  if (ifDefined_) dec.p.Printf("?");
  PrintResolvedCall(numIns_-1);
}

