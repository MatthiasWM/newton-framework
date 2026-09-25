

#include "Frames/Frames.h"
#include "Frames/StreamObjects.h"
#include "Funcs.h"
#include "NewtGlobals.h"
#include "NewtonPackage.h"
#include "Matt/BookWriter.h"
#include "Matt/PackageWriter.h"
#include "Matt/ObjectPrinter.h"
#include "Matt/ObjectPrinter.h"
#include "Utilities/DataStuffing.h"
#include "Frames/Interpreter.h"
#include "Frames/DebugAPI.h"
#include "Matt/EmbeddedScript.h"
#include "Matt/JSON.h"
#include "Matt/DAP.h"
#include "Frames/Compiler/InputStreams.h"
#include "Frames/Compiler/Compiler.h"
#include "REPTranslators.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <clocale>
#include <csignal>

#include <iostream>
#include <fstream>
#include <string>
#include <exception>
#include <stdexcept>

/* How to verify that reading a package, decompiling it, recompiling it,
   and writing the recompiled code to a package generates the same file:

./build/Xcode/Debug/newtc -pkg /Users/matt/dev/Gauges.pkg -decompile >a
./build/Xcode/Debug/newtc -pkg /Users/matt/dev/Gauges.pkg -debug bc -decompile >b
./build/Xcode/Debug/newtc -script a -debug bc -decompile >c
diff b c

 */


extern "C" void InitObjectSystem(void);
extern "C" void PrintObject(Ref inObj, int indent);
extern Ref ParseString(RefArg inStr);
extern Ref ParseFile(const char * inFilename);
extern void Disassemble(RefArg inFunc);
extern Ref MakeStringFromCString(const char * str);
extern void PrintCode(RefArg obj);
extern "C" Ref FDefineGlobalConstant(RefArg inRcvr, RefArg inTag, RefArg inObj);
// EnsureInternal

int handleArgs(int argc, char **argv);


int numGlobalRefs = 0;
std::string currentFileName = "<undefined>";

static bool forceNOS_ { false };
static bool debugAST_ { false };
static bool debugBC_ { false };
static std::string debugTrap_;

extern void handleArgHello();


/**
 \brief Make a NewtonScript function object for a C function.
 \param fn C function taking the receiver plus numArgs RefArgs, returning a Ref
 \param numArgs number of NewtonScript arguments
 */
static Ref makeCFunction(void *fn, int numArgs)
{
  RefVar cFn(AllocateFrame());
  SetFrameSlot(cFn, MakeSymbol("class"), kPlainCFunctionClass);
  SetFrameSlot(cFn, MakeSymbol("function"), (Ref)fn);
  SetFrameSlot(cFn, MakeSymbol("numargs"), MAKEINT(numArgs));
  return cFn;
}

/**
 \brief Make a C function callable from NewtonScript as a global function.
 \param name NewtonScript name of the function
 \param fn C function taking the receiver plus numArgs RefArgs, returning a Ref
 \param numArgs number of NewtonScript arguments
 */
static void defGlobalCFunction(const char *name, void *fn, int numArgs)
{
  RefVar cFn(makeCFunction(fn, numArgs));
  SetFrameSlot(gFunctionFrame, EnsureInternal(MakeSymbol(name)), cFn);
}

/**
 \brief Install the natives of "NS Debug Tools.pkg" (ARM code in the original).
 */
static void installNSDebugToolsNatives()
{
  defGlobalCFunction("NSDInstallBreakPoints", (void*)FNSDInstallBreakPoints, 1);
  defGlobalCFunction("NSDEnableBreakPoints", (void*)FNSDEnableBreakPoints, 1);
  defGlobalCFunction("NSDMakeNSDebugAPI", (void*)FNSDMakeNSDebugAPI, 0);
  defGlobalCFunction("NSDFindSlotName", (void*)FNSDFindSlotName, 2);
  defGlobalCFunction("NSDRefToHexString", (void*)FNSDRefToHexString, 1);

  // Methods of the debug API object: {_proto: NSDSelfFuncs, nsDebugAPI: ...}
  RefVar selfFuncs(AllocateFrame());
  auto addMethod = [&](const char *name, void *fn, int numArgs) {
    RefVar cFn(makeCFunction(fn, numArgs));
    SetFrameSlot(selfFuncs, MakeSymbol(name), cFn);
  };
  addMethod("AccurateStack", (void*)FNSDAccurateStack, 0);
  addMethod("NumStackFrames", (void*)FNSDNumStackFrames, 0);
  addMethod("Function", (void*)FNSDFunction, 1);
  addMethod("ProgramCounter", (void*)FNSDProgramCounter, 1);
  addMethod("SetProgramCounter", (void*)FNSDSetProgramCounter, 2);
  addMethod("Receiver", (void*)FNSDReceiver, 1);
  addMethod("Implementor", (void*)FNSDImplementor, 1);
  addMethod("GetVar", (void*)FNSDGetVar, 2);
  addMethod("SetVar", (void*)FNSDSetVar, 3);
  addMethod("FindVar", (void*)FNSDFindVar, 2);
  addMethod("SetFindVar", (void*)FNSDSetFindVar, 3);
  addMethod("NumTemps", (void*)FNSDNumTemps, 1);
  addMethod("TempValue", (void*)FNSDTempValue, 2);
  addMethod("SetTempValue", (void*)FNSDSetTempValue, 3);
  DefGlobalVar(EnsureInternal(MakeSymbol("NSDSelfFuncs")), selfFuncs);
}

/**
 \brief Initialize the newtc tool and the NewtonScript toolkit.
 */
bool init()
{
  std::setlocale(LC_ALL, "C");

  InitObjectSystem();

  // Compile for NewtonOS 2.x by default, like the 2.x ROM and NTK's
  // "Newton 2.0 Platform". NewtonOS 2.1 uses the same code format.
  // NOS 1.x code is only generated on request (-nos1, or "//! -nos1" in the
  // first line of a script, as written by the decompiler for NOS 1 packages).
  DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(1));
  // DefGlobalVar(SYMA(printDepth), MAKEINT(7));

  defGlobalCFunction("MakeBinaryFromHex", (void*)FStuffHex, 2);
  defGlobalCFunction("DefineGlobalConstant", (void*)FDefineGlobalConstant, 2);

  installNSDebugToolsNatives();

  // JSON <-> NewtonScript objects (for DAP, see Matt/JSON.h)
  defGlobalCFunction("JSONParse", (void*)FJSONParse, 1);
  defGlobalCFunction("JSONStringify", (void*)FJSONStringify, 1);

  // Debug Adapter Protocol messages (see Matt/DAP.h)
  defGlobalCFunction("DAPReceive", (void*)FDAPReceive, 0);
  defGlobalCFunction("DAPSend", (void*)FDAPSend, 1);
  defGlobalCFunction("DAPExit", (void*)FDAPExit, 1);
  defGlobalCFunction("DAPPrintObject", (void*)FDAPPrintObject, 1);

  return true;
}

/**
 \brief Create a new global variable `ref#` that will hold the incoming ref.
 The # increments with every call to this function.
 */
Ref addGlobalRef(RefArg inRef)
{
  char buf[32];
  snprintf(buf, 31, "ref%d", numGlobalRefs);
  RefVar symRefN = MakeSymbol(buf);
  Ref outRef = DefGlobalVar(symRefN, inRef);
  numGlobalRefs++;
  return outRef;
}

/**
 \brief Remove the latest global ref and update the ref counter.
 */
void dropGlobalRef()
{
  char buf[32];
  if (numGlobalRefs <= 0)
    throw(std::runtime_error("Can't drop global ref, no refs left."));
  numGlobalRefs--;
  snprintf(buf, 31, "ref%d", numGlobalRefs);
  RefVar symRefN = MakeSymbol(buf);
  DefGlobalVar(symRefN, NILREF);
}

/**
 * Get a ref from the ref stack, 0 being the latest ref, 1 the previous one, and so on.
 * @param index The index of the ref to get, 0 being the latest ref, 1 the previous one, and so on.
 */
Ref getGlobalRef(int index)
{
  if (index < 0 || index >= numGlobalRefs)
    throw(std::runtime_error("Can't get global ref, index out of range."));
  char buf[32];
  snprintf(buf, 31, "ref%d", numGlobalRefs-1-index);
  RefVar symRefN = MakeSymbol(buf);
  return GetGlobalVar(symRefN);
}

/**
 \brief Load a package file and write the resulting object into global ref.
 */
void handleArgPkg(const std::string &filename)
{
  currentFileName = filename;
  //fprintf(stderr, "    Reading '%s'\n", currentFileName.c_str());
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
  currentFileName = filename;
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
  currentFileName = filename;
  FILE *f = fopen(filename.c_str(), "rb");
  if (f) {
    char buf[81]; buf[0] = 0;
    fgets(buf, 80, f);
    if ((strcmp(buf, "//! -nos1\n") == 0) && !forceNOS_)
      DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(0));
    else if ((strcmp(buf, "//! -nos2\n") == 0) && !forceNOS_)
      DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(1));
    fclose(f);
  }
  Ref result = ParseFile(filename.c_str());
  addGlobalRef(result);
}

/**
 \brief Run (open) the current object, like tapping an app icon in NewtonOS.

 -run takes no argument. It works on the object held in ref# by a previous
 -pkg, -nsof, or -script command, so all three can be run the same way,
 e.g. `newtc -nsof app.nsof -run`. For a 'form package, the idea is to take
 its 'form part and open the main view.

 \todo Not implemented yet; prints a note and leaves ref# unchanged.
 */
void handleArgRun()
{
  std::cerr << "newtc: -run is not implemented yet, ignored." << std::endl;
}

/**
 \brief Compile and run a script and write the resulting object into ref#.
 */
void handleArgS(const std::string &script)
{
  currentFileName = "<script>";
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
 \brief Compile with debug information, like NTK's "Compile for debugging".
 Functions compiled from now on get a DebuggerInfo slot with the names of
 their arguments and locals (the compiler does it when the global
 dbgKeepVarNames is set), so the debugger can show and find variables by
 name. Planned: the debug map (source lines, Matt/CLAUDE.md Phase 8).
 */
void handleArgG()
{
  DefGlobalVar(MakeSymbol("dbgKeepVarNames"), TRUEREF);
}

extern const EmbeddedScript gNSDebugToolsScript;   // Matt/Debugger/NSDebugTools.ns
extern const EmbeddedScript gNSDShortCutsScript;   // Matt/Debugger/NSDShortCuts.ns

/**
 \brief Set up a debugging session like a Newton with Apple's debug tools.
 Loads NS Debug Tools and the NSD Shortcuts (both built into newtc from
 Matt/Debugger/), then does what the "Enable breakpoints" checkbox in the
 NS Debug Tools about box did: enable breakpoints and call the user's
 SetupMyDebug(true) (from the shortcuts: breakOnThrows, printDepth, ...).
 */
void handleArgDbg()
{
  if (!RunEmbeddedScript(gNSDebugToolsScript))
    throw(std::runtime_error("Can't load the debugger tools."));
  if (!RunEmbeddedScript(gNSDShortCutsScript))
    throw(std::runtime_error("Can't load the debugger shortcuts."));
  static const EmbeddedScript enable = { "<-dbg>",
    "NSDEnableBreakPoints(true);\n"
    "if HasPath(functions, 'SetupMyDebug) then SetupMyDebug(true);\n" };
  if (!RunEmbeddedScript(enable))
    throw(std::runtime_error("Can't enable the debugger."));
  // Code compiled from now on keeps its variable names (-g)
  handleArgG();
}

extern const EmbeddedScript gDAPScript;            // Matt/Debugger/DAP.ns

/**
 \brief Be a debug adapter (Debug Adapter Protocol) on stdin/stdout.
 Started by VS Code (or any DAP client) as `newtc -dap`. Sets up the
 debugger like -dbg, then the protocol in Matt/Debugger/DAP.ns handles the
 requests until the client launches a program, runs it like -script, and
 reports its end. See Matt/DAP.h.
 */
void handleArgDap()
{
  DAPStartIO();       // from here on, stdout carries only DAP messages
  handleArgDbg();     // its messages still go to the stdio translator, i.e. stderr
  // Off while no program runs; DAP:WaitForLaunch() sets it from the
  // client's exception filter.
  DefGlobalVar(MakeSymbol("breakOnThrows"), NILREF);
  DAPInstallTranslators();
  if (!RunEmbeddedScript(gDAPScript))
    throw(std::runtime_error("Can't load the debug adapter."));

  RefVar dap(GetGlobalVar(MakeSymbol("DAP")));
  int exitCode = 0;
  newton_try
  {
    RefVar launchArgs(DoMessage(dap, MakeSymbol("WaitForLaunch"), RA(NILREF)));
    if (IsFrame(launchArgs)) {
      std::string program = UTF8FromString(GetFrameSlot(launchArgs, MakeSymbol("program")));
      int exceptions = DAPExceptionCount();
      newton_try
      {
        FILE *f = fopen(program.c_str(), "rb");
        if (f == nullptr) {
          static std::string message;   // ThrowMsg keeps the pointer
          message = "Can't open the program \"" + program + "\"";
          ThrowMsg(message.c_str());
        }
        fclose(f);
        handleArgScript(program);
      }
      newton_catch_all
      {
        gREPout->exceptionNotify(CurrentException());
      }
      end_try;
      if (DAPExceptionCount() > exceptions)
        exitCode = 1;
      RefVar args(MakeArray(1));
      SetArraySlot(args, 0, MAKEINT(exitCode));
      DoMessage(dap, MakeSymbol("Finish"), args);
    }
  }
  newton_catch_all
  {
    gREPout->exceptionNotify(CurrentException());
    gREPout->flush();
  }
  end_try;
}

/**
 \brief Switch compiler to generate NOS 1.x compatible code which also runs on 2.x.
 */
void handleArgNos1()
{
  forceNOS_ = true;
  DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(0));
}

/**
 \brief Switch compiler to generate optimized code, but limited to run on NOS 2.x.
 */
void handleArgNos2()
{
  forceNOS_ = true;
  DefGlobalVar(MakeSymbol("compilerCompatibility"), MAKEINT(1));
}

/**
 \brief Create a new global variable `ref#` that will hold the incoming ref.
 The # increments with every call to this function.
 */
void handleArgClear()
{
  for (int i=0; i<numGlobalRefs; ++i) {
    char buf[32];
    snprintf(buf, 31, "ref%d", i);
    RefVar symRefN = MakeSymbol(buf);
    DefGlobalVar(symRefN, NILREF);
  }
  numGlobalRefs = 0;
}

/**
 \brief Set debugging flags.
 */
void handleArgDebug(const std::string &flag)
{
  if (flag == "ast") debugAST_ = true;
  if ((flag == "bytecode") || (flag == "bc")) debugBC_ = true;
}

/**
 \brief Set debugging trap.
 */
void handleArgTrap(const std::string &arg)
{
  debugTrap_ = arg;
}

/**
 \brief Write the object top of ref stack as a package file.
 \todo Error handling
 */
void handleArgOPkg(const std::string &filename)
{
  RefVar package = getGlobalRef(0);
  writePackageToFile(package, filename);
}

/**
 \brief Create an NSOF file from current ref.
 */
void handleArgONsof(const std::string &filename)
{
  CStdIOPipe outPipe(filename.c_str(), "wb");
  RefVar ref = getGlobalRef(0);
  CObjectWriter out(ref, outPipe, false);
  out.write();
}

/**
 \brief Create a PDF file from a book in current ref.
 */
void handleArgOPdf(const std::string &filename)
{
  RefVar package = getGlobalRef(0);
  if (writePackageBookToPDF(package, filename) == 1)
    fprintf(stderr, "^^^^ '%s'\n", currentFileName.c_str());
}

/**
 \brief Write the object in current ref as text to stdout.
 */
void handleArgPrint()
{
  RefVar ref0 = getGlobalRef(0);
  ObjectPrinter p(std::cout);
  p.Print(ref0);
}

/**
 \brief Write the object in current ref as text to stdout.
 Decompile functions as we encounter them.
 */
void handleArgDecompile()
{
  RefVar ref0 = getGlobalRef(0);
  ObjectPrinter p(std::cout);
  p.DebugAST(debugAST_);
  p.DebugBC(debugBC_);
  p.DebugTrap(debugTrap_);
  p.Decompile(ref0);
}

/**
 \brief Write the object in current ref as text to stdout.
 Decompile functions as we encounter them.
 */
void handleArgStats()
{
  RefVar ref0 = getGlobalRef(0);
  bool like = false;
  do {
    std::cout << "# ";
    if (!IsFrame(ref0)) { std::cout << "ERROR: can't read file."; break; }

    Ref signature = GetFrameSlot(ref0, MakeSymbol("signature"));
    if (!IsSymbol(signature)) { std::cout << ("ERROR: needs a signature."); break; }
    if (SymbolCompare(signature, MakeSymbol("package0"))==0) {
      std::cout << "Package0, ";
    } else if (SymbolCompare(signature, MakeSymbol("package1"))==0) {
      std::cout << "Package1, ";
    } else {
      std::cout << ("ERROR: unknown signature"); break;
    }

    Ref size = GetFrameSlot(ref0, MakeSymbol("size"));
    if (!IsInt(size)) { std::cout << ("ERROR: unknown size."); break; }
    std::cout << RefToInt(size)/1024 << " kBytes, ";

    // Flags: "autoRemove" "copyProtect" "invisible" "noCompression" "relocation" "useFasterCompression"

    Ref parts = GetFrameSlot(ref0, MakeSymbol("part"));
    if (!IsArray(parts)) { std::cout << ("ERROR: no parts found."); break; }
    std::cout << Length(parts) << " parts, ";
    if (Length(parts) != 1) { std::cout << ("ERROR: only exactly 1 part supported."); break; }

    Ref part = GetArraySlot(parts, 0);
    if (!IsFrame(part)) { std::cout << ("ERROR: part must be of type frame"); break; }

    Ref partFlags = GetFrameSlot(part, MakeSymbol("flags"));
    if (!IsFrame(partFlags)) { std::cout << ("ERROR: part flags must be of type frame"); break; }

    Ref partFlagsType = GetFrameSlot(partFlags, MakeSymbol("type"));
    if (!IsSymbol(partFlagsType) || (SymbolCompare(partFlagsType, MakeSymbol("nos"))!=0))
    { std::cout << ("ERROR: flags type must be 'nos"); break; }

    Ref partType = GetFrameSlot(part, MakeSymbol("type"));
    std::cout << "nos:";
    PrintObject(partType, 0);
    std::cout << std::endl;

    Ref c = GetFrameSlot(ref0, MakeSymbol("copyright"));
    std::cout << "# Copy: "; PrintObject(c, 0); std::cout << std::endl;
    Ref i = GetFrameSlot(ref0, MakeSymbol("info"));
    std::cout << "# Info: "; PrintObject(i, 0); // std::cout << std::endl;

    like = true;
  } while(0);
  std::cout << std::endl;
  if (!like) std::cout << "# ";
  std::cout << currentFileName << std::endl << std::endl;
}

/**
 * Return true if the given object is a package containing one part of type "form".
 * This is used to determine if we should apply the following commands to the
 * package when using -pkglist.
 */
bool packageIsForm(Ref r)
{
  if (!IsFrame(r)) return false;
  Ref signature = GetFrameSlot(r, MakeSymbol("signature"));
  if (!IsSymbol(signature)) return false;
  if ((SymbolCompare(signature, MakeSymbol("package0"))!=0) &&
      (SymbolCompare(signature, MakeSymbol("package1"))!=0)) return false;

  Ref parts = GetFrameSlot(r, MakeSymbol("part"));
  if (!IsArray(parts)) return false;
  if (Length(parts) != 1) return false;

  Ref part = GetArraySlot(parts, 0);
  if (!IsFrame(part)) return false;

  Ref partFlags = GetFrameSlot(part, MakeSymbol("flags"));
  if (!IsFrame(partFlags)) return false;

  Ref partFlagsType = GetFrameSlot(partFlags, MakeSymbol("type"));
  if (!IsSymbol(partFlagsType) || (SymbolCompare(partFlagsType, MakeSymbol("nos"))!=0)) return false;

  Ref partType = GetFrameSlot(part, MakeSymbol("type"));
  if (!IsString(partType)) return false;
  RefVar partTypeStr = ASCIIString(partType);
  if (strncmp(BinaryData(partTypeStr), "form", 4)!=0) return false;

  return true;
}

/**
 \brief Apply all following commands to each package in the file.
 */
int handleArgPkgList(const std::string &filename, int argc, char **argv)
{
  std::ifstream f(filename);
  if (!f.is_open()) {
    printf("newtc: ERROR: can't open package list. %s.\n", strerror(errno));
    return 0;
  }
  std::string pkgname;
  int ret = 0;
  bool first_package = true;
  int ref_index_bak = numGlobalRefs;
  while (std::getline(f, pkgname)) {
    size_t pos = pkgname.find_last_not_of("\r\n");
    if (pos != std::string::npos) {
      pkgname = pkgname.substr(0, pos + 1);
    }
    // Allow user to comment out packages from the file by starting the line with # or ;
    if (pkgname.empty() || pkgname[0] == '#' || pkgname[0] == ';') continue;
    if (pkgname[0] == '|') {
      // set breakpoint here to break into debugger
      continue;
    }
    // Restore stack package count for every package
    if (!first_package) {
      while (numGlobalRefs > ref_index_bak) {
        dropGlobalRef();
      }
    }
    first_package = false;
    try {
      handleArgPkg(pkgname);
    }
    catch(...) { }
    // Handle all remaining args in the command line for every "form" package
    if (packageIsForm(getGlobalRef(0))) {
      ret = handleArgs(argc, argv);
    }
    // If this was the last package, the ref stack is left with the results
  }
  return ret;
}

/**
 \brief Output a help message.
 Print a short description of all command line options.
 */
void handleArgHelp(const std::string &arg0)
{
  std::cout << R"==(
Usage:

  newtc [commands]

Handle NewtonScript files and sources by running
the commands in the given order.

  Input Commands
  -pkg <filename>         Load a package file and hold it as a Newton object
  -nsof <filename>        Load a Newton streaming object file
  -script <filename>      Read a source file, compile and run it, and hold the result
  -s <script>             Compile and run the script, and hold the result
  -hello                  Compile a "Hello World" app and hold it
  -run                    Run (open) the current object from -pkg, -nsof, or -script
                          (not implemented yet)

  Output Commands
  -opkg <filename>        Write the current object to a package file
  -onsof <filename>       Write the current object to a NSOF file
  -opdf <filename>        Write the first part as a PDF file if it is a book
  -print                  Print the current object, functions are just frames
  -decompile              Print the object with all functions decompiled
  -stats                  Print some package statistics about the object
  -help                   This help text

  Control
  -clear                  Delete all object in the hold
  -pkglist <filename>     Apply following commands to all 'form packages in the file

  Debugging
  -dbg                    Debug: load Apple's NS Debug Tools and NSD Shortcuts,
                          enable breakpoints and breakOnThrows, and compile the
                          following code with variable names (-g)
  -dap                    Be a debug adapter for VS Code: speak the Debug Adapter
                          Protocol on stdin/stdout, run the program given by the
                          client's "launch" request

  Options
  -g                      Compile with debug information: variable names (DebuggerInfo);
                          -dbg and -dap do this, too
  -nos1                   Compile for NewtonOS 1.x (compatible with NOS 2.x)
  -nos2                   Compile for NewtonOS 2.x and 2.1 (default)
  -debug ast              Print the progress of the AST while decompiling
  -debug bc               Print the ByteCode of functions before decompiling
  -trap path.to.func      Break into debugger when decompiling this function

)==";
}

int handleArgs(int argc, char **argv)
{
  int argi = 1;
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
        handleArgRun();
      } else if (cmd == "-s") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing script after -s ... ."));
        handleArgS(std::string(argv[argi++]));
      } else if (cmd == "-dbg") {
        handleArgDbg();
      } else if (cmd == "-dap") {
        handleArgDap();
      } else if (cmd == "-g") {
        handleArgG();
      } else if (cmd == "-hello") {
        handleArgHello();
      } else if (cmd == "-nos1") {
        handleArgNos1();
      } else if (cmd == "-nos2") {
        handleArgNos2();
      } else if (cmd == "-clear") {
        handleArgClear();
      } else if (cmd == "-drop") {
        dropGlobalRef();
      } else if (cmd == "-debug") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing description after -debug ... ."));
        handleArgDebug(std::string(argv[argi++]));
      } else if (cmd == "-trap") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing Ref Path after -trap ... ."));
        handleArgTrap(std::string(argv[argi++]));
      } else if (cmd == "-opkg") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -opkg ... ."));
        handleArgOPkg(std::string(argv[argi++]));
      } else if (cmd == "-onsof") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -onsof ... ."));
        handleArgONsof(std::string(argv[argi++]));
      } else if (cmd == "-opdf") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -opdf ... ."));
        handleArgOPdf(std::string(argv[argi++]));
      } else if (cmd == "-pkglist") {
        if ((argi >= argc) || (argv[argi][0] == '-'))
          throw(std::runtime_error("Missing filename after -pkglist ... ."));
        return handleArgPkgList(std::string(argv[argi]), argc-argi, argv+argi);
      } else if ((cmd == "--") || (cmd == "-print")) {
        handleArgPrint();
      } else if (cmd == "-decompile") {
        handleArgDecompile();
      } else if (cmd == "-stats") {
        handleArgStats();
      } else if ((cmd == "-help") || (cmd == "-h")) {
        handleArgHelp(argv[0]);
      } else {
        throw(std::runtime_error("Unknown command line argument: \"" + cmd + "\"."));
      }
    }
  } catch (const std::exception &ex) {
    std::cout << "newtc: ERROR: " << ex.what() << std::endl;
    return -1;
  } catch (... ) {
    std::cout << "newtc: ERROR: Unknown error." << std::endl;
    return -1;
  }
  return 0;
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
 variable, organized as ref0, ref1, and so on. To add a ref to the stack,
 use `addGlobalRef(ref)`, and to remove the latest ref, use `dropGlobalRef()`.

 To get the topmost entry on the ref stack, use `getGlobalRef(0)`, the previous
 one with `getGlobalRef(1)`, and so on.

 Command line options:
 - Reader:
 - [x] -pkg filename : read a package and create one object that includes the
 package description and all parts that could be read
 - [x] -nsof filename : read a Newton Script Object file and hold the contents
 as and object.
 - [x] -script filename : read a NewtonScript file, compile and run it, result is stored in a global ref#
 - [x] -s "script" : compile and run the script, result is stored in a global ref#
 - [ ] -run : run (open) the current object loaded by -pkg, -nsof, or -script, like tapping an app icon
 - [x] -hello : create the a Hello, Wold! application object
 - Controller:
 - [x] -nos1 : compile into NewtonOS 1.x format
 - [x] -nos2 : compile into NewtonOS 2.x format (default)
 - [ ] -pkg0 name symbol : generate a minimal `package0` package object
 - [ ] -pkg1 name symbol : generate a minimal `package1` package object
 - [ ] -addpart ??? : add the most recent object as the next part to ref0
 - [x] -clear : clear all ref# and start over at ref0
 - [x] -drop : drop the latest ref#, delete the object, and decrement the ref counter
 - [x] -debug level : (may be a bit pattern at some point)
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

 \todo Fix Package.Info read. We pick up stuff after the trailing 'nul'.

 */
int main(int argc, char **argv) {
  if (!init()) {
    printf("newtc: ERROR: Can't initialize.\n");
    return -1;
  }

  int ret = 0;
  RefVar symRef0 = MakeSymbol("ref0");
  DefGlobalVar(symRef0, NILREF);

  if (argc == 1) {
    //handleArgHelp(argv[0]);
    return 0;
  }

  ret = handleArgs(argc, argv);
  return ret;
}


/**
 \brief Create a Newton Object tree that generates a package of a Hello World! app.
 just call `newtc -hello -opkg hello.pkg` from the command line.
 */
void handleArgHello() {
  currentFileName = "<hello>";
  const char *script = R"*(

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
            stepChildren: {
              text: "Hello, world!",
              viewBounds: { left: 8, top: 24, right: 144, bottom: 56 },
              viewJustify: 8388614,
              _proto: @218
            }
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
