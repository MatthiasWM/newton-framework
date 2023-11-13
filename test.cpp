

#include "Frames/Frames.h"
#include "Funcs.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

extern "C" void InitObjectSystem(void);
extern "C" void  PrintObject(Ref inObj, int indent);
extern Ref ParseString(RefArg inStr);
extern Ref ParseFile(const char * inFilename);
extern void Disassemble(RefArg inFunc);
extern Ref MakeStringFromCString(const char * str);
extern void PrintCode(RefArg obj);

//  ./Packages/PartHandler.h:NewtonErr  LoadPackage(CEndpointPipe * inPipe, ObjectId * outPackageId, bool inWillRemove = true);
//  ./Packages/PartHandler.h:NewtonErr  LoadPackage(CPipe * inPipe, ObjectId * outPackageId, bool inWillRemove = true);
//  ./Packages/PartHandler.h:NewtonErr  LoadPackage(Ptr buffer, SourceType inType, ObjectId * outPackageId);
//  ./Packages/PartHandler.h:NewtonErr  LoadPackage(CEndpointPipe * inPipe, SourceType inType, ObjectId * outPackageId);
//  ./Packages/PartHandler.h:NewtonErr  LoadPackage(CPipe * inPipe, SourceType inType , ObjectId * outPackageId);


//bool  IsInt(Ref r) { return ISINT(r); }
//bool  IsChar(Ref r) { return ISCHAR(r); }
//bool  IsPtr(Ref r) { return ISPTR(r); }
//bool  IsMagicPtr(Ref r) { return RTAG(r) == kTagMagicPtr; }
//bool  IsRealPtr(Ref r) { return ISREALPTR(r); }


extern Ref *RSSYMviewer;

int main(int argc, char **argv)
{
  InitObjectSystem();
  printf("Hello world\n");
//  DefGlobalVar(SYMA(printDepth), MAKEINT(7));
//    RefVar  fn(NSGetGlobalFn(inSym));
//  newton_try
//  {
//    DoBlock(RA(bootRunInitScripts), RA(NILREF));
//  }
//  newton_catch(exRootException)
//  { }
//  end_try;
//  extern "C" Ref DoBlock(RefArg codeBlock, RefArg args);
// #include "Funcs.h"

//  Ref ParseString(RefArg inStr)
//  Ref ParseFile(const char * inFilename)
//  void Disassemble(RefArg inFunc);

  Ref src = MakeStringFromCString("7*6");
  Ref fn = ParseString(src);
  Ref ret = DoBlock(fn, RA(NILREF));
  PrintObject(ret, 0); puts("");
  Disassemble(fn);
  PrintCode(fn); puts("");

}
