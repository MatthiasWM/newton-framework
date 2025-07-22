
/*
 File:    MattsDecompiler.cc

 Decompile a NewtonScript function.

 Written by:  Matt, 2025.
 */

#include "MattsDecompiler.h"


#if 0

switch (a) {
  case 0:
    switch (b) {
      case 0: bc.bc = BC::Pop; break;
      case 1: bc.bc = BC::Dup; break;
      case 2: bc.bc = BC::Return; break;
      case 3: bc.bc = BC::PushSelf; break;
      case 4: bc.bc = BC::SetLexScope; break;
      case 5: bc.bc = BC::IterNext; break;
      case 6: bc.bc = BC::IterDone; break;
      case 7: bc.bc = BC::PopHandlers; break;
      default:
        std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
        break;
    }
    break;
  case 3: bc.bc = BC::Push; bc.arg = (int16_t)b; break;
  case 4: bc.bc = BC::PushConst; bc.arg = (int16_t)b; break;
  case 5: bc.bc = BC::Call; bc.arg = (int16_t)b; break;
  case 6: bc.bc = BC::Invoke; bc.arg = (int16_t)b; break;
  case 7: bc.bc = BC::Send; bc.arg = (int16_t)b; break;
  case 8: bc.bc = BC::SendIfDefined; bc.arg = (int16_t)b; break;
  case 9: bc.bc = BC::Resend; bc.arg = (int16_t)b; break;
  case 10: bc.bc = BC::ResendIfDefined; bc.arg = (int16_t)b; break;
  case 11: bc.bc = BC::Branch; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
  case 12: bc.bc = BC::BranchIfTrue; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
  case 13: bc.bc = BC::BranchIfFalse; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
  case 14: bc.bc = BC::FindVar; bc.arg = (int16_t)b; break;
  case 15: bc.bc = BC::GetVar; bc.arg = (int16_t)b; break;
  case 16: bc.bc = BC::MakeFrame; bc.arg = (int16_t)b; break;
  case 17: bc.bc = BC::MakeArray; bc.arg = (int16_t)b; break;
  case 18: bc.bc = BC::GetPath; bc.arg = (int16_t)b; break;
  case 19: bc.bc = BC::SetPath; bc.arg = (int16_t)b; break;
  case 20: bc.bc = BC::SetVar; bc.arg = (int16_t)b; break;
  case 21: bc.bc = BC::FindAndSetVar; bc.arg = (int16_t)b; break;
  case 22: bc.bc = BC::IncrVar; bc.arg = (int16_t)b; break;
  case 23: bc.bc = BC::BranchLoop; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
  case 24:
    switch (b) {
      case 0: bc.bc = BC::Add; break;
      case 1: bc.bc = BC::Subtract; break;
      case 2: bc.bc = BC::ARef; break;
      case 3: bc.bc = BC::SetARef; break;
      case 4: bc.bc = BC::Equals; break;
      case 5: bc.bc = BC::Not; break;
      case 6: bc.bc = BC::NotEquals; break;
      case 7: bc.bc = BC::Multiply; break;
      case 8: bc.bc = BC::Divide; break;
      case 9: bc.bc = BC::Div; break;
      case 10: bc.bc = BC::LessThan; break;
      case 11: bc.bc = BC::GreaterThan; break;
      case 12: bc.bc = BC::GreaterOrEqual; break;
      case 13: bc.bc = BC::LessOrEqual; break;
      case 14: bc.bc = BC::BitAnd; break;
      case 15: bc.bc = BC::BitOr; break;
      case 16: bc.bc = BC::BitNot; break;
      case 17: bc.bc = BC::NewIter; break;
      case 18: bc.bc = BC::Length; break;
      case 19: bc.bc = BC::Clone; break;
      case 20: bc.bc = BC::SetClass; break;
      case 21: bc.bc = BC::AddArraySlot; break;
      case 22: bc.bc = BC::Stringer; break;
      case 23: bc.bc = BC::HasPath; break;
      case 24: bc.bc = BC::ClassOf; break;
      default:
        std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
        break;
    }
    break;
  case 25: bc.bc = BC::NewHandler; bc.arg = (int16_t)b;
    for (int i=0; i<b; i++) {
      auto &exc_pc_bc = func[pc-2*i-1];
      if ((exc_pc_bc.bc == BC::PushConst) && ((exc_pc_bc.arg&3)==0)) {
        int exc_pc = exc_pc_bc.arg >> 2;
        func[pc_map[exc_pc]].references++;
      } else {
        std::cout << "ERROR: Transcoding new_handler at "<<pc<<": unexpected bytecodes.\n";
      }
    }
    break;
  default:
    std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
    break;
}

#endif

constexpr int kProvidesNone = 0;      // The node is defined enough to know that there is nothing on the stack
constexpr int kProvidesUnknown = -1;  // We don't know yet how many values will be on the stack
constexpr int kSpecialNode = -2;      // First or Last node. Stop searching.
constexpr int kJumpTarget = -3;
constexpr int kBranch = -4;
constexpr int kBranchIfFalse = -5;

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
};

class ASTFirstNode : public ASTNode {
public:
  void print(int i) override { indent(i); printf("<first>\n"); }
  int provides() override { return kSpecialNode; }
};

class ASTLastNode : public ASTNode {
public:
  void print(int i) override { indent(i); printf("<last>\n"); }
  int provides() override { return kSpecialNode; }
};

class ASTJumpTarget : public ASTNode {
  std::vector<int> origins_;
public:
  ASTJumpTarget(int pc) : ASTNode(pc) { }
  void print(int i) override {
    indent(i);
    printf("<%04d: jumptarget fwd:", pc_);
    for (auto a: origins_) printf(" %d", a);
    printf(">\n");
  }
  int provides() override { return kJumpTarget; }
  void addOrigin(int origin) { origins_.push_back(origin); };
  bool containsOrigin(int o) { return std::find(origins_.begin(), origins_.end(), o) != origins_.end(); }
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
};

class ASTBytecodeNode : public ASTNode {
protected:
  int a_ = 0;
  int b_ = 0;
public:
  ASTBytecodeNode(int pc, int a, int b)
  : ASTNode(pc), a_(a), b_(b) { }
  void print(int i) override { indent(i); printf("<%04d: bytecode %d %d>\n", pc_, a_, b_); }
};

// (A=3): -- literal
class AST_Push : public ASTBytecodeNode {
public:
  AST_Push(int pc, int a, int b) : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:Push %d>\n", pc_, b_); }
  int provides() override { return 1; }
};

// (A=4, B=signed): -- value
class AST_PushConst : public ASTBytecodeNode {
public:
  AST_PushConst(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:PushConst %d>\n", pc_, b_); }
  int provides() override { return 1; }
};

// (A=11): --
class AST_Branch : public ASTBytecodeNode {
public:
  AST_Branch(int pc, int a, int b) : ASTBytecodeNode(pc, a, b) { }
  int provides() override { return kBranch; }
  void print(int i) override {
    indent(i); printf("<%04d: BC:Branch>\n", pc_);
  }
};

//(A=0, b=3):  -- RCVR
class AST_PushSelf : public ASTBytecodeNode {
public:
  AST_PushSelf(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:PushSelf %d>\n", pc_, b_); }
  int provides() override { return 1; }
};

// (A=14): -- value
class AST_FindVar : public ASTBytecodeNode {
public:
  AST_FindVar(int pc, int a, int b)
  : ASTBytecodeNode(pc, a, b) { }
  void print(int i) override { indent(i); printf("<%04d: BC:FindVar %d>\n", pc_, b_); }
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

// (A=13): value --
class AST_BranchIfFalse : public AST_Consume1 {
public:
  AST_BranchIfFalse(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { if (in_) return kBranchIfFalse; else return kProvidesUnknown; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:BranchIfFalse>\n", pc_);
  }
  ASTNode *simplify(int *changes) override {
    // p(1) / AST_BranchIfFalse(A) / n * p(0) / p(1) / AST_Branch(B) / jumptarget(A) / n * p(0) / p(1) / jumptarget(B)
    // -- Resolve the condition first
    if (!in_) return AST_Consume1::simplify(changes);
    // -- We have the source of the condition. Now check if the rest matches if...then...else...
    int numIf = 1;
    int numElse = 1;
    ASTNode *nd = next;
    // ---- Skip any number of statements
    while (nd->provides() == kProvidesNone) { numIf++; nd = nd->next; }
    // ---- There must be exactly one expression
    if (nd->provides() != 1) return next;
    nd = nd->next;
    // ---- Followed by an unconditional branch
    if (nd->provides() != kBranch) return next;
    int branch_pc = nd->pc();
    nd = nd->next;
    // ---- Followed by a Jump Target with this node as the origin
    if (nd->provides() != kJumpTarget) return next;
    ASTJumpTarget *jt = static_cast<ASTJumpTarget*>(nd);
    if (!jt->containsOrigin(pc_)) return next; // TODO: and no other?!
    nd = nd->next;
    // ---- We are in the 'else' branch: Skip any number of statements
    while (nd->provides() == kProvidesNone) { numElse++; nd = nd->next; }
    // ---- There must be exactly one expression
    if (nd->provides() != 1) return next;
    nd = nd->next;
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
};

// (A=20): value --
class AST_SetVar : public AST_Consume1 {
public:
  AST_SetVar(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { return kProvidesNone; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:SetVar>\n", pc_);
  }
};

// (A=21): value --
class AST_FindAndSetVar : public AST_Consume1 {
public:
  AST_FindAndSetVar(int pc, int a, int b) : AST_Consume1(pc, a, b) { }
  int provides() override { return kProvidesNone; }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:FindAndSetVar %d>\n", pc_, b_);
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

// (A=24, B=0): num1 num2 -- result
class AST_Add : public AST_Consume2 {
public:
  AST_Add(int pc, int a, int b) : AST_Consume2(pc, a, b) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Add>\n", pc_);
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
    indent(i); printf("<%04d: BC:Call %d>\n", pc_, numIns_);
  }
};

// (A=7): arg1 arg2 ... argN name receiver -- result
class AST_Send : public AST_ConsumeN {
public:
  AST_Send(int pc, int a, int b)
  : AST_ConsumeN(pc, a, b, b+2) { }
  void print(int i) override {
    printChildren(i+1);
    indent(i); printf("<%04d: BC:Send %d>\n", pc_, numIns_);
  }
};

// (A:13) AST_BranchIfFalse
// (A:11) AST_Branch
// ASTIfTheElse:
// p(1) / AST_BranchIfFalse(A) / n * p(0) / p(1) / AST_Branch(B) / jumptarget(A) / n * p(0) / p(1) / jumptarget(B)

// -----------------------------------------------------------------------------

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
  void decompile(Ref ref);
  void solve();
  void generateAST(Ref instructions);
};


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
        case 2: return new AST_Return(pc, a, b);
        case 3: return new AST_PushSelf(pc, a, b);
      };
      break;
    case 3: return new AST_Push(pc, a, b);
    case 4: return new AST_PushConst(pc, a, b);
    case 5: return new AST_Call(pc, a, b);
    case 7: return new AST_Send(pc, a, b);
    case 11: return new AST_Branch(pc, a, b);
    case 13: return new AST_BranchIfFalse(pc, a, b);
    case 14: return new AST_FindVar(pc, a, b);
    case 20: return new AST_SetVar(pc, a, b);
    case 21: return new AST_FindAndSetVar(pc, a, b);
    case 24:
      switch (b) {
        case 0: return new AST_Add(pc, a, b);
        case 1: return new AST_Sub(pc, a, b);
      }
      break;
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

}

void Decompiler::print()
{
  puts("---- AST -------");
  for (ASTNode *nd = first_; nd; nd = nd->next) {
    nd->print(0);
  }
  puts("----------------");
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
  return noErr;
}

