/*
 File: NTKRemote.cc
 A remote Newton for NewtonScript. See NTKRemote.h.
 */

#include "NTKRemote.h"
#include "NTKInspector.h"
#include "NTKTransport.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "Frames/Funcs.h"
#include "REPTranslators.h"
#include "Matt/JSON.h"
#include "Matt/EmbeddedScript.h"
#include "Matt/PackageWriter.h"
#include "Matt/DAP.h"
#include "Frames/Compiler/InputStreams.h"
#include "Frames/Compiler/Compiler.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <map>
#include <vector>
#include <string>

extern void REPExceptionNotify(Exception * inException);

namespace {

double Seconds()
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// UTF-8 (from the text the Newton sends) as a NewtonScript string.
Ref StringFromUTF8(const std::string &inText)
{
  std::vector<UniChar> text;
  for (size_t i = 0; i < inText.size(); ) {
    unsigned c = (uint8_t)inText[i++];
    if (c >= 0xE0 && i + 1 < inText.size()) {
      c = ((c & 0x0F) << 12) | (((uint8_t)inText[i] & 0x3F) << 6) | ((uint8_t)inText[i + 1] & 0x3F);
      i += 2;
    } else if (c >= 0xC0 && i < inText.size()) {
      c = ((c & 0x1F) << 6) | ((uint8_t)inText[i] & 0x3F);
      i += 1;
    }
    text.push_back((UniChar)c);
  }
  text.push_back(0);
  return MakeString(text.data());
}

// Throw evt.ex.msg with a message (kept: the exception holds the pointer).
[[noreturn]] void Fail(const std::string &inMessage)
{
  static std::string message;
  message = inMessage;
  ThrowMsg(message.c_str());
  for (;;) {}       // ThrowMsg doesn't return
}


class Remote : public NTKInspector::Listener
{
public:
  NTKInspector inspector;
  bool started = false;
  RefStruct handler;
  int callsSent = 0;                    // 'code' calls, numbered from 1
  std::map<int, int> open;              // calls not answered: number -> break loop depth when sent
  int depth = 0;                        // the Newton's break loop depth ('eext' / 'bext')
  std::map<int, RefStruct> results;     // replies nobody took yet
  int resultsAwaited = 0;               // 'rslt' for 'pkg '/'pkgX' wanted ...
  std::deque<long> results2;            // ... and got
  bool connectionLost = false;

  // Call the handler's method, if there is one. False if not.
  bool notify(const char *inMethod, RefArg inArgs)
  {
    RefVar h(handler);
    if (!IsFrame(h))
      return false;
    bool defined = false;
    DoMessageIfDefined(h, MakeSymbol(inMethod), inArgs, &defined);
    return defined;
  }

  void ntkConnected(const std::string &inPeer) override
  {
    connectionLost = false;
    callsSent = 0;
    depth = 0;
    open.clear();
    results.clear();
    RefVar args(MakeArray(1));
    SetArraySlot(args, 0, MakeStringFromCString(inPeer.c_str()));
    if (!notify("Connected", args)) {
      printf("ntk: connected (%s)\n", inPeer.c_str());
      fflush(stdout);
    }
  }

  void ntkDisconnected(const std::string &inReason) override
  {
    connectionLost = true;
    open.clear();
    depth = 0;
    RefVar args(MakeArray(1));
    SetArraySlot(args, 0, MakeStringFromCString(inReason.c_str()));
    if (!notify("Disconnected", args)) {
      printf("ntk: disconnected (%s)\n", inReason.c_str());
      fflush(stdout);
    }
  }

  void ntkText(const std::string &inUTF8) override
  {
    RefVar args(MakeArray(1));
    SetArraySlot(args, 0, StringFromUTF8(inUTF8));
    if (!notify("Text", args)) {
      fputs(inUTF8.c_str(), stdout);
      fflush(stdout);
    }
  }

  void ntkResult(long inError, uint32_t inCommand) override
  {
    if ((inCommand == NTKCommand("pkg ") || inCommand == NTKCommand("pkgX")) && resultsAwaited > 0) {
      resultsAwaited--;
      results2.push_back(inError);
      return;
    }
    if (inError == 0)
      return;
    RefVar args(MakeArray(2));
    SetArraySlot(args, 0, MAKEINT(inError));
    SetArraySlot(args, 1, MakeStringFromCString(NTKCommandName(inCommand).c_str()));
    if (!notify("Result", args)) {
      printf("ntk: error %ld ('%s')\n", inError, NTKCommandName(inCommand).c_str());
      fflush(stdout);
    }
  }

  void ntkObject(const std::string &inCommand, RefArg inObject) override
  {
    RefVar args(MakeArray(2));
    SetArraySlot(args, 0, MakeStringFromCString(inCommand.c_str()));
    SetArraySlot(args, 1, inObject);
    if (!notify("Object", args)) {
      printf("ntk: %s ", inCommand.c_str());
      fflush(stdout);
      PrintObject(inObject, 0);
      gREPout->print("\n");
      gREPout->flush();
    }
  }

  // Which call a reply answers: replies carry no number. A call sent in a
  // break loop is answered before the call that runs into the break loop
  // (the Newton handles it there), calls at one depth in order: the oldest
  // open call of the deepest depth that has any.
  void ntkCodeResult(RefArg inResult) override
  {
    int number = -1, deepest = -1;
    for (auto &call : open)
      if (call.second > deepest) {
        deepest = call.second;
        number = call.first;
      }
    if (number < 0)
      return;                           // nobody asked
    open.erase(number);
    results[number] = RefStruct(inResult);
  }

  int send(RefArg inFunction)
  {
    int number = ++callsSent;
    open[number] = depth;
    inspector.call(inFunction);
    return number;
  }

  void ntkException(Exception &inException) override
  {
    RefVar data;
    if (Subexception(inException.name, exMessage))
      data = MakeStringFromCString(inException.data ? (const char *)inException.data : "");
    else if (Subexception(inException.name, exRefException))
      data = *(RefStruct *)inException.data;
    else
      data = MAKEINT((long)inException.data);
    RefVar args(MakeArray(2));
    SetArraySlot(args, 0, MakeStringFromCString(inException.name ? inException.name : ""));
    SetArraySlot(args, 1, data);
    if (!notify("Exception", args)) {
      fflush(stdout);
      REPExceptionNotify(&inException);
      gREPout->flush();
    }
  }

  void ntkBreakLoop(bool inEntered) override
  {
    if (inEntered)
      depth++;
    else if (depth > 0)
      depth--;
    RefVar args(MakeArray(1));
    SetArraySlot(args, 0, inEntered ? TRUEREF : NILREF);
    if (!notify("BreakLoop", args)) {
      printf("ntk: break loop %s\n", inEntered ? "entered" : "left");
      fflush(stdout);
    }
  }

  // Handle what comes, for at most inSeconds, until inDone() says so.
  // True if inDone() said so.
  template <typename Done>
  bool waitUntil(double inSeconds, Done inDone)
  {
    double end = Seconds() + inSeconds;
    for (;;) {
      if (inDone())
        return true;
      double left = end - Seconds();
      if (left <= 0)
        return false;
      int fd = inspector.notifyFd();
      bool ready;
      int ms = (int)(left * 1000) + 1;
      if (NTKWaitReadable(&fd, 1, ms < 100 ? ms : 100, &ready) < 0)
        return false;
      if (ready)
        inspector.handleEvents(*this);
      inspector.tick();
    }
  }

  double timeout()
  {
    RefVar t(GetGlobalVar(MakeSymbol("ntkTimeout")));
    return ISINT(t) ? (double)RINT(t) : 30.0;
  }

  void requireConnection()
  {
    if (!started)
      Fail("No remote Newton: call NTKOpen(target) first");
    if (!inspector.isConnected())
      Fail("The remote Newton is not connected");
  }
};

Remote *gRemote = nullptr;

Remote &R()
{
  if (!gRemote)
    gRemote = new Remote;
  return *gRemote;
}

} // namespace


Ref FNTKOpen(RefArg rcvr, RefArg inTarget)
{
  if (!IsString(inTarget))
    return MakeStringFromCString("NTKOpen: the target must be a string");
  Remote &r = R();
  if (r.started) {
    r.inspector.stop();
    r.started = false;
  }
  std::string error;
  if (!r.inspector.start(UTF8FromString(inTarget), error))
    return MakeStringFromCString(error.c_str());
  r.started = true;
  r.connectionLost = false;
  return NILREF;
}


Ref FNTKWaitConnected(RefArg rcvr, RefArg inSeconds)
{
  Remote &r = R();
  if (!r.started)
    return NILREF;
  double seconds = ISINT(inSeconds) ? (double)RINT(inSeconds) : 30.0;
  return r.waitUntil(seconds, [&] { return r.inspector.isConnected(); }) ? TRUEREF : NILREF;
}


Ref FNTKIsConnected(RefArg rcvr)
{
  return (gRemote && gRemote->started && gRemote->inspector.isConnected()) ? TRUEREF : NILREF;
}


Ref FNTKCall(RefArg rcvr, RefArg inFunction)
{
  Remote &r = R();
  r.requireConnection();
  if (!IsFunction(inFunction))
    Fail("NTKCall: a function expected");
  int number = r.send(inFunction);
  bool answered = r.waitUntil(r.timeout(), [&] {
    return r.results.count(number) > 0 || r.connectionLost;
  });
  if (r.results.count(number) == 0) {
    if (r.connectionLost)
      Fail("The remote Newton ended the connection");
    Fail(answered ? "No answer from the remote Newton" : "No answer from the remote Newton in time");
  }
  RefVar result(r.results[number]);
  r.results.erase(number);
  return result;
}


Ref FNTKEvaluate(RefArg rcvr, RefArg inFunction)
{
  Remote &r = R();
  r.requireConnection();
  if (!IsFunction(inFunction))
    Fail("NTKEvaluate: a function expected");
  r.inspector.evaluate(inFunction);
  return NILREF;
}


// Send 'pkg ' or 'pkgX' and wait for the Newton's 'rslt'.
static Ref WaitForResult(Remote &r)
{
  r.resultsAwaited++;
  bool done = r.waitUntil(r.timeout() * 4, [&] { return !r.results2.empty() || r.connectionLost; });
  if (!done || r.results2.empty()) {
    r.resultsAwaited = 0;
    Fail(r.connectionLost ? "The remote Newton ended the connection" : "No answer from the remote Newton");
  }
  long error = r.results2.front();
  r.results2.pop_front();
  return MAKEINT(error);
}


Ref FNTKInstallPackage(RefArg rcvr, RefArg inPackage)
{
  Remote &r = R();
  r.requireConnection();
  if (!IsBinary(inPackage))
    Fail("NTKInstallPackage: the package's bytes expected (a binary)");
  const uint8_t *p = (const uint8_t *)BinaryData(inPackage);
  r.inspector.loadPackage(std::vector<uint8_t>(p, p + Length(inPackage)));
  return WaitForResult(r);
}


Ref FNTKDeletePackage(RefArg rcvr, RefArg inName)
{
  Remote &r = R();
  r.requireConnection();
  if (!IsString(inName))
    Fail("NTKDeletePackage: the package's name expected");
  r.inspector.deletePackage(UTF8FromString(inName));
  return WaitForResult(r);
}


Ref FNTKPoll(RefArg rcvr, RefArg inSeconds)
{
  Remote &r = R();
  if (!r.started)
    return NILREF;
  double seconds = ISINT(inSeconds) ? (double)RINT(inSeconds) : 0.0;
  r.inspector.tick();
  int fd = r.inspector.notifyFd();
  bool ready = false;
  int ms = (int)(seconds * 1000);
  if (NTKWaitReadable(&fd, 1, ms, &ready) > 0 && ready) {
    r.inspector.handleEvents(r);
    return TRUEREF;
  }
  return NILREF;
}


Ref FNTKSetHandler(RefArg rcvr, RefArg inHandler)
{
  R().handler = inHandler;
  return NILREF;
}


Ref FNTKClose(RefArg rcvr)
{
  if (!gRemote || !gRemote->started)
    return NILREF;
  Remote &r = *gRemote;
  if (r.inspector.isConnected()) {
    // the Newton answers 'term' by ending the link itself (an LD); cutting
    // it first makes Toolkit say "connection lost"
    r.inspector.terminate();
    r.waitUntil(3.0, [&] { return r.connectionLost; });
  }
  r.inspector.stop();
  r.started = false;
  return NILREF;
}


// NTK's LoadDataFile(fileName, class): a file's bytes as a binary object of
// that class (a symbol; nil: 'binary). Not part of NewtonOS: NTK offered it
// to build scripts; here, e.g., to send a package to a Newton.
Ref FLoadDataFile(RefArg rcvr, RefArg inFileName, RefArg inClass)
{
  if (!IsString(inFileName))
    Fail("LoadDataFile: a file name expected");
  std::string path = UTF8FromString(inFileName);
  FILE *f = fopen(path.c_str(), "rb");
  if (!f)
    Fail("LoadDataFile: can't read " + path);
  std::vector<uint8_t> data;
  uint8_t chunk[8192];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
    data.insert(data.end(), chunk, chunk + n);
  fclose(f);
  RefVar cls(IsSymbol(inClass) ? (Ref)inClass : MakeSymbol("binary"));
  RefVar binary(AllocateBinary(cls, data.size()));
  if (!data.empty())
    memcpy(BinaryData(binary), data.data(), data.size());
  return binary;
}


// NTKMakePackage(frame): the package a frame describes (as -opkg writes it),
// as a binary of class 'package.
Ref FNTKMakePackage(RefArg rcvr, RefArg inPackage)
{
  std::string bytes = writePackageToMemory(inPackage);
  RefVar binary(AllocateBinary(MakeSymbol("package"), bytes.size()));
  memcpy(BinaryData(binary), bytes.data(), bytes.size());
  return binary;
}


// NTKPackageName(binary): the name in a package's directory (what 'pkgX'
// needs), or nil if it isn't a package.
Ref FNTKPackageName(RefArg rcvr, RefArg inPackage)
{
  if (!IsBinary(inPackage))
    return NILREF;
  const uint8_t *p = (const uint8_t *)BinaryData(inPackage);
  size_t size = Length(inPackage);
  if (size < 52 || (memcmp(p, "package0", 8) != 0 && memcmp(p, "package1", 8) != 0))
    return NILREF;
  auto u16 = [&](size_t i) { return (size_t)(p[i] << 8 | p[i + 1]); };
  auto u32 = [&](size_t i) { return (size_t)p[i] << 24 | (size_t)p[i + 1] << 16 | (size_t)p[i + 2] << 8 | p[i + 3]; };
  size_t offset = u16(24), length = u16(26);   // the name's InfoRef
  size_t start = 52 + u32(48) * 32 + offset;   // after the part entries
  if (start + length > size || length < 2)
    return NILREF;
  std::vector<UniChar> name;
  for (size_t i = 0; i + 1 < length; i += 2) {
    UniChar c = (UniChar)u16(start + i);
    if (c == 0)
      break;
    name.push_back(c);
  }
  name.push_back(0);
  return MakeString(name.data());
}


extern const EmbeddedScript gAgentScript;
extern const EmbeddedScript gRemoteScript;

// NTKLibrary(): the global NTKRemote (Matt/Debugger/Remote.ns), loaded with
// the agent (Agent.ns) the first time; without line tables and variable
// names, so the agent package stays small.
Ref FNTKLibrary(RefArg rcvr)
{
  RefVar library(GetGlobalVar(MakeSymbol("NTKRemote")));
  if (IsFrame(library))
    return library;
  RefVar names(GetGlobalVar(MakeSymbol("dbgKeepVarNames")));
  RefVar lines(GetGlobalVar(MakeSymbol("dbgKeepLineNumbers")));
  DefGlobalVar(MakeSymbol("dbgKeepVarNames"), NILREF);
  DefGlobalVar(MakeSymbol("dbgKeepLineNumbers"), NILREF);
  bool ok = RunEmbeddedScript(gAgentScript) && RunEmbeddedScript(gRemoteScript);
  DefGlobalVar(MakeSymbol("dbgKeepVarNames"), names);
  DefGlobalVar(MakeSymbol("dbgKeepLineNumbers"), lines);
  if (!ok)
    Fail("NTKLibrary: can't load Remote.ns");
  library = GetGlobalVar(MakeSymbol("NTKRemote"));
  const char *env = getenv("NEWTC_NSDT_PACKAGE");
  std::string tools;
  if (env && *env) {
    tools = env;
  } else if (const char *home = getenv("HOME")) {
    tools = std::string(home) + "/Azureus/unna2/apple/development/NTK/macntk/NTK 1.6.4b3/"
            "NTK 1.6.4b3/Newton Debug Tools 2.2/NS Debug Tools.pkg";
    struct stat info;
    if (stat(tools.c_str(), &info) != 0)
      tools.clear();
  }
  if (!tools.empty())
    SetFrameSlot(library, MakeSymbol("debugToolsPackage"), MakeStringFromCString(tools.c_str()));
  return library;
}


// NTKCallAsync(fn): send fn as 'code' and return its number at once;
// NTKCallResult(number) is {value: <the result>} once the reply is there
// (taken), else nil. For calls that may wait a long time (the Newton only
// answers when its REP is idle), while DAP requests must still be served.
Ref FNTKCallAsync(RefArg rcvr, RefArg inFunction)
{
  Remote &r = R();
  r.requireConnection();
  if (!IsFunction(inFunction))
    Fail("NTKCallAsync: a function expected");
  int number = r.send(inFunction);
  return MAKEINT(number);
}


Ref FNTKCallResult(RefArg rcvr, RefArg inNumber)
{
  Remote &r = R();
  if (!ISINT(inNumber))
    return NILREF;
  int number = (int)RINT(inNumber);
  auto it = r.results.find(number);
  if (it == r.results.end())
    return NILREF;
  RefVar answer(AllocateFrame());
  SetFrameSlot(answer, MakeSymbol("value"), RefVar(it->second));
  r.results.erase(it);
  return answer;
}


// NTKWaitAny(seconds): wait for the Newton or the DAP client, at most that
// long. Handles what the Newton sent (handler) and returns 'ntk; 'dap if a
// DAP request (or the end of the client's input) is waiting; nil if
// nothing came.
Ref FNTKWaitAny(RefArg rcvr, RefArg inSeconds)
{
  Remote &r = R();
  if (r.started)
    r.inspector.tick();
  if (DAPInputWaiting())
    return MakeSymbol("dap");
  double seconds = ISINT(inSeconds) ? (double)RINT(inSeconds) : 0.0;
  int fds[2] = { r.started ? r.inspector.notifyFd() : -1, DAPInputFd() };
  bool ready[2];
  if (NTKWaitReadable(fds, 2, (int)(seconds * 1000), ready) <= 0)
    return NILREF;
  if (ready[0]) {
    r.inspector.handleEvents(r);
    return MakeSymbol("ntk");
  }
  return ready[1] ? MakeSymbol("dap") : NILREF;
}


// NTKCompileFile(path): a NewtonScript file as one code block to run on a
// Newton with 'code': the whole file inside a try, so an exception comes
// back as {|DAP error|: <the exception>} (an exception in a 'code' block
// would end the connection, as the ROM does). The Newton runs a 'code'
// block at the top level (InterpretBlock), so assignments to new variables
// make globals, as in -script. The wrapper starts on the first line: the
// line numbers (line tables, with -g) stay those of the file. A compile
// error throws with the file and line, as -script shows.
Ref FNTKCompileFile(RefArg rcvr, RefArg inPath)
{
  if (!IsString(inPath))
    Fail("NTKCompileFile: a file name expected");
  std::string path = UTF8FromString(inPath);
  FILE *file = fopen(path.c_str(), "rb");
  if (!file)
    Fail("Can't open the program \"" + path + "\"");
  std::string text = "begin local |dap result|; try |dap result| := begin ";
  char chunk[8192];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), file)) > 0)
    text.append(chunk, n);
  fclose(file);
  text += "\nend onexception |evt.ex| do |dap result| := {|DAP error|: CurrentException()}; "
          "|dap result| end";
  FILE *fd = fmemopen((void *)text.data(), text.size(), "r");
  if (!fd)
    Fail("NTKCompileFile: out of memory");
  RefVar blocks(MakeArray(0));
  CStdioInputStream stream(fd, path.c_str());
  CCompiler compiler(&stream, true);
  bool failed = false;
  newton_try
  {
    while (!feof(fd)) {
      RefVar block(compiler.compile());
      if (NOTNIL(block))
        AddArraySlot(blocks, block);
    }
  }
  newton_catch(exRefException)
  {
    RefVar data(*(RefStruct *)CurrentException()->data);
    if (IsFrame(data) && ISNIL(GetFrameSlot(data, MakeSymbol("filename")))) {
      SetFrameSlot(data, MakeSymbol("filename"), MakeStringFromCString(stream.fileName()));
      SetFrameSlot(data, MakeSymbol("lineNumber"), MAKEINT(compiler.lineNo()));
    }
    gREPout->exceptionNotify(CurrentException());
    failed = true;
  }
  newton_catch_all
  {
    gREPout->exceptionNotify(CurrentException());
    failed = true;
  }
  end_try;
  fclose(fd);
  if (failed)
    Fail("The program has errors: " + path);
  return blocks;
}
