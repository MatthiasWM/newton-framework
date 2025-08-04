
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

#pragma mark - AST_BC_Push

bool AST_BC_Push::IsSymbol()
{
  return ::IsSymbol(dec.GetLiteral(b_));
}

void AST_BC_Push::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLiteral(b_);
}

#pragma mark - AST_BC_PushConst

bool AST_BC_PushConst::IsNIL()
{
  return (b_ == NILREF);
}

void AST_BC_PushConst::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.PrintConstant(b_);
}

#pragma mark - AST_BC_PushSelf

void AST_BC_PushSelf::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("self");
}

#pragma mark - AST_BC_FindVar

void AST_BC_FindVar::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLiteralAsTag(b_);
}

#pragma mark - AST_BC_GetVar

void AST_BC_GetVar::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLocal(b_);
}

#pragma mark - AST_BC_Pop

void AST_BC_Pop::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  in_->Print();
}

ASTNode *AST_BC_Pop::Resolve(Pass pass) {
  if (Resolved()) return next;
  if (pass == Pass::DataFlow) {
    // Remove the useless sequence "push-const *, pop" before it is picked
    // up in the compress path.
    if (dynamic_cast<AST_BC_PushConst*>(prev)) {
      ASTNode *nextNode = next;
      prev->Unlink();
      this->Unlink();
      return nextNode;
    }
    // TODO: Remove the useless sequence "find-and-set-var a, find-var a, pop"
    return super::Resolve(pass);
  }
  return next;
}

#pragma mark - AST_BC_Dup

void AST_BC_Dup::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_BC_SetVar

void AST_BC_SetVar::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLocal(b_);
  dec.p.Printf(" := ");
  in_->Print();
}

#pragma mark - AST_BC_FindAndSetVar

void AST_BC_FindAndSetVar::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.printLiteralAsTag(b_);
  dec.p.Printf(" := ");
  in_->Print();
}

#pragma mark - AST_BC_Not

void AST_BC_Not::Print(uint32_t flags) {
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

#pragma mark - AST_BC_Length

void AST_BC_Length::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("length(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - AST_BC_Clone

void AST_BC_Clone::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("clone(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - AST_BC_Stringer

void AST_BC_Stringer::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}
//  TODO: The input is an Array with at least two elements
//  a '1 && 2' is handled as a '1 & " " & 2', generating an array with three values
//  So the in_ node is AST_BC_MakeArray which consumes the inputs and the 'array symbol
//  void printSource() {
//    if (in_) {
//      AST_BC_MakeArray *array = dynamic_cast<AST_BC_MakeArray*>(in_);
//      if (array) {
//        dec.p.Printf(array->in[0] " & " array->in[1] ... )
//      }
//    }
//  }

#pragma mark - AST_BC_ClassOf

void AST_BC_ClassOf::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("ClassOf(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - AST_BC_BitNot

void AST_BC_BitNot::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("bNot(");
  in_->Print();
  dec.p.Printf(")");
}

#pragma mark - AST_BinaryExpression

ASTNode *AST_BinaryExpression::Resolve(Pass pass)
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

#pragma mark - AST_BinaryFunction

void AST_BinaryFunction::Print(uint32_t flags) {
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

#pragma mark - AST_BinaryOperator

void AST_BinaryOperator::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  int ppp = dec.precedence;
  bool parentheses = (dec.precedence > precedence_);
  dec.precedence = precedence_;
  if (parentheses) dec.p.Print("(");
  in1_->Print(); dec.p.Printf(" %s ", op_); in2_->Print();
  if (parentheses) dec.p.Print(")");
  dec.precedence = ppp;
}

#pragma mark - AST_BC_NewArray

// size class -- array

// if the class is 'array, generate `Array(size, nil)`
// with a custom class, generate `SetClass(Array(size, nil), 'class)`
void AST_BC_NewArray::Print(uint32_t flags)
{
  if (!Resolved()) return PrintNode(false);

  // Check if the array class is defined and not 'array
  bool setClass = false;
  Ref klass = NILREF;
  ASTNode *klassNd = in2_;
  if (klassNd->a() == 3) { // AST_BC_Push
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

#pragma mark - AST_BC_GetPath

void AST_BC_GetPath::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  in1_->Print();
  dec.p.Printf(".");
  PrintPathExpr(in2_);
}

#pragma mark - AST_BC_ARef

void AST_BC_ARef::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  in1_->Print();
  dec.p.Printf("["); in2_->Print(); dec.p.Printf("]");
}

#pragma mark - AST_BC_SetClass

void AST_BC_SetClass::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("SetClass(");
  in1_->Print(); dec.p.Printf(", ");
  in2_->Print(); dec.p.Printf(")");
}

#pragma mark - AST_BC_AddArraySlot

void AST_BC_AddArraySlot::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Printf("AddArraySlot(");
  in1_->Print(); dec.p.Printf(", ");
  in2_->Print(); dec.p.Printf(")");
}

#pragma mark - AST_BC_HasPath

void AST_BC_HasPath::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}

#pragma mark - AST_SetPath

int AST_SetPath::provides() {
  if (Resolved())
    return (b_ == 0) ? kProvidesNone : 1;
  else
    return kProvidesUnknown;
}

ASTNode *AST_SetPath::Resolve(Pass pass)
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

void AST_SetPath::PrintChildren(bool deep) {
  if (object_) object_->PrintNode(deep);
  if (path_) path_->PrintNode(deep);
  if (value_) value_->PrintNode(deep);
}

void AST_SetPath::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  object_->Print();
  dec.p.Print(".");
  PrintPathExpr(path_);
  dec.p.Print(" := ");
  value_->Print();
}

#pragma mark - AST_BC_SetARef

ASTNode *AST_BC_SetARef::Resolve(Pass pass)
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

void AST_BC_SetARef::PrintChildren(bool deep) {
  if (object_) object_->PrintNode(deep);
  if (index_) index_->PrintNode(deep);
  if (element_) element_->PrintNode(deep);
}

void AST_BC_SetARef::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
}


#pragma mark - AST_BC_MakeFrame

void AST_BC_MakeFrame::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  AST_BC_Push *mapNode { nullptr };
  if ( (mapNode = dynamic_cast<AST_BC_Push*>(ins_[numIns_-1])) ) {
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

#pragma mark - AST_BC_MakeArray

// val1 val2 ... valN class -- array
void AST_BC_MakeArray::Print(uint32_t flags) {
  if (!Resolved()) return PrintNode(false);
  dec.p.Print("[");
  dec.p.StartList(","); // TODO: is there a way to figure out if we need a deep list?
  // Check if the array class is defined and not 'array
  ASTNode *klassNd = ins_.back();
  if (klassNd->a() == 3) { // AST_BC_Push
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

