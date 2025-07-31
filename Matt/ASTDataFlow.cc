
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

#pragma mark -

bool AST_Push::IsSymbol()
{
  return ::IsSymbol(dec.GetLiteral(b_));
}

void AST_Push::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.printLiteral(b_);
  } else {
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_Push literal[%d] ###", pc_, b_);
  }
}

#pragma mark -

bool AST_PushConst::IsNIL()
{
  return (b_ == NILREF);
}

void AST_PushConst::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.PrintConstant(b_);
  } else {
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_PushConst value:%d ###", pc_, b_);
  }
}

#pragma mark -

void AST_PushSelf::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Printf("self");
  } else {
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_PushSelf ###", pc_);
  }
}

#pragma mark -

void AST_FindVar::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.printLiteralAsTag(b_);
  } else {
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_FindVar literal[%d] ###", pc_, b_);
  }
}

#pragma mark -

void AST_GetVar::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.printLocal(b_);
  } else {
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_GetVar local[%d] ###", pc_, b_);
  }
}

#pragma mark -

auto AST_Pop::ResolveDataFlow() -> std::tuple<bool, ASTNode*> {
  if (Resolved()) return { false, next };
  AST_Branch *branch = prev ? dynamic_cast<AST_Branch*>(prev) : nullptr;
  if (branch && (branch->b() > pc_)) {
    // If the sequence is 'branch; pop;', the pop can never be reached
    // because there is no jump target between them.
    // Lucky for us, break operations are by definition expressions, so
    // the pop is needed, which makes this a reliable way to find a
    // break instruction.
    // The AST_Break will take care of the jump target when resolved.
    AST_Break *breakNode = new AST_Break(dec, branch->pc(), 0, branch->b());
    delete branch->Unlink();
    this->ReplaceWith(breakNode);
    return { true, breakNode };
  }
  return AST_Consume1::ResolveDataFlow();
}

void AST_Pop::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    in_->Print();
    dec.p.Item();
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_Pop ###", pc_);
  }
}

#pragma mark -

void AST_Dup::Print() {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_Dup ###", pc_);
}

#pragma mark -

void AST_SetVar::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Item();
    dec.printLocal(b_);
    dec.p.Printf(" := ");
    in_->Print();
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_SetVar local[%d] ###", pc_, b_);
  }
}

#pragma mark -

void AST_FindAndSetVar::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.printLiteralAsTag(b_);
    dec.p.Printf(" := ");
    in_->Print();
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_FindAndSetVar literal[%d] ###", pc_, b_);
  }
}

#pragma mark -

void AST_Not::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    int ppp = dec.precedence;
    bool parentheses = (dec.precedence > 2);
    dec.precedence = 2;
    dec.p.Item();
    if (parentheses) dec.p.Printf("(");
    dec.p.Printf("not ");
    in_->Print();
    if (parentheses) dec.p.Printf(")");
    dec.precedence = ppp;
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_Not ###", pc_);
  }
}

#pragma mark -

void AST_Length::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Item();
    dec.p.Printf("length(");
    in_->Print();
    dec.p.Printf(")");
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_Length ###", pc_);
  }
}

#pragma mark -

void AST_Clone::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Item();
    dec.p.Printf("clone(");
    in_->Print();
    dec.p.Printf(")");
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_Clone ###", pc_);
  }
}

#pragma mark -

void AST_Stringer::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //      dec.p.Item();
  //      dec.p.Printf("clone(");
  //      in_->Print();
  //      dec.p.Printf(")");
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_Stringer ###", pc_);
  //    }
}
//  TODO: The input is an Array with at least two elements
//  a '1 && 2' is handled as a '1 & " " & 2', generating an array with three values
//  So the in_ node is AST_MakeArray which consumes the inputs and the 'array symbol
//  void printSource() {
//    if (in_) {
//      AST_MakeArray *array = dynamic_cast<AST_MakeArray*>(in_);
//      if (array) {
//        dec.p.Printf(array->in[0] " & " array->in[1] ... )
//      }
//    }
//  }

#pragma mark -

void AST_ClassOf::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Item();
    dec.p.Printf("ClassOf(");
    in_->Print();
    dec.p.Printf(")");
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_ClassOf ###", pc_);
  }
}

#pragma mark -

void AST_BitNot::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Item();
    dec.p.Printf("bNot(");
    in_->Print();
    dec.p.Printf(")");
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_BitNot ###", pc_);
  }
}

#pragma mark -

auto AST_BinaryExpression::ResolveDataFlow() -> std::tuple<bool, ASTNode*>
{
  if (!Resolved() && (prev->IsExpr()) && (prev->prev->IsExpr())) {
    in2_ = prev->Unlink();
    in1_ = prev->Unlink();
    return { true, this };
  } else {
    return { false, next };
  }
}

#pragma mark -

void AST_BinaryFunction::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    int ppp = dec.precedence;
    dec.precedence = 0;
    dec.p.Item();
    dec.p.Printf("%s(", func_);
    in1_->Print();
    dec.p.Printf(", ");
    in2_->Print();
    dec.p.Printf(")");
    dec.precedence = ppp;
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_BinaryFunction \"%s\" ###", pc_, func_);
  }
}

#pragma mark -

void AST_BinaryOperator::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    int ppp = dec.precedence;
    bool parentheses = (dec.precedence > precedence_);
    dec.precedence = precedence_;
    if (parentheses) dec.p.Print("(");
    in1_->Print(); dec.p.Printf(" %s ", op_); in2_->Print();
    if (parentheses) dec.p.Print(")");
    dec.precedence = ppp;
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_BinaryOperator \"%s\" ###", pc_, op_);
  }
}

#pragma mark -

void AST_NewArray::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_NewArray ###", pc_);
  //    }
}

#pragma mark -

void AST_GetPath::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    in1_->Print();
    dec.p.Printf(".");
    PrintPathExpr(in2_);
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_GetPath b:%d ###", pc_, b_);
  }
}

#pragma mark -

void AST_ARef::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Item();
    in1_->Print();
    dec.p.Printf("["); in2_->Print(); dec.p.Printf("]");
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_ARef ###", pc_);
  }
}

#pragma mark -

void AST_SetClass::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Item();
    dec.p.Printf("SetClass(");
    in1_->Print(); dec.p.Printf(", ");
    in2_->Print(); dec.p.Printf(")");
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_SetClass ###", pc_);
  }
}

#pragma mark -

void AST_AddArraySlot::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    dec.p.Item();
    dec.p.Printf("AddArraySlot(");
    in1_->Print(); dec.p.Printf(", ");
    in2_->Print(); dec.p.Printf(")");
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_AddArraySlot ###", pc_);
  }
}

#pragma mark -

void AST_HasPath::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_HasPath ###", pc_);
  //    }
}

#pragma mark -

int AST_SetPath::provides() {
  if (Resolved())
    return (b_ == 0) ? kProvidesNone : 1;
  else
    return kProvidesUnknown;
}

auto AST_SetPath::ResolveDataFlow() -> std::tuple<bool, ASTNode*> {
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

void AST_SetPath::PrintChildren() {
  dec.p.DeepList();
  if (object_) object_->Print();
  if (path_) path_->Print();
  if (value_) value_->Print();
  dec.p.EndList();
}

void AST_SetPath::Print() {
  if ((dec.output == Print::script) && Resolved()) {
    object_->Print();
    dec.p.Print(".");
    PrintPathExpr(path_);
    dec.p.Print(" := ");
    value_->Print();
  } else {
    if (dec.output == Print::deep) PrintChildren();
    dec.p.Item();
    printHeader();
    dec.p.Printf("%3d: AST_SetPath ###", pc_);
  }
}

#pragma mark -

auto AST_SetARef::ResolveControlFlow() -> std::tuple<bool, ASTNode*> {
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

void AST_SetARef::PrintChildren() {
  dec.p.DeepList();
  if (object_) object_->Print();
  if (index_) index_->Print();
  if (element_) element_->Print();
  dec.p.EndList();
}

void AST_SetARef::Print() {
  //    if ((dec.output == Print::script) && Resolved()) {
  //    } else {
  if (dec.output == Print::deep) PrintChildren();
  dec.p.Item();
  printHeader();
  dec.p.Printf("%3d: AST_SetARef ###", pc_);
  //    }
}


