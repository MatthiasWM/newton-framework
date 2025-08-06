
/*
 File:    Matt/ASTDataFlow.cc

 Matt's decompiler Abstract Syntax Tree.
 Data Flow nodes.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTDataFlow.h"
#include "Matt/ASTControlFlow.h"
#include "Matt/Decompiler.h"
#include "Matt/ObjectPrinter.h"

#include "Frames/Frames.h"

using namespace ast;

#pragma mark - BCPush

bool BCPush::IsSymbol()
{
  return ::IsSymbol(dec.GetLiteral(b_));
}

void BCPush::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLiteral(b_);
}

#pragma mark - BCPushConst

bool BCPushConst::IsNIL()
{
  return (b_ == NILREF);
}

void BCPushConst::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.PrintConstant(b_);
}

#pragma mark - BCPushSelf

void BCPushSelf::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("self");
}

#pragma mark - BCFindVar

void BCFindVar::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLiteralAsTag(b_);
}

#pragma mark - BCGetVar

void BCGetVar::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLocal(b_);
}

#pragma mark - BCPop

void BCPop::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  in_->Print();
}

Node *BCPop::Resolve(Pass pass) {
  if (Resolved()) return next;
  if (pass == Pass::DataFlow) {
    // Remove the useless sequence "push-const *, pop" before it is picked
    // up in the compress path.
    if (dynamic_cast<BCPushConst*>(prev)) {
      Node *nextNode = next;
      prev->Unlink();
      this->Unlink();
      return nextNode;
    }
    // TODO: Remove the useless sequence "find-and-set-var a, find-var a, pop"
    return super::Resolve(pass);
  }
  return next;
}

#pragma mark - BCDup

void BCDup::Print(uint32_t flags) {
  /* if (!Resolved()) */ return PrintNode(false);
}

#pragma mark - BCSetVar

void BCSetVar::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLocal(b_);
  dec.p.Printf(" := ");
  in_->Print();
}

#pragma mark - BCFindAndSetVar

void BCFindAndSetVar::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLiteralAsTag(b_);
  dec.p.Printf(" := ");
  in_->Print();
}

#pragma mark - BCNot

void BCNot::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  int ppp = dec.precedence;
  bool parentheses = (dec.precedence > 2);
  dec.precedence = 2;
  if (parentheses) dec.p.Printf("(");
  dec.p.Printf("not ");
  in_->Print();
  if (parentheses) dec.p.Printf(")");
  dec.precedence = ppp;
}

#pragma mark - BCLength

void BCLength::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("length(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - BCClone

void BCClone::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("clone(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - BCStringer

void BCStringer::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  BCMakeArray *array = dynamic_cast<BCMakeArray*>(in_);
  if (!array) return PrintNode(false);
  array->PrintAsStringer(flags);
}

#pragma mark - BCClassOf

void BCClassOf::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("ClassOf(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - BCBitNot

void BCBitNot::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("bNot(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - BinaryExpression

Node *BinaryExpression::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  if ((prev->IsExpr()) && (prev->prev->IsExpr())) {
    in2_ = prev->Unlink();
    in1_ = prev->Unlink();
    dec.numASTChanges++;
    return this;
  } else {
    return next;
  }
}

#pragma mark - BinaryFunction

void BinaryFunction::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  int ppp = dec.precedence;
  dec.precedence = 0;
  dec.p.Printf("%s(", func_);
  in1_->Print();
  dec.p.Printf(", ");
  in2_->Print();
  dec.p.Printf(")");
  dec.precedence = ppp;
}

#pragma mark - BinaryOperator

void BinaryOperator::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  int ppp = dec.precedence;
  bool parentheses = (dec.precedence > precedence_);
  dec.precedence = precedence_;
  if (parentheses) dec.p.Print("(");
  in1_->Print(); dec.p.Printf(" %s ", op_); in2_->Print();
  if (parentheses) dec.p.Print(")");
  dec.precedence = ppp;
}

#pragma mark - BCNewArray

// size class -- array

// if the class is 'array, generate `Array(size, nil)`
// with a custom class, generate `SetClass(Array(size, nil), 'class)`
void BCNewArray::Print(uint32_t flags)
{
  if (!Resolved()) return PrintNode(false);

  // Check if the array class is defined and not 'array
  bool setClass = false;
  Ref klass = NILREF;
  Node *klassNd = in2_;
  if (klassNd->a() == 3) { // BCPush
    int literalIx = klassNd->b();
    klass = dec.GetLiteral(literalIx);
    if (::IsSymbol(klass) && (SymbolCompare(klass, SYMA(array)) != 0)) {
      dec.p.Print("SetClass(");
      setClass = true;
    }
  }
  // Print all the members of the new array
  dec.p.Print("Array(");
  in1_->Print();
  dec.p.Print(", nil)");
  // Finish the 'SetClass' if we needed it
  if (setClass) {
    dec.p.Print(", ");
    in2_->Print();
    dec.p.Print(")");
  }
}

#pragma mark - BCGetPath

void BCGetPath::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  in1_->Print();
  dec.p.Printf(".");
  PrintPathExpr(in2_);
}

#pragma mark - BCARef

void BCARef::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  in1_->Print();
  dec.p.Printf("["); in2_->Print(); dec.p.Printf("]");
}

#pragma mark - BCSetClass

void BCSetClass::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("SetClass(");
  in1_->Print(); dec.p.Printf(", ");
  in2_->Print(); dec.p.Printf(")");
}

#pragma mark - BCAddArraySlot

void BCAddArraySlot::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("AddArraySlot(");
  in1_->Print(); dec.p.Printf(", ");
  in2_->Print(); dec.p.Printf(")");
}

#pragma mark - BCHasPath

void BCHasPath::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("HasPath(");
  in1_->Print(); dec.p.Printf(", ");
  in2_->Print(); dec.p.Printf(")");
}

#pragma mark - BCSetPath

int BCSetPath::provides() {
  if (Resolved())
    return (b_ == 0) ? kProvidesNone : 1;
  else
    return kProvidesUnknown;
}

Node *BCSetPath::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  if (   (prev->IsExpr())
      && (prev->prev->IsExpr())
      && (prev->prev->prev->IsExpr())) {
    value_ = prev->Unlink();
    path_ = prev->Unlink();
    object_ = prev->Unlink();
    dec.numASTChanges++;
    return next;
  }
  return next;
}

void BCSetPath::PrintChildren(bool deep) {
  if (object_) object_->PrintNode(deep);
  if (path_) path_->PrintNode(deep);
  if (value_) value_->PrintNode(deep);
}

void BCSetPath::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  object_->Print();
  dec.p.Print(".");
  PrintPathExpr(path_);
  dec.p.Print(" := ");
  value_->Print();
}

#pragma mark - BCSetARef

/**
 \class BCSetARef
 \brief Helper for 'foreach'.
 */

Node *BCSetARef::Resolve(Pass pass)
{
  if ((pass != Pass::DataFlow) || Resolved()) return next;

  if (   (prev->IsExpr())
      && (prev->prev->IsExpr())
      && (prev->prev->prev->IsExpr())) {
    element_ = prev; prev->Unlink();
    index_ = prev; prev->Unlink();
    object_ = prev; prev->Unlink();
    dec.numASTChanges++;
    return this;
  }
  return next;
}

void BCSetARef::PrintChildren(bool deep) {
  if (object_) object_->PrintNode(deep);
  if (index_) index_->PrintNode(deep);
  if (element_) element_->PrintNode(deep);
}

void BCSetARef::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  object_->Print();
  dec.p.Print("[");
  index_->Print();
  dec.p.Print("] := ");
  element_->Print();
}


#pragma mark - BCMakeFrame

void BCMakeFrame::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  BCPush *mapNode { nullptr };
  if ( (mapNode = dynamic_cast<BCPush*>(ins_[numIns_-1])) ) {
    dec.p.Print("{");
    dec.p.StartList(","); // TODO: is there a way to figure out if we need a deep list?
    Ref map = dec.GetLiteral(mapNode->b());
    int n = ComputeMapSize(map);
    for (int i = 0; i < n; i++) {
      dec.p.Item();
      Ref tag = GetTag(map, i);
      dec.p.PrintTag(tag);
      dec.p.Print(": ");
      if (i >= numIns_-1)
        dec.p.Printf("nil");
      else
        ins_[i]->Print();
      dec.p.ItemDone();
    }
    dec.p.Trailer(); dec.p.Print("}");
    dec.p.EndList();
  } else {
    assert(0);
  }
}

#pragma mark - BCMakeArray

// val1 val2 ... valN class -- array
void BCMakeArray::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Print("[");
  dec.p.StartList(","); // TODO: is there a way to figure out if we need a deep list?
  // Check if the array class is defined and not 'array
  Node *klassNd = ins_.back();
  if (klassNd->a() == 3) { // BCPush
    int literalIx = klassNd->b();
    Ref klass = dec.GetLiteral(literalIx);
    if (::IsSymbol(klass) && (SymbolCompare(klass, SYMA(array)) != 0)) {
      dec.p.Tag(); dec.p.PrintTag(klass); dec.p.Print(":");
    }
  }
  // Print all the members of the new array
  for (int i = 0; i < numIns_-1; i++) {
    dec.p.Item();
    ins_[i]->Print();
    dec.p.ItemDone();
  }
  dec.p.Trailer(); dec.p.Print("]");
  dec.p.EndList();
}

/**
 \brief Print members of array separated by '&' characters.
 */
void BCMakeArray::PrintAsStringer(uint32_t flags)
{
  // TODO: we could optimize for '& " " &' --> '&&'
  dec.p.StartList(" &");
  for (int i = 0; i < numIns_-1; i++) {
    dec.p.Item();
    ins_[i]->Print();
    dec.p.ItemDone();
  }
  dec.p.EndList();
}
