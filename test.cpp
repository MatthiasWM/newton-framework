

#include "Frames/Frames.h"
#include "Frames/StreamObjects.h"
#include "Funcs.h"
#include "NewtonPackage.h"
#include "NewtonPackageWriter.h"

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




const char *pkg_path = "/Users/matt/dev/Newton/Software/PeggySu.pkg";
//const char *pkg_path = "/Users/matt/dev/Newton/Software/Fahrenheit.pkg";

extern Ref *RSSYMviewer;

int main(int argc, char **argv)
{
  InitObjectSystem();
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
#if 1
  NewtonPackage pkg(pkg_path);
  Ref package = pkg.packageRef();
  PrintObject(package, 0); puts("");
//  Ref part = pkg.partRef(0);
//  PrintObject(part, 0); puts("");
//  Disassemble( GetFrameSlot(part, MakeSymbol("InstallScript")) );
//  PrintCode(GetFrameSlot(part, MakeSymbol("InstallScript"))); puts("");
  
//  writePackageToFile(package, "/Users/matt/dev/Newton/Software/Fahrenheit.out.pkg");
  writePackageToFile(package, "/Users/matt/dev/Newton/Software/PeggySu.out.pkg");
#endif
#if 0
  Ref src = MakeStringFromCString("if 1+2=3 then begin toast(3); trust(4); return test(2); end else return 3");
  Ref fn = ParseString(src);
//  Ref ret = DoBlock(fn, RA(NILREF));
//  PrintObject(ret, 0); puts("");
  puts("----");
  Disassemble(fn);
  puts("----");
  PrintCode(fn); puts("");
#endif
#if 0

  /*
   So ParseFile:

   Compile a file and interpret it.
   Top-level expressions are compiled and interpreted in sequence.
   Exceptions are caught and notified to gREPout but not rethrown.
   Args:    inFilename    path of file containing NewtonScript
   Return:  result of final codeblock execution

   RefVar data(*(RefStruct *)CurrentException()->data);
   if (IsFrame(data)
   &&  ISNIL(GetFrameSlot(data, SYMA(filename))))
   {
   SetFrameSlot(data, SYMA(filename), MakeStringFromCString(stream.fileName()));
   SetFrameSlot(data, SYMA(lineNumber), MAKEINT(compiler.lineNo()));
   }
   gREPout->exceptionNotify(CurrentException());

   Global Variables used:
    ParseFile:
      showCodeBlocks
      showLoadResults
    CCompile::compile
      LLVM
      compilerCompatibility (int) -> gCompilerCompatibility, 0=NOS1, 1=NOS2


   */




  const char *fname = "/Users/matt/dev/test.ns";
  const char *sname = "/Users/matt/dev/test.nsof";
  if (argc==2 || argc==3)
    fname = argv[1];
  if (argc==3)
    sname = argv[2];

  Ref func = ParseFile(fname);
  {
    CStdIOPipe pipe("/Users/matt/dev/test.nsof", "w");
    CObjectWriter writer(func, pipe, false);
    //writer.setCompressLargeBinaries();
    writer.write();
  }
  printf("Compiled %s to %s\n", fname, sname);

#endif
  return 0;
}
