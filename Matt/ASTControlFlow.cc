
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
    if (iter->IsStatement()) { body = iter; iter = iter->prev; } else break;
    if ( !(jt = ToBwd<JumpTarget>(&iter, false)) )  break;
    if ((jt->Origin() != pc()) || (jt->pc() != b()))    break;

    // -- The pattern matches. Replace everything with a CFLoop node
    // Check for a trailing "break targets"
    HandleBreakTargets(jt, iter, false);
    // It's a loop! Build a new node.
    CFLoop *loop = new CFLoop(dec, pc_, kProvidesOne, body->Unlink());
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
    if (it->IsStatement()) { body = it; it = it->prev; } else break;
    if ((jt1 = dynamic_cast<JumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;
    if (!(branch2 = dynamic_cast<BCBranch*>(it))) break;
    if ((branch2->b() != jt2->pc()) || (branch2->pc() != jt2->Origin())) break;
    // A useless "push-const nil, pop" was already removed in BCPop::Resolve()
    // If there are break targets, they will be removed below.

    // -- The pattern matches. Replace everything with a CFLoop node
    // Check for a trailing "push-nil, break targets, consumer"
    int prov = HandleBreakTargets(branch2, it, true);
    // Now create our while...do node:
    CFWhile *wd = new CFWhile(dec, pc(), prov, in_, body->Unlink());
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
    if (it->IsStatement()) { body = it; it = it->prev; } else break;
    if ((jt1 = dynamic_cast<JumpTarget*>(it))) it = it->prev; else break;
    if ((jt1->Origin() != pc()) || (jt1->pc() != b())) break;

    // -- The pattern matches. Replace everything with a CFLoop node
    int prov = HandleBreakTargets(jt1, it, true);
    // We did it. This is a while...do... construct!
    CFRepeat *ru = new CFRepeat(dec, pc(), prov, in_, body->Unlink());
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

/*
 This indicates the end of a 'for' loop. The pattern is:
 local 5:i, 6:|i|limit|, 7:|i|incr|
 BCSetVar b=5
 BCSetVar b=6
 BCSetVar b=7
 BCGetVar b=7
 BCGetVar b=5
 BCBranch 1
 JumpTarget 2
 Statements
 BCIncrVar a=22, b=5
 JumpTarget 1
 BCGetVar a=15, b=6
 BCBranchLoop 2
 */

BCBranchLoop::BCBranchLoop(Decompiler &d, int pc, int a, int b)
: Bytecode(d, pc, a, b)
{ }

Node *BCBranchLoop::Resolve(Pass pass)
{
  if (pass != Pass::ControlFlow) return next;
  do {
    // -- Here is our pattern. Store the result of our exploration here:
    int iter = -1, limit = -1, incr = -1;
    /* setvar iter  */  BCSetVar *setvar3 = nullptr;
    /* setvar limit */  BCSetVar *setvar2 = nullptr;
    /* setvar incr  */  BCSetVar *setvar1 = nullptr;
    /* getvar incr  */  BCGetVar *getvar3 = nullptr;
    /* getvar iter  */  BCGetVar *getvar2 = nullptr;
    /* branch 1     */  BCBranch *branch = nullptr;
    /* jumptarget 2 */  JumpTarget *jt2 = nullptr;
    /* n statments  */  Node *body = nullptr;
    /* incrvar iter */  BCIncrVar *incrVar = nullptr;
    /* jumptarget 1 */  JumpTarget *jt1 = nullptr;
    /* getvar limit */  BCGetVar *getvar1 = nullptr;
    /* branchloop 2 */  // <-- you are here

    // -- Try the pattern. We must trace backwards.
    Node *it = prev;
    if ( !(getvar1  = ToBwd<BCGetVar>(&it, true)) )    break;
    if ( !(jt1      = ToBwd<JumpTarget>(&it, false)) )  break;
    if ( !(incrVar  = ToBwd<BCIncrVar>(&it, false)) )  break;
    body = it; it = it->prev;
    if ( !(jt2      = ToBwd<JumpTarget>(&it, false)) )  break;
    if ( !(branch   = ToBwd<BCBranch>(&it, false)) )   break;
    if ( !(getvar2  = ToBwd<BCGetVar>(&it, true)) )    break;
    // TODO: the remainder is in a CodeBlock
//    ##### ---> Body
//        ##### [P: 1] pc=  0: BCPushConst a=4, b=4
//      ##### [P: 0] pc=  1: BCSetVar a=20, b=5
//        ##### [P: 1] pc=  2: BCPushConst a=4, b=400
//      ##### [P: 0] pc=  5: BCSetVar a=20, b=6
//        ##### [P: 1] pc=  6: BCPushConst a=4, b=8
//      ##### [P: 0] pc=  9: BCSetVar a=20, b=7
//      ##### [P: 1] pc= 12: BCGetVar a=15, b=7
//      ##### <--- Body
//    ##### [P: 1] pc=  1: CodeBlock a=0, b=0
    CodeBlock *lead = dynamic_cast<CodeBlock*>(it);
    if (!lead) break;
    int n = (int)lead->body_.size();
    if (n < 4) break;
    if ( !(getvar3  = dynamic_cast<BCGetVar*>(lead->body_[n-1])) ) break;
    if ( !(setvar1  = dynamic_cast<BCSetVar*>(lead->body_[n-2])) ) break;
    if ( !(setvar2  = dynamic_cast<BCSetVar*>(lead->body_[n-3])) ) break;
    if ( !(setvar3  = dynamic_cast<BCSetVar*>(lead->body_[n-4])) ) break;

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

    // -- The pattern matches. Replace everything with a CFForLoop node
    int prov = HandleBreakTargets(branch, it, true);
    // Build our for loop node
    CFForLoop *forLoopNode = new CFForLoop(dec, pc(), prov, setvar3, setvar2->input(), setvar1->input());
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
    // -- Here is our pattern. Store the result of our exploration here:
    int slot = -1, value = -1, iter = -1;
    /* expr         */  Node *slottedObj;
    /* pushconst    */  BCPushConst *deeplyNd;
    /* newiter      */  // <---- you are here
    /* setvar iter  */  BCSetVar *setvar1;
    /* branch 1     */  BCBranch *branch;
    /* jumptarget 2 */  JumpTarget *jt2;
    /* setvar value */  BCSetVar *setvar2;
    /* setvar slot  */  BCSetVar *setvar3 = nullptr; // optional
    /* statements   */  CodeBlock *body;
    /* iternext     */  BCIterNext *iternext;
    /* jumptarget 1 */  JumpTarget *jt1;
    /* iterdone     */  BCIterDone *iterdone;
    /* c.branch 2   */  BCBranchIfFalse *cbranch;

    // -- Try the pattern. We must trace backwards.
    // Walk backwards in the AST root
    Node *it = prev;
    bool deeply = false;
    if ( !(deeplyNd = ToBwd<BCPushConst>(&it, false)) ) break;
    if (deeplyNd->b() == 2) deeply = false;
    else if (deeplyNd->b() == 26) deeply = true;
    else break;
    slottedObj = it; if (!slottedObj->Resolved()) break;
    // Now walk forwards
    it = next;
    if ( !(setvar1 = ToFwd<BCSetVar>(&it, false)) ) break;
    iter = setvar1->b();
    if ( !(branch = ToFwd<BCBranch>(&it, false)) ) break;
    if ( !(jt2 = ToFwd<JumpTarget>(&it, false)) ) break;
    // The remaining header and the following body are in a single CodeBlock
    if ( !(body = ToFwd<CodeBlock>(&it, false)) ) break;
    // The first node in body must set the 'value'
    if (body->body_.size() < 1) break;
    if ( !(setvar2 = dynamic_cast<BCSetVar*>(body->body_[0])) ) break;
    value = setvar2->b(); if (value != iter-1) break; // b = value
    // If we have a second setvar, and b = value-1, it's the 'slot' variable
    if (   (body->body_.size()  >= 2 )
        && ((setvar3 = dynamic_cast<BCSetVar*>(body->body_[1])))
        && (setvar3->b() == iter-2 )) { slot = setvar3->b(); };
    // The remaining statements in the code block form the body

    // Now find the rest of the 'foreach' pattern
    if ( !(iternext = ToFwd<BCIterNext>(&it, true)) ) break;
    if ( !(jt1 = ToFwd<JumpTarget>(&it, false)) ) break;
    if ( !(iterdone = ToFwd<BCIterDone>(&it, false)) ) break;
    if ( !(cbranch = ToFwd<BCBranchIfFalse>(&it, false)) ) break;
    // TODO: we should still verify the two jump targets

    dec.useLocalAs(iter, Decompiler::Local::Use::iter);
    dec.useLocalAs(value, Decompiler::Local::Use::iter);
    if (slot != -1) dec.useLocalAs(slot, Decompiler::Local::Use::iter);

    // -- The pattern matches. Replace everything with a CFForLoop node
    cbranch->HandleBreakTargets(jt2, it, true);
    // 'foreach' resets the iterator to nil for garbage collection.
    // It would probably be wise to check that before removing it here:
    it->Unlink(); // push-const nil, set-var |value|iter|
    CFForEachSlotValueDo *foreachNode =
      new CFForEachSlotValueDo(dec, pc_, slottedObj, slot, value, deeply);
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

Node *BCNewIter::ResolveForeachSlotValueCollect()
{
// foreach slot value collect
//  5: slot: nil,
//  6: value: nil,
//  7: |slotvalue|iter|: nil,
//  8: |slotvalue|index|: nil,
//  9: |slotvalue|result|: nil

  // ##### [P: 1] pc=  8: push expr                           object
  // ##### [P: 1] pc=  9: BCPushConst a=4, b=2           deeply
  // ##### [P:-1] pc= 10: BCNewIter a=24, b=17           foreach
  // ##### [P:-1] pc= 13: BCSetVar a=20, b=7             iter
  //   ##### ---> Body
  //         ##### [P: 1] pc= 16: BCGetVar a=15, b=7     iter
  //         ##### [P: 1] pc= 19: BCPushConst a=4, b=20  5
  //       ##### [P: 1] pc= 22: BCARef a=24, b=2         iter[5]
  //       ##### [P: 1] pc= 23: BCPush a=3, b=0          'array
  //     ##### [P: 1] pc= 24: BCNewArray a=17, b=65535   new array[5]
  //   ##### [P: 0] pc= 27: BCSetVar a=20, b=9           result = new array[5]
  //     ##### [P: 1] pc= 30: BCPushConst a=4, b=0       0
  //   ##### [P: 0] pc= 31: BCSetVar a=20, b=8           index
  //   ##### <--- Body
  // ##### [P: 0] pc= 27: CodeBlock a=0, b=0
  // ##### [P:-4] pc= 34: BCBranch a=11, b=79            -> check for end
  // ##### [P:-3] pc= 37: JumpTarget a=0, b=0 from 83     <- next round
  //     ##### ---> Body
  //         ##### [P: 1] pc= 37: BCGetVar a=15, b=7     iter
  //         ##### [P: 1] pc= 40: BCPushConst a=4, b=4   1
  //       ##### [P: 1] pc= 41: BCARef a=24, b=2         iter[1]
  //     ##### [P: 0] pc= 42: BCSetVar a=20, b=6         value := iter[1]
  //         ##### [P: 1] pc= 43: BCGetVar a=15, b=7     iter
  //         ##### [P: 1] pc= 46: BCPushConst a=4, b=0   0
  //       ##### [P: 1] pc= 47: BCARef a=24, b=2         iter[0]
  //     ##### [P: 0] pc= 48: BCSetVar a=20, b=5         slot
  //     ##### [P: 1] pc= 49: BCGetVar a=15, b=9         result ERR -v
  //     ##### <--- Body
  //   ##### [P: 1] pc= 42: CodeBlock a=0, b=0
  //   ##### [P: 1] pc= 52: BCGetVar a=15, b=8           index ERR -v
  //     ##### ---> Body
  //         ##### [P: 1] pc= 55: BCGetVar a=15, b=5     slot
  //         ##### [P: 1] pc= 56: BCPush a=3, b=1
  //       ##### [P: 1] pc= 57: BCCall a=5, b=1
  //     ##### [P: 0] pc= 58: BCPop a=0, b=0
  //     ##### [P: 0] pc= 62: CFBreak a=0, b=89
  //     ##### [P: 1] pc= 66: BCGetVar a=15, b=6
  //     ##### <--- Body
  //   ##### [P: 1] pc= 58: CodeBlock a=0, b=0
  // ##### [P: 0] pc= 67: BCSetARef a=24, b=3  --- ERROR -^^: this should have consumed 3!
  // ##### [P:-1] pc= 68: BCPop a=0, b=0
  //   ##### [P: 1] pc= 69: BCPushConst a=4, b=4
  // ##### [P: 2] pc= 70: BCIncrVar a=22, b=8
  // ##### [P:-1] pc= 73: BCPop a=0, b=0
  // ##### [P:-1] pc= 74: BCPop a=0, b=0
  //   ##### [P: 1] pc= 75: BCGetVar a=15, b=7
  // ##### [P:-1] pc= 78: BCIterNext a=0, b=5
  // ##### [P:-3] pc= 79: JumpTarget a=0, b=0 from 34
  //   ##### [P: 1] pc= 79: BCGetVar a=15, b=7
  // ##### [P:-1] pc= 82: BCIterDone a=0, b=6
  // ##### [P:-1] pc= 83: BCBranchIfFalse a=13, b=37
  // ##### [P:-4] pc= 86: BCBranch a=11, b=94
  // ##### [P:-3] pc= 89: JumpTarget a=0, b=0 from 62
  // ##### [P:-1] pc= 89: BCSetVar a=20, b=9
  // ##### [P:-1] pc= 92: BCPop a=0, b=0
  // ##### [P:-1] pc= 93: BCPop a=0, b=0
  // ##### [P:-3] pc= 94: JumpTarget a=0, b=0 from 86
  // ##### [P: 1] pc= 94: BCGetVar a=15, b=9
  //   ##### ---> Body
  //     ##### [P: 1] pc= 97: BCPushConst a=4, b=2
  //   ##### [P: 0] pc= 98: BCSetVar a=20, b=9
  //     ##### [P: 1] pc=101: BCPushConst a=4, b=2
  //   ##### [P: 0] pc=102: BCSetVar a=20, b=7
  //   ##### <--- Body
  // ##### [P: 0] pc= 98: CodeBlock a=0, b=0
  // ##### [P:-1] pc=105: BCFindAndSetVar a=21, b=2
  //   ##### ---> Body
  //       ##### [P: 1] pc=106: BCFindVar a=14, b=2
  //       ##### [P: 1] pc=107: BCPush a=3, b=1
  //     ##### [P: 1] pc=108: BCCall a=5, b=1
  //   ##### [P: 0] pc=109: BCPop a=0, b=0
  //     ##### [P: 1] pc=110: BCPushConst a=4, b=4
  //   ##### [P: 1] pc=111: BCReturn a=0, b=2
  //   ##### <--- Body
  // ##### [P: 1] pc=109: CodeBlock a=0, b=0
  // ##### [P:-2] pc= -1: LastNode a=-1, b=-1


  // TODO: write me
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

/*
 This is the start of a 'try' block. The pattern is:
 try:
 n * BCPush, BCPushConst Tx
 BCNewHandler(n)
 try_body:
 Instructions
 BCPopHandlers
 BCBranch 1
 onexception x:
 JumpTarget Tx
 BCBranch 2
 end_handlers:
 BCPopHandlers
 end_try
 JumpTarget 1
 */

void BCNewHandler::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
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

