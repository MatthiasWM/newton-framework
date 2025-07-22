
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


class ASTNode;
class ASTBytecodeNode;
class ASTJumpTarget;

class Decompiler {
  NewtonPackagePrinter &printer_;
  int nos_ = 0;
  int numArgs_ = 0;
  RefVar args_;
  int numLocals_ = 0;
  RefVar locals_;
  int numLiterals_ = 0;
  RefVar literals_;
  ASTNode *first_ { nullptr };
  ASTNode *last_ { nullptr };
  std::map<int, ASTJumpTarget*> targets;
  void AddToTargets(int target, int origin);
  ASTNode *Append(ASTNode *lastNode, ASTNode *newNode);
  ASTBytecodeNode *NewBytecodeNode(int pc, int a, int b);
public:
  Decompiler(NewtonPackagePrinter &printer) : printer_( printer ) { }
  void print();
  void printSource();
  void printLiteral(int ix) {
    printer_.PrintRef(GetArraySlot(literals_, ix), 0, true);
  }
  void decompile(Ref ref);
  void solve();
  void generateAST(Ref instructions);
};

class PState {
public:
  Decompiler &dc;
  int indent { 0 };
  int precedence { 0 };

  PState(Decompiler &decompiler) : dc( decompiler ) { }
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
  virtual void print(int i) { indent(i); printf("<Error:base>\n"); }
  virtual int provides() { return kProvidesUnknown; }
  virtual int consumes() { return 0; }
  virtual ASTNode *simplify(int *changes) { (void)changes; return next; }
  void indent(int indent) { for (int i=0; i<indent; i++) printf("  "); }
  void unlink() { prev->next = next; next->prev = prev; prev = next = nullptr; }
  void replaceWith(ASTNode *nd) {
    prev->next = nd; next->prev = nd;
    nd->prev = prev; nd->next = next;
    prev = next = nullptr;
  }
  virtual void printSource(PState &p) { }
};

class ASTFirstNode : public ASTNode {
public:
  void print(int i) override { indent(i); printf("<first>\n"); }
  int provides() override { return kSpecialNode; }
  void printSource(PState &p) override { printf("begin\n"); indent(++p.indent);  }
};

class ASTLastNode : public ASTNode {
public:
  void print(int i) override { indent(i); printf("<last>\n"); }
  int provides() override { return kSpecialNode; }
  void printSource(PState &p) override { printf("end\n"); indent(--p.indent); }
};

class ASTJumpTarget : public ASTNode {
  std::vector<int> origins_;
public:
  ASTJumpTarget(int pc) : ASTNode(pc) { }
  void print(int i) override {
    indent(i);
    printf("<%04d: jumptarget from:", pc_);
    for (auto a: origins_) printf(" %d", a);
    printf(">\n");
  }
  int provides() override { return kJumpTarget; }
  void addOrigin(int origin) { origins_.push_back(origin); };
  bool containsOrigin(int o) { return std::find(origins_.begin(), origins_.end(), o) != origins_.end(); }
  bool containsOnly(int o) { return (origins_.size() == 1) && (origins_[0] == o); }
  void removeOrigin(int o) {
    auto it = std::find(origins_.begin(), origins_.end(), o);
    if (it != origins_.end()) origins_.erase(it);
  }
  bool empty() { return origins_.empty(); }

};

class ASTIfThenElseNode: public ASTNode {
  ASTNode *cond_ { nullptr };
  std::vector<ASTNode*> ifBranch_;
  std::vector<ASTNode*> elseBranch_;
public:
  ASTIfThenElseNode(int pc) : ASTNode(pc) { }
  void print(int i) override {
    indent(i); printf("<%04d: if>\n", pc_);
    cond_->print(i+1);
    indent(i); printf("<%04d: then>\n", pc_);
    for (auto *nd: ifBranch_) nd->print(i+1);
    indent(i); printf("<%04d: else>\n", pc_);
    for (auto *nd: elseBranch_) nd->print(i+1);
    indent(i); printf("<%04d: IfThenElse %zu %zu>\n", pc_, ifBranch_.size(), elseBranch_.size());
  }
  void setCond(ASTNode *nd) { cond_ = nd; }
  void addIf(ASTNode *nd) { ifBranch_.push_back(nd); }
  void addElse(ASTNode *nd) { elseBranch_.push_back(nd); }
  int provides() override { return 1; }
  void printSource(PState &p) override {
    printf("if "); cond_->printSource(p); printf("then begin\n"); indent(++p.indent);
    size_t last = ifBranch_.size()-1;
    for (size_t i = 0; i < last; i++) {
      ifBranch_[i]->printSource(p);
    }
    p.indent--;
    if (!ifBranch_.empty()) ifBranch_[last]->printSource(p);
    printf("end else begin\n"); indent(++p.indent);
    last = elseBranch_.size()-1;
    for (size_t i = 0; i < last; i++) {
      elseBranch_[i]->printSource(p);
    }
    p.indent--;
    if (!elseBranch_.empty()) elseBranch_[last]->printSource(p);
    printf("end;\n"); indent(p.indent);
  }

};

class ASTBytecodeNode : public ASTNode {
protected:
  int a_ = 0;
  int b_ = 0;
public:
  ASTBytecodeNode(int pc, int a, int b)
  : ASTNode(pc), a_(a), b_(b) { }
  void print(int i) override { indent(i); printf("<%04d: bytecode A=%d B=%d>\n", pc_, a_, b_); }
};

// (A=0, B=7): --
class AST_PopHandlers : public ASTBytecodeNode {
public:
  AST_PopHandlers(int pc, int a, int b) : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:PopHandlers>\n", pc_); }
  int provides() override { return kProvidesNone; }
};

// (A=3): -- literal
class AST_Push : public ASTBytecodeNode {
public:
  AST_Push(int pc, int a, int b) : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:Push literal[%d]>\n", pc_, b_); }
  int provides() override { return 1; }
  void printSource(PState &p) override { p.dc.printLiteral(b_); }
};

// (A=4, B=signed): -- value
class AST_PushConst : public ASTBytecodeNode {
public:
  AST_PushConst(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:PushConst value:%d>\n", pc_, b_); }
  int provides() override { return 1; }
  void printSource(PState &p) override { PrintObject(b_, 0); }
};

// (A=11): --
class AST_Branch : public ASTBytecodeNode {
public:
  AST_Branch(int pc, int a, int b) : ASTBytecodeNode(pc, a, b) { }
  int provides() override { return kBranch; }
  void print(int i) override {
    indent(i); printf("<%04d: BC:Branch pc:%d>\n", pc_, b_);
  }
};

//(A=0, B=3):  -- RCVR
class AST_PushSelf : public ASTBytecodeNode {
public:
  AST_PushSelf(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:PushSelf>\n", pc_); }
  int provides() override { return 1; }
};

// (A=14): -- value
class AST_FindVar : public ASTBytecodeNode {
public:
  AST_FindVar(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:FindVar literal[%d]>\n", pc_, b_); }
  int provides() override { return 1; }
  void printSource(PState &p) override { p.dc.printLiteral(b_); }
};

// (A=15): -- value
class AST_GetVar : public ASTBytecodeNode {
public:
  AST_GetVar(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:GetVar local[%d]>\n", pc_, b_); }
  int provides() override { return 1; }
};

class AST_Consume1 : public ASTBytecodeNode {
protected:
  ASTNode *in_ { nullptr };
public:
  AST_Consume1(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  void printChildren(int i) {
    if (in_) in_->print(i);
  }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: Consume1>\n", pc_);
  }
  int provides() override { if (in_) return 1; else return kProvidesUnknown; }
  int consumes() override { return 1; }
  ASTNode *simplify(int *changes) override {
    if (in_) return next; // nothing more to do
    if (prev->provides() == 1) {
      in_ = prev; prev->unlink();
      (*changes)++;
      return this;
    }
    return next;
  }
};

// (A=12): value --
class AST_BranchIfTrue : public AST_Consume1 {
public:
  AST_BranchIfTrue(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kBranchIfTrue; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:BranchIfTrue pc:%d>\n", pc_, b_);
  }
  ASTNode *simplify(int *changes) override {
    return next;
  }
};

// (A=13): value --
// NOTE: if...then...else... creates the same bytecode as "and"
// NOTE: "if not..." generates "not" and "BranchIfFalse" and is not optimized into "BranchIfTrue"
// NOTE: BranchIfTrue is generated by an "or" operation
// TODO: if the result of the 'if' operation is used, the last BC of both
// branches provides 1 and both branches must exist
// If the result is not used, all statements return 0, and we may not have an 'else' branch!
class AST_BranchIfFalse : public AST_Consume1 {
public:
  AST_BranchIfFalse(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kBranchIfFalse; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:BranchIfFalse pc:%d>\n", pc_, b_);
  }
  ASTNode *simplify(int *changes) override {
    // p(1) / AST_BranchIfFalse(A) / n * p(0) / p(1) / AST_Branch(B) / jumptarget(A) / n * p(0) / p(1) / jumptarget(B)
    // -- Resolve the condition first
    if (!in_) return AST_Consume1::simplify(changes);
    // -- We have the source of the condition. Now check if the rest matches if...then...else...
    int numIf = 0;
    int numElse = 0;
    ASTNode *nd = next;
    // ---- Skip any number of statements
    while (nd->provides() == kProvidesNone) { numIf++; nd = nd->next; }
    // ---- There must be none or one expression
//    if (nd->provides() != 1) return next;
//    nd = nd->next;
    if (nd->provides() == 1) { numIf++; nd = nd->next; }
    // ---- Followed by an unconditional branch
    if (nd->provides() != kBranch) return next;
    int branch_pc = nd->pc();
    nd = nd->next;
    // ---- Followed by a Jump Target with this node as the origin
    if (nd->provides() != kJumpTarget) return next;
    ASTJumpTarget *jt = static_cast<ASTJumpTarget*>(nd);
    if (!jt->containsOnly(pc_)) return next;
    nd = nd->next;
    // ---- We are in the 'else' branch: Skip any number of statements
    while (nd->provides() == kProvidesNone) { numElse++; nd = nd->next; }
    // ---- There must be none or one expression
//    if (nd->provides() != 1) return next;
//    nd = nd->next;
    if (nd->provides() == 1) { numElse++; nd = nd->next; }
    // ---- Followed by a Jump Target with the unconditional jump as the origin
    if (nd->provides() != kJumpTarget) return next;
    ASTJumpTarget *jt2 = static_cast<ASTJumpTarget*>(nd);
    if (!jt2->containsOrigin(branch_pc)) return next;

    // -- If we made it here, this is an if...then...else... construct
    ASTIfThenElseNode *ite = new ASTIfThenElseNode(pc());
    ite->setCond(in_);
    for (int i=0; i<numIf; i++) { ite->addIf(next); next->unlink(); }
    next->unlink(); // branch
    next->unlink(); // jump target
    for (int i=0; i<numElse; i++) { ite->addElse(next); next->unlink(); }
    // ---- Unlink this only if there are no more origins!
    jt2->removeOrigin(branch_pc);
    if (jt2->empty()) jt2->unlink(); // jump target
    replaceWith(ite); // replace this node with the ite node
    (*changes)++;
    return ite->next;
  }
};

// TODO: the very last return probably doesn't need to be printed
// TODO: return NIL is implied if there is no return statement
// (A=0, B=2): --
class AST_Return : public AST_Consume1 {
public:
  AST_Return(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { return kProvidesNone; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Return>\n", pc_);
  }
  void printSource(PState &p) override {
    if (in_) { printf("return "); in_->printSource(p); printf(";\n"); }
  }

};

// (A=0, B=0): value --
class AST_Pop : public AST_Consume1 {
public:
  AST_Pop(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Pop>\n", pc_);
  }
  void printSource(PState &p) override {
    if (in_) { in_->printSource(p); printf(";\n"); indent(p.indent); }
  }
};

// (A=0, B=1): x -- x x
class AST_Dup : public AST_Consume1 {
public:
  AST_Dup(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  // TODO: provides(2) does not match any consumers!
  // NOTE: we must find an actuall use case and the corresponding source code
  // NOTE: it may make sense to split this into a dup1 and dup2 node?!
  int provides() override { if (in_) return 2; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Dup>\n", pc_);
  }
};

// (A=0, B=4): func -- closure
class AST_SetLexScope : public AST_Consume1 {
public:
  AST_SetLexScope(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:AST_SetLexScope>\n", pc_);
  }
};

// (A=20): value --
class AST_SetVar : public AST_Consume1 {
public:
  AST_SetVar(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:SetVar local[%d]>\n", pc_, b_);
  }
};

// (A=21): value --
class AST_FindAndSetVar : public AST_Consume1 {
public:
  AST_FindAndSetVar(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:FindAndSetVar literal[%d]>\n", pc_, b_);
  }
  void printSource(PState &p) override {
    if (in_) { p.dc.printLiteral(b_); printf(" := "); in_->printSource(p); printf(";\n"); indent(p.indent); }
  }
};

// (A=24, B=5): value -- value
class AST_Not : public AST_Consume1 {
public:
  AST_Not(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Not>\n", pc_);
  }
};

// (A=24, B=18): object -- length
class AST_Length : public AST_Consume1 {
public:
  AST_Length(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Length>\n", pc_);
  }
  void printSource(PState &p) override {
    if (in_) { printf("Length("); in_->printSource(p); printf(")"); }
  }
};

// (A=24, B=19): object -- clone
class AST_Clone : public AST_Consume1 {
public:
  AST_Clone(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Clone>\n", pc_);
  }
  void printSource(PState &p) override {
    if (in_) {
      printf("Clone(");
      in_->printSource(p);
      printf(")");
    }
  }
};

// (A=24, B=22): array -- string
class AST_Stringer : public AST_Consume1 {
public:
  AST_Stringer(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Stringer>\n", pc_);
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
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:ClassOf>\n", pc_);
  }
  void printSource(PState &p) override {
    if (in_) {
      printf("ClassOf(");
      in_->printSource(p);
      printf(")");
    }
  }
};


// (A=22) addend -- addend value
class AST_IncrVar : public AST_Consume1 {
public:
  AST_IncrVar(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return 2; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:IncrVar local[%d]>\n", pc_, b_);
  }
};

// (A=0, B=5) iterator --
class AST_IterNext : public AST_Consume1 {
public:
  AST_IterNext(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kProvidesNone; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:IterNext>\n", pc_);
  }
};

// (A=0, B=6) iterator -- done
class AST_IterDone : public AST_Consume1 {
public:
  AST_IterDone(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return 1; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:IterDone>\n", pc_);
  }
};

class AST_Consume2 : public ASTBytecodeNode {
protected:
  ASTNode *in1_ { nullptr };
  ASTNode *in2_ { nullptr };
public:
  AST_Consume2(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  void printChildren(int i) {
    if (in1_) in1_->print(i);
    if (in2_) in2_->print(i);
  }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: Consume2>\n", pc_);
  }
  int provides() override { if (in1_ && in2_) return 1; else return kProvidesUnknown; }
  int consumes() override { return 2; }
  ASTNode *simplify(int *changes) override {
    if (in1_ && in2_) return next; // nothing more to do
    if ((prev->provides() == 1) && (prev->prev->provides() == 1)) {
      in2_ = prev; prev->unlink();
      in1_ = prev; prev->unlink();
      (*changes)++;
      return this;
    }
    return next;
  }
};

// (A=17, B=0xFFFF): size class -- array
class AST_NewArray : public AST_Consume2 {
public:
  AST_NewArray(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:NewArray>\n", pc_);
  }
};

// (A=18): object pathExpr -- value
class AST_GetPath : public AST_Consume2 {
public:
  AST_GetPath(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:GetPath b:%d>\n", pc_, b_);
  }
};

// (A=24, B=4): num1 num2 -- result
class AST_Equals : public AST_Consume2 {
public:
  AST_Equals(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Equals>\n", pc_);
  }
};

// (A=24, B=6): num1 num2 -- result
class AST_NotEquals : public AST_Consume2 {
public:
  AST_NotEquals(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:NotEquals>\n", pc_);
  }
};

// (A=24, B=10): num1 num2 -- result
class AST_LessThan : public AST_Consume2 {
public:
  AST_LessThan(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:LessThan>\n", pc_);
  }
};

// (A=24, B=11): num1 num2 -- result
class AST_GreaterThan : public AST_Consume2 {
public:
  AST_GreaterThan(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:GreaterThan>\n", pc_);
  }
};

// (A=24, B=12): num1 num2 -- result
class AST_GreaterOrEqual : public AST_Consume2 {
public:
  AST_GreaterOrEqual(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:GreaterOrEqual>\n", pc_);
  }
};

// (A=24, B=13): num1 num2 -- result
class AST_LessOrEqual : public AST_Consume2 {
public:
  AST_LessOrEqual(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:LessOrEqual>\n", pc_);
  }
};

// (A=24, B=0): num1 num2 -- result
class AST_Add : public AST_Consume2 {
public:
  AST_Add(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Add>\n", pc_);
  }
  void printSource(PState &p) override {
    if (in1_ && in2_) {
      int ppp = p.precedence;
      bool brackets = (p.precedence > 6);
      p.precedence = 6;
      if (brackets) printf("(");
      in1_->printSource(p); printf(" + "); in2_->printSource(p);
      if (brackets) printf(")");
      p.precedence = ppp;
    }
  }
};

// (A=24, B=1): num1 num2 -- result
class AST_Sub : public AST_Consume2 {
public:
  AST_Sub(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Sub>\n", pc_);
  }
};

// (A=24, B=7): num1 num2 -- result
class AST_Multiply : public AST_Consume2 {
public:
  AST_Multiply(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Multiply>\n", pc_);
  }
  void printSource(PState &p) override {
    if (in1_ && in2_) {
      int ppp = p.precedence;
      bool brackets = (p.precedence > 7);
      p.precedence = 7;
      if (brackets) printf("(");
      in1_->printSource(p); printf(" * "); in2_->printSource(p);
      if (brackets) printf(")");
      p.precedence = ppp;
    }
  }
};

// (A=24, B=8): num1 num2 -- result
class AST_Divide : public AST_Consume2 {
public:
  AST_Divide(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Divide>\n", pc_);
  }
};

// (A=24, B=9): num1 num2 -- result
class AST_Div : public AST_Consume2 {
public:
  AST_Div(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Div>\n", pc_);
  }
};

// (A=24, B=14): num1 num2 -- result
class AST_BitAnd : public AST_Consume2 {
public:
  AST_BitAnd(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:BitAnd>\n", pc_);
  }
};

// (A=24, B=15): num1 num2 -- result
class AST_BitOr : public AST_Consume2 {
public:
  AST_BitOr(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:BitOr>\n", pc_);
  }
};

// (A=24, B=16): num1 num2 -- result
class AST_BitNot : public AST_Consume2 {
public:
  AST_BitNot(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:BitNot>\n", pc_);
  }
};

// (A=24, B=2): object index -- element
class AST_ARef : public AST_Consume2 {
public:
  AST_ARef(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:ARef>\n", pc_);
  }
  void printSource(PState &p) override {
    if (in1_ && in2_) {
      in1_->printSource(p);
      printf("[");
      in2_->printSource(p);
      printf("]");
    }
  }
};

// (A=24, B=20): object class -- object
class AST_SetClass : public AST_Consume2 {
public:
  AST_SetClass(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:SetClass>\n", pc_);
  }
};

// (A=24, B=21): array object -- object
class AST_AddArraySlot : public AST_Consume2 {
public:
  AST_AddArraySlot(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:AddArraySlot>\n", pc_);
  }
  void printSource(PState &p) override {
    if (in1_ && in2_) {
      printf("AddArraySlot(");
      in1_->printSource(p);
      printf(", ");
      in2_->printSource(p);
      printf(")");
    }
  }
};

// (A=24, B=23): object pathExpr -- result
class AST_HasPath : public AST_Consume2 {
public:
  AST_HasPath(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:HasPath>\n", pc_);
  }
};

// (A=24, B=17): object deeply -- iterator
class AST_NewIter : public AST_Consume2 {
public:
  AST_NewIter(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:NewIter>\n", pc_);
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
  void printChildren(int i) {
    if (object_) object_->print(i);
    if (path_) path_->print(i);
    if (value_) value_->print(i);
  }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:SetARef>\n", pc_);
  }
  int provides() override {
    if (object_ && path_ && value_)
      return (b_ == 0) ? kProvidesNone : 1;
    else
      return kProvidesUnknown;
  }
  int consumes() override { return 3; }
  ASTNode *simplify(int *changes) override {
    if (object_ && path_ && value_) return next; // nothing more to do
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
  void printChildren(int i) {
    if (object_) object_->print(i);
    if (index_) index_->print(i);
    if (element_) element_->print(i);
  }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:SetARef>\n", pc_);
  }
  int provides() override { if (object_ && index_ && element_) return kProvidesNone; else return kProvidesUnknown; }
  int consumes() override { return 3; }
  ASTNode *simplify(int *changes) override {
    if (object_ && index_ && element_) return next; // nothing more to do
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
  void printChildren(int i) {
    if (incr_) incr_->print(i);
    if (index_) index_->print(i);
    if (limit_) limit_->print(i);
  }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BranchLoop pc:%d>\n", pc_, b_);
  }
  int provides() override { if (incr_ && index_ && limit_) return kProvidesNone; else return kProvidesUnknown; }
  int consumes() override { return 3; }
  ASTNode *simplify(int *changes) override {
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
};


class AST_ConsumeN : public ASTBytecodeNode {
protected:
  int numIns_;
  std::vector<ASTNode *> ins_;
public:
  AST_ConsumeN(int pc, int a, int b, int n)
  : ASTBytecodeNode(pc, a, b), numIns_(n) { }
  void printChildren(int i) {
    for (auto &nd: ins_) nd->print(i);
  }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: ConsumeN>\n", pc_);
  }
  int provides() override { if (ins_.empty()) return kProvidesUnknown; else return 1; }
  int consumes() override { return numIns_; }
  ASTNode *simplify(int *changes) override {
    if (!ins_.empty()) return next; // nothing more to do
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
};

// (A=5): arg1 arg2 ... argN name -- result
class AST_Call : public AST_ConsumeN {
public:
  AST_Call(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Call nArgs:%d>\n", pc_, numIns_-1);
  }
  // TODO: the following special functions (reserved words) need to be written "a op b"
  // div, mod, <<, >>, (note the precedence! 7, 7, 8, 8)
  // HasVar(a) can also be written as "a exists", but both are legal
  void printSource(PState &p) override {
    if (!ins_.empty()) {
      ins_[numIns_-1]->printSource(p);
      printf("(");
      for (int i=0; i<numIns_-2; i++) { ins_[i]->printSource(p); printf(", "); }
      if (numIns_ > 1) ins_[numIns_-2]->printSource(p);
      printf(")");
    }
  }

};

// (A=6): arg1 arg2 ... argN name receiver -- result
// call ... with (...)
class AST_Invoke : public AST_ConsumeN {
public:
  AST_Invoke(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+2) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Invoke nArgs:%d>\n", pc_, numIns_);
  }
};

// (A=7): arg1 arg2 ... argN name receiver -- result
class AST_Send : public AST_ConsumeN {
public:
  AST_Send(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+2) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Send nArgs:%d>\n", pc_, numIns_);
  }
};

// (A=8): arg1 arg2 ... argN name receiver -- result
class AST_SendIfDefined : public AST_ConsumeN {
public:
  AST_SendIfDefined(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+2) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:SendIfDefined nArgs:%d>\n", pc_, numIns_);
  }
};

// (A=9): arg1 arg2 ... argN name -- result
// inherited:Print(3);
class AST_Resend : public AST_ConsumeN {
public:
  AST_Resend(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Resend nArgs:%d>\n", pc_, numIns_);
  }
};

// (A=10): arg1 arg2 ... argN name -- result
// inherited:?Print(3);
class AST_ResendIfDefined : public AST_ConsumeN {
public:
  AST_ResendIfDefined(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:ResendIfDefined nArgs:%d>\n", pc_, numIns_);
  }
};

// (A=16, B=numVal) val1 val2 ... valN map -- frame
class AST_MakeFrame : public AST_ConsumeN {
public:
  AST_MakeFrame(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:MakeFrame nArgs:%d>\n", pc_, numIns_-1);
  }
};

// (A=17, B=numVal):  val1 val2 ... valN class -- array): arg1 arg2 ... argN name -- result
class AST_MakeArray : public AST_ConsumeN {
public:
  AST_MakeArray(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:MakeArray nArgs:%d>\n", pc_, numIns_-1);
  }
};

// (A=25): sym1 pc1 sym2 pc2 ... symN pcN --
class AST_NewHandler : public AST_ConsumeN {
public:
  AST_NewHandler(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+1) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:NewHandler nArgs:%d>\n", pc_, numIns_-1);
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
    numLocals_ = Length(argFrame) - 3 - numArgs_;
    // Make the list of names of the locals
    Ref map = ((FrameObject *)ObjectPtr(argFrame))->map;
    locals_ = MakeArray(numLocals_);
    for (int i=0; i<numLocals_; i++) {
      SetArraySlot(locals_, i, GetArraySlot(map, i + 4 + numArgs_));
    }
    // Make the list of names of the arguments
    args_ = MakeArray(numArgs_);
    for (int i=0; i<numArgs_; i++) {
      SetArraySlot(args_, i, GetArraySlot(map, i + 4));
    }
  } else if (klass == kPlainFuncClass) {
    nos_ = 2;
    Ref numArgs = GetFrameSlot(ref, SYMA(numArgs));
    numArgs_ = static_cast<int>((numArgs>>2) & 0x00003fff);
    numLocals_ = static_cast<int>(numArgs >> 18);
    // Make up names for the locals:
    locals_ = MakeArray(numLocals_);
    for (int i=0; i<numLocals_; i++) {
      char buf[32];
      snprintf(buf, 30, "loc_%04d", i);
      SetArraySlot(locals_, i, MakeSymbol(buf));
    }
    // Make the list of names of the arguments
    args_ = MakeArray(numArgs_);
    for (int i=0; i<numArgs_; i++) {
      char buf[32];
      snprintf(buf, 30, "arg_%04d", i);
      SetArraySlot(args_, i, MakeSymbol(buf));
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
    case 7: return new AST_Send(pc, a, b);
    case 8: return new AST_SendIfDefined(pc, a, b);
    case 9: return new AST_Resend(pc, a, b);
    case 10: return new AST_ResendIfDefined(pc, a, b);
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
        case 0: return new AST_Add(pc, a, b);
        case 1: return new AST_Sub(pc, a, b);
        case 2: return new AST_ARef(pc, a, b);
        case 3: return new AST_SetARef(pc, a, b);
        case 4: return new AST_Equals(pc, a, b);
        case 5: return new AST_Not(pc, a, b);
        case 6: return new AST_NotEquals(pc, a, b);
        case 7: return new AST_Multiply(pc, a, b);
        case 8: return new AST_Divide(pc, a, b);
        case 9: return new AST_Div(pc, a, b);
        case 10: return new AST_LessThan(pc, a, b);
        case 11: return new AST_GreaterThan(pc, a, b);
        case 12: return new AST_GreaterOrEqual(pc, a, b);
        case 13: return new AST_LessOrEqual(pc, a, b);
        case 14: return new AST_BitAnd(pc, a, b);
        case 15: return new AST_BitOr(pc, a, b);
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
      nd = nd->simplify(&changes);
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
  for (ASTNode *nd = first_; nd; nd = nd->next) {
    nd->print(0);
  }
  puts("----------------");
}

void Decompiler::printSource()
{
  PState pState(*this);
  puts("---- Source code -------");
  for (ASTNode *nd = first_; nd; nd = nd->next) {
    nd->printSource(pState);
  }
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
 - `numArgs`: bits 31 to 16 are the number of locals
 - `argFrame`: always `nil` in this format, bits 15 to 0 are the number of arguments
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
  d.printSource();
  return noErr;
}

