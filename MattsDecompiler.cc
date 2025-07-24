
/*
 File:    MattsDecompiler.cc

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#include "MattsDecompiler.h"

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

class Decompiler {
  NewtonPackagePrinter &printer_;
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
  void AddToTargets(int target, int origin);
  ASTNode *Append(ASTNode *lastNode, ASTNode *newNode);
  ASTBytecodeNode *NewBytecodeNode(int pc, int a, int b);
public:
  Decompiler(NewtonPackagePrinter &printer) : printer_( printer ) { }
  void print();
  void printRoot();
  void printSource();
  void printLiteral(int ix) {
    printer_.PrintRef(GetArraySlot(literals_, ix), 0, true);
  }
  void printLocal(int ix) {
    printer_.PrintRef(GetArraySlot(locals_, ix), 0, true);
  }
  void decompile(Ref ref);
  void solve();
  void generateAST(Ref instructions);
};

/**
 * @class PState
 * @brief A helper class that manages indents and dividing semicolons and commas.
 *
 * This works by deferring dividers, indents, and newlines until we actually print
 * the next chunk. That way, these parameters can be modified before they are
 * finally applied.
 *
 * @note This scheme should probably be ported to the NewtonPackagePrinter class,
 *       so that it can be used for printing packages as well.
 *
 * @enum Type
 *   - bytecode: Formatting for bytecode output.
 *   - deep: Formatting for deep analysis output.
 *   - script: Formatting for script output.
 *
 * @var Decompiler& dc
 *   Reference to the associated Decompiler instance.
 * @var Type type
 *   Current formatting type.
 * @var int indent
 *   Current indentation level.
 * @var int precedence
 *   Current precedence level for formatting. Controls if expressions must be wrapped in parentheses.
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
  enum class Type { bytecode, deep, script };
  Decompiler &dc;
  Type type { Type::bytecode };
  int indent { 0 };
  int precedence { 0 };
  bool indentPending_ { false };
  const char *textPending_ { nullptr };

  PState(Decompiler &decompiler, Type t) : dc( decompiler ), type( t ) { }
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
  int pc_ = -1;
public:
  ASTNode() = default;
  ASTNode(int pc) : pc_(pc) { }
  ASTNode *prev { nullptr };
  ASTNode *next { nullptr };
  int pc() { return pc_; }
  virtual ~ASTNode() = default;
  virtual int provides() { return kProvidesUnknown; }
  virtual int consumes() { return 0; }
  virtual ASTNode *Resolve(int *changes) { (void)changes; return next; }
  void indent(int inIndent) { for (int i=0; i<inIndent; i++) printf("  "); }
  void newline(int inIndent) { putchar('\n'); indent(inIndent); }
  void printHeader() { printf("###[%2d] ", provides()); }
  ASTNode *unlink() { prev->next = next; next->prev = prev; prev = next = nullptr; return this; }
  void replaceWith(ASTNode *nd) {
    prev->next = nd; next->prev = nd;
    nd->prev = prev; nd->next = next;
    prev = next = nullptr;
  }
  virtual void printSource(PState &p) { }

  virtual bool IsNIL() { return false; }
  virtual bool Resolved() = 0;
  virtual void Print(PState &p) = 0;
};

class ASTFirstNode : public ASTNode {
public:
  int provides() override { return kSpecialNode; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin(); printf("begin"); p.NewLine(+1);
    } else {
      p.Begin(); printHeader(); printf("ASTFirstNode ###"); p.NewLine();
    }
  }
};

class ASTLastNode : public ASTNode {
public:
  int provides() override { return kSpecialNode; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin(-1); printf("end"); p.NewLine();
    } else {
      p.Begin(); printHeader(); printf("ASTLastNode ###\n"); p.NewLine();
    }
  }
};

class ASTJumpTarget : public ASTNode {
  std::vector<int> origins_;
public:
  ASTJumpTarget(int pc) : ASTNode(pc) { }
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
    p.Begin();
    printHeader(); printf("%3d: ASTJumpTarget from", pc_);
    for (auto a: origins_) printf(" %d", a);
    printf(" ###");
    p.NewLine();
  }
};

class ASTBytecodeNode : public ASTNode {
protected:
  int a_ = 0;
  int b_ = 0;
public:
  ASTBytecodeNode(int pc, int a, int b)
  : ASTNode(pc), a_(a), b_(b) { }
  int b() { return b_; }
  bool Resolved() override { return false; }
  void Print(PState &p) override {
    p.Begin(); printHeader();
    printf("%3d: ERROR: ASTBytecodeNode a=%d, b=%d ###", pc_, a_, b_);
    p.NewLine();
  }
};

// (A=0, B=7): --
class AST_PopHandlers : public ASTBytecodeNode {
public:
  AST_PopHandlers(int pc, int a, int b) : ASTBytecodeNode(pc, a, b) { }
  int provides() override { return kProvidesNone; }
  // Don't know yet
  bool Resolved() override { return false; }
  void Print(PState &p) override {
    p.Begin(); printHeader();
    printf("%3d: AST_PopHandlers ###", pc_);
    p.NewLine();
  }
};

// (A=3): -- literal
class AST_Push : public ASTBytecodeNode {
public:
  AST_Push(int pc, int a, int b) : ASTBytecodeNode(pc, a, b) { }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      p.dc.printLiteral(b_);
      p.End();
    } else {
      p.Begin(); printHeader();
      printf("%3d: AST_Push literal[%d] ###", pc_, b_);
      p.NewLine();
    }
  }
};

// (A=4, B=signed): -- value
class AST_PushConst : public ASTBytecodeNode {
public:
  AST_PushConst(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      PrintObject(b_, 0);
      p.End();
    } else {
      p.Begin(); printHeader();
      printf("%3d: AST_PushConst value:%d ###", pc_, b_);
      p.NewLine();
    }
  }
  bool IsNIL() override { return (b_ == NILREF); }
};

// TODO: name nodes so that we can see the difference between a Bytecode and a generated node
// TODO: generated nodes should derive from another subclass
// TODO: This is a bug in the ByteCode: loop does not leave a value on the stack, but return assumes that there is one!
// Is that a bug in newton_framework, or is that in the original compiler too?
class ASTLoop : public ASTNode {
protected:
  std::vector<ASTNode*> body_;
public:
  ASTLoop(int pc) : ASTNode(pc) { }
  void add(ASTNode *nd) { body_.push_back(nd); }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      if (body_.size() > 1) {
        printf("loop begin"); p.NewLine(+1);
        for (auto &nd: body_) {
          nd->Print(p); p.NewLine(";");
        }
        p.Begin(-1); printf("end"); p.NewLine(";");
      } else if (body_.size() == 1) {
        printf("loop "); p.NewLine(+1); // loop only one instruction forever (could be an if...break)
        body_[0]->Print(p); p.indent--;
      } else {
        printf("loop nil"); // special case, loops forever
      }
      p.NewLine(";");
    } else {
      p.Begin(); printHeader();
      printf("%3d: ASTLoop ###", pc_);
      p.NewLine();
    }
  };
};

// (A=11): --
class AST_Branch : public ASTBytecodeNode {
public:
  AST_Branch(int pc, int a, int b) : ASTBytecodeNode(pc, a, b) { }
  int provides() override { return kBranch; }
  bool Resolved() override { return false; }
  ASTNode *Resolve(int *changes) override {
    if (b_ < pc_) {
      // `loop`: JumpTarget A; n*stmt; branch A;
      int numStmt = 0;
      ASTNode *nd = prev;
      while (nd->provides() == kProvidesNone) {
        numStmt++;
        nd = nd->prev;
      }
      ASTJumpTarget *jt = dynamic_cast<ASTJumpTarget*>(nd);
      if (!jt) return next;
      if (!jt->containsOrigin(pc_)) return next;

      // It's a loop! Build a new node.
      ASTLoop *loop = new ASTLoop(pc_);
      nd = jt->next;
      jt->removeOrigin(pc_);
      if (jt->empty()) delete jt->unlink();
      for (int i=numStmt; i>0; --i) {
        ASTNode *nx = nd->next;
        loop->add(nd);
        nd->unlink();
        nd = nx;
      }
      this->replaceWith(loop);
      return loop;
    }

    return next;
  }
  void Print(PState &p) override {
    p.Begin(); printHeader();
    printf("%3d: AST_Branch pc:%d ###", pc_, b_);
    p.NewLine();
  }
};

// (A=0, B=3):  -- RCVR
class AST_PushSelf : public ASTBytecodeNode {
public:
  AST_PushSelf(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("self");
      p.End();
    } else {
      p.Begin(); printHeader();
      printf("%3d: AST_PushSelf ###", pc_);
      p.NewLine();
    }
  }
};

// (A=14): -- value
class AST_FindVar : public ASTBytecodeNode {
public:
  AST_FindVar(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin(); p.dc.printLiteral(b_); p.End();
    } else {
      p.Begin(); printHeader();
      printf("%3d: AST_FindVar literal[%d] ###", pc_, b_);
      p.NewLine();
    }
  }
};

// (A=15): -- value
class AST_GetVar : public ASTBytecodeNode {
public:
  AST_GetVar(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  int provides() override { return 1; }
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      p.dc.printLocal(b_);
      p.End();
    } else {
      p.Begin(); printHeader();
      printf("%3d: AST_GetVar local[%d] ###", pc_, b_);
      p.NewLine();
    }
  }
};

class AST_Consume1 : public ASTBytecodeNode {
protected:
  ASTNode *in_ { nullptr };
public:
  AST_Consume1(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  int provides() override { if (in_) return 1; else return kProvidesUnknown; }
  int consumes() override { return 1; }
  ASTNode *Resolve(int *changes) override {
    if (in_) return next; // nothing more to do
    if (prev->provides() == 1) {
      in_ = prev; prev->unlink();
      (*changes)++;
      return this;
    }
    return next;
  }
  bool Resolved() override { return (in_ != nullptr); }
  void PrintChildren(PState &p) {
    if (in_) { p.indent++; in_->Print(p); p.indent--; }
  }
  void Print(PState &p) override {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader();
    printf("%3d: ERROR: AST_Consume1 ###", pc_);
    p.NewLine();
  }
};

// (A=12): value --
class AST_BranchIfTrue : public AST_Consume1 {
public:
  AST_BranchIfTrue(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kBranchIfTrue; else return kProvidesUnknown; }
  ASTNode *Resolve(int *changes) override {
    return next;
  }
  void Print(PState &p) override {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader();
    printf("%3d: AST_BranchIfTrue pc:%d ###", pc_, b_);
    p.NewLine();
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
  ASTIfThenElseNode(int pc, bool returnsAValue) : ASTNode(pc), returnsAValue_(returnsAValue) { }
  void setCond(ASTNode *nd) { cond_ = nd; }
  void addIf(ASTNode *nd) { ifBranch_.push_back(nd); }
  void addElse(ASTNode *nd) { elseBranch_.push_back(nd); }
  int provides() override { return returnsAValue_ ? 1 : 0; }
  /// This node only exists if all nodes involved are resolved.
  bool Resolved() override { return true; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      bool needBeginEnd = ((ifBranch_.size() > 1) || (elseBranch_.size() > 1));
      // >> if (condition) the begin
      p.Begin(); printf("if "); cond_->Print(p); printf(" then");
      if (needBeginEnd) printf(" begin");
      p.NewLine(+1);
      // >>   if-Branch
      for (auto &nd: ifBranch_) {
        p.Begin(); nd->Print(p); p.Divider(";");
      }
      if (returnsAValue_) p.NewLine(";");
      if (!elseBranch_.empty()) {
        // >> end else if
        p.Begin(-1);
        if (needBeginEnd) printf("end else begin"); else printf("else");
        p.NewLine(+1);
        // >>   else-Branch
        for (auto &nd: elseBranch_) {
          p.Begin(); nd->Print(p); p.Divider(";");
        }
        if (returnsAValue_) p.NewLine(";");
      }
      if (needBeginEnd) {
        p.Begin(-1); printf("end"); p.NewLine(";");
      } else {
        p.indent--;
      }
    } else {
      if (p.type == PState::Type::deep) {
        p.Begin(); printHeader(); printf("%3d: ASTIfThenElseNode: if ###", pc_); p.NewLine(+1);
        cond_->Print(p);
        p.Begin(-1); printHeader(); printf("%3d: ASTIfThenElseNode: then ###", pc_); p.NewLine(+1);
        for (auto *nd: ifBranch_) nd->Print(p);
        p.Begin(-1); printHeader(); printf("%3d: ASTIfThenElseNode: else ###", pc_); p.NewLine(+1);
        for (auto *nd: elseBranch_) nd->Print(p);
        p.indent--;
      }
      p.Begin(); printHeader();
      printf("%3d: ASTIfThenElseNode %s- %zu %zu ###", pc_,
             returnsAValue_ ? "Expression " : "",
             ifBranch_.size(), elseBranch_.size());
      p.NewLine();
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
  AST_BranchIfFalse(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kBranchIfFalse; else return kProvidesUnknown; }
  ASTNode *Resolve(int *changes) override {
    // Track if the if/then/else returns a value
    bool returnsAValue = false;
    ASTJumpTarget *jt1 = nullptr;
    ASTJumpTarget *jt2 = nullptr;
    int numIf = 0;
    int numElse = 0;
    int jt2origin = pc_;

    // Step 1: Make sure the condition input is resolved
    if (!in_) return AST_Consume1::Resolve(changes);

    // Step 2: Start pattern matching for if/then/else structure
    ASTNode *nd = next;

    // Step 3: Count statements in the 'if' branch (nodes that provide nothing)
    while (nd->provides() == kProvidesNone) {
      numIf++;
      nd = nd->next;
    }

    // Step 4: Check if the next node is an expression (provides a value)
    // If so, this is an if/then/else expression, not just a statement
    if (nd->provides() == 1) {
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
      if (nd->provides() != kJumpTarget) return next;
      jt1 = static_cast<ASTJumpTarget*>(nd);
      if (!jt1->containsOnly(pc_)) return next;
      nd = nd->next;

      // Step 7: Count statements in the 'else' branch
      while (nd->provides() == kProvidesNone) {
        numElse++;
        nd = nd->next;
      }

      // Step 8: If this is an expression, check for a single value in the else branch
      if (returnsAValue) {
        if (nd->provides() != 1) return next;
        numElse++;
        nd = nd->next;
      }

      // Step 9: The next node must be a jump target for the unconditional branch
      if (nd->provides() != kJumpTarget) return next;
      jt2 = static_cast<ASTJumpTarget*>(nd);
      if (!jt2->containsOrigin(branch_pc)) return next;
      jt2origin = branch_pc;
    } else if (nd->provides() == kJumpTarget) {
      // Step 10: Handle the case with no else branch (simple if/then)
      jt2 = static_cast<ASTJumpTarget*>(nd);
      if (!jt2->containsOrigin(pc_)) return next;
    } else {
      // Pattern does not match any known if/then/else structure
      return next;
    }

    // Step 11: Build the ASTIfThenElseNode and replace the matched nodes
    ASTIfThenElseNode *ite = new ASTIfThenElseNode(pc(), returnsAValue);
    ite->setCond(in_);
    // Add all nodes from the 'if' branch
    for (int i=0; i<numIf; i++) {
      ite->addIf(next);
      next->unlink();
    }
    if (numElse > 0) {
      // Remove branch and jump target nodes before the else branch
      next->unlink(); // branch
      next->unlink(); // jump target
      // Add all nodes from the 'else' branch
      for (int i=0; i<numElse; i++) {
        ite->addElse(next);
        next->unlink();
      }
    }
    // Step 12: Remove the jump target if it has no more origins
    jt2->removeOrigin(jt2origin);
    if (jt2->empty()) jt2->unlink(); // jump target

    // Step 13: Replace this node with the new if/then/else node
    replaceWith(ite);
    (*changes)++;
    return ite->next;
  }
  bool Resolved() override { return false; }
  void Print(PState &p) override {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader();
    printf("%3d: AST_BranchIfFalse pc:%d ###", pc_, b_);
    p.NewLine();
  }
};

// TODO: the very last return probably doesn't need to be printed
// TODO: return NIL is implied if there is no return statement
// (A=0, B=2): --
class AST_Return : public AST_Consume1 {
public:
  AST_Return(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { return kProvidesNone; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("return "); in_->Print(p);
      p.NewLine(";");
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader();
      printf("%3d: AST_Return ###", pc_);
      p.NewLine();
    }
  }
};

class AST_Break : public AST_Consume1 {
public:
  AST_Break(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("break");
      if (!in_->IsNIL()) {
        printf(" ");
        in_->Print(p);
      }
      p.NewLine(";");
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader();
      printf("%3d: AST_Break target=%d ###", pc_, b_);
      p.NewLine();
    }
  }
};

// (A=0, B=0): value --
class AST_Pop : public AST_Consume1 {
public:
  AST_Pop(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  ASTNode *Resolve(int *changes) override {
    if (Resolved()) return next;
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
          return next;
        }
        if ((nd->pc() == target) && (nd->provides() == kJumpTarget)) break;
        nd = nd->next;
      }
      ASTJumpTarget *jt = dynamic_cast<ASTJumpTarget*>(nd);
      if (!jt) ThrowMsg("AST_Pop:Resolve: should have been a Jump Target!");
      if (!jt->containsOrigin(branch->pc())) ThrowMsg("AST_Pop:Resolve: address of `break` not in Jump Target!");
      jt->removeOrigin(branch->pc());
      if (jt->empty()) delete jt->unlink();

      // Replace this node and the previous branch with an AST_Break
      AST_Break *breakNode = new AST_Break(branch->pc(), 0, branch->b());
      delete branch->unlink();
      this->replaceWith(breakNode);
      return breakNode;
    }
    return AST_Consume1::Resolve(changes);
  }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin(); in_->Print(p); p.NewLine(";");
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader();
      printf("%3d: AST_Pop ###", pc_);
      p.NewLine();
    }
  }
};

// (A=0, B=1): x -- x x
class AST_Dup : public AST_Consume1 {
public:
  AST_Dup(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  // TODO: provides(2) does not match any consumers!
  // NOTE: we must find an actual use case and the corresponding source code
  // NOTE: it may make sense to split this into a dup1 and dup2 node?!
  int provides() override { if (in_) return 2; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader();
    printf("%3d: AST_Dup ###", pc_);
    p.NewLine();
  }
};

// (A=0, B=4): func -- closure
class AST_SetLexScope : public AST_Consume1 {
public:
  AST_SetLexScope(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader();
    printf("%3d: AST_SetLexScope ###", pc_);
    p.NewLine();
  }
};

// (A=20): value --
class AST_SetVar : public AST_Consume1 {
public:
  AST_SetVar(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      p.dc.printLocal(b_);
      printf(" := ");
      in_->Print(p);
      p.NewLine(";");
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader();
      printf("%3d: AST_SetVar local[%d] ###", pc_, b_);
      p.NewLine();
    }
  }
};

// (A=21): value --
class AST_FindAndSetVar : public AST_Consume1 {
public:
  AST_FindAndSetVar(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      p.dc.printLiteral(b_);
      printf(" := ");
      in_->Print(p);
      p.NewLine(";");
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader();
      printf("%3d: AST_FindAndSetVar literal[%d] ###", pc_, b_);
      p.NewLine();
    }
  }
};

// (A=24, B=5): value -- value
class AST_Not : public AST_Consume1 {
public:
  AST_Not(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      int ppp = p.precedence;
      bool parentheses = (p.precedence > 2);
      p.precedence = 2;
      p.Begin();
      if (parentheses) printf("(");
      printf("not ");
      in_->Print(p);
      if (parentheses) printf(")");
      p.End();
      p.precedence = ppp;
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_Not ###", pc_); p.NewLine();
    }
  }
};

// (A=24, B=18): object -- length
class AST_Length : public AST_Consume1 {
public:
  AST_Length(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("length(");
      in_->Print(p);
      printf(")");
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_Length ###", pc_); p.NewLine();
    }
  }
};

// (A=24, B=19): object -- clone
class AST_Clone : public AST_Consume1 {
public:
  AST_Clone(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("clone(");
      in_->Print(p);
      printf(")");
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_Clone ###", pc_); p.NewLine();
    }
  }
};

// (A=24, B=22): array -- string
class AST_Stringer : public AST_Consume1 {
public:
  AST_Stringer(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void Print(PState &p) override {
//    if ((p.type == PState::Type::script) && Resolved()) {
//      p.Begin();
//      printf("clone(");
//      in_->Print(p);
//      printf(")");
//      p.End();
//    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_Stringer ###", pc_); p.NewLine();
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
  AST_ClassOf(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("ClassOf(");
      in_->Print(p);
      printf(")");
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_ClassOf ###", pc_); p.NewLine();
    }
  }
};

// (A=24, B=16): num -- result
class AST_BitNot : public AST_Consume1 {
public:
  AST_BitNot(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("bNot(");
      in_->Print(p);
      printf(")");
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_BitNot ###", pc_); p.NewLine();
    }
  }
};

// (A=22) addend -- addend value
class AST_IncrVar : public AST_Consume1 {
public:
  AST_IncrVar(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return 2; else return kProvidesUnknown; }
  void Print(PState &p) override {
//    if ((p.type == PState::Type::script) && Resolved()) {
//    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_IncrVar local[%d] ###", pc_, b_); p.NewLine();
//    }
  }
};

// (A=0, B=5) iterator --
class AST_IterNext : public AST_Consume1 {
public:
  AST_IterNext(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void Print(PState &p) override {
    //    if ((p.type == PState::Type::script) && Resolved()) {
    //    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: AST_IterNext ###", pc_); p.NewLine();
    //    }
  }
};

// (A=0, B=6) iterator -- done
class AST_IterDone : public AST_Consume1 {
public:
  AST_IterDone(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    //    if ((p.type == PState::Type::script) && Resolved()) {
    //    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: AST_IterDone ###", pc_); p.NewLine();
    //    }
  }
};

class AST_Consume2 : public ASTBytecodeNode {
protected:
  ASTNode *in1_ { nullptr };
  ASTNode *in2_ { nullptr };
public:
  AST_Consume2(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  int provides() override { if (in1_ && in2_) return 1; else return kProvidesUnknown; }
  int consumes() override { return 2; }
  ASTNode *Resolve(int *changes) override {
    if (in1_ && in2_) return next; // nothing more to do
    if ((prev->provides() == 1) && (prev->prev->provides() == 1)) {
      in2_ = prev; prev->unlink();
      in1_ = prev; prev->unlink();
      (*changes)++;
      return this;
    }
    return next;
  }
  bool Resolved() override { return (in1_ != nullptr) && (in2_ != nullptr); }
  void PrintChildren(PState &p) {
    p.indent++;
    if (in1_) in1_->Print(p);
    if (in2_) in2_->Print(p);
    p.indent--;
  }
  void Print(PState &p) override {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: ERROR: AST_Consume2 ###", pc_); p.NewLine();
  }
};

// num1 num2 -- result
class AST_BinaryFunction : public AST_Consume2 {
protected:
  const char *func_ { nullptr };
public:
  AST_BinaryFunction(int pc, int a, int b, const char *func)
  : AST_Consume2(pc, a, b), func_(func) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      int ppp = p.precedence;
      p.precedence = 0;
      p.Begin();
      printf("%s(", func_);
      in1_->Print(p);
      printf(", ");
      in2_->Print(p);
      printf(")");
      p.End();
      p.precedence = ppp;
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_BinaryFunction \"%s\" ###", pc_, func_); p.NewLine();
    }
  }
};

// num1 num2 -- result
class AST_BinaryOperator : public AST_Consume2 {
protected:
  const char *op_ { nullptr };
  int precedence_ { 0 };
public:
  AST_BinaryOperator(int pc, int a, int b, const char *op, int precedence)
  : AST_Consume2(pc, a, b), op_(op), precedence_(precedence) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      int ppp = p.precedence;
      bool parentheses = (p.precedence > precedence_);
      p.precedence = precedence_;
      p.Begin();
      if (parentheses) printf("(");
      in1_->Print(p); printf(" %s ", op_); in2_->Print(p);
      if (parentheses) printf(")");
      p.End();
      p.precedence = ppp;
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_BinaryOperator \"%s\" ###", pc_, op_); p.NewLine();
    }
  }
};

// (A=17, B=0xFFFF): size class -- array
class AST_NewArray : public AST_Consume2 {
public:
  AST_NewArray(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    //    if ((p.type == PState::Type::script) && Resolved()) {
    //    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: AST_NewArray ###", pc_); p.NewLine();
    //    }
  }
};

// (A=18): object pathExpr -- value
class AST_GetPath : public AST_Consume2 {
public:
  AST_GetPath(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    //    if ((p.type == PState::Type::script) && Resolved()) {
    //    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: AST_GetPath b:%d ###", pc_, b_); p.NewLine();
    //    }
  }
};

// (A=24, B=2): object index -- element
class AST_ARef : public AST_Consume2 {
public:
  AST_ARef(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      in1_->Print(p);
      printf("["); in2_->Print(p); printf("]");
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_ARef ###", pc_); p.NewLine();
    }
  }
};

// (A=24, B=20): object class -- object
class AST_SetClass : public AST_Consume2 {
public:
  AST_SetClass(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("SetClass(");
      in1_->Print(p); printf(", ");
      in2_->Print(p); printf(")");
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_SetClass ###", pc_); p.NewLine();
    }
  }
};

// (A=24, B=21): array object -- object
class AST_AddArraySlot : public AST_Consume2 {
public:
  AST_AddArraySlot(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("AddArraySlot(");
      in1_->Print(p); printf(", ");
      in2_->Print(p); printf(")");
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_AddArraySlot ###", pc_); p.NewLine();
    }
  }
};

// (A=24, B=23): object pathExpr -- result
class AST_HasPath : public AST_Consume2 {
public:
  AST_HasPath(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  int provides() override { if (Resolved()) return 1; else return kProvidesUnknown; }
  void Print(PState &p) override {
    //    if ((p.type == PState::Type::script) && Resolved()) {
    //    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: AST_HasPath ###", pc_); p.NewLine();
    //    }
  }
};

// (A=24, B=17): object deeply -- iterator
class AST_NewIter : public AST_Consume2 {
public:
  AST_NewIter(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  bool Resolved() override { return false; }
  void Print(PState &p) override {
    //    if ((p.type == PState::Type::script) && Resolved()) {
    //    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: AST_NewIter ###", pc_); p.NewLine();
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
  AST_SetPath(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  int provides() override {
    if (Resolved())
      return (b_ == 0) ? kProvidesNone : 1;
    else
      return kProvidesUnknown;
  }
  int consumes() override { return 3; }
  ASTNode *Resolve(int *changes) override {
    if (Resolved()) return next; // nothing more to do
    if (   (prev->provides() == 1)
        && (prev->prev->provides() == 1)
        && (prev->prev->provides() == 1)) {
      value_ = prev; prev->unlink();
      path_ = prev; prev->unlink();
      object_ = prev; prev->unlink();
      (*changes)++;
      return this;
    }
    return next;
  }
  bool Resolved() override { return (object_ != nullptr) && (path_ != nullptr) && (value_ != nullptr); }
  void PrintChildren(PState &p) {
    p.indent++;
    if (object_) object_->Print(p);
    if (path_) path_->Print(p);
    if (value_) value_->Print(p);
    p.indent--;
  }
  void Print(PState &p) override {
    //    if ((p.type == PState::Type::script) && Resolved()) {
    //    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: AST_SetPath ###", pc_); p.NewLine();
    //    }
  }

};

// (A=24, B=3): object index element -- element
class AST_SetARef : public ASTBytecodeNode {
protected:
  ASTNode *object_ { nullptr };
  ASTNode *index_ { nullptr };
  ASTNode *element_ { nullptr };
public:
  AST_SetARef(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  int provides() override { if (object_ && index_ && element_) return kProvidesNone; else return kProvidesUnknown; }
  int consumes() override { return 3; }
  ASTNode *Resolve(int *changes) override {
    if (Resolved()) return next; // nothing more to do
    if (   (prev->provides() == 1)
        && (prev->prev->provides() == 1)
        && (prev->prev->provides() == 1)) {
      element_ = prev; prev->unlink();
      index_ = prev; prev->unlink();
      object_ = prev; prev->unlink();
      (*changes)++;
      return this;
    }
    return next;
  }
  bool Resolved() override { return (object_ != nullptr) && (index_ != nullptr) && (element_ != nullptr); }
  void PrintChildren(PState &p) {
    p.indent++;
    if (object_) object_->Print(p);
    if (index_) index_->Print(p);
    if (element_) element_->Print(p);
    p.indent--;
  }
  void Print(PState &p) override {
    //    if ((p.type == PState::Type::script) && Resolved()) {
    //    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: AST_SetARef ###", pc_); p.NewLine();
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
  AST_BranchLoop(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  int provides() override { if (incr_ && index_ && limit_) return kProvidesNone; else return kProvidesUnknown; }
  int consumes() override { return 3; }
  ASTNode *Resolve(int *changes) override {
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
    return next;
  }
  bool Resolved() override { return (incr_ != nullptr) && (index_ != nullptr) && (limit_ != nullptr); }
  void PrintChildren(PState &p) {
    p.indent++;
    if (incr_) incr_->Print(p);
    if (index_) index_->Print(p);
    if (limit_) limit_->Print(p);
    p.indent--;
  }
  void Print(PState &p) override {
    //    if ((p.type == PState::Type::script) && Resolved()) {
    //    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: AST_BranchLoop ###", pc_); p.NewLine();
    //    }
  }
};


class AST_ConsumeN : public ASTBytecodeNode {
protected:
  int numIns_;
  std::vector<ASTNode *> ins_;
public:
  AST_ConsumeN(int pc, int a, int b, int n)
  : ASTBytecodeNode(pc, a, b), numIns_(n) { }
  int provides() override { if (ins_.empty()) return kProvidesUnknown; else return 1; }
  int consumes() override { return numIns_; }
  ASTNode *Resolve(int *changes) override {
    if (Resolved()) return next; // nothing more to do
    ASTNode *nd = this;
    for (int i=0; i<numIns_; i++) {
      nd = nd->prev;
      if (nd->provides() != 1) { nd = nullptr; break; }
    }
    if (nd == nullptr) return next;
    for (int i=0; i<numIns_; i++) {
      ins_.push_back(nd);
      nd = nd->next;
      nd->prev->unlink();
    }
    (*changes)++;
    return this;
  }
  bool Resolved() override { return (ins_.size() == (size_t)numIns_); }
  void PrintChildren(PState &p) {
    p.indent++;
    for (auto &nd: ins_) nd->Print(p);
    p.indent--;
  }
  void Print(PState &p) override {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader(); printf("%3d: ERROR: AST_ConsumeN ###", pc_); p.NewLine();
  }
};

// (A=5): arg1 arg2 ... argN name -- result
class AST_Call : public AST_ConsumeN {
public:
  AST_Call(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  // TODO: the following special functions (reserved words) need to be written "a op b"
  // mod, <<, >>, (note the precedence! 7, 8, 8)
  // HasVar(a) can also be written as "a exists", but both are legal
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      ins_[numIns_-1]->Print(p);
      printf("(");
      for (int i=0; i<numIns_-1; i++) {
        p.Begin(); ins_[i]->Print(p); p.Divider(", ");
      }
      p.ClearDivider();
      p.Begin(); printf(")"); p.End();
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_Call n=%d ###", pc_, b_); p.NewLine();
    }
  }
};

// (A=6): arg1 arg2 ... argN func -- result
// call ... with (...)
class AST_Invoke : public AST_ConsumeN {
public:
  AST_Invoke(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("invoke ");
      ins_[numIns_-1]->Print(p);
      printf(" with (");
      for (int i=0; i<numIns_-1; i++) {
        p.Begin(); ins_[i]->Print(p); p.Divider(", ");
      }
      p.ClearDivider();
      p.Begin(); printf(")"); p.End();
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader(); printf("%3d: AST_Invoke n=%d ###", pc_, numIns_); p.NewLine();
    }
  }
};

// (A=7): arg1 arg2 ... argN name receiver -- result
class AST_Send : public AST_ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  AST_Send(int pc, int a, int b, bool ifDefined)
  : AST_ConsumeN(pc, a, b, b+2), ifDefined_(ifDefined) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      ins_[numIns_-2]->Print(p);
      if (ifDefined_)
        printf(":?");
      else
        printf(":");
      ins_[numIns_-1]->Print(p);
      printf("(");
      for (int i=0; i<numIns_-2; i++) {
        p.Begin(); ins_[i]->Print(p); p.Divider(", ");
      }
      p.ClearDivider();
      p.Begin(); printf(")"); p.End();
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader();
      if (ifDefined_)
        printf("%3d: AST_Send if defined n=%d ###", pc_, numIns_);
      else
        printf("%3d: AST_Send n=%d ###", pc_, numIns_);
      p.NewLine();
    }
  }
};

// (A=9): arg1 arg2 ... argN name -- result
// inherited:Print(3);
class AST_Resend : public AST_ConsumeN {
protected:
  bool ifDefined_ { false };
public:
  AST_Resend(int pc, int a, int b, bool ifDefined)
  : AST_ConsumeN(pc, a, b, b+1), ifDefined_(ifDefined) { }
  void Print(PState &p) override {
    if ((p.type == PState::Type::script) && Resolved()) {
      p.Begin();
      printf("inherited:");
      if (ifDefined_) printf("?");
      ins_[numIns_-1]->Print(p);
      printf("(");
      for (int i=0; i<numIns_-1; i++) {
        p.Begin(); ins_[i]->Print(p); p.Divider(", ");
      }
      p.ClearDivider();
      p.Begin(); printf(")"); p.End();
      p.End();
    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader();
      if (ifDefined_)
        printf("%3d: AST_Resend if defined n=%d ###", pc_, numIns_);
      else
        printf("%3d: AST_Resend n=%d ###", pc_, numIns_);
      p.NewLine();
    }
  }
};

// (A=16, B=numVal) val1 val2 ... valN map -- frame
class AST_MakeFrame : public AST_ConsumeN {
public:
  AST_MakeFrame(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  void Print(PState &p) override {
//    if ((p.type == PState::Type::script) && Resolved()) {
//    } else {
      if (p.type == PState::Type::deep) PrintChildren(p);
      p.Begin(); printHeader();
      printf("%3d: AST_MakeFrame n=%d ###", pc_, numIns_);
      p.NewLine();
//    }
  }
};

// (A=17, B=numVal):  val1 val2 ... valN class -- array): arg1 arg2 ... argN name -- result
class AST_MakeArray : public AST_ConsumeN {
public:
  AST_MakeArray(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  void Print(PState &p) override {
//    if ((p.type == PState::Type::script) && Resolved()) {
//    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader();
    printf("%3d: AST_MakeArray n=%d ###", pc_, numIns_);
    p.NewLine();
//    }
  }
};

// (A=25): sym1 pc1 sym2 pc2 ... symN pcN --
class AST_NewHandler : public AST_ConsumeN {
public:
  AST_NewHandler(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b*2) { }
  void Print(PState &p) override {
//    if ((p.type == PState::Type::script) && Resolved()) {
//    } else {
    if (p.type == PState::Type::deep) PrintChildren(p);
    p.Begin(); printHeader();
    printf("%3d: AST_NewHandler n=%d ###", pc_, numIns_);
    p.NewLine();
//    }
  }
};

// -----------------------------------------------------------------------------

void Decompiler::decompile(Ref ref)
{
  puts("\n==== Matt's Decompiler:");
  Ref klass = GetFrameSlot(ref, SYMA(class));
  if (IsSymbol(klass) && SymbolCompare(klass, SYMA(CodeBlock))==0) {
    nos_ = 1;
    Ref numArgs = GetFrameSlot(ref, SYMA(numArgs));
    numArgs_ = RefToInt(numArgs);
    Ref argFrame = GetFrameSlot(ref, SYMA(argFrame));
    int argFrameLength = Length(argFrame);
    numLocals_ = argFrameLength - 3 - numArgs_;
    locals_ = MakeArray(argFrameLength);
    // Make the list of names of the locals
    Ref map = ((FrameObject *)ObjectPtr(argFrame))->map;
    for (int i=0; i<argFrameLength; i++) {
      SetArraySlot(locals_, i, GetArraySlot(map, i));
    }
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
    SetArraySlot(locals_, 0, SYMA(_parent));
    SetArraySlot(locals_, 0, SYMA(_implementor));
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
        case 0: return new AST_Pop(pc, a, b);
        case 1: return new AST_Dup(pc, a, b);
        case 2: return new AST_Return(pc, a, b);
        case 3: return new AST_PushSelf(pc, a, b);
        case 4: return new AST_SetLexScope(pc, a, b);//        case 4: bc.bc = BC::SetLexScope; break;
        case 5: return new AST_IterNext(pc, a, b);
        case 6: return new AST_IterDone(pc, a, b);
        case 7: return new AST_PopHandlers(pc, a, b);
      };
      break;
    case 3: return new AST_Push(pc, a, b);
    case 4: return new AST_PushConst(pc, a, b);
    case 5: return new AST_Call(pc, a, b);
    case 6: return new AST_Invoke(pc, a, b);
    case 7: return new AST_Send(pc, a, b, false); // Send
    case 8: return new AST_Send(pc, a, b, true); // SendIfDefined
    case 9: return new AST_Resend(pc, a, b, false); // Resend
    case 10: return new AST_Resend(pc, a, b, true); // ResendIfDefined
    case 11: return new AST_Branch(pc, a, b);
    case 12: return new AST_BranchIfTrue(pc, a, b);
    case 13: return new AST_BranchIfFalse(pc, a, b);
    case 14: return new AST_FindVar(pc, a, b);
    case 15: return new AST_GetVar(pc, a, b);
    case 16: return new AST_MakeFrame(pc, a, b);
    case 17:
      if (b == 0xFFFF)
        return new AST_NewArray(pc, a, b);
      else
        return new AST_MakeArray(pc, a, b);
    case 18: return new AST_GetPath(pc, a, b);
    case 19: return new AST_SetPath(pc, a, b);
    case 20: return new AST_SetVar(pc, a, b);
    case 21: return new AST_FindAndSetVar(pc, a, b);
    case 22: return new AST_IncrVar(pc, a, b);
    case 23: return new AST_BranchLoop(pc, a, b);
    case 24:
      switch (b) {
        case 0: return new AST_BinaryOperator(pc, a, b, "+", 6); // Add
        case 1: return new AST_BinaryOperator(pc, a, b, "-", 6); // Sub
        case 2: return new AST_ARef(pc, a, b);
        case 3: return new AST_SetARef(pc, a, b);
        case 4: return new AST_BinaryOperator(pc, a, b, "=", 3); // Equals
        case 5: return new AST_Not(pc, a, b);
        case 6: return new AST_BinaryOperator(pc, a, b, "<>", 3); // NotEquals
        case 7: return new AST_BinaryOperator(pc, a, b, "*", 7); // Multiply
        case 8: return new AST_BinaryOperator(pc, a, b, "/", 7); // Divide
        case 9: return new AST_BinaryOperator(pc, a, b, "div", 7); // 'div'
        case 10: return new AST_BinaryOperator(pc, a, b, "<", 3); // LessThan
        case 11: return new AST_BinaryOperator(pc, a, b, ">", 3); // GreaterThan
        case 12: return new AST_BinaryOperator(pc, a, b, ">=", 3); // GreaterOrEqual
        case 13: return new AST_BinaryOperator(pc, a, b, "<=", 3); // LessOrEqual
        case 14: return new AST_BinaryFunction(pc, a, b, "bAnd"); // BitAnd
        case 15: return new AST_BinaryFunction(pc, a, b, "bOr"); // BitOr
        case 16: return new AST_BitNot(pc, a, b);
        case 17: return new AST_NewIter(pc, a, b);
        case 18: return new AST_Length(pc, a, b);
        case 19: return new AST_Clone(pc, a, b);
        case 20: return new AST_SetClass(pc, a, b);
        case 21: return new AST_AddArraySlot(pc, a, b);
        case 22: return new AST_Stringer(pc, a, b);
        case 23: return new AST_HasPath(pc, a, b);
        case 24: return new AST_ClassOf(pc, a, b);
      }
      break;
    case 25: return new AST_NewHandler(pc, a, b);
  }
  return new ASTBytecodeNode(pc, a, b);
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

  // Now run the byte codes again and create a linked list of instrcutions
  ASTNode *nd = first_ = new ASTFirstNode();
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
  last_ = Append(nd, new ASTLastNode());
}

void Decompiler::AddToTargets(int target, int origin) {
  ASTJumpTarget *t = nullptr;
  auto it = targets.find(target);
  if (it == targets.end())
    targets.insert(std::make_pair(target, t = new ASTJumpTarget(target)));
  else
    t = it->second;
  t->addOrigin(origin);
  printf("Jump Target: %d to %d\n", origin, target);
}

void Decompiler::solve()
{
  bool solved = true;
  do {
    solved = true;
    int changes = 0;
    ASTNode *nd = first_;
    while (nd) {
      nd = nd->Resolve(&changes);
      if (changes > 0) {
        print();
        solved = false;
        changes = 0;
      }
    }
  } while (!solved);
  // TODO: Refine: remove faulty code (double return) and unnecessary code.
}

void Decompiler::print()
{
  puts("---- AST -------");
  PState pState(*this, PState::Type::deep);
  for (ASTNode *nd = first_; nd; nd = nd->next) {
    nd->Print(pState);
  }
  puts("----------------");
}

void Decompiler::printRoot()
{
  puts("---- Root -------");
  PState pState(*this, PState::Type::bytecode);
  for (ASTNode *nd = first_; nd; nd = nd->next) {
    nd->Print(pState);
  }
  puts("-----------------");
}

void Decompiler::printSource()
{
  PState pState(*this, PState::Type::script);
  puts("---- Source code -------");
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
  puts("------------------------");
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
NewtonErr mDecompile(Ref ref, NewtonPackagePrinter &printer)
{
  Decompiler d(printer);
  d.decompile(ref);
  d.printRoot();
  d.printSource();
  return noErr;
}

