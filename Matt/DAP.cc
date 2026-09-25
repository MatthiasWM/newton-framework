/*
 File: DAP.cc

 The Debug Adapter Protocol for `newtc -dap`: message framing and the REP
 translators. See DAP.h; the protocol itself is Matt/Debugger/DAP.ns.
 */

#include "Matt/DAP.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "Frames/Funcs.h"
#include "Frames/Interpreter.h"
#include "REPTranslators.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace {

FILE *gDAPIn = stdin;
FILE *gDAPOut = stdout;
long gDAPSeq = 0;             // "seq" of the last message sent
bool gDAPInputEnded = false;  // end of stdin: the client is gone
int gDAPExceptionCount = 0;

/*------------------------------------------------------------------------------
  Read one message: header lines up to an empty line, then Content-Length
  bytes of JSON. Header lines end with CRLF; a plain LF is accepted, too.
  Returns false at end of input.
------------------------------------------------------------------------------*/

bool ReadMessage(std::string &outJSON)
{
  long length = -1;
  for (;;) {
    std::string line;
    int ch;
    while ((ch = getc(gDAPIn)) != EOF && ch != '\n')
      line += (char)ch;
    if (ch == EOF)
      return false;
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
  outJSON.resize((size_t)length);
  if (length > 0 && fread(&outJSON[0], 1, (size_t)length, gDAPIn) != (size_t)length)
    return false;
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

} // namespace


void DAPStartIO(void)
{
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
  const char * reason = "pause";      // the program called BreakLoop()
  switch (gBreakLoopReason) {
    case kBreakLoopCalled: break;
    case kBreakLoopBreakPoint: reason = "breakpoint"; break;
    case kBreakLoopStep: reason = "step"; break;
    case kBreakLoopException: reason = "exception"; break;
  }
  gBreakLoopReason = kBreakLoopCalled;
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
{ }

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
