

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
#include "Frames/NewtonScript.h"
#include "Frames/DebugAPI.h"
#include "Matt/EmbeddedScript.h"
#include "Matt/JSON.h"
#include "Matt/DAP.h"
#include "Matt/LineTables.h"
#include "Utilities/Unimplemented.h"
#include "Stores/HostStore.h"
#include "Host/Timers.h"
#include "Matt/EventLoop.h"
#include "Matt/TestWindow.h"
#include "Host/Root.h"
#include "Frames/Compiler/InputStreams.h"
#include "Frames/Compiler/Compiler.h"
#include "REPTranslators.h"
#if defined(NEWTC_NEWTPLAY)
#include "Matt/NewtPlay.h"
#include <vector>
#endif

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

extern "C" const char * GetFramesErrorString(NewtonErr inErr);

// Why a package's app didn't open (installPackage; for NewtPlay's alert):
// empty if it did, or if nothing went wrong.
static std::string gAppOpenError;

// An exception as a line of text for a user ("Index out of bounds (string
// or array)"); the REP prints more.
static std::string ExceptionText(Exception * inException)
{
  if (inException == nullptr)
    return "an error";
  if (Subexception(inException->name, exMessage) && inException->data)
    return (const char *)inException->data;
  if (Subexception(inException->name, exRefException) && inException->data) {
    RefVar data(*(RefStruct *)inException->data);
    if (IsFrame(data)) {
      RefVar err(GetFrameSlot(data, SYMA(errorCode)));
      if (ISINT(err)) {
        const char * text = GetFramesErrorString(RINT(err));
        if (text != nullptr && text[0] != '-')
          return text;
        return "error " + std::to_string(RINT(err));
      }
    }
  }
  return inException->name ? inException->name : "an error";
}


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
static Ref FDAPLoadPackage(RefArg rcvr, RefArg inPath, RefArg inMap);

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
  defGlobalCFunction("StubName", (void*)FStubName, 1);   // Utilities/Unimplemented.h
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
  defGlobalCFunction("DAPPause", (void*)FDAPPause, 0);
  defGlobalCFunction("DAPCallWithSelf", (void*)FDAPCallWithSelf, 3);
  defGlobalCFunction("DAPErrorText", (void*)FDAPErrorText, 1);
  defGlobalCFunction("DAPCaptureOutput", (void*)FDAPCaptureOutput, 1);
  defGlobalCFunction("DAPLoadPackage", (void*)FDAPLoadPackage, 2);

  // Source lines of functions compiled with -g (see Matt/LineTables.h)
  InstallLineTables();
  defGlobalCFunction("LineOfPC", (void*)FLineOfPC, 2);
  defGlobalCFunction("CodeForLine", (void*)FCodeForLine, 2);
  defGlobalCFunction("StartLineStep", (void*)FStartLineStep, 4);

#if NEWTC_USES_FLTK
  // A window for testing the event loop (see Matt/TestWindow.h)
  defGlobalCFunction("TestWindow", (void*)FTestWindow, 3);
  defGlobalCFunction("TestWindowClick", (void*)FTestWindowClick, 0);
  defGlobalCFunction("TestWindowClose", (void*)FTestWindowClose, 0);
  defGlobalCFunction("TestCloseWindow", (void*)FTestCloseWindow, 1);
  defGlobalCFunction("TestTap", (void*)FTestTap, 2);
  defGlobalCFunction("TestDrag", (void*)FTestDrag, 3);
  defGlobalCFunction("TestPen", (void*)FTestPen, 5);
  defGlobalCFunction("TestLater", (void*)FTestLater, 1);
  defGlobalCFunction("TestPick", (void*)FTestPick, 1);
  defGlobalCFunction("TestMenuSnapshot", (void*)FTestMenuSnapshot, 1);
  defGlobalCFunction("TestPixel", (void*)FTestPixel, 3);
  defGlobalCFunction("TestSnapshot", (void*)FTestSnapshot, 2);
#endif

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

/*
 Installing a package the way the ROM does (ROM NewtonScript, decompiled
 with newtc; see Matt/CLAUDE.md, "How a Newton installs and opens a
 package"):
   RegisterNewPackage(pkgRef, ...)   checks every part's DoNotInstall(), adds
                                     the package to the "Packages" soup, then
   SafeActivatePackageQT(...)        -> ActivatePackage(pkgRef) (C++), which
                                     calls each part's handler; for 'form and
                                     'auto parts (Packages/Parts.cc)
   InstallPart(installInfo)          -> InstallFormPart / InstallAutoPart (in
                                     a try: an error is reported with Notify
                                     and the next part is still installed).
 newtc has no store, soups, or C++ package manager, so installPackage() does
 the C++ steps itself: the DoNotInstall check, then for each 'form and
 'auto part the install info that Packages/Parts.cc makes, handed to the
 ROM's own InstallPart. InstallFormPart copies the install script
 (EnsureInternal: package objects are read-only); line tables follow the
 copy (10.2b). It keeps the app's base view (BuildContext) in the root view.
 Then newtc opens that app right away (no Extras drawer, one app at a
 time): GetRoot().(app):Open().
 */

// The ROM's message (RegisterNewPackage), through GetRoot():Notify(
// kNotifyAlert, "Newton", ...) as in the ROM.
static void reportInstallError(RefArg package, ArrayIndex inPart)
{
  RefVar name(GetFrameSlot(package, MakeSymbol("name")));
  char text[512];
  snprintf(text, sizeof(text), "An error occurred installing the package \"%s\". It may not work with this system. "
           "Contact the software publisher for further information. (Part %u)",
           IsString(name) ? UTF8FromString(name).c_str() : "?", (unsigned)inPart);
  RefVar args(MakeArray(3));
  SetArraySlot(args, 0, MAKEINT(3));   // kNotifyAlert
  SetArraySlot(args, 1, MakeStringFromCString("Newton"));
  SetArraySlot(args, 2, MakeStringFromCString(text));
  DoMessage(RootView(), MakeSymbol("Notify"), args);
}

// The install info for a part, as InstallPart (Packages/Parts.cc) makes it:
// a clone of the ROM's canonicalFramePartInstallInfo. No store: package id
// and type 0, no device (deviceKind 0), no packageStyle (so the Extras
// drawer isn't asked).
static Ref partInstallInfo(RefArg package, RefArg part, ArrayIndex inPart, RefArg inType)
{
  RefVar info(Clone(RA(canonicalFramePartInstallInfo)));
  RefVar name(GetFrameSlot(package, MakeSymbol("name")));
  SetFrameSlot(info, MakeSymbol("partType"), inType);
  SetFrameSlot(info, MakeSymbol("partFrame"), GetFrameSlot(part, MakeSymbol("data")));
  SetFrameSlot(info, MakeSymbol("packageId"), MAKEINT(0));
  SetFrameSlot(info, MakeSymbol("packageName"), IsString(name) ? (Ref)name : MakeStringFromCString(""));
  SetFrameSlot(info, MakeSymbol("partIndex"), MAKEINT(inPart));
  RefVar size(GetFrameSlot(part, MakeSymbol("size")));
  SetFrameSlot(info, MakeSymbol("size"), ISINT(size) ? (Ref)size : MAKEINT(0));
  SetFrameSlot(info, MakeSymbol("packageType"), MAKEINT(0));
  SetFrameSlot(info, MakeSymbol("deviceKind"), MAKEINT(0));
  SetFrameSlot(info, MakeSymbol("deviceNumber"), MAKEINT(0));
  SetFrameSlot(info, MakeSymbol("packageStyle"), NILREF);
  return info;
}

/**
 \brief Install a package like a Newton does (-run, and -dap with a package
 as the program); see above. Nothing is installed if a part's DoNotInstall()
 returns non-nil or throws.
 \return false if `package` is no package.
 */
bool installPackage(RefArg package)
{
  if (!IsFrame(package))
    return false;
  RefVar parts(GetFrameSlot(package, MakeSymbol("part")));
  if (!IsArray(parts))
    return false;
  ArrayIndex count = Length(parts);

  // RegisterNewPackage: may the package be installed?
  bool install = true;
  for (ArrayIndex i = 0; i < count && install; ++i) {
    RefVar partFrame(GetFrameSlot(GetArraySlot(parts, i), MakeSymbol("data")));
    if (!IsFrame(partFrame))
      continue;
    newton_try
    {
      RefVar noArgs(MakeArray(0));
      bool defined;
      if (NOTNIL(DoMessageIfDefined(partFrame, MakeSymbol("DoNotInstall"), noArgs, &defined))) {
        REPprintf("The package's DoNotInstall() returned non-nil: not installed.\n");
        install = false;
      }
    }
    newton_catch_all
    {
      gREPout->exceptionNotify(CurrentException());
      reportInstallError(package, i);
      install = false;
    }
    end_try;
  }
  if (!install)
    return true;

  // ActivatePackage -> the ROM's InstallPart, for each 'form and 'auto part
  RefVar installPart(GetFrameSlot(gFunctionFrame, MakeSymbol("InstallPart")));
  for (ArrayIndex i = 0; i < count; ++i) {
    RefVar part(GetArraySlot(parts, i));
    RefVar type(GetFrameSlot(part, MakeSymbol("type")));
    if (!IsFrame(GetFrameSlot(part, MakeSymbol("data"))) || !IsString(type))
      continue;
    std::string typeName = UTF8FromString(type);
    if (typeName != "form" && typeName != "auto")
      continue;
    RefVar args(MakeArray(1));
    SetArraySlot(args, 0, partInstallInfo(package, part, i, MakeSymbol(typeName.c_str())));
    DoBlock(installPart, args);   // reports its own errors (Notify)
  }

  // newtc runs one app and has no Extras drawer to tap: open the form
  // part's app right away, GetRoot().(app):Open()
  for (ArrayIndex i = 0; i < count; ++i) {
    RefVar part(GetArraySlot(parts, i));
    RefVar type(GetFrameSlot(part, MakeSymbol("type")));
    if (!IsString(type) || UTF8FromString(type) != "form")
      continue;
    RefVar app(GetFrameSlot(GetFrameSlot(part, MakeSymbol("data")), MakeSymbol("app")));
    RefVar view(IsSymbol(app) ? GetFrameSlot(RootView(), app) : NILREF);
    if (!IsFrame(view))
      continue;
    newton_try
    {
      RefVar noArgs(MakeArray(0));
      DoMessage(view, MakeSymbol("Open"), noArgs);
    }
    newton_catch_all
    {
      gREPout->exceptionNotify(CurrentException());
      gAppOpenError = ExceptionText(CurrentException());
    }
    end_try;
    break;
  }
  return true;
}

/**
 \brief Run (open) the current object, like tapping an app icon in NewtonOS.

 -run takes no argument. It works on the object held in ref# by a previous
 -pkg, -nsof, or -script command, so all three can be run the same way,
 e.g. `newtc -nsof app.nsof -run`. For a package, installPackage() runs
 its install scripts and opens its app.

 \todo Run objects from -nsof and -script.
 */
void handleArgRun()
{
  RefVar ref0 = getGlobalRef(0);
  if (!installPackage(ref0))
    std::cerr << "newtc: -run: only packages can be run so far, ignored." << std::endl;
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
 name, and a lineTable slot (global dbgKeepLineNumbers): the source file
 and (pc, line) pairs, for source-level debugging.
 */
void handleArgG()
{
  DefGlobalVar(MakeSymbol("dbgKeepVarNames"), TRUEREF);
  DefGlobalVar(MakeSymbol("dbgKeepLineNumbers"), TRUEREF);
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
 \param port -1: talk on stdin/stdout; else wait for one client on this
   TCP port (-dap-server), and stdin/stdout stay as they are.
 */
void handleArgDap(int port = -1)
{
  if (port < 0) {
    DAPStartIO();     // from here on, stdout carries only DAP messages
  } else if (!DAPStartServer(port)) {
    throw(std::runtime_error("Can't start the DAP server."));
  }
  handleArgDbg();     // its messages still go to the stdio translator (stderr with -dap)
  // Off while no program runs; DAP:WaitForLaunch() sets it from the
  // client's exception filter.
  DefGlobalVar(MakeSymbol("breakOnThrows"), NILREF);
  DAPInstallTranslators();
  // The adapter's own code gets no line tables or variable names (like the
  // tools): line stepping and source frames must not stop in it.
  DefGlobalVar(MakeSymbol("dbgKeepLineNumbers"), NILREF);
  DefGlobalVar(MakeSymbol("dbgKeepVarNames"), NILREF);
  bool loaded = RunEmbeddedScript(gDAPScript);
  handleArgG();
  if (!loaded)
    throw(std::runtime_error("Can't load the debug adapter."));

  RefVar dap(GetGlobalVar(MakeSymbol("DAP")));
  int exitCode = 0;
  newton_try
  {
    RefVar launchArgs(DoMessage(dap, MakeSymbol("WaitForLaunch"), RA(NILREF)));
    if (IsFrame(launchArgs)) {
      std::string program = UTF8FromString(GetFrameSlot(launchArgs, MakeSymbol("program")));
      // a package was loaded by the launch request (DAPLoadPackage)
      RefVar package(GetFrameSlot(dap, MakeSymbol("package")));
      int exceptions = DAPExceptionCount();
      newton_try
      {
        if (IsFrame(package)) {
          DAPSetPolling(true);    // pause, setBreakpoints, ... while it runs
          installPackage(package);
          DAPSetPolling(false);
        } else {
          FILE *f = fopen(program.c_str(), "rb");
          if (f == nullptr) {
            static std::string message;   // ThrowMsg keeps the pointer
            message = "Can't open the program \"" + program + "\"";
            ThrowMsg(message.c_str());
          }
          fclose(f);
          DAPSetPolling(true);    // pause, setBreakpoints, ... while it runs
          handleArgScript(program);
          DAPSetPolling(false);
        }
      }
      newton_catch_all
      {
        gREPout->exceptionNotify(CurrentException());
      }
      end_try;
      DAPSetPolling(false);
      CancelLineStep();   // a step still running when the program ended
      // the program opened a window: it runs on in its events
      RunEventLoop();
      if (DAPExceptionCount() > exceptions)
        exitCode = 1;
      // -stubs report: in the Debug Console while the session still runs
      if (GetStubReport() && gStubNotify) {
        gStubNotify(StubReport().c_str());
        SetStubReport(false);
      }
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
 \brief Decompile the object in current ref into a file, plus a debug map.
 Like -decompile, but writes the source to `filename` and, next to it,
 `<name>.nsdbg`: for each decompiled function its path in the object, a hash
 of its instructions, and its line table into the source (JSON). Loading
 the package with -nsdbg lets the debugger show and step through the
 decompiled source while the package's code stays as it is.
 */
void handleArgODecompile(const std::string &filename)
{
  RefVar ref0 = getGlobalRef(0);
  std::vector<ObjectPrinter::DebugMapFunction> functions;
  {
    std::ofstream source(filename);
    if (!source)
      throw(std::runtime_error("Can't write \"" + filename + "\"."));
    ObjectPrinter p(source);
    p.DebugAST(debugAST_);
    p.DebugBC(debugBC_);
    p.DebugTrap(debugTrap_);
    p.debugMap_ = &functions;
    p.Decompile(ref0);
  }
  char resolved[PATH_MAX];
  std::string sourcePath = realpath(filename.c_str(), resolved) ? resolved : filename;
  std::string mapPath = filename;
  size_t dot = mapPath.rfind('.');
  size_t slash = mapPath.rfind('/');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
    mapPath.erase(dot);
  mapPath += ".nsdbg";
  std::ofstream map(mapPath);
  if (!map)
    throw(std::runtime_error("Can't write \"" + mapPath + "\"."));
  map << "{\"format\": \"nsdbg\", \"version\": 1,\n \"source\": " << QuoteJSON(sourcePath)
      << ",\n \"functions\": [";
  for (size_t i = 0; i < functions.size(); ++i) {
    auto &fn = functions[i];
    map << (i ? "," : "") << "\n  {\"path\": " << fn.path << ", \"hash\": \"" << fn.hash << "\", \"lines\": [";
    for (size_t j = 0; j < fn.lines.size(); ++j)
      map << (j ? ", " : "") << fn.lines[j].first << ", " << fn.lines[j].second;
    map << "]}";
  }
  map << "\n]}\n";
}

/**
 \brief Load a debug map (from -odecompile) for the object in current ref.
 Its functions (found by the hash of their instructions) get the map's line
 tables, so the debugger shows the decompiled source for them.
 */
static std::string readDebugMap(const std::string &filename)
{
  std::ifstream file(filename);
  if (!file)
    throw(std::runtime_error("Can't read \"" + filename + "\"."));
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void handleArgNsdbg(const std::string &filename)
{
  RefVar ref0 = getGlobalRef(0);
  std::string json = readDebugMap(filename);
  int total = 0, matched = 0;
  newton_try
  {
    matched = LoadDebugMap(ref0, json, &total);
  }
  newton_catch_all
  {
    gREPout->exceptionNotify(CurrentException());
  }
  end_try;
  std::cerr << "newtc: debug map \"" << filename << "\": " << matched << " of "
            << total << " functions found" << std::endl;
}

/**
 \brief DAPLoadPackage(path, mapPath) -> {package, map, found, total}.
 For a DAP launch with a package as the program: loads the package (it
 becomes ref0) and its debug map, mapPath or else <path without .pkg>.nsdbg
 if there is one (map: the file loaded, or nil; found/total: functions of
 the map found in the package). Throws with a message if a file can't be read.
 */
static Ref FDAPLoadPackage(RefArg rcvr, RefArg inPath, RefArg inMap)
{
  static std::string message;   // ThrowMsg keeps the pointer
  std::string path = UTF8FromString(inPath);
  std::string mapPath = IsString(inMap) ? UTF8FromString(inMap) : std::string();
  bool mapGiven = !mapPath.empty();
  std::string json;
  try {
    FILE *f = fopen(path.c_str(), "rb");
    if (f == nullptr)
      throw(std::runtime_error("Can't open the program \"" + path + "\""));
    fclose(f);
    handleArgPkg(path);
    if (!mapGiven) {
      mapPath = path;
      size_t dot = mapPath.rfind('.');
      size_t slash = mapPath.rfind('/');
      if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        mapPath.erase(dot);
      mapPath += ".nsdbg";
      FILE *m = fopen(mapPath.c_str(), "rb");
      if (m == nullptr)
        mapPath.clear();
      else
        fclose(m);
    }
    if (!mapPath.empty())
      json = readDebugMap(mapPath);
  } catch (std::exception &e) {
    message = e.what();
    ThrowMsg(message.c_str());
  }
  RefVar package(getGlobalRef(0));
  int total = 0, found = 0;
  if (!mapPath.empty())
    found = LoadDebugMap(package, json, &total);
  RefVar result(AllocateFrame());
  SetFrameSlot(result, MakeSymbol("package"), package);
  SetFrameSlot(result, MakeSymbol("map"), mapPath.empty() ? NILREF : (Ref)MakeStringFromCString(mapPath.c_str()));
  SetFrameSlot(result, MakeSymbol("found"), MAKEINT(found));
  SetFrameSlot(result, MakeSymbol("total"), MAKEINT(total));
  return result;
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
  -nsdbg <filename>       Load a debug map (from -odecompile) for the object held
  -nsof <filename>        Load a Newton streaming object file
  -script <filename>      Read a source file, compile and run it, and hold the result
  -s <script>             Compile and run the script, and hold the result
  -hello                  Compile a "Hello World" app and hold it
  -run                    Run the current object: a package from -pkg is installed
                          (DoNotInstall, then its InstallScript) and its app opened
                          (with FLTK: in a window; newtc runs until it is closed);
                          running -nsof or -script objects is not implemented yet

  Output Commands
  -opkg <filename>        Write the current object to a package file
  -onsof <filename>       Write the current object to a NSOF file
  -opdf <filename>        Write the first part as a PDF file if it is a book
  -print                  Print the current object, functions are just frames
  -decompile              Print the object with all functions decompiled
  -odecompile <filename>  Decompile into a file, and write a debug map next to it
                          (<name>.nsdbg: decompiled source lines for the functions)
  -stats                  Print some package statistics about the object
  -help                   This help text

  Control
  -clear                  Delete all object in the hold
  -pkglist <filename>     Apply following commands to all 'form packages in the file
  -store <filename>       Keep the internal store (the soups) in this file: read it
                          at the start, save it when a change is committed and at
                          the end. Without it, the store is new and empty each time.

  Debugging
  -stubs <mode>           What a call to a built-in function that isn't implemented
                          yet (a stub) does: log (default: say so once, return nil),
                          throw (a NewtonScript exception), quiet (return nil);
                          report: list the stubs called at the end. Several with
                          commas, e.g. throw,report. Also NEWTC_STUBS=<mode>.
  -dbg                    Debug: load Apple's NS Debug Tools and NSD Shortcuts,
                          enable breakpoints and breakOnThrows, and compile the
                          following code with variable names (-g)
  -dap                    Be a debug adapter for VS Code: speak the Debug Adapter
                          Protocol on stdin/stdout, run the program given by the
                          client's "launch" request: a .ns file, or a .pkg that is
                          installed like with -run, with its debug map ("debugMap",
                          else the .nsdbg next to it)
  -dap-server <port>      Like -dap, but wait for one client on this TCP port
                          (localhost), e.g. VS Code with "debugServer": <port>;
                          for running newtc itself in a debugger
  -dap-log <filename>     Write all DAP messages to this file (before -dap or
                          -dap-server)

  Options
  -g                      Compile with debug information: variable names (DebuggerInfo)
                          and line tables; -dbg and -dap do this, too
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
      } else if (cmd == "-dap-server") {
        if (argi>=argc)
          throw(std::runtime_error("-dap-server: port number expected."));
        handleArgDap(std::stoi(argv[argi++]));
      } else if (cmd == "-dap-log") {
        if (argi>=argc)
          throw(std::runtime_error("-dap-log: file name expected."));
        if (!DAPStartLog(argv[argi++]))
          throw(std::runtime_error("-dap-log: can't open the file."));
      } else if (cmd == "-g") {
        handleArgG();
      } else if (cmd == "-store") {
        // set before the store was made (main); just skip the file here
        if (argi >= argc)
          throw(std::runtime_error("-store: expected a file name."));
        argi++;
      } else if (cmd == "-stubs") {
        if (argi >= argc || !SetStubOptions(argv[argi++]))
          throw(std::runtime_error("-stubs: expected log, throw, quiet, or report."));
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
      } else if (cmd == "-odecompile") {
        if (argi>=argc)
          throw(std::runtime_error("-odecompile: file name expected."));
        handleArgODecompile(argv[argi++]);
      } else if (cmd == "-nsdbg") {
        if (argi>=argc)
          throw(std::runtime_error("-nsdbg: file name expected."));
        handleArgNsdbg(argv[argi++]);
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
#if defined(NEWTC_NEWTPLAY)
  // NewtPlay: a package to run, or newtc (Matt/NewtPlay.h)
  std::vector<std::string> playArgs = newtplay::Arguments(argc, argv);
  std::vector<char *> playArgv;
  if (playArgs.empty())
    return 0;   // nothing to run (cancelled, or not a package: said so)
  for (std::string & arg : playArgs)
    playArgv.push_back(&arg[0]);
  playArgv.push_back(nullptr);
  argc = int(playArgs.size());
  argv = playArgv.data();
#endif
  // -dap: stdout carries only DAP messages, also while the arguments before
  // -dap run (VSNewt's "args", e.g. -pkg ... -nsdbg ...)
  for (int i = 1; i < argc; ++i)
    if (strcmp(argv[i], "-dap") == 0)
      DAPStartIO();
  // -store: the internal store's file, before init() makes the store
  for (int i = 1; i + 1 < argc; ++i)
    if (strcmp(argv[i], "-store") == 0)
      CHostStore::SetFile(argv[i + 1]);
  // Stubs that newtc's own start calls are counted, but not the program's
  // business: no log (see Utilities/Unimplemented.h)
  SetStubMode(kStubQuiet);
  if (!init()) {
    printf("newtc: ERROR: Can't initialize.\n");
    return -1;
  }
  SetStubMode(kStubLog);
  EnableTimers();   // calls later: the program's, not the ROM's start's (Host/Timers.h)
  // The ROM's start sets printLength to 16 (the Inspector's limit) once it
  // can read the System soup; newtc prints whole arrays and frames (-dap
  // sets its own limits)
  DefGlobalVar(MakeSymbol("printLength"), NILREF);
  // The display: portrait, as a MessagePad 2x00 held upright (320 by 480,
  // the app area 320 by 434 above the button bar; Matt, 2026-09-30). The
  // ROM's image has the landscape one; its CreateDisplayParams starts from
  // GetRawDisplayParams(GetOrientation()) too (GetAppParams() reads it,
  // and so does the placing of the root view's children, Host/FLTK/Links).
  newton_try
  {
    RefVar orientation(NSCallGlobalFn(MakeSymbol("GetOrientation")));
    DefGlobalVar(MakeSymbol("displayParams"), NSCallGlobalFn(MakeSymbol("GetRawDisplayParams"), orientation));
  }
  newton_catch_all
  { }
  end_try;
  if (const char * stubs = getenv("NEWTC_STUBS"))
    if (!SetStubOptions(stubs))
      fprintf(stderr, "newtc: NEWTC_STUBS: expected log, throw, quiet, or report.\n");

  int ret = 0;
  RefVar symRef0 = MakeSymbol("ref0");
  DefGlobalVar(symRef0, NILREF);

  if (argc == 1) {
    //handleArgHelp(argv[0]);
    return 0;
  }

  ret = handleArgs(argc, argv);
#if defined(NEWTC_NEWTPLAY)
  newtplay::CheckStarted(gAppOpenError);   // the package's app is up, or say why not
#endif
  // a program opened a window: it runs on in its events (see Matt/EventLoop.h)
  RunEventLoop();
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
          viewBounds: { left: 0, top: 50, right: 200, bottom: 120 },
          stepChildren: [
            stepChildren: {
              text: "Say Hello",
              viewBounds: { left: 50, top: 25, right: 150, bottom: 50 },
              buttonClickScript: func()
                ModalConfirm("Hello World of NewtonScript.\n\nHow exciting to see you!", ["OK"]),
              _proto: @226
            }
          ],
          viewQuitScript: func() Print("Goodbye"),
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
