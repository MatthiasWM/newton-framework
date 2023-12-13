

#include "Frames/Frames.h"
#include "Funcs.h"
#include "NewtonPackage.h"

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


//  .globl  _RSSYMGetDefaultStore
//_RSYMGetDefaultStore:    Ref  MAKEPTR(SYMGetDefaultStore)
//_RSSYMGetDefaultStore:  Ref  _RSYMGetDefaultStore
//SYMGetDefaultStore:  Ref    kHeaderSize + 4 + 16 + kFlagsBinary
//Ref    0, 0x55552
//  .long    0x529B1862
//  .asciz  "GetDefaultStore"
//  .align  8
//#define kHeaderSize 24
//#define kFlagsBinary (0x40<<24)
//struct StructGetDefaultStore{ uintptr_t hdr, gc, id; uint32_t hash; char sym[16]; };
//struct StructGetDefaultStore SYMGetDefaultStore = { kHeaderSize+16+kFlagsBinary, 0, 0x55552, 0x529B1862, "GetDefaultStore" };
//uintptr_t RSYMGetDefaultStore = MAKEPTR(&SYMGetDefaultStore);
//uintptr_t RSSYMGetDefaultStore = RSYMGetDefaultStore; // _RSSYMGetDefaultStore




const char *pkg_path = "/Users/matt/dev/Newton/Software/mpg.pkg";

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

//#define INIVAR(x_, age_, sex_ ,s_) \
//union {\
//struct { ..., char name[sizeof(s_)]; } x;\
//person p;\
//} x_ = { (age_), (sex_), (s_) }
//
//INIVAR(your_var, 55, 'M', "THE NAME");

#if 0
  Ref src = MakeStringFromCString("7*6");
  Ref fn = ParseString(src);
  Ref ret = DoBlock(fn, RA(NILREF));
  PrintObject(ret, 0); puts("");
  Disassemble(fn);
  PrintCode(fn); puts("");
#endif

  NewtonPackage pkg(pkg_path);
  Ref part = pkg.partRef(0);
  PrintObject(part, 0); puts("");
  Disassemble( GetFrameSlot(part, MakeSymbol("InstallScript")) );
  PrintCode(GetFrameSlot(part, MakeSymbol("InstallScript"))); puts("");
}
