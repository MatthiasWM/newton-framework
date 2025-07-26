

#include "Frames/Frames.h"
#include "Frames/StreamObjects.h"
#include "Funcs.h"
#include "NewtGlobals.h"
#include "NewtonPackage.h"
#include "Matt/PackageWriter.h"
#include "Matt/ObjectPrinter.h"
#include "Utilities/DataStuffing.h"
#include "Frames/Interpreter.h"
#include "Frames/Compiler/InputStreams.h"
#include "Frames/Compiler/Compiler.h"
#include "REPTranslators.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

#include <iostream>
#include <string>
#include <exception>
#include <stdexcept>


extern "C" void InitObjectSystem(void);
extern "C" void PrintObject(Ref inObj, int indent);
extern Ref ParseString(RefArg inStr);
extern Ref ParseFile(const char * inFilename);
extern void Disassemble(RefArg inFunc);
extern Ref MakeStringFromCString(const char * str);
extern void PrintCode(RefArg obj);




int currentRefIndex = 0;

extern void handleArgHello();


/**
 \brief Initialize the newtc tool and the NewtonScript toolkit.
 */
bool init()
{
  InitObjectSystem();

  DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(0));
  // DefGlobalVar(SYMA(printDepth), MAKEINT(7));

  Ref hexFn = AllocateFrame();
  SetFrameSlot(hexFn, MakeSymbol("class"), kPlainCFunctionClass);
  SetFrameSlot(hexFn, MakeSymbol("function"), (Ref)FStuffHex);
  SetFrameSlot(hexFn, MakeSymbol("numargs"), MAKEINT(2));
  SetFrameSlot(gFunctionFrame, EnsureInternal(MakeSymbol("MakeBinaryFromHex")), hexFn);

  return true;
}

/**
 \brief Create a new global variable `ref#` that will hold the incoming ref.
 The # increments with every call to this function.
 */
void addGlobalRef(RefArg inRef)
{
  char buf[32];
  snprintf(buf, 31, "ref%d", currentRefIndex);
  RefVar symRefN = MakeSymbol(buf);
  DefGlobalVar(symRefN, inRef);
  currentRefIndex++;
}

/**
 \brief Load a package file and write the resulting object into global ref.
 */
void handleArgPkg(const std::string &filename)
{
  NewtonPackage pkg(filename.c_str());
  Ref package = pkg.packageRef();
  if (package == NILREF) {
    throw(std::runtime_error("Can't read package."));
  }
  addGlobalRef(package);
}

/**
 \brief Load an NSOF file and write the resulting object into global ref.
 */
void handleArgNsof(const std::string &filename)
{
  CStdIOPipe inPipe(filename.c_str(), "rb");
  CObjectReader reader(inPipe);
  RefVar ref = reader.read();
  if (ref == NILREF) {
    throw(std::runtime_error("Can't read NSOF."));
  }
  addGlobalRef(ref);
}

/**
 \brief Load a script from a text file and write the resulting object into ref#.
 */
void handleArgScript(const std::string &filename)
{
  Ref result = ParseFile(filename.c_str());
  addGlobalRef(result);
}

/**
 \brief Load a script from a text file, compile it, and write the result object into ref#.
 \todo Check Newton exception handling
 */
void handleArgRun(const std::string &filename)
{
  Ref fn = ParseFile(filename.c_str());
  Ref result = InterpretBlock(fn, RA(NILREF));
  addGlobalRef(result);
}

/**
 \brief Compile a script and write the resulting object into ref#.
 */
void handleArgS(const std::string &script)
{
  RefVar result;
  RefVar codeBlock;
  RefVar src = MakeStringFromCString(script.c_str());
  CStringInputStream stream(src);

  CCompiler compiler(&stream, true);

  newton_try
  {
    while (!stream.end())
    {
      codeBlock = compiler.compile();
      result = NOTNIL(codeBlock) ? InterpretBlock(codeBlock, RA(NILREF)) : NILREF;
    }
  }
  newton_catch(exRefException)
  {
    RefVar data(*(RefStruct *)CurrentException()->data);
    if (IsFrame(data)) {
      SetFrameSlot(data, SYMA(filename), MakeStringFromCString("<script>"));
      SetFrameSlot(data, SYMA(lineNumber), MAKEINT(compiler.lineNo()));
    }
    gREPout->exceptionNotify(CurrentException());
  }
  newton_catch_all
  {
    gREPout->exceptionNotify(CurrentException());
  }
  end_try;
  
  addGlobalRef(result);
}

/**
 \brief Compile and run a script and write the resulting object into ref#.
 */
void handleArgR(const std::string &script)
{
  Ref src = MakeStringFromCString(script.c_str());
  Ref fn = ParseString(src);
  Ref result = InterpretBlock(fn, RA(NILREF));
  addGlobalRef(result);
  addGlobalRef(result);
}

/**
 \brief Switch compiler to generate NOS 1.x compatible code which also runs on 2.x.
 */
void handleArgNos1()
{
  DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(0));
}

/**
 \brief Switch compiler to generate optimized code, but limited to run on NOS 2.x.
 */
void handleArgNos2()
{
  DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(1));
}

/**
 \brief Create a new global variable `ref#` that will hold the incoming ref.
 The # increments with every call to this function.
 */
void handleArgClear()
{
  for (int i=0; i<currentRefIndex; ++i) {
    char buf[32];
    snprintf(buf, 31, "ref%d", i);
    RefVar symRefN = MakeSymbol(buf);
    DefGlobalVar(symRefN, NILREF);
  }
  currentRefIndex = 0;
}

/**
 \brief Write the object in global `ref0` as a package file.
 \todo Error handling
 */
void handleArgOPkg(const std::string &filename)
{
  RefVar package = GetGlobalVar(MakeSymbol("ref0"));
  writePackageToFile(package, filename);
}

/**
 \brief Create an NSOF file form ref0.
 */
void handleArgONsof(const std::string &filename)
{
  CStdIOPipe outPipe(filename.c_str(), "wb");
  RefVar ref = GetGlobalVar(MakeSymbol("ref0"));
  CObjectWriter out(ref, outPipe, false);
  out.write();
}

/*
 Frames/StreamObjects.h:

 CObjectReader(CPipe & inPipe);
 CObjectReader(CPipe & inPipe, RefArg inOptions);
 ~CObjectReader();
 void      setPrecedentsForReading(void);
 void      setFunctionsAllowed(bool inAllowed);
 Ref      read(void);
 ArrayIndex  size(void);                // NRG


 CStdIOPipe pipe("/Users/matt/dev/test.nsof", "w");
 CObjectWriter writer(func, pipe, false);
 //writer.setCompressLargeBinaries();
 writer.write();
 */

/**
 \brief Write the object `ref0` as text to stdout.
 */
void handleArgPrint()
{
  RefVar ref0 = GetGlobalVar(MakeSymbol("ref0"));
  ObjectPrinter p(std::cout);
  p.Print(ref0);
}

/**
 \brief Write the object `ref0` as text to stdout.
 Decompile functions as we encounter them.
 */
void handleArgDecompile()
{
  RefVar ref0 = GetGlobalVar(MakeSymbol("ref0"));
  ObjectPrinter p(std::cout);
  p.Decompile(ref0);
}


/**
 \brief The NewtonScript compiler and decompiler, main entry point.

 This is a command line NewtonScript that will compile and run scripts, read
 and write NSOF files, and read and create NewtonOS package files.

 It can print detailed and decompiled Newton Object, that will create
 functionally the same Newton Object when recompiled. This allows us to
 decompile existing packages, apply fixes, and recompile them into a
 package again.

 In the future, newtc will be able to extract and reintegrate binary resources
 for graphics and sounds.

 Command line options that read data will store the result in the global
 variable `ref#`, starting with #=0, incrementing the # with every newly created
 object. Writing options will always write `ref0`.

 Command line options:
  - Reader:
    - [x] -pkg filename : read a package and create one object that includes the
      package description and all parts that could be read
    - [x] -nsof filename : read a Newton Script Object file and hold the contents
      as and object.
    - [x] -script filename : read a NewtonScript file and compile the script
    - [x] -run filename : read, compile, and run some Newton Script
    - [x] -s "script" : compile the script, result is stored in a global ref#
    - [x] -r "script" : compile and run the script, result is stored in a global ref#
    - [x] -hello : create the a Hello, Wold! application object
  - Controller:
    - [x] -nos1 : compile into NewtonOS 1.x format (default)
    - [x] -nos2 : compile into NewtonOS 2.x format
    - [ ] -pkg0 name symbol : generate a minimal `package0` package object
    - [ ] -pkg1 name symbol : generate a minimal `package1` package object
    - [ ] -addpart ??? : add the most recent object as the next part to ref0
    - [x] -clear : clear all ref# and start over at ref0
    - [ ] -debug level : (may be a bit pattern at some point)
    - [ ] -compare : compares ref0 and ref1, clears, and sets ref0 to true or nil
  - Writer:
    - [x] -opkg filename : write ref0 as a package
    - [x] -onsof filename : write ref0 as a Newton Script Object File
    - [x] -print : print ref0 to stdout, don't print the contents of binary objects
    - [x] -decompile : print ref0 to stdout, decompile all functions
    - [ ] -decompose directory : decompile, and extract all known binary resources
    - [ ] -hex : write as a hexadecimal dump
    - [ ] -diff : compare the decompiled text output of ref0 and ref1
    - [x] -- : same as -print

  \todo not much of a difference between -s and -r, or -script and -run, right?
 */
int main(int argc, char **argv)
{
  if (!init()) {
    printf("newtc: ERROR: Can't initialize.\n");
    return -1;
  }
  int argi = 1;
  RefVar symRef0 = MakeSymbol("ref0");
  DefGlobalVar(symRef0, NILREF);
  try {
    while (argi < argc) {
      std::string cmd = argv[argi++];
      if (cmd.empty()) {
        // just skip an empty arg
      } else if (cmd == "-pkg") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -pkg ... ."));
        handleArgPkg(std::string(argv[argi++]));
      } else if (cmd == "-nsof") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -nsof ... ."));
        handleArgNsof(std::string(argv[argi++]));
      } else if (cmd == "-script") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -script ... ."));
        handleArgScript(std::string(argv[argi++]));
      } else if (cmd == "-run") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -run ... ."));
        handleArgRun(std::string(argv[argi++]));
      } else if (cmd == "-s") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing script after -s ... ."));
        handleArgS(std::string(argv[argi++]));
      } else if (cmd == "-r") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing script after -r ... ."));
        handleArgR(std::string(argv[argi++]));
      } else if (cmd == "-hello") {
        handleArgHello();
      } else if (cmd == "-nos1") {
        handleArgNos1();
      } else if (cmd == "-nos2") {
        handleArgNos2();
      } else if (cmd == "-clear") {
        handleArgClear();
      } else if (cmd == "-opkg") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -opkg ... ."));
        handleArgOPkg(std::string(argv[argi++]));
      } else if (cmd == "-onsof") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -onsof ... ."));
        handleArgONsof(std::string(argv[argi++]));
      } else if ((cmd == "--") || (cmd == "-print")) {
        handleArgPrint();
      } else if (cmd == "-decompile") {
        handleArgDecompile();
      } else {
        throw(std::runtime_error("Unknown command line argument: \"" + cmd + "\"."));
      }
    }
  } catch (const std::exception &ex) {
    std::cout << "newtc: ERROR: " << ex.what() << std::endl;
  } catch (... ) {
    printf("newtc: ERROR: Unknown error.\n");
    return -1;
  }
  return 0;
}

/**
 \brief Create a Newton Object tree that generates a package of a Hello World! app.
 just call `newtc -hello -opkg hello.pkg` from the command line.
 */
void handleArgHello() {
  const char *script = R"*(
    
myLabel := {
  text: "Hello, world!",
  viewBounds: { left: 8, top: 24, right: 144, bottom: 56 },
  viewJustify: 8388614,
  _proto: @218
};

{
  signature: 'package0,
  id: "xxxx",
  flags: { noCompression: true },
  version: 1,
  copyright: "\u00A9\u2025, newtc",
  name: "hello:SIG",
  modifyDate: 0,
  info: "newt 0.1",
  part: [
    {
      offset: 0,
      size: 2296,
      type: "form",
      flags: { type: 'nos, Notify: true },
      info: "newtc 0.1; platform file MessagePad v5",
      data: {
        app: '|hello:SIG|,
        text: "Hello",
        icon: {
          mask: MakeBinaryFromHex("000000000004000000000000001B0018000000000000000000001C0000003F0000003F001FFF7F003FFF7E003FFFFE003FFFFC003FFFFC003FFFF8003FFFF8003FFFF0003FFFF0003FFFE0003FFFE0003FFFC0003FFFC0003FFF80003FFF00003FFF80003FFF80003FFF80001FFF00001FFF00000FFE000000000000", 'mask),
          bits: MakeBinaryFromHex("000000000004000000000000001B0018000000000000000000001C0000003F00000033001FFF7B003FFF6E003000CE0037FCCC0037FD9C0034059800355338003403300035567000340660003554E000340CC000354FC000340B8000360F000037FE800037FD8000300B8000180300001FFF00000FFE000000000000", 'bits),
          bounds: { left: 0, top: 0, right: 24, bottom: 27 }
        },
        theForm: {
          viewBounds: { left: -12, top: 56, right: 140, bottom: 152 },
          viewClickScript: func(arg) begin end,
          stepChildren: [
            stepChildren: myLabel
          ],
          _proto: @180,
          appSymbol: '|hello:SIG|
        },
        installScript: func(part)
        begin
          part:?devInstallScript(part);
          if HasSlot(part, 'devInstallScript) then
            RemoveSlot(part, 'devInstallScript);
          return part.installScript := nil;
        end
      }
    }
  ]
};
    )*";
  Ref src = MakeStringFromCString(script);
  Ref fn = ParseString(src);
  Ref result = DoBlock(fn, RA(NILREF));
  addGlobalRef(result);
}


// Just some leftovers from previous version to give me a reminder

// NOTE: in Stores/FlashRange.cc:138, the flash memory is mapped to a file on the PC:
// fpath  const char *  "/Users/matt/Library/Application Support/Newton/Internal"  0x000000010115a980
// If forNTK is not defined, the flash file is filled with 0xFF at every launch.

//const char *pkg_path = "/Users/matt/dev/Newton/Software/PeggySu.pkg";
//const char *pkg_path = "/Users/matt/dev/Newton/Software/Fahrenheit.pkg";
//      NewtonPackage pkg("/Users/matt/dev/Einstein/loop.ntk.pkg");

//extern Ref *RSSYMviewer;

//  Disassemble( GetFrameSlot(part, MakeSymbol("InstallScript")) );
//  PrintCode(GetFrameSlot(part, MakeSymbol("InstallScript"))); puts("");

//  PrintObject(package, 0); puts("");
//  printPackage(package);

//  RefVar data(*(RefStruct *)CurrentException()->data);
//  if (IsFrame(data)
//      &&  ISNIL(GetFrameSlot(data, SYMA(filename))))
//  {
//    SetFrameSlot(data, SYMA(filename), MakeStringFromCString(stream.fileName()));
//    SetFrameSlot(data, SYMA(lineNumber), MAKEINT(compiler.lineNo()));
//  }
//  gREPout->exceptionNotify(CurrentException());

//  const char *fname = "/Users/matt/dev/test.ns";
//  const char *sname = "/Users/matt/dev/test.nsof";



// NTK uses these (and possibly more) global methods to assemble views
// fgUseStepChildren -> kUseStepChildren -> projectSettings.useStepChildren
// for all options, see newton-toolkit: buildPkg in ProjectDocument.mm
// stepChildren hold user created view templates (the contents of a group)
// viewChildren hold system created views, like the clock in the app window
#if 0
Ref
AddStepForm(RefArg parent, RefArg child) {
  RefVar childArraySym(fgUseStepChildren? SYMA(stepChildren) : SYMA(viewChildren));
  if (!FrameHasSlot(parent, childArraySym)) {
    SetFrameSlot(parent, childArraySym, AllocateArray(childArraySym, 0));
  }
  AddArraySlot(GetFrameSlot(parent, childArraySym), child);
}

Ref
StepDeclare(RefArg parent, RefArg child, RefArg tag) {
  RefVar childContextArraySym(fgUseStepChildren? SYMA(stepAllocateContext) : SYMA(allocateContext));
  if (!FrameHasSlot(parent, childContextArraySym)) {
    SetFrameSlot(parent, childContextArraySym, MakeArray(0));
  }
  AddArraySlot(GetFrameSlot(parent, childContextArraySym), tag);
  AddArraySlot(GetFrameSlot(parent, childContextArraySym), child);
}
#endif
