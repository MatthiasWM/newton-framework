/*
 File:    ObjectPrinter.cc

 Prints a NewtonScript object tree that resembles a Package into a
 source file that can be recompiled into the same Package.

 Written by:  Matt, 2025.
 */

/*
 This class will take an NS Object tree and print it as a source code file,
 so it can be recompiled into the same object tree. It focuses and understanding
 and printing the contents of a package .pkg file.

 The main use for this module will be printing existing package files
 into a human readable and easily editable format. This will make it possible
 to understand nuances of NewtonOS and fix shortcomings in the packages.

 More importantly, it will eventually allow us to test every package on a
 simulated NewtonOS, and find issues in the simulation by allowing us to
 single-step through NewtonScript source code files.

 So far, we can read package files, convert them into a NewtonScript object
 tree, and write that tree back into the same package file. We can also compile
 hand-written NewtonScript files into such an Object tree, and then write a
 valid package.

 Goals:

 - Convert the tree into a crude text representation
   - understand and write all non-pointer refs
   - write slotted objects (Forms and Arrays)
   - write binary objects as MakeBinaryFromHex
   - write known binary objects in their native form (Real, String, ...)
 - Write objects with additional demands in separate blocks
   - mark everything that needs special handling
     - separate out the package definition and the app description and icons
     - separate out 'stepView' members so we can define individual Forms and Views
     - find objects with multiple references, so they get only written once
       but referenced as often as needed
   - find and order based on reference hierarchy (find circular dependencies)
   - find good labels
   - maybe generate a TOC at the top?
 - Improve text output by removing MakeBinaryFromHex wherever possible
   - write images as .png import commands
   - write sounds as .wav import commands
   - and the big one: decompile NS functions into readable NewtonScript
   - if we find ARM machine instructions, convert them back into assembler
 - Find common known patterns and write them, so they are easier to understand
   - NTK uses AddStepForm() and StepDeclare. Does it make sense to generate that code
   - Does it make sense to recreate NTK files from the source code? Layouts?
 - Will the compiler optimize multiple use of the same Symbol?
 - Will the compiler optimize Frame Maps?
 - Verify that all package-to-script-to-package conversion produce the same package

 */

#include "Matt/ObjectPrinter.h"

#include "Iterators.h"
#include "Matt/Decompiler.h"

#include <iostream>

// struct Node { value_t value; std::vector<std::shared_ptr<Node>> children; std::weak_ptr<Node> parent; };

// Run the graph and build a dependency tree.
// Every node has a list of children that the node references.
// Every node has a list of parents that reference the node
// There is also a flag for nodes that are always written separated
// Run the tree and find circular dependencies (somehow we need to resolve those last)
// Recurse down the tree: write everything that is marked or has multiple parents first
//   Write a node and all dependencies, only if it was already written write the reference instead
// Write the commands to resolve all cyclic dependencies


/**
 \brief Return true if this ref is managed by a node in the map.

 A node is created for every ref that can potentially be referenced multiple
 time or otherwise need special handling.

 This will create a node if none was created yet.
 */
bool ObjectPrinter::HasNode(Ref ref)
{
  if (map.find(ref)!=map.end()) return true;
  if (IsReal(ref)) return false;
  if (ISREALPTR(ref) && !IsSymbol(ref)) {
    map.insert(std::make_pair(ref, std::make_shared<Node>(ref)));
    return true;
  }
  return false;
}

void ObjectPrinter::PrintDependents(Ref ref)
{
  //printf("PrintDependents: 0x%016llx\n", (uint64_t)ref);
  if (ISREALPTR(ref)) {
    ObjHeader *o = ObjectPtr(ref);
    if (ISSLOTTED(o)) {
      map[ref]->visited = true;
      SlottedObject *f = (SlottedObject*)o;
      uint32_t numSlots = (f->size - sizeof(ObjHeader)) / sizeof(Ref); // first slot is class or map!
      for (uint32_t i=1; i<numSlots; i++) {
        Ref slot = f->slot[i];
        if (HasNode(slot)) {
          if (map[slot]->IsSpecial()) {
            PrintPartialTree(slot);
          } else {
            PrintDependents(slot);
          }
        }
      }
    }
  }
}

void ObjectPrinter::PrintIndent(int indent) {
  for (int i=0; i<indent; i++) printf("  ");
}


/**
 \brief Print a frame that contains NewtonScript function.
 */
void ObjectPrinter::PrintFunction(Ref ref, int indent)
{
  if (optionDecompile_) {
    mDecompile(ref, *this, debugAST_);
  } else {
    printf("{\n");
    bool first = true;
    CObjectIterator iter(ref, false);
    for ( ; !iter.done(); iter.next()) {
      if (!first) printf(",\n");
      if (first) first = false;
      PrintIndent(indent+1);
      Ref tag = iter.tag();
      if (IsSymbol(tag))
        printf("%s", SymbolName(tag));
      else
        PrintObject(iter.tag(), 0);
      printf(": ");
      Ref slot = iter.value();
      if (HasNode(slot) && map[slot]->IsSpecial()) {
        printf("%s", map[slot]->label.c_str());
      } else {
        PrintRef(slot, indent+1);
      }
    }
    if (!first) printf("\n");
    PrintIndent(indent); printf("}");
  }
}


// TODO: Sooooo, if this is a literal *and* a slotted object, the '[' or '{'
// must be preceded with a tick ('), *and* all symbols inside the object
// must *not* be preceded with a tick, aso it's either:
// a := '[test, toast, [butter]]; or a := ['test, 'toast, ['butter]];
// The first is generated at compile time, the second at run time, resulting
// in the same object at the end (but at very different execution speeds).
// In NewtonScriptProgramLanguage.pdf, see 'object' vs. 'constructor'
//
// TODO: Ok, well, to get this perfect, we need to look at
// Frames/Compiler/y.tab.c , generated from Frames/Compiler/NewtonScript.y
// which gives us not only the entire syntax, but also hints at what
// bytecodes and objects make up a nodes in the syntax tree. If we want to do
// this right, we implement everything based on that source.
//
void ObjectPrinter::PrintRef(Ref ref, int indent, bool symbolTick) {
  if (ISREALPTR(ref)) {
    if (IsFunction(ref)) {
      PrintFunction(ref, indent);
    } else if (IsFrame(ref)) {
      printf("{\n");
      bool first = true;
      CObjectIterator iter(ref, false);
      for ( ; !iter.done(); iter.next()) {
        if (!first) printf(",\n");
        if (first) first = false;
        PrintIndent(indent+1);
        Ref tag = iter.tag();
        if (IsSymbol(tag))
          printf("%s", SymbolName(tag));
        else
          PrintObject(iter.tag(), 0);
        printf(": ");
        Ref slot = iter.value();
        if (HasNode(slot) && map[slot]->IsSpecial()) {
          printf("%s", map[slot]->label.c_str());
        } else {
          PrintRef(slot, indent+1);
        }
      }
      if (!first) printf("\n");
      PrintIndent(indent); printf("}");
    } else if (IsArray(ref)) {
      printf("[\n");
      Ref tag = ClassOf(ref);
      if (IsSymbol(tag) && SymbolCompare(tag, SYMA(array)) != 0) {
        PrintIndent(indent+1);
        printf("%s:\n", SymbolName(tag));
      }
      bool first = true;
      CObjectIterator iter(ref, false);
      for ( ; !iter.done(); iter.next()) {
        if (!first) printf(",\n");
        if (first) first = false;
        PrintIndent(indent+1);
        Ref slot = iter.value();
        if (HasNode(slot) && map[slot]->IsSpecial()) {
          printf("%s\n", map[slot]->label.c_str());
        } else {
          PrintRef(slot, indent+1);
        }
      }
      if (!first) printf("\n");
      PrintIndent(indent); printf("]");
    } else if ((symbolTick == false) && IsSymbol(ref)) {
      printf("%s", SymbolName(ref));
    } else {
      PrintObject(ref, indent);
    }
  } else {
    PrintObject(ref, indent);
  }
}

void ObjectPrinter::PrintPartialTree(Ref ref)
{
  if (HasNode(ref)) {
    Node *nd = map[ref].get();
    if (nd->printed) return;
    PrintDependents(ref);
    if (nd->printed) return;
    nd->printed = true;
    printf("%s := \n", nd->label.c_str());
    PrintRef(ref, 0);
    printf(";\n\n");
  }
}



void ObjectPrinter::AddObject(Ref ref)
{
  map[ref]->visited = true;
  if (ISREALPTR(ref)) {
    ObjHeader *o = ObjectPtr(ref);
    if (ISSLOTTED(o)) {
      map[ref]->visited = true;
      SlottedObject *f = (SlottedObject*)o;
      uint32_t numSlots = (f->size - sizeof(ObjHeader)) / sizeof(Ref); // first slot is class or map!
      for (uint32_t i=1; i<numSlots; i++) {
        Ref slot = f->slot[i];
        AddRef(slot);
        if (HasNode(slot)) {
          map[ref]->children.push_back(map[slot]);
          map[slot]->numParents++;
          //printf("Link 0x%016lx to 0x%016lx, %d\n", map[ref]->ref, map[slot]->ref, map[slot]->numParents);
        }
      }
    }
  }
}

void ObjectPrinter::AddRef(Ref ref)
{
  if (HasNode(ref)) {
    if (!map[ref]->visited)
      AddObject(ref);
  } else {
    // Nothing to do.
  }
}

void ObjectPrinter::SetNodeLabels() {
  for (auto &ndi: map) {
    Node *nd = ndi.second.get();
    if (nd->label.empty()) {
      char buffer[80];
      snprintf(buffer, 80, "Ref_0x%016lx", nd->ref);
      nd->label = buffer;
    }
  }
}

void ObjectPrinter::BuildNodeTree(Ref package)
{
  AddRef(package);
  map[package]->special = true;
  SetNodeLabels();
}

void ObjectPrinter::TestPrint(Ref package)
{
  BuildNodeTree(package);
  map[package]->label = "package";
  printf("-----------------------\n");
  PrintPartialTree(package);
  printf("%s;\n", map[package]->label.c_str());

}

void printPackage(Ref package) {
  ObjectPrinter p(std::cout);
  p.TestPrint(package);
}

void ObjectPrinter::Print(RefArg ref)
{
  map.clear();
  if (IsArray(ref) || IsFrame(ref)) {
    BuildNodeTree(ref);
    PrintPartialTree(ref);
  } else {
    PrintRef(ref, 0);
  }
  puts("");
}

void ObjectPrinter::Decompile(RefArg ref)
{
  OptionDecompile(true);
  Print(ref);
}

#if 0

/* NewtonScript Grammar */
%{
typedef long Ref;
#define YYDEBUG 1
#define YYSTYPE Ref
%}


/* yacc/bison Declarations */
%token	kTokenOptionalExpr
%token	kTokenConst     // lex: string or char (".." or @..)
%token	kTokenSymbol    // lex: explicit piped symbol: |text|
%token	kTokenInteger   // all digits, or 0x... and hex digits
%token	kTokenReal      // all digits with a '.' and possible an 'e' or 'E'
%token	kTokenRefConst  // lex: Also know as a Magic Pointer (sigh!)

%token	kTokenBegin
%token	kTokenEnd
%token	kTokenFunc
%token	kTokenNative
%token	kTokenGlobal
%token	kTokenGFunction // ???
%token	kTokenConstant
%token	kTokenIf
%token	kTokenThen
%token	kTokenElse
%token	kTokenTry
%token	kTokenOnexception
%token	kTokenBuildArray
%token	kTokenBuildFrame
%token	kTokenLocal
%token	kTokenLoop
%token	kTokenFor
%token	kTokenTo
%token	kTokenBy
%token	kTokenWhile
%token	kTokenRepeat
%token	kTokenUntil
%token	kTokenDo
%token	kTokenCall
%token	kTokenWith
%token	kTokenInvoke
%token	kTokenForeach
%token	kTokenIn
%token	kTokenDeeply
%token	kTokenSelf
%token	kTokenInherited
%token	kTokenReturn
%token	kTokenBreak
%token	kTokenKeepGoing
%token	kTokenAssign
%token	kTokenAnd
%token	kTokenOr
%token	kTokenNot
%token	kTokenLEQ
%token	kTokenGEQ
%token	kTokenEQL
%token	kTokenNEQ
%token	kTokenExists
%token	kTokenAmperAmper
%token	kTokenDiv
%token	kTokenMod
%token	kTokenLShift
%token	kTokenRShift
%token	kTokenUMinus
%token	kTokenSendIfDefined

%left		kTokenAssign
%left		kTokenAnd kTokenOr
%left		kTokenNot
%left		'<' kTokenLEQ '>' kTokenGEQ kTokenEQL kTokenNEQ
%nonassoc	kTokenExists
%left		'&' kTokenAmperAmper
%left		'+' '-'
%left		'*' '/' kTokenDiv kTokenMod
%left		kTokenLShift kTokenRShift
%left		kTokenUMinus


/* Grammar follows */
%%
input:		/* empty */
					{	$$ = MakeArray(0); }
				|	command_plus
					/* nothing to do here */
				;

command_plus:	command
					{	$$ = MakeArray(1);
						SetArraySlot($$, 0, $1); }
				|	command_plus ';'
					{	$$ = $1;  if (doConstituents) YYACCEPT; }
					command
					{	$$ = $1;  AddArraySlot($$, $4); }
				;

command:			expr
					/* nothing to do here */
				|	global_decl
					/* nothing to do here */
				;

expr:				constant
					/* nothing to do here */
				|	lvalue
					/* nothing to do here */
				|	assignment
					/* nothing to do here */
				|	kTokenSelf
					{	$$ = AllocatePT1(kTokenSelf, RA(NILREF)); }
				|	kTokenBegin expr_seq kTokenEnd
					{	$$ = AllocatePT1(kTokenBegin, $2); }
				|	'(' expr ')'
					{	$$ = $2; }
				|	expr '+' expr
					{	$$ = AllocatePT2('+', $1, $3); }
				|	expr '-' expr
					{	$$ = AllocatePT2('-', $1, $3); }
				|	expr '*' expr
					{	$$ = AllocatePT2('*', $1, $3); }
				|	expr '/' expr
					{	$$ = AllocatePT2('/', $1, $3); }
				|	expr kTokenDiv expr
					{	$$ = AllocatePT2(kTokenDiv, $1, $3); }
				|	expr '&' expr
					{	$$ = AllocatePT2('&', $1, $3); }
				|	expr kTokenAmperAmper expr
					{	$$ = AllocatePT2(kTokenAmperAmper, $1, $3); }
				|	expr kTokenMod expr
					{	$$ = AllocatePT2(kTokenMod, $1, $3); }
				|	'-' expr
					{	$$ = AllocatePT1(kTokenUMinus, $2); }
				|	expr kTokenLShift expr
					{	$$ = AllocatePT2(kTokenLShift, $1, $3); }
				|	expr kTokenRShift expr
					{	$$ = AllocatePT2(kTokenRShift, $1, $3); }
				|	expr '<' expr
					{	$$ = AllocatePT2('<', $1, $3); }
				|	expr '>' expr
					{	$$ = AllocatePT2('>', $1, $3); }
				|	expr kTokenLEQ expr
					{	$$ = AllocatePT2(kTokenLEQ, $1, $3); }
				|	expr kTokenGEQ expr
					{	$$ = AllocatePT2(kTokenGEQ, $1, $3); }
				|	expr kTokenEQL expr
					{	$$ = AllocatePT2(kTokenEQL, $1, $3); }
				|	expr kTokenNEQ expr
					{	$$ = AllocatePT2(kTokenNEQ, $1, $3); }
				|	expr kTokenAnd expr
					{	$$ = AllocatePT2(kTokenAnd, $1, $3); }
				|	expr kTokenOr expr
					{	$$ = AllocatePT2(kTokenOr, $1, $3); }
				|	kTokenNot expr
					{	$$ = AllocatePT1(kTokenNot, $2); }
				|	lvalue kTokenExists
					{	$$ = AllocatePT1(kTokenExists, $1); }
				|	send_exists_expr kTokenExists
					{	$$ = AllocatePT1(kTokenExists, $1); }
				|	funcall_expr
					/* nothing to do here */
				|	send_expr
					/* nothing to do here */
				|	if_expr
					/* nothing to do here */
				|	loop_expr
					/* nothing to do here */
				|	lambda_expr
					/* nothing to do here */
				|	try_expr
					/* nothing to do here */
				|	constructor
					/* nothing to do here */
				|	local_decl
					/* nothing to do here */
				|	constant_decl
					/* nothing to do here */
				|	break_expr
					/* nothing to do here */
				|	return_expr
					/* nothing to do here */
				;

constant:		kTokenConst
					{	$$ = AllocatePT1(kTokenConst, $1); }
				|	kTokenInteger
					{	$$ = AllocatePT1(kTokenConst, $1); }
				|	kTokenReal
					{	$$ = AllocatePT1(kTokenConst, $1); }
				|	'\'' sexpr
					{	$$ = AllocatePT1(kTokenConst, $2); }
				|	kTokenRefConst
					{	$$ = AllocatePT1(kTokenConst, $1); }
				;

lvalue:			kTokenSymbol
					{	$$ = AllocatePT1(kTokenSymbol, $1); }
				|	expr '.' '(' expr ')'
					{	$$ = AllocatePT2('.', $1, $4); }
				|	expr '.' kTokenSymbol
					{	$$ = AllocatePT2('.', $1, AllocatePT1(kTokenConst, $3)); }
				|	expr '[' expr ']'
					{	$$ = AllocatePT2('[', $1, $3); }
				;

assignment:		lvalue kTokenAssign expr
					{	$$ = AllocatePT2(kTokenAssign, $1, $3); }
				;

local_decl:		kTokenLocal local_plus
					{	$$ = AllocatePT2(kTokenLocal, GetArraySlot($2, 0), GetArraySlot($2, 1)); }
				;

constant_decl:	kTokenConstant constant_init_plus
					{	$$ = AllocatePT1(kTokenConstant, $2); }
				;

global_decl:	kTokenGlobal kTokenSymbol
					{	$$ = AllocatePT2(kTokenGlobal, $2, AllocatePT1(kTokenConst, RA(NILREF))); }
				|	kTokenGlobal kTokenSymbol kTokenAssign expr
					{	$$ = AllocatePT2(kTokenGlobal, $2, $4); }
				|	kTokenGlobal kTokenSymbol '(' formal_args ')' expr
					{	/* CHECK THIS� */
						RefVar	fn(MakeArray(2));
						SetArraySlot(fn, 0, AllocatePT1(kTokenConst, $2));
						SetArraySlot(fn, 1, AllocatePT5(kTokenFunc, GetArraySlot($4, 0), $6, RA(NILREF), GetArraySlot($4, 1), RA(NILREF)));
						$$ = AllocatePT2(kTokenCall, SYMDefGlobalFn, fn); }
				|	kTokenFunc kTokenSymbol '(' formal_args ')' expr
					{	/* �AND THIS */
						RefVar	fn(MakeArray(2));
						SetArraySlot(fn, 0, AllocatePT1(kTokenConst, $2));
						SetArraySlot(fn, 1, AllocatePT5(kTokenFunc, GetArraySlot($4, 0), $6, RA(NILREF), GetArraySlot($4, 1), RA(NILREF)));
						$$ = AllocatePT2(kTokenCall, SYMDefGlobalFn, fn); }
				;

break_expr:		kTokenBreak expr
					{	AllocatePT1(kTokenBreak, $2); }
				|	kTokenBreak
					{	AllocatePT1(kTokenBreak, AllocatePT1(kTokenConst, RA(NILREF))); }
				;

return_expr:	kTokenReturn expr
					{	AllocatePT1(kTokenReturn, $2); }
				|	kTokenReturn
					{	AllocatePT1(kTokenReturn, AllocatePT1(kTokenConst, RA(NILREF))); }
				;

funcall_expr:	kTokenSymbol '(' expr_star ')'
					{	$$ = AllocatePT2(kTokenCall, $1, $3); }
				|	kTokenCall expr kTokenWith '(' expr_star ')'
					{	$$ = AllocatePT2(kTokenInvoke, $2, $5); }
				;

send_exists_expr:	expr ':' kTokenSymbol
					{	$$ = AllocatePT2(':', $3, $1); }
				|	':' kTokenSymbol
					{	$$ = AllocatePT2(':', $2, AllocatePT1(kTokenSelf, RA(NILREF))); }
				;

send_expr:		expr ':' kTokenSymbol '(' expr_star ')'
					{	$$ = AllocatePT3(':', $3, $1, $5); }
				|	kTokenInherited ':' kTokenSymbol '(' expr_star ')'
					{	$$ = AllocatePT3(':', $3, RA(NILREF), $5); }
				|	':' kTokenSymbol '(' expr_star ')'
					{	$$ = AllocatePT3(':', $2, AllocatePT1(kTokenSelf, RA(NILREF)), $4); }
				|	expr kTokenSendIfDefined kTokenSymbol '(' expr_star ')'
					{	$$ = AllocatePT3(kTokenSendIfDefined, $3, $1, $5); }
				|	kTokenInherited kTokenSendIfDefined kTokenSymbol '(' expr_star ')'
					{	$$ = AllocatePT3(kTokenSendIfDefined, $3, RA(NILREF), $5); }
				|	kTokenSendIfDefined kTokenSymbol '(' expr_star ')'
					{	$$ = AllocatePT3(kTokenSendIfDefined, $2, AllocatePT1(kTokenSelf, RA(NILREF)), $4); }
				;

if_expr:			kTokenIf expr kTokenThen expr kTokenElse expr
					{	$$ = AllocatePT3(kTokenIf, $2, $4, $6); }
				|	kTokenIf expr kTokenThen expr
					{	$$ = AllocatePT3(kTokenIf, $2, $4, RA(NILREF)); }
				;

loop_expr:		infinite_loop
					{	/* nothing to do here */ }
				|	for_loop
					{	/* nothing to do here */ }
				|	with_loop
					{	/* nothing to do here */ }
				|	while_loop
					{	/* nothing to do here */ }
				|	repeat_loop
					{	/* nothing to do here */ }
				;

infinite_loop: kTokenLoop expr
					{	$$ = AllocatePT1(kTokenLoop, $2); }
				;

for_loop:		kTokenFor kTokenSymbol kTokenAssign expr kTokenTo expr kTokenDo expr
					{	$$ = AllocatePT5(kTokenFor, $2, $4, $6, AllocatePT1(kTokenConst, MAKEINT(1)), $8); }
				|	kTokenFor kTokenSymbol kTokenAssign expr kTokenTo expr kTokenBy expr kTokenDo expr
					{	$$ = AllocatePT5(kTokenFor, $2, $4, $6, $8, $10); }
				;

with_loop:		kTokenForeach kTokenSymbol ',' kTokenSymbol optional_deeply kTokenIn expr withverb expr
					{	RefVar	req(MakeArray(2));
						SetArraySlot(req, 0, $2);
						SetArraySlot(req, 1, $4);
						$$ = AllocatePT5(kTokenForeach, $8, $7, $9, req, $5); }
				|	kTokenForeach kTokenSymbol optional_deeply kTokenIn expr withverb expr
					{	RefVar	req(MakeArray(1));
						SetArraySlot(req, 0, $2);
						$$ = AllocatePT5(kTokenForeach, $6, $5, $7, req, $3); }
				;

optional_deeply:	/* empty */
					{	$$ = NILREF; }
				|	kTokenDeeply
					{	$$ = TRUEREF; }
				;

withverb:		kTokenDo
					{	$$ = SYMmap; }
				|	kTokenSymbol
					{	if (!EQ($1, SYMA(collect)))
							syntaxError("FOREACH requires DO or COLLECT");
						$$ = SYMcollect; }
				;

while_loop:		kTokenWhile expr kTokenDo expr
					{	$$ = AllocatePT2(kTokenWhile, $2, $4); }
				;

repeat_loop:	kTokenRepeat expr_seq kTokenUntil expr
					{	$$ = AllocatePT2(kTokenRepeat, $2, $4); }
				;

lambda_expr:	kTokenFunc '(' formal_args ')' expr
					{	$$ = AllocatePT5(kTokenFunc, GetArraySlot($3, 0), $5, RA(NILREF), GetArraySlot($3, 1), RA(NILREF)); }
				|	kTokenFunc kTokenNative '(' formal_args ')' expr
					{	$$ = AllocatePT5(kTokenFunc, GetArraySlot($4, 0), $6, RA(TRUEREF), GetArraySlot($4, 1), RA(NILREF)); }
				|	kTokenFunc '+' '(' formal_args ')' expr
					{	$$ = AllocatePT5(kTokenFunc, GetArraySlot($4, 0), $6, RA(TRUEREF), GetArraySlot($4, 1), RA(NILREF)); }
				;

try_expr:		kTokenTry expr_seq handle_plus
					{	$$ = AllocatePT2(kTokenTry, AllocatePT1(kTokenBegin, $2), $3); }
				;

handle_plus:	handle_expr
					{	$$ = MakeArray(1);
						SetArraySlot($$, 0, $1); }
				|	handle_plus handle_expr
					{	$$ = $1;
						AddArraySlot($$, $2); }
				;

handle_expr:	kTokenOnexception kTokenSymbol kTokenDo expr
					{	$$ = AllocatePT2(kTokenOnexception, $2, $4); }
				;

constructor:	'[' expr_star ']'
					{	$$ = AllocatePT1(kTokenBuildArray, $2); }
				|	'[' kTokenSymbol ':' expr_star ']'
					{	$$ = AllocatePT1(kTokenBuildArray, $4);
						SetClass($4, $2); }
				|	'{' frame_slot_star '}'
					{	$$ = AllocatePT1(kTokenBuildFrame, $2); }
				;

expr_star:		/* empty */
					{	$$ = MakeArray(0); }
				|	expr_plus
					/* nothing to do here */
				;

expr_plus:		expr
					{	$$ = MakeArray(1);
						SetArraySlot($$, 0, $1); }
				|	expr_plus ',' expr
					{	$$ = $1;
						AddArraySlot($$, $3); }
				;

expr_seq:		/* empty */
					{	$$ = MakeArray(0); }
				|	expr
					{	$$ = MakeArray(1);
						SetArraySlot($$, 0, $1); }
				|	expr_seq ';' expr
					{	AddArraySlot($1, $3);
						$$ = $1; }
				;

formal_args:	/* empty */
					{	$$ = MakeArray(2);
						SetArraySlot($$, 0, MakeArray(0));
						SetArraySlot($$, 1, MakeArray(0)); }
				|	arg_plus
					/* nothing to do here */
				;

arg_plus:		kTokenSymbol
					{	$$ = MakeArray(2);
						RefVar	argNames(MakeArray(1));
						RefVar	argTypes(MakeArray(1));
						SetArraySlot($$, 0, argNames);
						SetArraySlot($$, 1, argTypes);
						SetArraySlot(argNames, 0, $1); }
				|	kTokenSymbol kTokenSymbol
					{	$$ = MakeArray(2);
						RefVar	argNames(MakeArray(1));
						RefVar	argTypes(MakeArray(1));
						SetArraySlot($$, 0, argNames);
						SetArraySlot($$, 1, argTypes);
						SetArraySlot(argNames, 0, $2);
						SetArraySlot(argTypes, 0, $1); }
				|	arg_plus ',' kTokenSymbol
					{	$$ = $1;
						RefVar	argNames(GetArraySlot($1, 0));
						RefVar	argTypes(GetArraySlot($1, 1));
						CObjectIterator	iter(argNames);
						for ( ; !iter.done(); iter.next())
						{
							if (EQ(iter.value(), $3))
							{
								Str255	str;
								sprintf(str, "Duplicate argument name: %s\n", SymbolName($3));
								warning(str);
								break;
							}
						}
						AddArraySlot(argNames, $3);
						AddArraySlot(argTypes, RA(NILREF)); }
				|	arg_plus ',' kTokenSymbol kTokenSymbol
					{	$$ = $1;
						RefVar	argNames(GetArraySlot($1, 0));
						RefVar	argTypes(GetArraySlot($1, 1));
						CObjectIterator	iter(argNames);
						for ( ; !iter.done(); iter.next())
						{
							if (EQ(iter.value(), $3))
							{
								Str255	str;
								sprintf(str, "Duplicate argument name: %s\n", SymbolName($3));
								warning(str);
								break;
							}
						}
						AddArraySlot(argNames, $4);
						AddArraySlot(argTypes, $3); }
				;

local_plus:		kTokenSymbol local_clause
					{	if (NOTNIL($2))
						{
							$$ = $2;
							SetArraySlot($$, 1, $1);
						}
						else
						{
							$$ = MakeArray(2);
							RefVar	locals(MakeArray(2));
							SetArraySlot(locals, 0, $1);
							SetArraySlot($$, 0, locals);
						} }
				|	kTokenSymbol kTokenAssign expr
					{	$$ = MakeArray(2);
						RefVar	locals(MakeArray(2));
						SetArraySlot(locals, 0, $1);
						SetArraySlot(locals, 1, $3);
						SetArraySlot($$, 0, locals); }
				|	local_plus ',' kTokenSymbol
					{	$$ = $1;
						RefVar	locals(GetArraySlot($1, 0));
						CObjectIterator	iter(locals);
						for ( ; !iter.done(); iter.next())
						{
							if (EQ(iter.value(), $3))
							{
								Str255	str;
								sprintf(str, "Duplicate variable name: %s\n", SymbolName($3));
								warning(str);
								break;
							}
						}
						AddArraySlot(locals, $3);
						AddArraySlot(locals, RA(NILREF)); }
				|	local_plus ',' kTokenSymbol kTokenAssign expr
					{	$$ = $1;
						RefVar	locals(GetArraySlot($1, 0));
						CObjectIterator	iter(locals);
						for ( ; !iter.done(); iter.next())
						{
							if (EQ(iter.value(), $3))
							{
								Str255	str;
								sprintf(str, "Duplicate variable name: %s\n", SymbolName($3));
								warning(str);
								break;
							}
						}
						AddArraySlot(locals, $3);
						AddArraySlot(locals, $5); }
				;

local_clause:	/* empty */
					{	$$ = NILREF; }
				|	kTokenSymbol
					{	$$ = MakeArray(2);
						RefVar	locals(MakeArray(2));
						SetArraySlot(locals, 0, $1);
						SetArraySlot($$, 0, locals); }
				|	kTokenSymbol kTokenAssign expr
					{	$$ = MakeArray(2);
						RefVar	locals(MakeArray(2));
						SetArraySlot(locals, 0, $1);
						SetArraySlot(locals, 1, $3);
						SetArraySlot($$, 0, locals); }
				;

constant_init_plus:	kTokenSymbol kTokenAssign expr
					{	$$ = MakeArray(2);
						SetArraySlot($$, 0, $1);
						SetArraySlot($$, 1, $3); }
				|	constant_init_plus ',' kTokenSymbol kTokenAssign constant
					{	$$ = $1;
						AddArraySlot($$, $3);
						AddArraySlot($$, $5); }
				;

frame_slot_star:	/* empty */
					{	$$ = AllocateFrame(); }
				|	frame_slot_plus
					/* nothing to do here */
				;

frame_slot_plus:	kTokenSymbol ':' expr
					{	$$ = AllocateFrame();
						SetFrameSlot($$, $1, $3); }
				|	frame_slot_plus ',' kTokenSymbol ':' expr
					{	$$ = $1;
						if (FrameHasSlot($$, $3))
						{
							Str255	str;
							sprintf(str, "duplicate slot name: %s", SymbolName($3));
							warning(str);
						}
						SetFrameSlot($$, $3, $5); }
				;

sexpr:			kTokenConst
					/* nothing to do here */
				|	kTokenInteger
					/* nothing to do here */
				|	'-' kTokenInteger
					{	$$ = MAKEINT(-RINT($2)); }
				|	kTokenReal
					/* nothing to do here */
				|	'-' kTokenReal
					{	$$ = MakeReal(-CDouble($2)); }
				|	kTokenRefConst
					/* nothing to do here */
				|	path_expr
					/* nothing to do here */
				|	'[' sexpr_star ']'
					{	$$ = $2; }
				|	'[' kTokenSymbol ':' sexpr_star ']'
					{	$$ = $4;
						SetClass($4, $2); }
				|	'{' sexpr_frame_slot_star '}'
					{	$$ = $2; }
				;

path_expr:		kTokenSymbol
					/* nothing to do here */
				|	path_expr '.' kTokenSymbol
					{	if (EQ(ClassOf($1), SYMA(pathExpr)))
						{
							AddArraySlot($1, $3);
							$$ = $1;
						}
						else
						{
							$$ = AllocateArray(SYMA(pathExpr), 2);
							SetArraySlot($$, 0, $1);
							SetArraySlot($$, 1, $3);
						} }
				;

sexpr_star:		/* empty */
					{	$$ = MakeArray(0); }
				|	sexpr_plus
					/* nothing to do here */
				;

sexpr_plus:		sexpr
					{	$$ = MakeArray(1);
						SetArraySlot($$, 0, $1); }
				|	sexpr_plus ',' sexpr
					{	$$ = $1;
						AddArraySlot($1, $3); }
				;

sexpr_frame_slot_star :	/* empty */
					{	$$ = AllocateFrame(); }
				|	sexpr_frame_slot_plus
					/* nothing to do here */
				;

sexpr_frame_slot_plus:	kTokenSymbol ':' sexpr
					{	$$ = AllocateFrame();
						SetFrameSlot($$, $1, $3); }
				|	sexpr_frame_slot_plus ',' kTokenSymbol ':' sexpr
					{	$$ = $1;
						if (FrameHasSlot($$, $3))
						{
							Str255	str;
							sprintf(str, "duplicate slot name: %s", SymbolName($3));
							warning(str);
						}
						SetFrameSlot($$, $3, $5); }
				;

%%

#endif
