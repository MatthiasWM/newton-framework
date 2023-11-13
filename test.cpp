

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


void hex16(void *src) {
  int n = 16;
  uint8_t *s = (uint8_t*)src;
  printf("%016llx: ", s);
  for (int i=0; i<n; i++) {
    printf("%02x ", s[i]);
    if (i==7) printf(" ");
  }
  printf(" ");
  for (int i=0; i<n; i++) {
    uint8_t c = s[i];
    if (c<32 || c>126) c = '.';
    printf("%c", c);
  }
  printf("\n");
}

void hex(void *src, int n) {
  for (int i=0; i<n; i+=16) {
    hex16(((uint8_t*)src) + i);
  }
}

void hexref(Ref r) {
  printf("====\n");
  hex(&r, 16);
  if (IsInt(r)) printf("int: %d\n", RefToInt(r));
  if (IsPtr(r)) {
    printf("----\n");
    ObjHeader *p = ObjectPtr(r);
    printf("size: %d, flags: %02x, locks: %d, slots: %d\n",
           p->size, p->flags, p->gc.count.locks, p->gc.count.slots);
    if (ISLARGEBINARY(p)) printf("Large Binary\n");
    if (ISSLOTTED(p)) printf("Is slotted\n");
    if (ISARRAY(p)) printf("Is an array\n");
    if (ISFRAME(p)) printf("Is a frame\n");
    hex(p, 64);
  }
}

void hexarg(RefArg ra) {
  Ref r = ra;
  hexref(r);
}

//bool  IsInt(Ref r) { return ISINT(r); }
//bool  IsChar(Ref r) { return ISCHAR(r); }
//bool  IsPtr(Ref r) { return ISPTR(r); }
//bool  IsMagicPtr(Ref r) { return RTAG(r) == kTagMagicPtr; }
//bool  IsRealPtr(Ref r) { return ISREALPTR(r); }


extern Ref *RSSYMviewer;

int main(int argc, char **argv)
{

  Ref r = *RSSYMviewer;
  hexref(r);

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
