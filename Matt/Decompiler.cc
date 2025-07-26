
/*
 File:    MattsDecompiler.cc

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#include "Matt/Decompiler.h"

#include <algorithm>
#include <tuple>

// Reverse int CCompiler::walkForCode(RefArg inGraph, bool inFinalNode)

// FIXME: when writing symbols, we must *really* know when to precede them with a tick and when not!
// So, for example, "if HasSlot(p, devInstallScript) then"
// should actually be "if HasSlot(p, 'devInstallScript) then"!
// See the difference?!

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

class ASTNode;
class ASTBytecodeNode;
class ASTJumpTarget;

enum class Print { bytecode, deep, script };

class Decompiler {
public:
  ObjectPrinter &p;
protected:
  int nos_ = 0;
  int numArgs_ = 0;
  int numLocals_ = 0;
  RefVar locals_;         ///< An array of the symbols in argFrame:
  ///< _nextArgFrame, _parent, _implementor, parameters, locals
  int numLiterals_ = 0;
  RefVar literals_;       // Array of Refs
  ASTNode *first_ { nullptr };
  ASTNode *last_ { nullptr };
  std::map<int, ASTJumpTarget*> targets;
  bool debugAST_ { false };

  void AddToTargets(int target, int origin);
  ASTNode *Append(ASTNode *lastNode, ASTNode *newNode);
  ASTBytecodeNode *NewBytecodeNode(int pc, int a, int b);
public:
  Decompiler(ObjectPrinter &printer) : p( printer ) { }
  Ref GetLiteral(int i) { return GetArraySlot(literals_, i); }
  ObjectPrinter *Printer() { return &p; }
  void DebugAST(bool v) { debugAST_ = v;}
  void print();
  void printRoot();
  void printSource();
  void printLiteral(int ix, bool tickSymbols = false) {
    // TODO: if the literal is slotted, we may need to precede it with a tick (').
    p.PrintRef(GetArraySlot(literals_, ix), 0, tickSymbols);
  }
  void printLocal(int ix, bool tickSymbols = false) {
    p.PrintRef(GetArraySlot(locals_, ix), 0, tickSymbols);
  }
  void decompile(Ref ref);
  void printPathExpr(RefArg pathExpr);
  void solve();
  void generateAST(Ref instructions);

  // Print state:
  Print output { Print::bytecode };
  int precedence { 0 }; // During printout, store the precedence of the current operation
};

/**
 * @class PState
 * @brief A helper class that manages indents and dividing semicolons and commas.
 *
 * This works by deferring dividers, indents, and newlines until we actually print
 * the next chunk. That way, these parameters can be modified before they are
 * finally applied.
 *
 * @note This scheme should probably be ported to the ObjectPrinter class,
 *       so that it can be used for printing packages as well.
 *
 * @enum Type
 *   - bytecode: Formatting for bytecode output.
 *   - deep: Formatting for deep analysis output.
 *   - script: Formatting for script output.
 *
 * @var Type type
 *   Current formatting type.
 * @var int indent
 *   Current indentation level.
 *
 * @fn PState(Decompiler &decompiler, Type t)
 *   Constructs a PState with the given Decompiler and formatting type.
 * @fn void ClearDivider()
 *   Clears any pending divider text.
 * @fn void Begin(int delta=0)
 *   Begins a new formatting block, optionally adjusting indentation.
 * @fn void NewLine(const char *div, int delta=0)
 *   Starts a new line with an optional divider and indentation adjustment.
 * @fn void NewLine(int delta=0)
 *   Starts a new line with optional indentation adjustment.
 * @fn void Divider(const char *div)
 *   Sets a divider text to be output before the next line.
 * @fn void End(int delta=0)
 *   Ends a formatting block, optionally adjusting indentation.
 */
class PState {
public:
  //enum class Type { bytecode, deep, script };
  //Type type { Type::bytecode };
  int indent { 0 };
  bool indentPending_ { false };
  const char *textPending_ { nullptr };

  PState() { }
  void ClearDivider() { textPending_ = nullptr; }
  void Begin(int delta=0) {
    indent += delta;
    if (textPending_) { printf("%s", textPending_); textPending_ = nullptr; }
    if (indentPending_) { puts(""); for (int i=0; i<indent; i++) printf("  "); indentPending_ = false; }
  }
  void NewLine(const char *div, int delta=0) { textPending_ = div; indentPending_ = true; indent += delta; }
  void NewLine(int delta=0) { NewLine(nullptr, delta); }
  void Divider(const char *div) { textPending_ = div; }  // Should this reset indentPending_?
  void End(int delta=0) { indent += delta; } // Should this reset textPending_ and indentPending_?
};

constexpr int kProvidesNone = 0;      // The node is defined enough to know that there is nothing on the stack
constexpr int kProvidesUnknown = -1;  // We don't know yet how many values will be on the stack
constexpr int kSpecialNode = -2;      // First or Last node. Stop searching.
constexpr int kJumpTarget = -3;
constexpr int kBranch = -4;
constexpr int kBranchIfFalse = -5;
constexpr int kBranchIfTrue = -6;

class ASTNode {
protected:
  Decompiler &dec;  // Quick access to the decompiler state and the ObjectPrinter (dec.p.)
  int pc_ = -1;
public:

  ASTNode(Decompiler &d) : dec(d) { }
  ASTNode(Decompiler &d, int pc) : dec(d), pc_(pc) { }
  ASTNode *prev { nullptr };
  ASTNode *next { nullptr };
  int pc() { return pc_; }
  virtual ~ASTNode() = default;
  virtual int provides() { return kProvidesUnknown; }
  virtual int consumes() { return 0; } // Never called
  void printHeader() { printf("###[%2d] ", provides()); }

  /** Remove this node from the linked list. Don;t use this for First and Last. */
  ASTNode *Unlink() { prev->next = next; next->prev = prev; prev = next = nullptr; return this; }
  void UnlinkRange(ASTNode *last);
  void ReplaceWith(ASTNode *nd);

  /** Return true if all this node does is put a NIL on the stack */
  virtual bool IsNIL() { return false; }
  /** Return true if the node returns a symbol (does not catch all cases!) */
  virtual bool IsSymbol() { return false; }
  /** Return if the node pushes a single value on the stack. */
  bool IsExpr() { return (provides() == 1); }
  /** Return if the node pushes no value on the stack and is not a control node. */
  bool IsStatement() { return (provides() == 0); }

  /** Resolve all data flow patterns.
   \return true if the call changed the AST
   \return the next node in the list
   */
  virtual auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> { return {false, next}; }
  /** Resolve all control flow patterns.
   \return true if the call changed the AST
   \return the next node in the list
   */
  virtual auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> { return { false, next }; }
  /** Return true if we know everything there is to know about this node. */
  virtual bool Resolved() = 0;

  /** Print the node, either for debugging or for the final script reconstruction. */
  virtual void Print(PState &p) = 0;
};

/** Unlink all nodes, starting with this, up to last */
void ASTNode::UnlinkRange(ASTNode *last) {
  ASTNode *nd = this;
  for (;;) {
    ASTNode *nx = nd; nd = nx->next; nx->Unlink();
    if ((nx == last) || (nd == nullptr)) break;
  }
}

/** Replace this node with another node in the linked list. */
void ASTNode::ReplaceWith(ASTNode *nd) {
  prev->next = nd; next->prev = nd;
  nd->prev = prev; nd->next = next;
  prev = next = nullptr;
}


class ASTFirstNode : public ASTNode {
public:
  ASTFirstNode(Decompiler &d) : ASTNode(d) { }
  int provides() override { return kSpecialNode; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin(); printf("begin"); dec.p.NewLine(+1);
    } else {
      dec.p.Begin(); printHeader(); printf("ASTFirstNode ###"); dec.p.NewLine();
    }
  }
};

class ASTLastNode : public ASTNode {
public:
  ASTLastNode(Decompiler &d) : ASTNode(d) { }
  int provides() override { return kSpecialNode; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin(-1); printf("end"); dec.p.NewLine();
    } else {
      dec.p.Begin(); printHeader(); printf("ASTLastNode ###\n"); dec.p.NewLine();
    }
  }
};

class ASTJumpTarget : public ASTNode {
  std::vector<int> origins_;
public:
  ASTJumpTarget(Decompiler &d, int pc) : ASTNode(d, pc) { }
  int provides() override { return kJumpTarget; }
  void addOrigin(int origin) { origins_.push_back(origin); };
  bool containsOrigin(int o) { return std::find(origins_.begin(), origins_.end(), o) != origins_.end(); }
  bool containsOnly(int o) { return (origins_.size() == 1) && (origins_[0] == o); }
  void removeOrigin(int o) {
    auto it = std::find(origins_.begin(), origins_.end(), o);
    if (it != origins_.end()) origins_.erase(it);
  }
  int size() { return (int)origins_.size(); }
  bool empty() { return origins_.empty(); }
  /// Node can never be resolved, but will be removed if all origins were resolved
  bool Resolved() override { return false; }
  void Print(PState &p) override {
    dec.p.Begin();
    printHeader(); printf("%3d: ASTJumpTarget from", pc_);
    for (auto a: origins_) printf(" %d", a);
    printf(" ###");
    dec.p.NewLine();
  }
};

class ASTBytecodeNode : public ASTNode {
protected:
  int a_ = 0;
  int b_ = 0;
public:
  ASTBytecodeNode(Decompiler &d, int pc, int a, int b)
  : ASTNode(d, pc), a_(a), b_(b) { }
  int b() { return b_; }
  bool Resolved() override { return false; }
  void Print(PState &p) override {
    dec.p.Begin(); printHeader();
    printf("%3d: ERROR: ASTBytecodeNode a=%d, b=%d ###", pc_, a_, b_);
    dec.p.NewLine();
  }
};

// (A=0, B=7): --
class AST_PopHandlers : public ASTBytecodeNode {
public:
  AST_PopHandlers(Decompiler &d, int pc, int a, int b) : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { return kProvidesNone; }
  // Don't know yet
  bool Resolved() override { return false; }
  void Print(PState &p) override {
    dec.p.Begin(); printHeader();
    printf("%3d: AST_PopHandlers ###", pc_);
    dec.p.NewLine();
  }
};

// (A=3): -- literal
class AST_Push : public ASTBytecodeNode {
public:
  AST_Push(Decompiler &d, int pc, int a, int b) : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { return 1; }
  bool IsSymbol() override { return ::IsSymbol(dec.GetLiteral(b_)); }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      dec.printLiteral(b_);
      dec.p.End();
    } else {
      dec.p.Begin(); printHeader();
      printf("%3d: AST_Push literal[%d] ###", pc_, b_);
      dec.p.NewLine();
    }
  }
};

// (A=4, B=signed): -- value
class AST_PushConst : public ASTBytecodeNode {
public:
  AST_PushConst(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      PrintObject(b_, 0);
      dec.p.End();
    } else {
      dec.p.Begin(); printHeader();
      printf("%3d: AST_PushConst value:%d ###", pc_, b_);
      dec.p.NewLine();
    }
  }
  bool IsNIL() override { return (b_ == NILREF); }
};

// TODO: name nodes so that we can see the difference between a Bytecode and a generated node
// TODO: generated nodes should derive from another subclass
class ASTLoop : public ASTNode {
protected:
  std::vector<ASTNode*> body_;
public:
  ASTLoop(Decompiler &d, int pc) : ASTNode(d, pc) { }
  void add(ASTNode *nd) { body_.push_back(nd); }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      if (body_.size() > 1) {
        printf("loop begin"); dec.p.NewLine(+1);
        for (auto &nd: body_) {
          nd->Print(p); dec.p.NewLine(";");
        }
        dec.p.Begin(-1); printf("end"); dec.p.NewLine(";");
      } else if (body_.size() == 1) {
        printf("loop "); dec.p.NewLine(+1); // loop only one instruction forever (could be an if...break)
        body_[0]->Print(p); dec.p.Indent(-1);
      } else {
        printf("loop nil"); // special case, loops forever
      }
      dec.p.NewLine(";");
    } else {
      if (dec.output == Print::deep) {
        dec.p.Indent(+1);
        for (auto &nd: body_) nd->Print(p);
        dec.p.Indent(-1);
      }
      dec.p.Begin(); printHeader();
      printf("%3d: ASTLoop ###", pc_);
      dec.p.NewLine();
    }
  };
};

// (A=11): --
class AST_Branch : public ASTBytecodeNode {
public:
  AST_Branch(Decompiler &d, int pc, int a, int b) : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { return kBranch; }
  bool Resolved() override { return false; }
  auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> override {
    if (Resolved()) return { false, next };
    do {
      // `loop`: JumpTarget A; n*stmt; branch A;
      if (b_ > pc_) break;
      int numStmt = 0;
      ASTNode *nd = prev;
      while (nd->IsStatement()) { numStmt++; nd = nd->prev; }
      ASTJumpTarget *jt = dynamic_cast<ASTJumpTarget*>(nd);
      if (!jt) break;;
      if (!jt->containsOrigin(pc_)) break;;

      // It's a loop! Build a new node.
      ASTLoop *loop = new ASTLoop(dec, pc_);
      nd = jt->next;
      jt->removeOrigin(pc_);
      if (jt->empty()) delete jt->Unlink();
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
  void Print(PState &p) override {
    dec.p.Begin(); printHeader();
    printf("%3d: AST_Branch pc:%d ###", pc_, b_);
    dec.p.NewLine();
  }
};

// (A=0, B=3):  -- RCVR
class AST_PushSelf : public ASTBytecodeNode {
public:
  AST_PushSelf(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("self");
      dec.p.End();
    } else {
      dec.p.Begin(); printHeader();
      printf("%3d: AST_PushSelf ###", pc_);
      dec.p.NewLine();
    }
  }
};

// (A=14): -- value
class AST_FindVar : public ASTBytecodeNode {
public:
  AST_FindVar(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin(); dec.printLiteral(b_); dec.p.End();
    } else {
      dec.p.Begin(); printHeader();
      printf("%3d: AST_FindVar literal[%d] ###", pc_, b_);
      dec.p.NewLine();
    }
  }
};

// (A=15): -- value
class AST_GetVar : public ASTBytecodeNode {
public:
  AST_GetVar(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      dec.printLocal(b_);
      dec.p.End();
    } else {
      dec.p.Begin(); printHeader();
      printf("%3d: AST_GetVar local[%d] ###", pc_, b_);
      dec.p.NewLine();
    }
  }
};

class AST_Consume1 : public ASTBytecodeNode {
protected:
  ASTNode *in_ { nullptr };
public:
  AST_Consume1(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  int consumes() override { return 1; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override {
    if (!Resolved() && prev->IsExpr()) {
      in_ = prev;
      prev->Unlink();
      return { true, next };
    } else {
      return { false, next };
    }
  }
  bool Resolved() override { return (in_ != nullptr); }
  void PrintChildren(PState &p) {
    if (in_) { dec.p.Indent(+1); in_->Print(p); dec.p.Indent(-1); }
  }
  void Print(PState &p) override {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader();
    printf("%3d: ERROR: AST_Consume1 ###", pc_);
    dec.p.NewLine();
  }
};

class ASTWhileDo : public ASTNode {
protected:
  ASTNode *cond_ { nullptr };
  std::vector<ASTNode*> body_;
public:
  ASTWhileDo(Decompiler &d, int pc, ASTNode *condition) : ASTNode(d, pc), cond_(condition) { }
  void add(ASTNode *nd) { body_.push_back(nd); }
  int provides() override { return kProvidesNone; }
  bool Resolved() override { return true; }
  void PrintChildren(PState &p) {
    dec.p.Indent(+1);
    for (auto &nd: body_) nd->Print(p);
    dec.p.Indent(-1);
  }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("while "); cond_->Print(p); printf(" do");
      if (body_.size() > 1) {
        printf(" begin"); dec.p.NewLine(+1);
        for (auto &nd: body_) {
          nd->Print(p); dec.p.NewLine(";");
        }
        dec.p.Begin(-1); printf("end"); dec.p.NewLine(";");
      } else if (body_.size() == 1) {
        dec.p.NewLine(+1); // loop only one instruction forever (could be an if...break)
        body_[0]->Print(p); dec.p.Indent(-1);
      } else {
        printf("nil"); // special case, loops forever
      }
      dec.p.NewLine(";");
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      printf("%3d: ASTWhileDo ###", pc_);
      dec.p.NewLine();
    }
  };
};


// (A=12): value --
class AST_BranchIfTrue : public AST_Consume1 {
public:
  AST_BranchIfTrue(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kBranchIfTrue; else return kProvidesUnknown; }
  auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> override {
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
      if (!jt1 || !jt1->containsOnly(this->pc_)) break;
      nd = nd->prev;
      // Finally, we expect an unconditional jump to jt2
      AST_Branch *branch = dynamic_cast<AST_Branch*>(nd);
      if (!branch || (branch->b() != jt2->pc())) break;
      if (!jt2->containsOnly(branch->pc())) break;

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
  void Print(PState &p) override {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader();
    printf("%3d: AST_BranchIfTrue pc:%d ###", pc_, b_);
    dec.p.NewLine();
  }
};

/**
 \brief This node replaces an if/then/else pattern.
 \note In an if/then/else expression, if the else-branch is just pushing the
  `nil` constant, the else-branch need not be printed as a script.
 \note if this creates a short `if a then b else nil` expression, this may
  originally have been an `a and b` statement.
 \see AST_BranchIfFalse
 */
class ASTIfThenElseNode: public ASTNode {
  ASTNode *cond_ { nullptr };
  std::vector<ASTNode*> ifBranch_;
  std::vector<ASTNode*> elseBranch_;
  bool returnsAValue_ { false };
public:
  ASTIfThenElseNode(Decompiler &d, int pc, bool returnsAValue) : ASTNode(d, pc), returnsAValue_(returnsAValue) { }
  void setCond(ASTNode *nd) { cond_ = nd; }
  void addIf(ASTNode *nd) { ifBranch_.push_back(nd); }
  void addElse(ASTNode *nd) { elseBranch_.push_back(nd); }
  int provides() override { return returnsAValue_ ? 1 : 0; }
  /// This node only exists if all nodes involved are resolved.
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      bool needBeginEnd = ((ifBranch_.size() > 1) || (elseBranch_.size() > 1));
      // >> if (condition) the begin
      dec.p.Begin(); printf("if "); cond_->Print(p); printf(" then");
      if (needBeginEnd) printf(" begin");
      dec.p.NewLine(+1);
      // >>   if-Branch
      for (auto &nd: ifBranch_) {
        dec.p.Begin(); nd->Print(p); dec.p.Divider(";");
      }
      if (returnsAValue_) dec.p.NewLine(";");
      if (!elseBranch_.empty()) {
        // >> end else if
        dec.p.Begin(-1);
        if (needBeginEnd) printf("end else begin"); else printf("else");
        dec.p.NewLine(+1);
        // >>   else-Branch
        for (auto &nd: elseBranch_) {
          dec.p.Begin(); nd->Print(p); dec.p.Divider(";");
        }
        if (returnsAValue_) dec.p.NewLine(";");
      }
      if (needBeginEnd) {
        dec.p.Begin(-1); printf("end"); dec.p.NewLine(";");
      } else {
        dec.p.Indent(-1);
      }
    } else {
      if (dec.output == Print::deep) {
        dec.p.Begin(); printHeader(); printf("%3d: ASTIfThenElseNode: if ###", pc_); dec.p.NewLine(+1);
        cond_->Print(p);
        dec.p.Begin(-1); printHeader(); printf("%3d: ASTIfThenElseNode: then ###", pc_); dec.p.NewLine(+1);
        for (auto *nd: ifBranch_) nd->Print(p);
        dec.p.Begin(-1); printHeader(); printf("%3d: ASTIfThenElseNode: else ###", pc_); dec.p.NewLine(+1);
        for (auto *nd: elseBranch_) nd->Print(p);
        dec.p.Indent(-1);
      }
      dec.p.Begin(); printHeader();
      printf("%3d: ASTIfThenElseNode %s- %zu %zu ###", pc_,
             returnsAValue_ ? "Expression " : "",
             ifBranch_.size(), elseBranch_.size());
      dec.p.NewLine();
    }
  }
};

// (A=13): value --
/**
 \brief Based on this node, find the pattern of an if/then or if/then/else structure in the AST.

 This class checks for three different pattern, generating one of three possible variations
 of the ASTIfThenElseNode. If one of the pattern matches, the new ASTIfThenElseNode
 will replace all other code involved. Note that the terminating BranchTarget is only
 removed if if has no other origins.

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
 \see ASTIfThenElseNode
 */
class AST_BranchIfFalse : public AST_Consume1 {
public:
  AST_BranchIfFalse(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kBranchIfFalse; else return kProvidesUnknown; }
  auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> override {
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
      if (!jt1->containsOnly(pc_)) return { false, next };
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
      if (!jt2->containsOrigin(branch_pc)) return { false, next };
      jt2origin = branch_pc;
    } else if (nd->provides() == kJumpTarget) {
      // Step 10: Handle the case with no else branch (simple if/then)
      jt2 = static_cast<ASTJumpTarget*>(nd);
      if (!jt2->containsOrigin(pc_)) return { false, next };
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
    // Step 12: Remove the jump target if it has no more origins
    jt2->removeOrigin(jt2origin);
    if (jt2->empty()) jt2->Unlink(); // jump target

    // Step 13: Replace this node with the new if/then/else node
    ReplaceWith(ite);
    return { true, ite };
  }
  bool Resolved() override { return false; }
  void Print(PState &p) override {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader();
    printf("%3d: AST_BranchIfFalse pc:%d ###", pc_, b_);
    dec.p.NewLine();
  }
};

// TODO: the very last return probably doesn't need to be printed
// It's actually a bug in the newt-framework compiler. NTK does not generate the extra return bytecode
// TODO: return NIL is implied if there is no return statement in the source code
// TODO: handle implied return values nicely, so we don;t generate "return a := b;"
// (A=0, B=2): --
class AST_Return : public AST_Consume1 {
public:
  AST_Return(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { return kProvidesNone; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("return "); in_->Print(p);
      dec.p.NewLine(";");
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      printf("%3d: AST_Return ###", pc_);
      dec.p.NewLine();
    }
  }
};

class AST_Break : public AST_Consume1 {
public:
  AST_Break(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("break");
      if (!in_->IsNIL()) {
        printf(" ");
        in_->Print(p);
      }
      dec.p.NewLine(";");
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      printf("%3d: AST_Break target=%d ###", pc_, b_);
      dec.p.NewLine();
    }
  }
};

// (A=0, B=0): value --
class AST_Pop : public AST_Consume1 {
public:
  AST_Pop(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override {
    if (Resolved()) return { false, next };
    AST_Branch *branch = prev ? dynamic_cast<AST_Branch*>(prev) : nullptr;
    if (branch && (branch->b() > pc_)) {
      // If the sequence is 'branch; pop;', the pop can never be reached, which
      // appears to be an unoptimizes NS `break` instruction.
      // So replace ATS_Pop with AST_Break;

      // NOTE: We must also release the target, but that leaves us no way to
      // verify that this is the matching break statement
      ASTNode *nd = next;
      int target = branch->b();
      for (;;) {
        if (nd == nullptr) {
          ThrowMsg("AST_Pop:Resolve: break target node not found!");
          return { false, next };
        }
        if ((nd->pc() == target) && (nd->provides() == kJumpTarget)) break;
        nd = nd->next;
      }
      ASTJumpTarget *jt = dynamic_cast<ASTJumpTarget*>(nd);
      if (!jt) ThrowMsg("AST_Pop:Resolve: should have been a Jump Target!");
      if (!jt->containsOrigin(branch->pc())) ThrowMsg("AST_Pop:Resolve: address of `break` not in Jump Target!");
      jt->removeOrigin(branch->pc());
      if (jt->empty()) delete jt->Unlink();

      // Replace this node and the previous branch with an AST_Break
      AST_Break *breakNode = new AST_Break(dec, branch->pc(), 0, branch->b());
      delete branch->Unlink();
      this->ReplaceWith(breakNode);
      return { true, breakNode };
    }
    return AST_Consume1::ResolveDataFlow();
  }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin(); in_->Print(p); dec.p.NewLine(";");
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      printf("%3d: AST_Pop ###", pc_);
      dec.p.NewLine();
    }
  }
};

// (A=0, B=1): x -- x x
class AST_Dup : public AST_Consume1 {
public:
  AST_Dup(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  // TODO: provides(2) does not match any consumers!
  // NOTE: we must find an actual use case and the corresponding source code
  // NOTE: it may make sense to split this into a dup1 and dup2 node?!
  int provides() override { if (Resolved()) return 2; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader();
    printf("%3d: AST_Dup ###", pc_);
    dec.p.NewLine();
  }
};

// (A=0, B=4): func -- closure
class AST_SetLexScope : public AST_Consume1 {
public:
  AST_SetLexScope(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader();
    printf("%3d: AST_SetLexScope ###", pc_);
    dec.p.NewLine();
  }
};

// (A=20): value --
class AST_SetVar : public AST_Consume1 {
public:
  AST_SetVar(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      dec.printLocal(b_);
      printf(" := ");
      in_->Print(p);
      dec.p.NewLine(";");
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      printf("%3d: AST_SetVar local[%d] ###", pc_, b_);
      dec.p.NewLine();
    }
  }
};

// (A=21): value --
class AST_FindAndSetVar : public AST_Consume1 {
public:
  AST_FindAndSetVar(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      dec.printLiteral(b_);
      printf(" := ");
      in_->Print(p);
      dec.p.NewLine(";");
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      printf("%3d: AST_FindAndSetVar literal[%d] ###", pc_, b_);
      dec.p.NewLine();
    }
  }
};

// (A=24, B=5): value -- value
class AST_Not : public AST_Consume1 {
public:
  AST_Not(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      int ppp = dec.precedence;
      bool parentheses = (dec.precedence > 2);
      dec.precedence = 2;
      dec.p.Begin();
      if (parentheses) printf("(");
      printf("not ");
      in_->Print(p);
      if (parentheses) printf(")");
      dec.p.End();
      dec.precedence = ppp;
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_Not ###", pc_); dec.p.NewLine();
    }
  }
};

// (A=24, B=18): object -- length
class AST_Length : public AST_Consume1 {
public:
  AST_Length(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("length(");
      in_->Print(p);
      printf(")");
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_Length ###", pc_); dec.p.NewLine();
    }
  }
};

// (A=24, B=19): object -- clone
class AST_Clone : public AST_Consume1 {
public:
  AST_Clone(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("clone(");
      in_->Print(p);
      printf(")");
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_Clone ###", pc_); dec.p.NewLine();
    }
  }
};

// (A=24, B=22): array -- string
class AST_Stringer : public AST_Consume1 {
public:
  AST_Stringer(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  void Print(PState &p) override {
//    if ((dec.output == Print::script) && Resolved()) {
//      dec.p.Begin();
//      printf("clone(");
//      in_->Print(p);
//      printf(")");
//      dec.p.End();
//    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_Stringer ###", pc_); dec.p.NewLine();
//    }
  }
//  TODO: The input is an Array with at least two elements
//  a '1 && 2' is handled as a '1 & " " & 2', generating an array with three values
//  So the in_ node is AST_MakeArray which consumes the inputs and the 'array symbol
//  void printSource(PState &p) override {
//    if (in_) {
//      AST_MakeArray *array = dynamic_cast<AST_MakeArray*>(in_);
//      if (array) {
//        printf(array->in[0] " & " array->in[1] ... )
//      }
//    }
//  }
};

// (A=24, B=24): object -- class
class AST_ClassOf : public AST_Consume1 {
public:
  AST_ClassOf(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("ClassOf(");
      in_->Print(p);
      printf(")");
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_ClassOf ###", pc_); dec.p.NewLine();
    }
  }
};

// (A=24, B=16): num -- result
class AST_BitNot : public AST_Consume1 {
public:
  AST_BitNot(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("bNot(");
      in_->Print(p);
      printf(")");
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_BitNot ###", pc_); dec.p.NewLine();
    }
  }
};

// (A=22) addend -- addend value
class AST_IncrVar : public AST_Consume1 {
public:
  AST_IncrVar(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return 2; else return kProvidesUnknown; }
  void Print(PState &p) override {
//    if ((dec.output == Print::script) && Resolved()) {
//    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_IncrVar local[%d] ###", pc_, b_); dec.p.NewLine();
//    }
  }
};

// (A=0, B=5) iterator --
class AST_IterNext : public AST_Consume1 {
public:
  AST_IterNext(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    //    if ((dec.output == Print::script) && Resolved()) {
    //    } else {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader(); printf("%3d: AST_IterNext ###", pc_); dec.p.NewLine();
    //    }
  }
};

// (A=0, B=6) iterator -- done
class AST_IterDone : public AST_Consume1 {
public:
  AST_IterDone(Decompiler &d, int pc, int a, int b) : AST_Consume1(d, pc, a, b) { }
  int provides() override { if (in_) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    //    if ((dec.output == Print::script) && Resolved()) {
    //    } else {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader(); printf("%3d: AST_IterDone ###", pc_); dec.p.NewLine();
    //    }
  }
};

class AST_Consume2 : public ASTBytecodeNode {
protected:
  ASTNode *in1_ { nullptr };
  ASTNode *in2_ { nullptr };
public:
  AST_Consume2(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  int consumes() override { return 2; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override {
    if (Resolved()) return { false, next }; // nothing more to do
    if ((prev->IsExpr()) && (prev->prev->IsExpr())) {
      in2_ = prev; prev->Unlink();
      in1_ = prev; prev->Unlink();
      return { true, this };
    }
    return { false, next };
  }
  bool Resolved() override { return (in1_ != nullptr) && (in2_ != nullptr); }
  void PrintChildren(PState &p) {
    dec.p.Indent(+1);
    if (in1_) in1_->Print(p);
    if (in2_) in2_->Print(p);
    dec.p.Indent(-1);
  }
  void Print(PState &p) override {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader(); printf("%3d: ERROR: AST_Consume2 ###", pc_); dec.p.NewLine();
  }
};

class AST_BinaryExpression : public AST_Consume2 {
public:
  AST_BinaryExpression(Decompiler &d, int pc, int a, int b)
  : AST_Consume2(d, pc, a, b) { }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override
  {
    if (!Resolved() && (prev->IsExpr()) && (prev->prev->IsExpr())) {
      in2_ = prev->Unlink();
      in1_ = prev->Unlink();
      return { true, this };
    } else {
      return { false, next };
    }
  }
};

// num1 num2 -- result
// bAnd, bOr
class AST_BinaryFunction : public AST_BinaryExpression {
protected:
  const char *func_ { nullptr };
public:
  AST_BinaryFunction(Decompiler &d, int pc, int a, int b, const char *func)
  : AST_BinaryExpression(d, pc, a, b), func_(func) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      int ppp = dec.precedence;
      dec.precedence = 0;
      dec.p.Begin();
      printf("%s(", func_);
      in1_->Print(p);
      printf(", ");
      in2_->Print(p);
      printf(")");
      dec.p.End();
      dec.precedence = ppp;
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_BinaryFunction \"%s\" ###", pc_, func_); dec.p.NewLine();
    }
  }
};

// num1 num2 -- result
// +, -, *, /, div, =, <>, <, <=, >, >=
class AST_BinaryOperator : public AST_BinaryExpression {
protected:
  const char *op_ { nullptr };
  int precedence_ { 0 };
public:
  AST_BinaryOperator(Decompiler &d, int pc, int a, int b, const char *op, int precedence)
  : AST_BinaryExpression(d, pc, a, b), op_(op), precedence_(precedence)
  { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      int ppp = dec.precedence;
      bool parentheses = (dec.precedence > precedence_);
      dec.precedence = precedence_;
      dec.p.Begin();
      if (parentheses) printf("(");
      in1_->Print(p); printf(" %s ", op_); in2_->Print(p);
      if (parentheses) printf(")");
      dec.p.End();
      dec.precedence = ppp;
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_BinaryOperator \"%s\" ###", pc_, op_); dec.p.NewLine();
    }
  }
};

// (A=17, B=0xFFFF): size class -- array
class AST_NewArray : public AST_Consume2 {
public:
  AST_NewArray(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    //    if ((dec.output == Print::script) && Resolved()) {
    //    } else {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader(); printf("%3d: AST_NewArray ###", pc_); dec.p.NewLine();
    //    }
  }
};

// (A=18): object pathExpr -- value
class AST_GetPath : public AST_Consume2 {
public:
  AST_GetPath(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      in1_->Print(p);
      printf(".");
      if (in2_->IsSymbol()) {
        // if in2_ generates a symbol, print that (AST_Push literal[18])
        in2_->Print(p);
      } else {
        // but if in2_ is a pathExpr, put in2 in parentheses (AST_FindVar literal[17])
        printf("("); in2_->Print(p); printf(")");
      }
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_GetPath b:%d ###", pc_, b_); dec.p.NewLine();
    }
  }
};

// (A=24, B=2): object index -- element
class AST_ARef : public AST_Consume2 {
public:
  AST_ARef(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      in1_->Print(p);
      printf("["); in2_->Print(p); printf("]");
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_ARef ###", pc_); dec.p.NewLine();
    }
  }
};

// (A=24, B=20): object class -- object
class AST_SetClass : public AST_Consume2 {
public:
  AST_SetClass(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("SetClass(");
      in1_->Print(p); printf(", ");
      in2_->Print(p); printf(")");
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_SetClass ###", pc_); dec.p.NewLine();
    }
  }
};

// (A=24, B=21): array object -- object
class AST_AddArraySlot : public AST_Consume2 {
public:
  AST_AddArraySlot(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("AddArraySlot(");
      in1_->Print(p); printf(", ");
      in2_->Print(p); printf(")");
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_AddArraySlot ###", pc_); dec.p.NewLine();
    }
  }
};

// (A=24, B=23): object pathExpr -- result
class AST_HasPath : public AST_Consume2 {
public:
  AST_HasPath(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    //    if ((dec.output == Print::script) && Resolved()) {
    //    } else {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader(); printf("%3d: AST_HasPath ###", pc_); dec.p.NewLine();
    //    }
  }
};

// (A=24, B=17): object deeply -- iterator
class AST_NewIter : public AST_Consume2 {
public:
  AST_NewIter(Decompiler &d, int pc, int a, int b) : AST_Consume2(d, pc, a, b) { }
  bool Resolved() override { return false; }
  void Print(PState &p) override {
    //    if ((dec.output == Print::script) && Resolved()) {
    //    } else {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader(); printf("%3d: AST_NewIter ###", pc_); dec.p.NewLine();
    //    }
  }
};

// (A=19)
//    B=0: object pathExpr value --
//    B=1: object pathExpr value -- value
class AST_SetPath : public ASTBytecodeNode {
protected:
  ASTNode *object_ { nullptr };
  ASTNode *path_ { nullptr };
  ASTNode *value_ { nullptr };
public:
  AST_SetPath(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override {
    if (Resolved())
      return (b_ == 0) ? kProvidesNone : 1;
    else
      return kProvidesUnknown;
  }
  int consumes() override { return 3; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override {
    if (Resolved()) return { false, next }; // nothing more to do
    if (   (prev->IsExpr())
        && (prev->prev->IsExpr())
        && (prev->prev->prev->IsExpr())) {
      value_ = prev->Unlink();
      path_ = prev->Unlink();
      object_ = prev->Unlink();
      return { true, this };
    }
    return { false, next };
  }
  bool Resolved() override { return (object_ != nullptr) && (path_ != nullptr) && (value_ != nullptr); }
  void PrintChildren(PState &p) {
    dec.p.Indent(+1);
    if (object_) object_->Print(p);
    if (path_) path_->Print(p);
    if (value_) value_->Print(p);
    dec.p.Indent(-1);
  }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      object_->Print(p);
      printf(".");
      //dec.printPathExpr(path_);
      path_->Print(p);
      printf(" := ");
      value_->Print(p);
      dec.p.NewLine(";");
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_SetPath ###", pc_); dec.p.NewLine();
    }
  }

};

// (A=24, B=3): object index element -- element
class AST_SetARef : public ASTBytecodeNode {
protected:
  ASTNode *object_ { nullptr };
  ASTNode *index_ { nullptr };
  ASTNode *element_ { nullptr };
public:
  AST_SetARef(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { if (object_ && index_ && element_) return kProvidesNone; else return kProvidesUnknown; }
  int consumes() override { return 3; }
  auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> override {
    if (Resolved()) return { false, next }; // nothing more to do
    if (   (prev->IsExpr())
        && (prev->prev->IsExpr())
        && (prev->prev->IsExpr())) {
      element_ = prev; prev->Unlink();
      index_ = prev; prev->Unlink();
      object_ = prev; prev->Unlink();
      return { true, this };
    }
    return { false, next };
  }
  bool Resolved() override { return (object_ != nullptr) && (index_ != nullptr) && (element_ != nullptr); }
  void PrintChildren(PState &p) {
    dec.p.Indent(+1);
    if (object_) object_->Print(p);
    if (index_) index_->Print(p);
    if (element_) element_->Print(p);
    dec.p.Indent(-1);
  }
  void Print(PState &p) override {
    //    if ((dec.output == Print::script) && Resolved()) {
    //    } else {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader(); printf("%3d: AST_SetARef ###", pc_); dec.p.NewLine();
    //    }
  }
};

// (A=23) incr index limit --
class AST_BranchLoop : public ASTBytecodeNode {
protected:
  ASTNode *incr_ { nullptr };
  ASTNode *index_ { nullptr };
  ASTNode *limit_ { nullptr };
public:
  AST_BranchLoop(Decompiler &d, int pc, int a, int b)
  : ASTBytecodeNode(d, pc, a, b) { }
  int provides() override { if (incr_ && index_ && limit_) return kProvidesNone; else return kProvidesUnknown; }
  int consumes() override { return 3; }
//  auto ResolveControlFlow() -> std::tuple<bool, ASTNode*> {
    // SetVar n   = expr (start)
    // SetVar n+1 = expr (limit)
    // SetVar n+2 = expr (index)
    // GetVar n+2
    // GetVar n
    // A: Branch C
    // B: JumpTarget from D
    // x * stmt (body)
    // GetVar n+2
    // IncrVar
    // C: JumpTarget from A
    // GetVar n+1
    // D: BranchLoop B
//    return next;
//  }
  bool Resolved() override { return (incr_ != nullptr) && (index_ != nullptr) && (limit_ != nullptr); }
  void PrintChildren(PState &p) {
    dec.p.Indent(+1);
    if (incr_) incr_->Print(p);
    if (index_) index_->Print(p);
    if (limit_) limit_->Print(p);
    dec.p.Indent(-1);
  }
  void Print(PState &p) override {
    //    if ((dec.output == Print::script) && Resolved()) {
    //    } else {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader(); printf("%3d: AST_BranchLoop ###", pc_); dec.p.NewLine();
    //    }
  }
};


class AST_ConsumeN : public ASTBytecodeNode {
protected:
  int numIns_;
  std::vector<ASTNode *> ins_;
public:
  AST_ConsumeN(Decompiler &d, int pc, int a, int b, int n)
  : ASTBytecodeNode(d, pc, a, b), numIns_(n) { }
  int provides() override { if (ins_.empty()) return kProvidesUnknown; else return 1; }
  int consumes() override { return numIns_; }
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override {
    if (Resolved()) return { false, next }; // nothing more to do
    ASTNode *nd = this;
    for (int i=0; i<numIns_; i++) {
      nd = nd->prev;
      if (!nd->IsExpr()) { nd = nullptr; break; }
    }
    if (nd == nullptr) return { false, next };
    for (int i=0; i<numIns_; i++) {
      ins_.push_back(nd);
      nd = nd->next;
      nd->prev->Unlink();
    }
    return { true, this };
  }
  bool Resolved() override { return (ins_.size() == (size_t)numIns_); }
  void PrintChildren(PState &p) {
    dec.p.Indent(+1);
    for (auto &nd: ins_) nd->Print(p);
    dec.p.Indent(-1);
  }
  void Print(PState &p) override {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader(); printf("%3d: ERROR: AST_ConsumeN ###", pc_); dec.p.NewLine();
  }
};

// (A=5): arg1 arg2 ... argN name -- result
class AST_Call : public AST_ConsumeN {
public:
  AST_Call(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  // The following special functions (reserved words) need to be written "a op b"
  // mod, <<, >>, (note the precedence! 7, 8, 8)
  // HasVar(a) can also be written as "a exists", but both are legal
  auto ResolveDataFlow() -> std::tuple<bool, ASTNode*> override {
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
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      ins_[numIns_-1]->Print(p);
      printf("(");
      for (int i=0; i<numIns_-1; i++) {
        dec.p.Begin(); ins_[i]->Print(p); dec.p.Divider(", ");
      }
      dec.p.ClearDivider();
      dec.p.Begin(); printf(")"); dec.p.End();
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_Call n=%d ###", pc_, b_); dec.p.NewLine();
    }
  }
};

// (A=6): arg1 arg2 ... argN func -- result
// call ... with (...)
class AST_Invoke : public AST_ConsumeN {
public:
  AST_Invoke(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("invoke ");
      ins_[numIns_-1]->Print(p);
      printf(" with (");
      for (int i=0; i<numIns_-1; i++) {
        dec.p.Begin(); ins_[i]->Print(p); dec.p.Divider(", ");
      }
      dec.p.ClearDivider();
      dec.p.Begin(); printf(")"); dec.p.End();
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader(); printf("%3d: AST_Invoke n=%d ###", pc_, numIns_); dec.p.NewLine();
    }
  }
};

// (A=7): arg1 arg2 ... argN name receiver -- result
class AST_Send : public AST_ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  AST_Send(Decompiler &d, int pc, int a, int b, bool ifDefined)
  : AST_ConsumeN(d, pc, a, b, b+2), ifDefined_(ifDefined) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      ins_[numIns_-2]->Print(p);
      if (ifDefined_)
        printf(":?");
      else
        printf(":");
      ins_[numIns_-1]->Print(p);
      printf("(");
      for (int i=0; i<numIns_-2; i++) {
        dec.p.Begin(); ins_[i]->Print(p); dec.p.Divider(", ");
      }
      dec.p.ClearDivider();
      dec.p.Begin(); printf(")"); dec.p.End();
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      if (ifDefined_)
        printf("%3d: AST_Send if defined n=%d ###", pc_, numIns_);
      else
        printf("%3d: AST_Send n=%d ###", pc_, numIns_);
      dec.p.NewLine();
    }
  }
};

// (A=9): arg1 arg2 ... argN name -- result
// inherited:Print(3);
class AST_Resend : public AST_ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  AST_Resend(Decompiler &d, int pc, int a, int b, bool ifDefined)
  : AST_ConsumeN(d, pc, a, b, b+1), ifDefined_(ifDefined) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      printf("inherited:");
      if (ifDefined_) printf("?");
      ins_[numIns_-1]->Print(p);
      printf("(");
      for (int i=0; i<numIns_-1; i++) {
        dec.p.Begin(); ins_[i]->Print(p); dec.p.Divider(", ");
      }
      dec.p.ClearDivider();
      dec.p.Begin(); printf(")"); dec.p.End();
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      if (ifDefined_)
        printf("%3d: AST_Resend if defined n=%d ###", pc_, numIns_);
      else
        printf("%3d: AST_Resend n=%d ###", pc_, numIns_);
      dec.p.NewLine();
    }
  }
};

// (A=16, B=numVal) val1 val2 ... valN map -- frame
class AST_MakeFrame : public AST_ConsumeN {
public:
  AST_MakeFrame(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      // TODO: What is the correct call to traverse the map or find a map entry by index?
      RefVar map = NILREF;
      int mapSize = numIns_-1;
      do {
        AST_Push *mapNode = dynamic_cast<AST_Push*>(ins_[numIns_-1]);
        if (!map) break;
        RefVar maybeMap = dec.GetLiteral(mapNode->b());
        if (!IsArray(maybeMap)) break;
        map = maybeMap;
        mapSize = Length(map)-1; // mapSize is allowed to be greater than numIns_-1,
        // adding slots with value NIL (add 1 for the supermap entry)!
      } while (0);
      printf("{");
      for (int i=0; i<mapSize; i++) {
        dec.p.Begin();
        if (map == NILREF)
          printf("map%d", i);
        else
          dec.Printer()->PrintRef(GetArraySlot(map, i+1), 0, false);
        printf(": ");
        if (i >= numIns_-1)
          printf("nil");
        else
          ins_[i]->Print(p);
        dec.p.Divider(", ");
      }
      dec.p.ClearDivider();
      dec.p.Begin(); printf("}"); dec.p.End();
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      printf("%3d: AST_MakeFrame n=%d ###", pc_, numIns_);
      dec.p.NewLine();
    }
  }
};

// (A=17, B=numVal):  val1 val2 ... valN class -- array): arg1 arg2 ... argN name -- result
class AST_MakeArray : public AST_ConsumeN {
public:
  AST_MakeArray(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b+1) { }
  void Print(PState &p) override {
    if ((dec.output == Print::script) && Resolved()) {
      dec.p.Begin();
      // TODO: we could check if ins_[numIns_-1] is AST_Push and the pushed
      // literal is 'array, and not write the array class.
      printf("[");
      ins_[numIns_-1]->Print(p);
      printf(": ");
      for (int i=0; i<numIns_-1; i++) {
        dec.p.Begin(); ins_[i]->Print(p); dec.p.Divider(", ");
      }
      dec.p.ClearDivider();
      dec.p.Begin(); printf("]"); dec.p.End();
      dec.p.End();
    } else {
      if (dec.output == Print::deep) PrintChildren(p);
      dec.p.Begin(); printHeader();
      printf("%3d: AST_MakeArray n=%d ###", pc_, numIns_);
      dec.p.NewLine();
    }
  }
};

// (A=25): sym1 pc1 sym2 pc2 ... symN pcN --
class AST_NewHandler : public AST_ConsumeN {
public:
  AST_NewHandler(Decompiler &d, int pc, int a, int b)
  : AST_ConsumeN(d, pc, a, b, b*2) { }
  void Print(PState &p) override {
//    if ((dec.output == Print::script) && Resolved()) {
//    } else {
    if (dec.output == Print::deep) PrintChildren(p);
    dec.p.Begin(); printHeader();
    printf("%3d: AST_NewHandler n=%d ###", pc_, numIns_);
    dec.p.NewLine();
//    }
  }
};

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
    locals_ = MakeArray(0);
    // Make the list of names of the locals
    MapSlots(argFrame,
             [](RefArg tag, RefArg, unsigned long locals)->long { AddArraySlot(locals, tag); return NILREF; },
             locals_);
  } else if (klass == kPlainFuncClass) {
    nos_ = 2;
    Ref numArgs = GetFrameSlot(ref, SYMA(numArgs));
    numArgs_ = static_cast<int>((numArgs>>2) & 0x00003fff);
    numLocals_ = static_cast<int>(numArgs >> 18);
    // Make up names for the locals:
    // _nextArgFrame, _parent, _implementor, parameters, locals
    int argFrameLength = 3 + numArgs_ + numLocals_;
    locals_ = MakeArray(argFrameLength);
    SetArraySlot(locals_, 0, SYMA(_nextArgFrame));
    SetArraySlot(locals_, 1, SYMA(_parent));
    SetArraySlot(locals_, 2, SYMA(_implementor));
    for (int i=0; i<numArgs_; i++) {
      char buf[32];
      snprintf(buf, 30, "arg%d", i);
      SetArraySlot(locals_, i + 3, MakeSymbol(buf));
    }
    for (int i=0; i<numLocals_; i++) {
      char buf[32];
      snprintf(buf, 30, "loc%d", i);
      SetArraySlot(locals_, i + 3 + numArgs_, MakeSymbol(buf));
    }
  } else {
    ThrowMsg("Decompiler::decompile(): Unknown Function Signature");
  }
  literals_ = GetFrameSlot(ref, SYMA(literals));
  if (!ISNIL(literals_))
    numLiterals_ = Length(literals_);

  Ref instructions = GetFrameSlot(ref, SYMA(instructions));
  generateAST(instructions);
  print();
  solve();
}

/**
 \brief Append a new node to the giveNode.
 This is optimized, so 'node' must not have a 'next' link. This also does not
 update 'first_' or 'last_'. This is used for initialization.
 \return the new node
 */
ASTNode *Decompiler::Append(ASTNode *node, ASTNode *newNode) {
  assert(node);
  assert(newNode);
  node->next = newNode;
  newNode->prev = node;
  return newNode;
}

/**
 Create a node that corresponds to the given bytecode.
 */
ASTBytecodeNode *Decompiler::NewBytecodeNode(int pc, int a, int b)
{
  switch (a) {
    case 0:
      switch (b) {
        case 0: return new AST_Pop(*this, pc, a, b);
        case 1: return new AST_Dup(*this, pc, a, b);
        case 2: return new AST_Return(*this, pc, a, b);
        case 3: return new AST_PushSelf(*this, pc, a, b);
        case 4: return new AST_SetLexScope(*this, pc, a, b);//        case 4: bc.bc = BC::SetLexScope; break;
        case 5: return new AST_IterNext(*this, pc, a, b);
        case 6: return new AST_IterDone(*this, pc, a, b);
        case 7: return new AST_PopHandlers(*this, pc, a, b);
      };
      break;
    case 3: return new AST_Push(*this, pc, a, b);
    case 4: return new AST_PushConst(*this, pc, a, b);
    case 5: return new AST_Call(*this, pc, a, b);
    case 6: return new AST_Invoke(*this, pc, a, b);
    case 7: return new AST_Send(*this, pc, a, b, false); // Send
    case 8: return new AST_Send(*this, pc, a, b, true); // SendIfDefined
    case 9: return new AST_Resend(*this, pc, a, b, false); // Resend
    case 10: return new AST_Resend(*this, pc, a, b, true); // ResendIfDefined
    case 11: return new AST_Branch(*this, pc, a, b);
    case 12: return new AST_BranchIfTrue(*this, pc, a, b);
    case 13: return new AST_BranchIfFalse(*this, pc, a, b);
    case 14: return new AST_FindVar(*this, pc, a, b);
    case 15: return new AST_GetVar(*this, pc, a, b);
    case 16: return new AST_MakeFrame(*this, pc, a, b);
    case 17:
      if (b == 0xFFFF)
        return new AST_NewArray(*this, pc, a, b);
      else
        return new AST_MakeArray(*this, pc, a, b);
    case 18: return new AST_GetPath(*this, pc, a, b);
    case 19: return new AST_SetPath(*this, pc, a, b);
    case 20: return new AST_SetVar(*this, pc, a, b);
    case 21: return new AST_FindAndSetVar(*this, pc, a, b);
    case 22: return new AST_IncrVar(*this, pc, a, b);
    case 23: return new AST_BranchLoop(*this, pc, a, b);
    case 24:
      switch (b) {
        case 0: return new AST_BinaryOperator(*this, pc, a, b, "+", 6); // Add
        case 1: return new AST_BinaryOperator(*this, pc, a, b, "-", 6); // Sub
        case 2: return new AST_ARef(*this, pc, a, b);
        case 3: return new AST_SetARef(*this, pc, a, b);
        case 4: return new AST_BinaryOperator(*this, pc, a, b, "=", 3); // Equals
        case 5: return new AST_Not(*this, pc, a, b);
        case 6: return new AST_BinaryOperator(*this, pc, a, b, "<>", 3); // NotEquals
        case 7: return new AST_BinaryOperator(*this, pc, a, b, "*", 7); // Multiply
        case 8: return new AST_BinaryOperator(*this, pc, a, b, "/", 7); // Divide
        case 9: return new AST_BinaryOperator(*this, pc, a, b, "div", 7); // 'div'
        case 10: return new AST_BinaryOperator(*this, pc, a, b, "<", 3); // LessThan
        case 11: return new AST_BinaryOperator(*this, pc, a, b, ">", 3); // GreaterThan
        case 12: return new AST_BinaryOperator(*this, pc, a, b, ">=", 3); // GreaterOrEqual
        case 13: return new AST_BinaryOperator(*this, pc, a, b, "<=", 3); // LessOrEqual
        case 14: return new AST_BinaryFunction(*this, pc, a, b, "bAnd"); // BitAnd
        case 15: return new AST_BinaryFunction(*this, pc, a, b, "bOr"); // BitOr
        case 16: return new AST_BitNot(*this, pc, a, b);
        case 17: return new AST_NewIter(*this, pc, a, b);
        case 18: return new AST_Length(*this, pc, a, b);
        case 19: return new AST_Clone(*this, pc, a, b);
        case 20: return new AST_SetClass(*this, pc, a, b);
        case 21: return new AST_AddArraySlot(*this, pc, a, b);
        case 22: return new AST_Stringer(*this, pc, a, b);
        case 23: return new AST_HasPath(*this, pc, a, b);
        case 24: return new AST_ClassOf(*this, pc, a, b);
      }
      break;
    case 25: return new AST_NewHandler(*this, pc, a, b);
  }
  return new ASTBytecodeNode(*this, pc, a, b);
}


/**
 \brief Create the initial AST which is not a tree at all, just a list of nodes.

 Convert every instruction into one or more nodes and chain them together in
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

  // Find all the jump instructions and create the target nodes.
  for (int i=0; i<nbc; i++) {
    int pc = i;
    uint8_t cmd = bc[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) { b = bc[i+1]<<8 | bc[i+2]; i += 2; }
    if ((a==11)||(a==12)||(a==13)||(a==23))
      AddToTargets(b, pc);
  }

  // Now run the byte codes again and create a linked list of instructions
  ASTNode *nd = first_ = new ASTFirstNode(*this);
  for (int i=0; i<nbc; i++) {
    int pc = i;
    uint8_t cmd = bc[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) { b = bc[i+1]<<8 | bc[i+2]; i += 2; }
    auto it = targets.find(pc);
    if (it != targets.end())
      nd = Append(nd, it->second);
    nd = Append(nd, NewBytecodeNode(pc, a, b));
  }
  last_ = Append(nd, new ASTLastNode(*this));

  // The code generator occasionally appends two consecutive return commends.
  // We fix that by deleting the second return.
  if (dynamic_cast<AST_Return*>(nd) && dynamic_cast<AST_Return*>(nd->prev))
    delete nd->Unlink();
}

void Decompiler::AddToTargets(int target, int origin) {
  ASTJumpTarget *t = nullptr;
  auto it = targets.find(target);
  if (it == targets.end())
    targets.insert(std::make_pair(target, t = new ASTJumpTarget(*this, target)));
  else
    t = it->second;
  t->addOrigin(origin);
  if (debugAST_) printf("Jump Target: %d to %d\n", origin, target);
}

/**
 Decompile the AST as much as possible in multiple runs.
 */
void Decompiler::solve()
{
  // Repeat everything until there are no more changes in the AST.
  bool astChanged = false;
  for (;;) {
    astChanged = false;
    // Resolve the easiest part, all simple expressions, first.
    bool dataFlowChanged = false;
    // Repeat until everything we can solve in data flow is solved.
    for (;;) {
      dataFlowChanged = false;
      ASTNode *nd = first_;
      for (;;) {
        // Call Resolve on one node.
        auto [changed, next] = nd->ResolveDataFlow();
        // If something changed, start over.
        // NOTE: this is pretty crass and may be tuned to use `next` instead of `first`
        if (changed) { dataFlowChanged = true; nd = first_; print(); continue; }
        // If we are at the end of the list, abort
        if (next == nullptr) break;
        nd = next;
      }
      if (dataFlowChanged == false) break;
      astChanged = true;
    }
    // Resolve the control flow Next.
    bool controlFlowChanged = false;
    ASTNode *nd = first_;
    for (;;) {
      // Call Resolve on one node.
      auto [changed, next] = nd->ResolveControlFlow();
      // If something changed, start over from the very beginning.
      // NOTE: this is pretty crass and may need to be tuned.
      if (changed) { controlFlowChanged = true; print(); break; }
      // If we are at the end of the list, abort
      if (next == nullptr) break;
      nd = next;
    }
    if (controlFlowChanged) { astChanged = true; continue; }
    // We have done all that we could
    if (!astChanged) break;
  }
  // TODO: Refine: remove faulty code (double return) and unnecessary code.
}

void Decompiler::print()
{
  if (!debugAST_) return;
  puts("---- AST -------");
  PState pState;
  output = Print::deep;
  for (ASTNode *nd = first_; nd; nd = nd->next) {
    nd->Print(pState);
  }
  puts("----------------");
}

void Decompiler::printRoot()
{
  if (!debugAST_) return;
  puts("---- Root -------");
  PState pState;
  output = Print::bytecode;
  for (ASTNode *nd = first_; nd; nd = nd->next) {
    nd->Print(pState);
  }
  puts("-----------------");
}

void Decompiler::printSource()
{
  PState pState;
  output = Print::script;

  pState.Begin();
  printf("func(");
  for (int i=0; i<numArgs_; i++) {
    printLocal(i + 3);
    if (i < numArgs_-1) printf(", ");
  }
  printf(")");
  pState.NewLine();

  first_->Print(pState);

  // list locals first! "local a;" ...
  if (numLocals_) {
    for (int i = 0; i < numLocals_; ++i) {
      pState.Begin();
      printf("local ");
      printLocal(i + 3 + numArgs_);
      printf(";");
      pState.NewLine();
    }
    pState.Begin(); pState.NewLine();
  }

  // now print the source code for all AST nodes
  for (ASTNode *nd = first_->next; nd; nd = nd->next) {
    nd->Print(pState);
  }
  pState.ClearDivider(); pState.Begin(); pState.End();
}

/**
 A path expression can take one of three forms:
 - an integer
 - a symbol
 - an array of class pathExpr
 */
void Decompiler::printPathExpr(RefArg pathExpr)
{
  printf("<<");
  PrintObject(pathExpr, 0);
  printf(">>");
}


/**
 \brief Print a frame that contains NewtonScript function.
 Check if tis is actually NewtonScript. No support for native or binary.
 Must be `class: #0x32` for newer apps, and
 \note For now, this simply prints the Frame as is. We need to decompile
 the `instructions` and merge them with the `literals`. For the old function
 call, we should also name the locals and arguments correctly.
 \note Decompiling NewtonScript is not too complicated. It has a few quirks,
 for example it generates unused bytecode. It's important to test continuously.
 - decompress the bytecode into "wordcode"
 - find all jump target addresses and store the source address
 - generate an AST and reduce it as much as possible
 - now try to find the typical pattern for control flow, reduce, and try again
 - if nothing can be applied anymore, the root of the AST should be a single value
 with the entire tree behind it. It should now be easy to generate source
 code for every node in the AST.
 - make sure that the source code is nicely formatted ;-)

 ```
 `DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(1));`
 - `class: #0x32`:
 - `instructions`: 'instructions bytecode as binary data
 - `literals`: 'literals, an array of values
 - `numArgs`: bits 31 to 16 are the number of locals, bits 15 to 0 are the number of arguments
 - `argFrame`: always `nil` in this format
 ```

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
  d.printRoot();
  d.printSource();
  return noErr;
}

