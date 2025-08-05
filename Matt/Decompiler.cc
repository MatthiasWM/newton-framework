
/*
 File:    MattsDecompiler.cc

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#include "Matt/Decompiler.h"

#include "Matt/AST.h"
#include "Matt/ASTAdmin.h"
#include "Matt/ASTDataFlow.h"
#include "Matt/ASTControlFlow.h"
#include "Matt/ASTControlFlowHelper.h"

#include "Frames/Frames.h"

#include <algorithm>
#include <tuple>

using namespace ast;

// Reverse int CCompiler::walkForCode(RefArg inGraph, bool inFinalNode)

/* FIXME: all allocated AST nodes should be kept in a single vector and never
    be deleted eval, but instead when the Decompiler is deleted. WHen new nodes
    are created at eval time, the must be added to that same cleanup vector.
 */
/* FIXME: we must compress multiple statements into some compound node.
    So, um, this is legal: `a := 1 + begin a; b; c end + 4;`
    `begin a; b; c end` generated two statements and one expression, and
    is compiled into bytecode and the seen as a single expression.

    I assume that after each DataFlow pass, we need some Compress pass.
    We would search for [stmt, stmt, stmt, ..., expr] and warp that in a single
    compound expression.
    If that doesn't match, find [stmt, stmt, stmt, ...] and warp that in a single
    compound statement.

    When printing, just print `begin stmt; stmt; stmt; ...; expr end` or
    `begin stmt; stmt; stmt; ... end` respectively.

    This would move the functionality of CodeBlock into a different pass
    and change the code of all derived classes.

    WARNING: Will this resolve correctly if we have, for example
    `a := 0; if b = 2 then...`, or will that generate something like
    `if begin a := 0; b = 2 end then...`? With the compression in a lower
    priority pass, `if b = 2` should resolve before the compression gets a chance.
    But I guess we just have to try.

 */
/* TODO: Remove these kind of sequences:
    Found at the end of a while loop:
      ###[ 1]  11: BCFindVar literal[0] ###
      ###[-1]  12: BCPop ###
    Found after a while loop:
      ###[ 1]  17: BCPushConst value:2 ###
      ###[-1]  18: BCPop ###
    In if then statements and elsewhere: (generates `a:=3; a` so that the result
    is back on the stack. This creates aa superfluous line of code.
      ###[-1]  10: BCFindAndSetVar literal[0] ###
      ###[ 1]  11: BCFindVar literal[0] ###
*/
// TODO: in NTK, we can check a box to create debug information. The decompiler should be aware of
// debug information in the code. Especially with nos2, this can restore argument
// names. In any format, it can give names to our views in the stepChildren array.

/*
 Precedence Table:
 12: slot access '.'
 11: send, conditional send
 10: array element []
 9: unary minus
 8: <<, >>
 7: divide, div, multiply, mod
 6: add, subtract
 5: stringer (&, &&)
 4: "exists"
 3: comparisons (<, >, =, <>, ...)
 2: not
 1: and, or
 0: assign :=
 */

/*
 Control Flow:
 - for:     `for` counter := expr `to` inital [`by` increment] `do` expr
 - foreach: `foreach` slot [, value] [`deeply`] `in` frame_or_array (`do` or `collect`) expr
 - loop     `loop` expr
 - while    `while` condition `do` expression
 - repeat   `repeat` expression `until` condition
 - break    can appear anywhere inside those loops
            generates "expr branch pop",
            the pop seems to be never reached, but turns `break` into a statement.
 - exceptions: try [begin] ... onexception ... do ... [end]
               throw(...) , rethrow()
 */


// -----------------------------------------------------------------------------

void Decompiler::decompile(Ref ref)
{
  if (debugAST_) puts("\n==== Matt's Decompiler:");
  Ref klass = GetFrameSlot(ref, SYMA(class));
  if (IsSymbol(klass) && SymbolCompare(klass, SYMA(CodeBlock))==0) {
    nos_ = 1;
    Ref numArgs = GetFrameSlot(ref, SYMA(numArgs));
    numArgs_ = RefToInt(numArgs);
    Ref argFrame = GetFrameSlot(ref, SYMA(argFrame));
    int argFrameLength = Length(argFrame);
    numLocals_ = argFrameLength - 3 - numArgs_;
    locals_.clear();
    // Make the list of names of the locals
    MapSlots(argFrame,
             [](RefArg tag, RefArg, uintptr_t user_data)->long {
                  Decompiler *self = (Decompiler*)user_data;
                  Decompiler::Local l = { tag, Local::Use::undefined };
                  self->locals_.push_back(l);
                  return NILREF;
                },
             (uintptr_t)this);
    locals_[0].use = Local::Use::system; // _nextArgFrame
    locals_[1].use = Local::Use::system; // _parent
    locals_[2].use = Local:: Use::system; // _implementor
    for (int i=0; i<numArgs_; i++)
      locals_[i+3].use = Local::Use::arg;
    for (int i=0; i<numLocals_; i++)
      locals_[i+3+numArgs_].use = Local::Use::local;
  } else if (klass == kPlainFuncClass) {
    nos_ = 2;
    Ref numArgs = GetFrameSlot(ref, SYMA(numArgs));
    numArgs_ = static_cast<int>((numArgs>>2) & 0x00003fff);
    numLocals_ = static_cast<int>(numArgs >> 18);
    // Make up names for the locals:
    // _nextArgFrame, _parent, _implementor, parameters, locals
    locals_.clear();
    locals_.push_back( { SYMA(_nextArgFrame), Local::Use::system } );
    locals_.push_back( { SYMA(_parent), Local::Use::system } );
    locals_.push_back( { SYMA(_implementor), Local::Use::system } );
    for (int i=0; i<numArgs_; i++) {
      char buf[32];
      snprintf(buf, 30, "arg%d", i);
      locals_.push_back( { MakeSymbol(buf), Local::Use::arg } );
    }
    for (int i=0; i<numLocals_; i++) {
      char buf[32];
      snprintf(buf, 30, "loc%d", i);
      locals_.push_back( { MakeSymbol(buf), Local::Use::local } );
    }
  } else {
    ThrowMsg("Decompiler::decompile(): Unknown Function Signature");
  }

  literals_ = GetFrameSlot(ref, SYMA(literals));
  if (!ISNIL(literals_))
    numLiterals_ = Length(literals_);

  Ref instructions = GetFrameSlot(ref, SYMA(instructions));
  generateAST(instructions);
  printAST("Initial AST");
  solve();
}

/**
 \brief Append a new node to the giveNode.
 This is optimized, so 'node' must not have a 'next' link. This also does not
 update 'first_' or 'last_'. This is used for initialization.
 \return the new node
 */
Node *Decompiler::Append(Node *node, Node *newNode) {
  assert(node);
  assert(newNode);
  node->next = newNode;
  newNode->prev = node;
  return newNode;
}

/**
 Create a node that corresponds to the given bytecode.
 */
Bytecode *Decompiler::NewBytecodeNode(int pc, int a, int b)
{
  switch (a) {
    case 0:
      switch (b) {
        case 0: return new BCPop(*this, pc, a, b);
        case 1: return new BCDup(*this, pc, a, b);
        case 2: return new BCReturn(*this, pc, a, b);
        case 3: return new BCPushSelf(*this, pc, a, b);
        case 4: return new BCSetLexScope(*this, pc, a, b);//        case 4: bc.bc = BC::SetLexScope; break;
        case 5: return new BCIterNext(*this, pc, a, b);
        case 6: return new BCIterDone(*this, pc, a, b);
        case 7: return new BCPopHandlers(*this, pc, a, b);
      };
      break;
    case 3: return new BCPush(*this, pc, a, b);
    case 4: return new BCPushConst(*this, pc, a, b);
    case 5: return new BCCall(*this, pc, a, b);
    case 6: return new BCInvoke(*this, pc, a, b);
    case 7: return new BCSend(*this, pc, a, b, false); // Send
    case 8: return new BCSend(*this, pc, a, b, true); // SendIfDefined
    case 9: return new BCResend(*this, pc, a, b, false); // Resend
    case 10: return new BCResend(*this, pc, a, b, true); // ResendIfDefined
    case 11: return new BCBranch(*this, pc, a, b);
    case 12: return new BCBranchIfTrue(*this, pc, a, b);
    case 13: return new BCBranchIfFalse(*this, pc, a, b);
    case 14: return new BCFindVar(*this, pc, a, b);
    case 15: return new BCGetVar(*this, pc, a, b);
    case 16: return new BCMakeFrame(*this, pc, a, b);
    case 17:
      if (b == 0xFFFF)
        return new BCNewArray(*this, pc, a, b);
      else
        return new BCMakeArray(*this, pc, a, b);
    case 18: return new BCGetPath(*this, pc, a, b);
    case 19: return new SetPath(*this, pc, a, b);
    case 20: return new BCSetVar(*this, pc, a, b);
    case 21: return new BCFindAndSetVar(*this, pc, a, b);
    case 22: return new BCIncrVar(*this, pc, a, b);
    case 23: return new BCBranchLoop(*this, pc, a, b);
    case 24:
      switch (b) {
        case 0: return new BinaryOperator(*this, pc, a, b, "+", 6); // Add
        case 1: return new BinaryOperator(*this, pc, a, b, "-", 6); // Sub
        case 2: return new BCARef(*this, pc, a, b);
        case 3: return new BCSetARef(*this, pc, a, b);
        case 4: return new BinaryOperator(*this, pc, a, b, "=", 3); // Equals
        case 5: return new BCNot(*this, pc, a, b);
        case 6: return new BinaryOperator(*this, pc, a, b, "<>", 3); // NotEquals
        case 7: return new BinaryOperator(*this, pc, a, b, "*", 7); // Multiply
        case 8: return new BinaryOperator(*this, pc, a, b, "/", 7); // Divide
        case 9: return new BinaryOperator(*this, pc, a, b, "div", 7); // 'div'
        case 10: return new BinaryOperator(*this, pc, a, b, "<", 3); // LessThan
        case 11: return new BinaryOperator(*this, pc, a, b, ">", 3); // GreaterThan
        case 12: return new BinaryOperator(*this, pc, a, b, ">=", 3); // GreaterOrEqual
        case 13: return new BinaryOperator(*this, pc, a, b, "<=", 3); // LessOrEqual
        case 14: return new BinaryFunction(*this, pc, a, b, "bAnd"); // BitAnd
        case 15: return new BinaryFunction(*this, pc, a, b, "bOr"); // BitOr
        case 16: return new BCBitNot(*this, pc, a, b);
        case 17: return new BCNewIter(*this, pc, a, b);
        case 18: return new BCLength(*this, pc, a, b);
        case 19: return new BCClone(*this, pc, a, b);
        case 20: return new BCSetClass(*this, pc, a, b);
        case 21: return new BCAddArraySlot(*this, pc, a, b);
        case 22: return new BCStringer(*this, pc, a, b);
        case 23: return new BCHasPath(*this, pc, a, b);
        case 24: return new BCClassOf(*this, pc, a, b);
      }
      break;
    case 25: return new BCNewHandler(*this, pc, a, b);
  }
  return new Bytecode(*this, pc, a, b);
}

/**
 \brief Create the initial AST which is not a tree at all, just a list of nodes.

 Convert every bytecode instruction into one or more nodes and chain them together in
 a doubly linked list. Jump instruction generate additional "jump targets" at
 their jump destination that are linked into the list as well. One address
 can only hold one jump target, but the target can hold multiple jump
 origins in a backwards and forwards list.
 */
void Decompiler::generateAST(Ref instructions)
{
  if (!IsBinary(instructions)) ThrowMsg("Decompiler::generateAST: `instructions` must be binary Ref.");
  uint8_t *bc = (uint8_t*)BinaryData(instructions);
  int nbc = Length(instructions);

  // TODO: horrible hack: we push every BCPushConst(int) in case we encounter BCNewHandler
  std::vector<int> pushConstList;
  std::vector<int> pushLitList;

  // Find all the jump instructions and create the target nodes.
  for (int i=0; i<nbc; i++) {
    int pc = i;
    uint8_t cmd = bc[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) { b = bc[i+1]<<8 | bc[i+2]; i += 2; }
    // branch, brach-if-true, branch-if-false, branch-if-loop-not-done
    // TODO: a = 25, new-handlers
    if ((a==11)||(a==12)||(a==13)||(a==23))
      AddToTargets(b, pc);
    if (a==3)
      pushLitList.push_back(b);
    if (a==4 && IsInt(b)) // BCPushConst
      pushConstList.push_back(RefToInt(b));
    if (a==25) { // new-handler
      int n = (int)pushConstList.size();
      assert(b <= n);
      int nl = (int)pushConstList.size();
      assert(b <= nl);
      for (int t=0; t<b; ++t) {
        AddToTargets(pushConstList[n-t-1], pc, pushLitList[nl-t-1]);
      }
    }
  }

  // Now run the byte codes again and create a linked list of instructions
  Node *nd = first_ = new FirstNode(*this);
  for (int i=0; i<nbc; i++) {
    int pc = i;
    uint8_t cmd = bc[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) { b = bc[i+1]<<8 | bc[i+2]; i += 2; }
    if (targetMap_.contains(pc)) {
      auto &target = targetMap_[pc];
      for (auto &t : target)
        nd = Append(nd, t.second);
    }
    nd = Append(nd, NewBytecodeNode(pc, a, b));
  }
  last_ = Append(nd, new LastNode(*this));

  // The code generator occasionally appends two consecutive return commends.
  // We fix that by deleting the second return.
  if (dynamic_cast<BCReturn*>(nd) && dynamic_cast<BCReturn*>(nd->prev))
    delete nd->Unlink();
}

void Decompiler::AddToTargets(int target, int origin, int excp)
{
  // Use negative numbers to sort forward jumps closest to furthest.
  // BAckward jumps are automatically closest to furthest.
  int sort = origin < target ? -origin : target;
  if (excp == -1) {
    targetMap_[target][sort] = new JumpTarget(*this, target, origin);
  } else {
    targetMap_[target][sort] = new ExceptionHandler(*this, target, origin, excp);
  }
  if (debugAST_) printf("Jump Target: from %d to %d\n", origin, target);
}


/**
 \brief Decompile the AST as much as possible in multiple loops.
 The AST starts as a linear list of Bytecode nodes. The solver runs over the
 root nodes front to back, finding patterns that can be resolved into
 dependencies. It does that until no more patterns are found.

 For example:
 ```
 push 3 (provides 1 value)
 push 3 (provides 1 value)
 add (consumes 2 values, but doesn't know yet what it provides)
 ```

 `push` provides a value on the stack. `add` expects two values on the stack,
 so the `push` nodes become dependent on `add`

 Resolving to:
 ```
   push 3 (resolved, no longer checked)
   push 5 (resolved)
 add (provides 1 value)
 ```

 One trick is that unresolved nodes can not be part of pattern. Only if all
 inputs exist will the decompiler take them into account.

 To figure out control flow, jump commands and the jump destinations are
 AST nodes as well. This will avoid conflict between data flow and
 control flow analysis. Nevertheless, data flow has always priority, and
 control flow is checked in a secondary loop.
 */
void Decompiler::solve()
{
  for (;;) { // ControlFlow Pass: outer loop, run until neither changes anything
    // ---- Data Flow Pass
    numASTChanges = 0;
    for (Node *nd = first_; nd && !numASTChanges; nd = nd->Resolve(Node::Pass::DataFlow)) { }
    if (numASTChanges > 0) { printAST("DataFlow Pass"); continue; }
    p.Item(); p.Print("DataFlow passes done"); p.ItemDone();
    // ---- Compression Pass
    if (compressAST()) { printAST("Compression Pass"); continue; }
    p.Item(); p.Print("Compression passes done"); p.ItemDone();
    // ---- Control Flow Pass
    numASTChanges = 0;
    for (Node *nd = first_; nd && !numASTChanges; nd = nd->Resolve(Node::Pass::ControlFlow)) { }
    if (numASTChanges > 0) { printAST("CodeFlow Pass"); continue; }
    p.Item(); p.Print("CodeFlow passes done"); p.ItemDone();
    // ---- No more changes on any level
    break;
  }
}

/**
 \brief Combine multiple statements into a single Code Block.

 This method walks the Root layer of the AST and finds multiple consecutive
 statements, or one or more statements followed by an expression, and groups
 them into a new node that presents as a single statement or expression.

 This resolves only the first occurrence of this pattern and then returns true.
 To compress the entire AST, this method should be called until it returns false.

 \return true if there were any changes.
 */
bool Decompiler::compressAST()
{
  Node *nd = first_;
  while (nd) {
    if (nd->IsStatement()) {
      int numStmts = 1;
      Node *it = nd->next;
      bool isExpr = false;
      while (it) {
        if (it->IsExpr()) {
          isExpr = true;
          numStmts++;
          break;
        }
        if (!it->IsStatement()) {
          break;
        }
        numStmts++;
        it = it->next;
      }
      if (numStmts > 1) {
        CodeBlock *codeBlock = new CodeBlock(*this, nd->pc(), isExpr ? kProvidesOne : kProvidesNone);
        // Insert codeBlock before nd
        nd->InsertBefore(codeBlock);
        codeBlock->moveToBody(nd, numStmts);
        nd = codeBlock->next;
        return true;
      }
    }
    nd = nd->next;
  }
  return false;
}

/**
 \brief Print the full Abstract Syntax Tree.
 This prints the list of root nodes and all their dependencies.

 NS Bytecode's data flow is stack oriented. Dependencies are printed first
 with an indent to make it easy to follow the data flow.
 */
void Decompiler::printAST(const char *label)
{
  if (!debugAST_) return;
  p.PrintDivider(label);
  p.DeepList();
  output = Print::deep;
  for (Node *nd = first_; nd; nd = nd->next) {
    nd->PrintNode(true);
  }
  p.EndList();
  p.PrintDivider("");
}


/**
 \brief Print all nodes in the first layer of the AST.
 The decompiler only ever looks at the first layer of AST nodes. Printing this
 out help us to find patterns where the decompiler git stuck.
 A completely resolved ST has a list of 0 or more statements and a single
 final expression.
 */
void Decompiler::printASTRoot()
{
  if (!debugAST_) return;
  p.PrintDivider("AST Root Nodes");
  p.DeepList("");
  output = Print::bytecode;
  for (Node *nd = first_; nd; nd = nd->next) {
    nd->PrintNode(false);
  }
  p.EndList();
  p.PrintDivider("");
}


/**
 \brief Convert an Abstarct Syntax Tree (AST) back into reabale NewtonScript source code.
 Walks the tree and lets nodes output the appropriate source code.
 If a node can not print itself, it will output an AST node description for
 debugging.
 */
void Decompiler::printSource()
{
  output = Print::script;

  // Print the function header and argument list
  p.Tag();
  p.Print("func(");
  p.StartList(",");
  for (int i=0; i<numArgs_; i++) {
    p.Item(); p.Print(""); printLocal(i + 3);
  }
  p.EndList();
  p.Print(")");

  // Print the begin statement
  p.Tag();
  p.Print("begin");
  p.DeepList(";");

  // List all locals first! "local a;" ...
  if (numLocals_) {
    bool localsPrinted = false;
    for (int i = 0; i < numLocals_; ++i) {
      // Don't print locals that are used as iterators in 'for' or 'foreach'
      if (localUsedAs(i + 3 + numArgs_, Local::Use::local)) {
        p.Item();
        p.Printf("local ");
        printLocal(i + 3 + numArgs_);
        p.ItemDone();
        localsPrinted = true;
      }
    }
    if (localsPrinted) {
      p.Tag();
      p.Print(""); // generate an empty line
    }
  }

  // Now print all the top level nodes from the AST.
  // If everything was decompiled correctly, this should be 0 or more
  // statements, followed by one expression
  for (Node *nd = first_->next; nd; nd = nd->next) {
    p.Item();
    nd->Print(kPrintSuppressList);
    p.ItemDone();
  }

  // Print the end marker of the function
  p.Trailer(); p.Print("end");
  p.EndList();
}


/**
 \brief Print a frame that contains a NewtonScript function.
 Check if this is actually NewtonScript. No support for native or binary.
 Is `class: #0x32` for newer apps.
 - decompress the bytecode into a flat AST
 - find all jump target addresses and store them as AST nodes as well
 - reduce the AST as much as possible, generating an actual tree
    - find data flow and move nodes into dependencies
    - find control flow patterns and reorder nodes
    - if nothing can be applied anymore, the root of the AST should be a list
      of 0 or more statements, followed by exactly one expression
 - now walk the tree and generate nicely readable source code.

 This is a description of the newer function format, using an abbreviated frame
 ```
 `DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(1));`
 - `class: #0x32`:
 - `instructions`: 'instructions bytecode as binary data
 - `literals`: 'literals, an array of values
 - `numArgs`: bits 31 to 16 are the number of locals, bits 15 to 0 are the number of arguments
 - `argFrame`: always `nil` in this format
 ```

 This is the older format that still contains a lot of variable names that help
 us generate more readable code.
 ```
 `DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(0));`
 - `class`: 'CodeBlock,
 - `instructions`:
 - `literals`:
 - `argFrame`: {
      _nextArgFrame: {
        _nextArgFrame: nil,
        _parent: nil,
        _implementor: nil
      },
      _parent: nil,
      _implementor: nil,
      ab: nil,  // Argument 0
      cd: nil,  // Argument 1 (see numArgs)
      x: nil,   // Local 0
      y: nil,   // ...
      z: nil    // Local 2
    },
 - `numArgs`: 2
 ```
 */
NewtonErr mDecompile(Ref ref, ObjectPrinter &printer, bool debugAST)
{
  Decompiler d(printer);
  d.DebugAST(debugAST);
  d.decompile(ref);
  d.printASTRoot();
  d.printSource();
  return noErr;
}

