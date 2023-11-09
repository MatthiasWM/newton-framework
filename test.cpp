
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

extern "C" void InitObjectSystem(void);




#include <cstdint>

#define kHeaderSize 20
#define kFlagsBinary (0x40<<24)

typedef struct {
  uint32_t hdr;
  long gc;
  long ref;
  uint32_t sum;
  char txt[11];
} Sym11;

Sym11 SYMphoneIndex { kHeaderSize + 4 + 11 + kFlagsBinary, 0, 0x55552, 0xFF6482E2, "phoneIndex" };



#if 0



//  .globl  SYMslot
//SYMslot:  .long    kHeaderSize + 4 + 5 + kFlagsBinary
//Ref    0, 0x55552
//  .long    0x01C71AB2
//  .asciz  "slot"
//  .align  2

#define kHeaderSize 20
#define kFlagsBinary (0x40<<24)

constexpr int len(const char *str) {
//  return strlen(str);
//  int x = tolower(str[0]);
  return std::string::traits_type::length(str);
}

constexpr int i = len("Hallo");


//template< size_t N >
//constexpr size_t length( char const (&)[N] )
//{
//  return N-1;
//}


Sym4 SYMslotCPP = { kHeaderSize + 4 + 5 + kFlagsBinary, 0x55552, 0x01C71AB2, "slot" };

#endif


#if 0
ld: warning: pointer not aligned at _RbootInitNSGlobals+0x0 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RSData.o
ld: warning: pointer not aligned at _RbootInitNSGlobals+0x8 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RSData.o
ld: warning: pointer not aligned at _RbootInitNSGlobals+0x10 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RSData.o
ld: warning: pointer not aligned at _RbootInitNSGlobals+0x18 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RSData.o
ld: warning: pointer not aligned at _RbootInitNSGlobals+0x28 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RSData.o
ld: warning: pointer not aligned at _RbootInitNSGlobals+0x38 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RSData.o
ld: warning: pointer not aligned at _RbootInitNSGlobals+0x48 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RSData.o
ld: warning: pointer not aligned at _RbootInitNSGlobals+0x58 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RSData.o
ld: warning: pointer not aligned at _RbootInitNSGlobals+0x60 from /Users/matt/dev/newton-framework/build/Xcode/build/test.b


ld: warning: pointer not aligned at _gROMDataStart+0xC from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o
ld: warning: pointer not aligned at _gROMDataStart+0xDC from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o
ld: warning: pointer not aligned at _gROMDataStart+0xE4 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o
ld: warning: pointer not aligned at _gROMDataStart+0xEC from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o
ld: warning: pointer not aligned at _gROMDataStart+0xF4 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o
ld: warning: pointer not aligned at _gROMDataStart+0xFC from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o

ld: warning: pointer not aligned at _gROMDataStart+0xC from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o
ld: warning: pointer not aligned at _gROMDataStart+0xDC from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o
ld: warning: pointer not aligned at _gROMDataStart+0xE4 from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o
ld: warning: pointer not aligned at _gROMDataStart+0xEC from /Users/matt/dev/newton-framework/build/Xcode/build/test.build/Debug/Objects-normal/x86_64/RefData.o
#endif


int main(int argc, char **argv)
{
  static_assert(sizeof("test")==5, "Is it?");


//#error HELP!
//  InitObjectSystem();
  printf("Hello world\n");

}
