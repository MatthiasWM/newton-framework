/*
 File: DAP.cc

 The Debug Adapter Protocol for `newtc -dap`: message framing and the REP
 translators. See DAP.h; the protocol itself is Matt/Debugger/DAP.ns.
 */

#include "Matt/DAP.h"
#include "Matt/JSON.h"
#include "Matt/LineTables.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "Frames/Funcs.h"
#include "Frames/Interpreter.h"
#include "Frames/Compiler/Compiler.h"
#include "REPTranslators.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#define read _read
#else
#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

int gDAPInFd = 0;             // stdin
FILE *gDAPOut = stdout;
long gDAPSeq = 0;             // "seq" of the last message sent
bool gDAPInputEnded = false;  // end of stdin: the client is gone
int gDAPExceptionCount = 0;
bool gDAPPolling = false;     // look for requests while the program runs
int gDAPBreakLoopDepth = 0;   // > 0 while stopped in a break loop
bool gDAPPauseRequested = false;
FILE *gDAPLog = nullptr;      // -dap-log: every message, both ways

// -dap-log: one line per message, as in the test transcripts: "-> " from
// the client to newtc, "<- " from newtc to the client.
void LogMessage(const char *direction, const std::string &json)
{
  if (gDAPLog == nullptr)
    return;
  fprintf(gDAPLog, "%s%s\n", direction, json.c_str());
  fflush(gDAPLog);
}

/*------------------------------------------------------------------------------
  Input. newtc reads stdin itself (not through a FILE), so that
  InputWaiting() can tell whether a message is waiting without blocking:
  data in a FILE's buffer is invisible to select().
------------------------------------------------------------------------------*/

std::string gInBuffer;        // read, not yet used

// Read more input (blocks). False at end of input.
bool FillBuffer(void)
{
  char buf[4096];
  long n = read(gDAPInFd, buf, sizeof(buf));
  if (n <= 0)
    return false;
  gInBuffer.append(buf, (size_t)n);
  return true;
}

// True if input is waiting (or has ended), without blocking.
bool InputWaiting(void)
{
  if (!gInBuffer.empty())
    return true;
#if defined(_WIN32)
  DWORD available = 0;
  if (!PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE), NULL, 0, NULL, &available, NULL))
    return true;    // not a pipe, or closed: let the reader find out
  return available > 0;
#else
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(gDAPInFd, &fds);
  struct timeval noWait = { 0, 0 };
  return select(gDAPInFd + 1, &fds, NULL, NULL, &noWait) > 0;
#endif
}

/*------------------------------------------------------------------------------
  Read one message: header lines up to an empty line, then Content-Length
  bytes of JSON. Header lines end with CRLF; a plain LF is accepted, too.
  Returns false at end of input.
------------------------------------------------------------------------------*/

bool ReadMessage(std::string &outJSON)
{
  long length = -1;
  for (;;) {
    size_t eol;
    while ((eol = gInBuffer.find('\n')) == std::string::npos)
      if (!FillBuffer())
        return false;
    std::string line = gInBuffer.substr(0, eol);
    gInBuffer.erase(0, eol + 1);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty()) {
      if (length >= 0)
        break;
      continue;   // no header yet: skip stray empty lines
    }
    if (strncasecmp(line.c_str(), "Content-Length:", 15) == 0)
      length = strtol(line.c_str() + 15, nullptr, 10);
  }
  while (gInBuffer.size() < (size_t)length)
    if (!FillBuffer())
      return false;
  outJSON = gInBuffer.substr(0, (size_t)length);
  gInBuffer.erase(0, (size_t)length);
  LogMessage("-> ", outJSON);
  return true;
}

/*------------------------------------------------------------------------------
  Send one message. `json` is a JSON object; "seq" is added as its first
  member.
------------------------------------------------------------------------------*/

void SendMessage(const std::string &json)
{
  std::string message = "{\"seq\":" + std::to_string(++gDAPSeq);
  if (json.size() > 2)
    message += ",";
  message += json.substr(1);
  fprintf(gDAPOut, "Content-Length: %zu\r\n\r\n", message.size());
  fwrite(message.data(), 1, message.size(), gDAPOut);
  fflush(gDAPOut);
  LogMessage("<- ", message);
}

/*------------------------------------------------------------------------------
  Read the next message and hand it to the NewtonScript side,
  DAP:Dispatch(message). Returns false at end of input.
------------------------------------------------------------------------------*/

bool ReceiveAndDispatch(void)
{
  std::string json;
  if (!ReadMessage(json)) {
    gDAPInputEnded = true;
    return false;
  }
  RefVar message(ParseJSON(json.data(), json.size()));
  RefVar args(MakeArray(1));
  SetArraySlot(args, 0, message);
  DoMessage(GetGlobalVar(MakeSymbol("DAP")), MakeSymbol("Dispatch"), args);
  return true;
}

/*------------------------------------------------------------------------------
  The interpreter's poll (gDebuggerPoll, Interpreter.h): while the program
  runs, handle the requests that are waiting. True if one of them was
  "pause": the interpreter then stops in a break loop.
------------------------------------------------------------------------------*/

bool DAPPoll(void)
{
  static bool inPoll = false;
  if (!gDAPPolling || gDAPBreakLoopDepth > 0 || inPoll || gDAPInputEnded)
    return false;
  inPoll = true;
  newton_try
  {
    while (InputWaiting() && ReceiveAndDispatch())
      ;
  }
  newton_catch_all
  {
    fprintf(stderr, "newtc: error handling a DAP request while running\n");
  }
  end_try;
  inPoll = false;
  bool pause = gDAPPauseRequested;
  gDAPPauseRequested = false;
  return pause;
}

/*------------------------------------------------------------------------------
  The compiler's statement hook (gCompiledStatementHook): the program's next
  top-level statement is compiled and about to run; DAP:NewCode() installs
  the pending breakpoints that have code now.
------------------------------------------------------------------------------*/

void DAPCompiledStatement(RefArg inCodeBlock)
{
  if (!gDAPPolling)   // only while the program runs
    return;
  newton_try
  {
    DoMessage(GetGlobalVar(MakeSymbol("DAP")), MakeSymbol("NewCode"), RA(NILREF));
  }
  newton_catch_all
  {
    fprintf(stderr, "newtc: error installing breakpoints in new code\n");
  }
  end_try;
}

} // namespace


void DAPSetPolling(bool inPolling)
{
  gDAPPolling = inPolling;
  gDebuggerPoll = DAPPoll;
  gCompiledStatementHook = DAPCompiledStatement;
}


bool DAPStartLog(const char *inPath)
{
  gDAPLog = fopen(inPath, "w");
  return gDAPLog != nullptr;
}


bool DAPStartServer(int inPort)
{
#if defined(_WIN32)
  fprintf(stderr, "newtc: -dap-server is not supported on Windows yet\n");
  return false;
#else
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0)
    return false;
  int yes = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  struct sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // this machine only
  address.sin_port = htons((uint16_t)inPort);
  if (bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0
   || listen(listener, 1) < 0) {
    perror("newtc: -dap-server");
    close(listener);
    return false;
  }
  fprintf(stderr, "newtc: waiting for a DAP client on port %d\n", inPort);
  int connection = accept(listener, nullptr, nullptr);
  close(listener);
  if (connection < 0)
    return false;
  fprintf(stderr, "newtc: DAP client connected\n");
  signal(SIGPIPE, SIG_IGN);   // a client that goes away is end of input
  gDAPInFd = connection;
  gDAPOut = fdopen(dup(connection), "wb");
  return gDAPOut != nullptr;
#endif
}


void DAPStartIO(void)
{
  static bool started = false;
  if (started)
    return;     // done already (newtc does it first thing when -dap is given)
  started = true;
  fflush(stdout);
#if defined(_WIN32)
  _setmode(_fileno(stdin), _O_BINARY);
#endif
  int dapFd = dup(fileno(stdout));
  dup2(fileno(stderr), fileno(stdout));
  gDAPOut = fdopen(dapFd, "wb");
#if defined(_WIN32)
  _setmode(dapFd, _O_BINARY);
#endif
}


int DAPExceptionCount(void)
{
  return gDAPExceptionCount;
}


/*------------------------------------------------------------------------------
  NewtonScript functions
------------------------------------------------------------------------------*/

// DAPCallWithSelf(fn, receiver, args): call fn with self = receiver (for
// "evaluate": code in a stopped method sees its slots like the method does).
Ref FDAPCallWithSelf(RefArg rcvr, RefArg inFn, RefArg inReceiver, RefArg inArgs)
{
  return DoScript(inReceiver, inFn, inArgs);
}


extern "C" const char * GetFramesErrorString(NewtonErr inErr);

// DAPErrorText(errorCode): the text the REPL shows for an error code, e.g.
// "Undefined variable" for -48807; nil if there is none.
Ref FDAPErrorText(RefArg rcvr, RefArg inCode)
{
  if (!ISINT(inCode))
    return NILREF;
  const char * text = GetFramesErrorString((NewtonErr)RINT(inCode));
  return text ? MakeStringFromCString(text) : NILREF;
}


Ref FDAPPause(RefArg rcvr)
{
  if (gDAPPolling && gDAPBreakLoopDepth == 0)
    gDAPPauseRequested = true;
  return NILREF;
}


Ref FDAPReceive(RefArg rcvr)
{
  std::string json;
  if (!ReadMessage(json)) {
    gDAPInputEnded = true;
    return NILREF;
  }
  return ParseJSON(json.data(), json.size());
}


Ref FDAPSend(RefArg rcvr, RefArg inMessage)
{
  if (!IsFrame(inMessage))
    ThrowBadTypeWithFrameData(kNSErrNotAFrame, inMessage);
  gREPout->flush();   // pending output goes first
  SendMessage(ToJSON(inMessage));
  return NILREF;
}


Ref FDAPExit(RefArg rcvr, RefArg inCode)
{
  gREPout->flush();
  fflush(gDAPOut);
  fflush(stderr);
  exit(ISINT(inCode) ? (int)RINT(inCode) : 0);
}


#pragma mark -
/*------------------------------------------------------------------------------
  P D A P O u t T r a n s l a t o r
  Everything NewtonScript prints becomes a DAP "output" event, one per line
  (or at flush()). Exceptions have the category "stderr", the rest "stdout".
------------------------------------------------------------------------------*/

extern void PrintObjectAux(Ref inObj, int indent, int inDepth);

PROTOCOL PDAPOutTranslator : public POutTranslator
{
public:
  PROTOCOL_IMPL_HEADER_MACRO(PDAPOutTranslator)
  PDAPOutTranslator * make(void) override;
  void        destroy(void) override;

  NewtonErr   init(void * inContext) override;
  Timeout     idle(void) override;
  void        consumeFrame(RefArg inObj, int inDepth, int indent) override;
  void        prompt(int inLevel) override;
  int         print(const char * inFormat, ...) override;
  int         vprint(const char * inFormat, va_list args) override;
  int         putc(int inCh) override;
  void        flush(void) override;

  void        enterBreakLoop(int) override;
  void        exitBreakLoop(void) override;

  void        stackTrace(void * interpreter) override;
  void        exceptionNotify(Exception * inException) override;

  std::string printToString(RefArg inObj);   // not part of the protocol
  std::string callToString(RefArg inFn);     // not part of the protocol

private:
  void        write(const char * inText, size_t inLen);

  std::string * fText;          // text not sent yet
  const char *  fCategory;      // "stdout" or "stderr"
  bool          fLastWasCR;
  std::string * fCapture;       // if set, write() also appends here
  bool          fCaptureOnly;   // and doesn't send anything
  std::string * fStopText;      // the exception that stops in the break loop
};

PDAPOutTranslator * gDAPOutTranslator = nullptr;

const CClassInfo *
PDAPOutTranslator::classInfo(void)
{
  static CClassInfo _classInfo = {
    .fName = "PDAPOutTranslator",
    .fInterface = "POutTranslator",
    .fSignature = "\0",
    .fSizeofProc = []()->size_t { return sizeof(PDAPOutTranslator); },
    .fAllocProc = []()->CProtocol* { return new PDAPOutTranslator(); },
    .fFreeProc = [](CProtocol* p)->void { delete p; },
    .fVersion = 0,
    .fFlags = 0
  };
  return &_classInfo;
}

PROTOCOL_IMPL_SOURCE_MACRO(PDAPOutTranslator)

PDAPOutTranslator *
PDAPOutTranslator::make(void)
{
  fText = new std::string;
  fCategory = "stdout";
  fLastWasCR = false;
  fCapture = nullptr;
  fCaptureOnly = false;
  fStopText = new std::string;
  return this;
}

void
PDAPOutTranslator::destroy(void)
{
  delete fText, fText = nullptr;
  delete fStopText, fStopText = nullptr;
}

NewtonErr
PDAPOutTranslator::init(void * inArgs)
{ return noErr; }

Timeout
PDAPOutTranslator::idle(void)
{ return kNoTimeout; }

void
PDAPOutTranslator::consumeFrame(RefArg inObj, int inDepth, int indent)
{
  PrintObjectAux(inObj, indent, inDepth);
}

void
PDAPOutTranslator::prompt(int inLevel)
{ }

// Like PStdioOutTranslator::write(): CR and CRLF become LF. A complete line
// is sent at once.
void
PDAPOutTranslator::write(const char * inText, size_t inLen)
{
  for (size_t i = 0; i < inLen; ++i) {
    char ch = inText[i];
    if (ch == '\n' && fLastWasCR) {
      fLastWasCR = false;
      continue;
    }
    fLastWasCR = (ch == '\r');
    if (fLastWasCR)
      ch = '\n';
    if (fCapture != nullptr)
      *fCapture += ch;
    if (fCaptureOnly)
      continue;
    *fText += ch;
    if (ch == '\n')
      flush();
  }
}

int
PDAPOutTranslator::vprint(const char * inFormat, va_list args)
{
  char buf[256];
  va_list argsCopy;
  va_copy(argsCopy, args);
  int len = vsnprintf(buf, sizeof(buf), inFormat, args);
  if (len >= (int)sizeof(buf)) {
    std::string big((size_t)len + 1, 0);
    vsnprintf(&big[0], big.size(), inFormat, argsCopy);
    write(big.data(), (size_t)len);
  } else if (len > 0) {
    write(buf, (size_t)len);
  }
  va_end(argsCopy);
  return len;
}

int
PDAPOutTranslator::print(const char * inFormat, ...)
{
  va_list args;
  va_start(args, inFormat);
  int result = vprint(inFormat, args);
  va_end(args);
  return result;
}

int
PDAPOutTranslator::putc(int inCh)
{
  char ch = (char)inCh;
  write(&ch, 1);
  return inCh;
}

// Sends the pending text. Builds the JSON as a C++ string: this runs while
// the object printer walks NewtonScript objects, so it must not allocate any.
void
PDAPOutTranslator::flush(void)
{
  if (fText->empty())
    return;
  std::string text;
  text.swap(*fText);
  SendMessage(std::string("{\"type\":\"event\",\"event\":\"output\",\"body\":{\"category\":\"")
              + fCategory + "\",\"output\":" + QuoteJSON(text) + "}}");
}

// The program stopped: tell the client why, DAP:Stopped(reason, text).
// gBreakLoopReason says why (Interpreter.h); for an exception, text is the
// message exceptionNotify() printed just before.
void
PDAPOutTranslator::enterBreakLoop(int inLevel)
{
  const char * reason = "breakloop";  // the program called BreakLoop()
  switch (gBreakLoopReason) {
    case kBreakLoopCalled: break;
    case kBreakLoopBreakPoint: reason = "breakpoint"; break;
    case kBreakLoopStep: reason = "step"; break;
    case kBreakLoopException: reason = "exception"; break;
    case kBreakLoopPause: reason = "pause"; break;
  }
  gBreakLoopReason = kBreakLoopCalled;
  CancelLineStep();   // any stop ends a line step (e.g. a breakpoint on the way)
  ++gDAPBreakLoopDepth;
  std::string text;
  text.swap(*fStopText);
  while (!text.empty() && (text.back() == '\n' || text.back() == ' '))
    text.pop_back();
  size_t start = text.find_first_not_of(' ');
  text = (start == std::string::npos) ? std::string() : text.substr(start);
  static const std::string kPrefix = "!!! Exception: ";   // see REPExceptionNotify
  if (text.compare(0, kPrefix.size(), kPrefix) == 0)
    text.erase(0, kPrefix.size());
  flush();

  RefVar args(MakeArray(2));
  SetArraySlot(args, 0, MakeStringFromCString(reason));
  if (!text.empty())
    SetArraySlot(args, 1, MakeStringFromCString(text.c_str()));
  DoMessage(GetGlobalVar(MakeSymbol("DAP")), MakeSymbol("Stopped"), args);
}

void
PDAPOutTranslator::exitBreakLoop(void)
{
  --gDAPBreakLoopDepth;
}

void
PDAPOutTranslator::stackTrace(void * interpreter)
{
  REPStackTrace(interpreter);
}

// An exception as "stderr" output. If it is about to stop in a break loop
// (breakOnThrows), the text is also kept for the "stopped" event, and it is
// not counted as an error of the program: the program may still catch it.
std::string
PDAPOutTranslator::printToString(RefArg inObj)
{
  flush();
  std::string text;
  fCapture = &text;
  fCaptureOnly = true;
  unwind_protect
  {
    PrintObject(inObj, 0);
  }
  on_unwind
  {
    fCapture = nullptr;
    fCaptureOnly = false;
  }
  end_unwind;
  return text;
}

// What calling inFn (no arguments) prints, instead of sending it.
std::string
PDAPOutTranslator::callToString(RefArg inFn)
{
  flush();
  std::string text;
  std::string * savedCapture = fCapture;
  bool savedCaptureOnly = fCaptureOnly;
  fCapture = &text;
  fCaptureOnly = true;
  unwind_protect
  {
    DoBlock(inFn, RA(NILREF));
  }
  on_unwind
  {
    fCapture = savedCapture;
    fCaptureOnly = savedCaptureOnly;
  }
  end_unwind;
  return text;
}

void
PDAPOutTranslator::exceptionNotify(Exception * inException)
{
  flush();
  fCategory = "stderr";
  bool stopping = (gBreakLoopReason == kBreakLoopException);
  if (stopping) {
    fStopText->clear();
    fCapture = fStopText;
  }
  REPExceptionNotify(inException);
  fCapture = nullptr;
  flush();
  fCategory = "stdout";
  if (!stopping)
    ++gDAPExceptionCount;
}


#pragma mark -
/*------------------------------------------------------------------------------
  P D A P I n T r a n s l a t o r
  A break loop reads DAP requests: produceFrame() waits for the next message
  and dispatches it. It returns nil, so the break loop has nothing to
  evaluate and print; a request handler that resumes the program calls
  ExitBreakLoop().
------------------------------------------------------------------------------*/

PROTOCOL PDAPInTranslator : public PInTranslator
{
public:
  PROTOCOL_IMPL_HEADER_MACRO(PDAPInTranslator)
  PDAPInTranslator * make(void) override;
  void        destroy(void) override;

  NewtonErr   init(void * inContext) override;
  Timeout     idle(void) override;
  bool        frameAvailable(void) override;
  Ref         produceFrame(int inLevel) override;
  bool        inputEnded(void) override;
};

const CClassInfo *
PDAPInTranslator::classInfo(void)
{
  static CClassInfo _classInfo = {
    .fName = "PDAPInTranslator",
    .fInterface = "PInTranslator",
    .fSignature = "\0",
    .fSizeofProc = []()->size_t { return sizeof(PDAPInTranslator); },
    .fAllocProc = []()->CProtocol* { return new PDAPInTranslator(); },
    .fFreeProc = [](CProtocol* p)->void { delete p; },
    .fVersion = 0,
    .fFlags = 0
  };
  return &_classInfo;
}

PROTOCOL_IMPL_SOURCE_MACRO(PDAPInTranslator)

PDAPInTranslator *
PDAPInTranslator::make(void)
{ return this; }

void
PDAPInTranslator::destroy(void)
{ }

NewtonErr
PDAPInTranslator::init(void * inArgs)
{ return noErr; }

Timeout
PDAPInTranslator::idle(void)
{ return kNoTimeout; }

bool
PDAPInTranslator::frameAvailable(void)
{ return !gDAPInputEnded; }

Ref
PDAPInTranslator::produceFrame(int inLevel)
{
  ReceiveAndDispatch();
  return NILREF;
}

bool
PDAPInTranslator::inputEnded(void)
{ return gDAPInputEnded; }


void DAPInstallTranslators(void)
{
  PDAPOutTranslator::classInfo()->registerProtocol();
  PDAPInTranslator::classInfo()->registerProtocol();
  gREPout->flush();
  gDAPOutTranslator = (PDAPOutTranslator *)MakeByName("POutTranslator", "PDAPOutTranslator");
  gREPout = gDAPOutTranslator;
  gREPin = (PInTranslator *)MakeByName("PInTranslator", "PDAPInTranslator");
}


// DAPPrintObject(obj): what Print(obj) would print (without the newline),
// as a string; follows printDepth, printLength, prettyPrint. For the
// values of variables. Only in -dap mode.
Ref FDAPPrintObject(RefArg rcvr, RefArg inObj)
{
  if (gDAPOutTranslator == nullptr || gREPout != gDAPOutTranslator)
    return NILREF;
  return MakeStringFromCString(gDAPOutTranslator->printToString(inObj).c_str());
}


// DAPCaptureOutput(fn): call fn (no arguments) and return what it printed
// (Write, Print, ...) as a string, with LF line ends. Only in -dap mode.
Ref FDAPCaptureOutput(RefArg rcvr, RefArg inFn)
{
  if (gDAPOutTranslator == nullptr || gREPout != gDAPOutTranslator)
    return NILREF;
  return MakeStringFromCString(gDAPOutTranslator->callToString(inFn).c_str());
}
