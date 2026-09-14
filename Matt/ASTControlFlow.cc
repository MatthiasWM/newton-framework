
/*
 File:    Matt/ASTControlFlow.h

 Matt's decompiler Abstract Syntax Tree.
 Control Flow nodes based on bytecodes.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTControlFlow.h"
#include "Matt/ASTControlFlowHelper.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/ASTPattern.h"

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
// DONE: foreach...in...collect...
// DONE: try...onexception...do...
// DONE: call a function inside a function (BCSetLexScope)
// DONE: and (no dedicated matcher needed -- `a and b` compiles to the exact
//   same bytecode as `if a then b else nil`; BuildIfThenElseExprPattern in
//   ASTControlFlowPatterns.cc already builds the CFIfThen, and the
//   pre-existing print-time sugar in CFIfThen::Print() already recognizes a
//   bare-nil else-branch and prints "and". Verified round-tripping in every
//   context tried: return, assignment, nested. See Matt/CLAUDE.md.
// DONE: or

#pragma mark - conditions and loops -

#pragma mark - BCBranch

/**
 \brief Try to resolve this bytecode as part of a 'break' instruction.
 If the sequence is 'branch; pop;', the pop can never be reached
 because there is no jump target between them.
 Lucky for us, break operations are by definition expressions, so
 the pop is needed, which makes this a reliable way to find a
 break instruction.
 The CFBreak will take care of the jump target when resolved.

 The instruction right after an unconditional forward branch is always
 dead code (execution can never fall through to it) -- usually a leftover
 `Pop` (the break value would otherwise need discarding by whatever
 follows), which is what the check below originally required exclusively.
 But when `break` is the *entire* "then" branch of an `if...then[...else]`
 (e.g. `if cond then break; end`), the compiler's own if/then(/else)
 scaffolding emits its normal closing `Branch` right there instead -- also
 dead code, just a different opcode, found via corpus-scale testing
 (Test/run_corpus.py) on real packages using exactly this idiom. Unlike a
 dead `Pop` (genuinely disposable, nothing else ever needs it), a dead
 `Branch` there is the *enclosing* if/then(/else) pattern's own required
 `kBi2`/else-branch marker (`ASTControlFlowPatterns.cc`), so it must be
 left alone -- not unlinked here -- for that pattern to still find it.
 \return the next node if this is a 'break', or nullptr if no match was found.
 */
Node *BCBranch::ResolveBreak()
{
  do {
    // -- Check the pattern
    if (b_ < pc_) break;
    if (!prev->IsExpr()) break;
    bool nextIsDeadPop = dynamic_cast<BCPop*>(next) != nullptr;
    bool nextIsDeadBranch = dynamic_cast<BCBranch*>(next) != nullptr;
    if (!nextIsDeadPop && !nextIsDeadBranch) break;
    // -- It applies. Replace the instructions and remove the dead Pop, if
    // any -- a dead Branch is left in place (see class comment).
    CFBreak *breakNode = dec.MakeNode<CFBreak>(dec, pc(), b(), prev->Unlink());
    if (nextIsDeadPop) next->Unlink();
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
    // If this resolves to 'loop', it's part of the control flow; matched by
    // the pattern engine (Matt/ASTControlFlowPatterns.cc) rather than a
    // hand-written ResolveXxx() here.
    if (Node *nextNode = pattern::TryResolve(this)) return nextNode;
  }
  return next;
}

void BCBranch::Print(uint32_t flags) {
  return PrintNode(false);
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
//  if (pass == Pass::DataFlow) {
  // Give the compress pass a chance to build a larger condition
  if ((pass == Pass::ControlFlow) && (!in_)) {
    if (!in_ && prev->IsExpr()) {
      // Single-node only, deliberately -- this anchor tag is shared by two
      // different registered patterns (BuildWhilePattern and
      // BuildOrPattern, ASTControlFlowPatterns.cc), and this consumption
      // runs before either's own Builder chain (and thus before we know
      // which one, if either, will end up matching) even starts. A `while`
      // loop's condition can legitimately span more than one bytecode
      // instruction (see BuildWhilePattern's own class comment for why and
      // how it extends this single node backward itself, once it already
      // knows it's specifically trying to match a loop) -- but `a or b`'s
      // left operand `a` has no reliable boundary marker the way a loop's
      // condition does (a loop's is always preceded by a JumpTarget; `a`
      // is simply preceded by whatever ordinary code came before it in the
      // function). Attempting the same backward walk unconditionally here
      // was tried and reverted: it silently absorbed unrelated preceding
      // statements into `or`'s left operand -- caught by the 12-package
      // sample, exactly the over-merging failure mode `compressAST()`'s
      // removal (Stage 8) was supposed to eliminate for good.
      in_ = prev;
      prev->Unlink();
      dec.numASTChanges++;
      return next;
    } else {
      return next;
    }
  }
  if ((pass == Pass::ControlFlow) && (in_)) {
    // `while...do` and `a or b` are both matched by the pattern engine
    // (Matt/ASTControlFlowPatterns.cc) rather than hand-written ResolveXxx()
    // methods here.
    if (Node *nextNode = pattern::TryResolve(this)) return nextNode;
  }
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
 \see Matt/ASTControlFlowPatterns.cc -- the three patterns described above
 are registered there (BuildIfThenPattern/BuildIfThenElsePattern/
 BuildIfThenElseExprPattern), not hand-matched in this class anymore.
 */

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
    // `if...then...else...` (all three shapes) and `repeat...until` are both
    // matched by the pattern engine (Matt/ASTControlFlowPatterns.cc) rather
    // than hand-written ResolveXxx() methods here.
    if (Node *nextNode = pattern::TryResolve(this)) return nextNode;
  }
  return next;
}

void BCBranchIfFalse::Print(uint32_t flags) {
  return PrintNode(false);
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

Node *BCReturn::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;
  if (prev->IsExpr()) {
    in_ = prev;
    prev->Unlink();
  } else if (prev->IsStatement()) {
    // The function's actual final construct is a genuine statement,
    // producing no explicit value (e.g. a statement-shaped if/then/else
    // whose branches both leave nothing behind, kProvidesNone by design).
    // NewtonScript still implicitly returns nil in that case, and this
    // instruction still needs *some* operand (see class comment above:
    // "technically it is an expression"), so synthesize one rather than
    // leaving this stuck unresolved forever -- matches the pre-existing
    // TODO on this class ("return NIL is implied if there is no return
    // statement in the source code"), found to actually matter via
    // corpus-scale testing (Test/run_corpus.py).
    in_ = NewNil();
  } else {
    return next;
  }
  dec.numASTChanges++;
  return next;
}

void BCReturn::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("return ");
  in_->Print();
}

#pragma mark - For loop -

#pragma mark - BCIncrVar

void BCIncrVar::Print(uint32_t flags) {
  return PrintNode(false);
}

#pragma mark - BCBranchLoop

BCBranchLoop::BCBranchLoop(Decompiler &d, int pc, int a, int b)
: Bytecode(d, pc, a, b)
{ }

Node *BCBranchLoop::Resolve(Pass pass)
{
  if (pass != Pass::ControlFlow) return next;
  // `for...to...by...do` is matched by the pattern engine
  // (Matt/ASTControlFlowPatterns.cc) rather than a hand-written matcher
  // here.
  if (Node *nextNode = pattern::TryResolve(this)) return nextNode;
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
  return PrintNode(false);
}

Node *BCNewIter::Resolve(Pass pass)
{
  if ((pass != Pass::ControlFlow) || Resolved()) return next;

  // `foreach...do` and `foreach...collect` are both matched by the pattern
  // engine (Matt/ASTControlFlowPatterns.cc) rather than hand-written
  // ResolveXxx() methods here.
  if (Node *ret = pattern::TryResolve(this)) return ret;
  return next;
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
  return PrintNode(false);
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
  return PrintNode(false);
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
  // `try...onException...do` is matched by the pattern engine
  // (Matt/ASTControlFlowPatterns.cc) rather than a hand-written matcher
  // here.
  if (Node *nextNode = pattern::TryResolve(this)) return nextNode;
  return next;
}

#pragma mark - BCPopHandlers

void BCPopHandlers::Print(uint32_t flags) {
  return PrintNode(false);
}

#pragma mark - Calls -

#pragma mark - BCSetLexScope

/*
 BCSetLexScope is generated before calling a function that is defined inside a
 function. When decompiling, a function following BCSetLexScope must be
 declared inline or as a variable. Otherwise it must be declared
 as a global const and linked.
 */
void BCSetLexScope::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  // This prints an entire function.
  in_->Print();
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
      op = dec.MakeNode<BinaryOperator>(dec, pc_, a_, b_, "<<", kPrecedenceShift);
    } else if (strcmp(name, ">>")==0) {
      op = dec.MakeNode<BinaryOperator>(dec, pc_, a_, b_, ">>", kPrecedenceShift);
    } else if (strcasecmp(name, "mod")==0) {
      op = dec.MakeNode<BinaryOperator>(dec, pc_, a_, b_, "mod", kPrecedenceMulDiv);
    }
    if (op) {
      prev->Unlink();
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

