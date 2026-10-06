/*
 File: NTKDevice.cc
 newtc playing the Newton for NTK. See NTKDevice.h.
 */

#include "NTKDevice.h"
#include "NTKInspector.h"
#include "NTKTransport.h"
#include "MNP.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "Frames/Funcs.h"
#include "Frames/DebugAPI.h"
#include "Frames/Iterators.h"
#include "Frames/NewtGlobals.h"
#include "NewtonPackage.h"
#include "Matt/JSON.h"
#include "REPTranslators.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

#if !defined(_WIN32)
#include <unistd.h>
#endif

extern Ref gREPContext;
extern void PrintObjectAux(Ref inObj, int indent, int inDepth);
extern void REPIdle(void);
extern void REPStackTrace(void * interpreter);
extern bool installPackage(RefArg package, bool inOpenApp, RefVar *outParts);

namespace {

/*--- The nub: the link and the commands from the desktop ---*/

const long kDockErrBadHeader = -28016;
const size_t kTextBufferSize = 255;     // PNTKOutTranslator's buffer

uint32_t GetU32(const uint8_t *p)
{
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

std::vector<uint8_t> U32(uint32_t v)
{
  return { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
}

// A package downloaded with 'pkg ': its parts' install info and remove
// frames (for the ROM's RemovePart). Its memory stays: the package's
// objects are in it.
struct InstalledPackage {
  std::string name;
  NewtonPackage *package;
  RefStruct parts;              // [[installInfo, removeFrame], ...]
  RefStruct frame;              // the package as installed (packageRef() makes a new one each time)
  long size;                    // its bytes
};

struct Nub {
  std::vector<InstalledPackage> installed;
  MNPLink link;
  NTKPacketReader reader { false };       // packets from the desktop
  std::deque<NTKPacket> packets;          // read, not handled yet
  bool linkUp = false;
  bool ended = false;                     // 'term', or the link is gone
  RefStruct frame;                        // from 'lscb', for the REP
  bool frameAvailable = false;

  void send(uint32_t inCommand, const std::vector<uint8_t> &inData = {}, long inLength = -1)
  {
    link.send(NTKMakePacket(inCommand, inData.data(), inData.size(), inLength));
  }

  void sendResult(long inError)
  {
    send(NTKCommand("rslt"), U32((uint32_t)inError));
  }

  // Take the link's events; wait at most inTimeoutMs for the first.
  void pump(int inTimeoutMs)
  {
    int fd = link.notifyFd();
    bool ready;
    if (inTimeoutMs > 0 && NTKWaitReadable(&fd, 1, inTimeoutMs, &ready) <= 0)
      return;
    MNPLink::Event event;
    while (link.nextEvent(event)) {
      switch (event.kind) {
        case MNPLink::Event::kConnected:
          fprintf(stderr, "ntk-device: connected to %s\n", event.text.c_str());
          break;
        case MNPLink::Event::kLinkUp:
          linkUp = true;
          reader.reset();
          break;
        case MNPLink::Event::kData: {
          reader.feed(event.data.data(), event.data.size());
          NTKPacket packet;
          while (reader.next(packet))
            packets.push_back(packet);
          break;
        }
        case MNPLink::Event::kLinkDown:
          fprintf(stderr, "ntk-device: link down (%s)\n", event.text.c_str());
          linkUp = false;
          ended = true;
          break;
      }
    }
  }

  long loadPackage(const std::vector<uint8_t> &inData);
  void removePackage(const std::string &inName);

  // TNTKNub::DoCommand: one command from the desktop. 'lscb' leaves a frame
  // for the REP; the others are answered here.
  void doCommand(const NTKPacket &inPacket);

  // The ROM's NTKShutdown: 'term', and the connection ends.
  void shutdown(const char *inWhy)
  {
    fprintf(stderr, "ntk-device: %s\n", inWhy);
    send(NTKCommand("term"));
    for (int i = 0; i < 30 && link.pending() > 0 && link.isUp(); i++)
      usleep(100000);
    link.disconnect();
    ended = true;
  }
};

Nub *gNub = nullptr;


void Nub::doCommand(const NTKPacket &inPacket)
{
  uint32_t command = inPacket.command;
  const std::vector<uint8_t> &d = inPacket.data;

  if (command == NTKCommand("lscb")) {
    // the REP's next frame (ProduceFrame reads it and answers 'rslt 0')
    RefVar block;
    bool ok = true;
    newton_try
    {
      block = NTKUnflatten(d.data(), d.size());
    }
    newton_catch_all
    {
      ok = false;
    }
    end_try;
    if (!ok) {
      shutdown("a bad code block in 'lscb'");
      return;
    }
    frame = block;
    frameAvailable = true;

  } else if (command == NTKCommand("code")) {
    // HandleCodeBlock: a u32 (ignored), the code block; 'rslt 0' when it is
    // read, then 'code' with the request's length and the result. An
    // exception ends the connection, as on a Newton (Toolkit Protocol.md 2.3).
    if (d.size() < 4) {
      shutdown("a bad 'code' packet");
      return;
    }
    RefVar block;
    RefVar result;
    bool ok = true;
    newton_try
    {
      block = NTKUnflatten(d.data() + 4, d.size() - 4);
    }
    newton_catch_all
    {
      ok = false;
    }
    end_try;
    if (!ok) {
      shutdown("a bad code block in 'code'");
      return;
    }
    sendResult(0);
    newton_try
    {
      RefVar context(gREPContext);
      result = InterpretBlock(block, context);
    }
    newton_catch_all
    {
      ok = false;
    }
    end_try;
    if (!ok) {
      shutdown("an exception in a 'code' block ends the connection (as on a Newton)");
      return;
    }
    std::vector<uint8_t> nsof = NTKFlatten(result);
    std::vector<uint8_t> data = U32((uint32_t)nsof.size());
    data.insert(data.end(), nsof.begin(), nsof.end());
    send(NTKCommand("code"), data, (long)inPacket.length);

  } else if (command == NTKCommand("pkg ")) {
    sendResult(loadPackage(d));

  } else if (command == NTKCommand("pkgX")) {
    // the name as UTF-16 (big-endian) with a NUL; no error if it isn't there
    std::string name;
    for (size_t i = 0; i + 1 < d.size(); i += 2) {
      unsigned c = (unsigned)(d[i] << 8 | d[i + 1]);
      if (c == 0)
        break;
      if (c < 0x80) {
        name += (char)c;
      } else if (c < 0x800) {
        name += (char)(0xC0 | (c >> 6));
        name += (char)(0x80 | (c & 0x3F));
      } else {
        name += (char)(0xE0 | (c >> 12));
        name += (char)(0x80 | ((c >> 6) & 0x3F));
        name += (char)(0x80 | (c & 0x3F));
      }
    }
    removePackage(name);
    sendResult(0);

  } else if (command == NTKCommand("stou")) {
    sendResult(0);

  } else if (command == NTKCommand("term")) {
    fprintf(stderr, "ntk-device: the desktop ended the connection\n");
    link.disconnect();                      // the Newton answers 'term' with an LD
    ended = true;

  } else {
    sendResult(kDockErrBadHeader);
  }
}


// PNTKInTranslator::LoadPackage: install a package (not opened, as on a
// Newton). The ROM's errors: -10402 "The package already exists".
long Nub::loadPackage(const std::vector<uint8_t> &inData)
{
  const long kPackageExists = -10402;
  const long kBadPackage = -10401;
  void *memory = malloc(inData.size());
  memcpy(memory, inData.data(), inData.size());
  NewtonPackage *package = new NewtonPackage(memory);
  RefVar packageRef(package->packageRef());
  if (!IsFrame(packageRef)) {
    fprintf(stderr, "ntk-device: 'pkg ': not a package\n");
    return kBadPackage;
  }
  RefVar nameRef(GetFrameSlot(packageRef, MakeSymbol("name")));
  std::string name = IsString(nameRef) ? UTF8FromString(nameRef) : std::string();
  for (const InstalledPackage &p : installed)
    if (p.name == name)
      return kPackageExists;
  RefVar parts;
  bool ok = true;
  newton_try
  {
    ok = installPackage(packageRef, false, &parts);
  }
  newton_catch_all
  {
    gREPout->exceptionNotify(CurrentException());
    ok = false;
  }
  end_try;
  if (!ok)
    return kBadPackage;
  fprintf(stderr, "ntk-device: installed \"%s\"\n", name.c_str());
  installed.push_back(InstalledPackage{ name, package, RefStruct(parts), RefStruct(packageRef),
                                        (long)inData.size() });
  return 0;
}

// TNTKNub::DeletePackage: the ROM's RemovePart for each part.
void Nub::removePackage(const std::string &inName)
{
  for (size_t i = 0; i < installed.size(); i++) {
    if (installed[i].name != inName)
      continue;
    RefVar parts(installed[i].parts);
    RefVar removePart(GetFrameSlot(gFunctionFrame, MakeSymbol("RemovePart")));
    for (ArrayIndex j = 0; j < Length(parts); j++) {
      RefVar entry(GetArraySlot(parts, j));
      RefVar args(MakeArray(2));
      SetArraySlot(args, 0, GetArraySlot(entry, 0));
      SetArraySlot(args, 1, GetArraySlot(entry, 1));
      newton_try
      {
        DoBlock(removePart, args);
      }
      newton_catch_all
      {
        gREPout->exceptionNotify(CurrentException());
      }
      end_try;
    }
    fprintf(stderr, "ntk-device: removed \"%s\"\n", inName.c_str());
    // the package's memory stays: its objects may still be referenced
    installed.erase(installed.begin() + (long)i);
    return;
  }
}


/*--- The translators ---*/

PROTOCOL PNTKDeviceInTranslator : public PInTranslator
{
public:
  PROTOCOL_IMPL_HEADER_MACRO(PNTKDeviceInTranslator)
  PNTKDeviceInTranslator * make(void) override { return this; }
  void        destroy(void) override {}
  NewtonErr   init(void * inContext) override { return noErr; }
  Timeout     idle(void) override;
  bool        frameAvailable(void) override { return gNub->frameAvailable; }
  Ref         produceFrame(int inLevel) override;
  bool        inputEnded(void) override { return gNub->ended; }
};

const CClassInfo *
PNTKDeviceInTranslator::classInfo(void)
{
  static CClassInfo _classInfo = {
    .fName = "PNTKDeviceInTranslator",
    .fInterface = "PInTranslator",
    .fSignature = "\0",
    .fSizeofProc = []()->size_t { return sizeof(PNTKDeviceInTranslator); },
    .fAllocProc = []()->CProtocol* { return new PNTKDeviceInTranslator(); },
    .fFreeProc = [](CProtocol* p)->void { delete p; },
    .fVersion = 0,
    .fFlags = 0
  };
  return &_classInfo;
}

PROTOCOL_IMPL_SOURCE_MACRO(PNTKDeviceInTranslator)

// PNTKInTranslator::Idle: if no frame waits for the REP and a command came
// in, do it. Here it also waits a little for one (newtc has no other idle
// work), so the REP loop doesn't spin.
Timeout
PNTKDeviceInTranslator::idle(void)
{
  if (!gNub->frameAvailable && gNub->packets.empty() && !gNub->ended)
    gNub->pump(200);
  else
    gNub->pump(0);
  if (!gNub->frameAvailable && !gNub->packets.empty()) {
    NTKPacket packet = gNub->packets.front();
    gNub->packets.pop_front();
    gNub->doCommand(packet);
  }
  return kNoTimeout;
}

// PNTKInTranslator::ProduceFrame: the frame from 'lscb', and 'rslt 0'.
Ref
PNTKDeviceInTranslator::produceFrame(int inLevel)
{
  gNub->frameAvailable = false;
  RefVar block(gNub->frame);
  gNub->frame = NILREF;
  gNub->sendResult(0);
  return block;
}


PROTOCOL PNTKDeviceOutTranslator : public POutTranslator
{
public:
  PROTOCOL_IMPL_HEADER_MACRO(PNTKDeviceOutTranslator)
  PNTKDeviceOutTranslator * make(void) override { fText = new std::string; return this; }
  void        destroy(void) override { delete fText; fText = nullptr; }
  NewtonErr   init(void * inContext) override { return noErr; }
  Timeout     idle(void) override { return kNoTimeout; }
  void        consumeFrame(RefArg inObj, int inDepth, int indent) override;
  void        prompt(int inLevel) override {}
  int         print(const char * inFormat, ...) override;
  int         vprint(const char * inFormat, va_list args) override;
  int         putc(int inCh) override;
  void        flush(void) override;
  void        enterBreakLoop(int inLevel) override;
  void        exitBreakLoop(void) override;
  void        stackTrace(void * interpreter) override;
  void        exceptionNotify(Exception * inException) override;
private:
  void        add(const char * inText, size_t inLength);
  std::string * fText;          // text not sent yet
};

const CClassInfo *
PNTKDeviceOutTranslator::classInfo(void)
{
  static CClassInfo _classInfo = {
    .fName = "PNTKDeviceOutTranslator",
    .fInterface = "POutTranslator",
    .fSignature = "\0",
    .fSizeofProc = []()->size_t { return sizeof(PNTKDeviceOutTranslator); },
    .fAllocProc = []()->CProtocol* { return new PNTKDeviceOutTranslator(); },
    .fFreeProc = [](CProtocol* p)->void { delete p; },
    .fVersion = 0,
    .fFlags = 0
  };
  return &_classInfo;
}

PROTOCOL_IMPL_SOURCE_MACRO(PNTKDeviceOutTranslator)

// Text goes out as 'text' packets of at most 255 bytes, sent at a CR or
// when the buffer is full (PNTKOutTranslator::Print, FlushText). A Newton
// ends lines with CR; newtc's printing sometimes uses LF.
void
PNTKDeviceOutTranslator::add(const char * inText, size_t inLength)
{
  for (size_t i = 0; i < inLength; i++) {
    char c = inText[i] == '\n' ? '\r' : inText[i];
    fText->push_back(c);
    if (c == '\r' || fText->size() >= kTextBufferSize)
      flush();
  }
}

void
PNTKDeviceOutTranslator::flush(void)
{
  if (fText->empty() || !gNub->linkUp)
    return;
  gNub->send(NTKCommand("text"), std::vector<uint8_t>(fText->begin(), fText->end()));
  fText->clear();
}

int
PNTKDeviceOutTranslator::vprint(const char * inFormat, va_list args)
{
  char buffer[1024];
  int n = vsnprintf(buffer, sizeof(buffer), inFormat, args);
  if (n > 0)
    add(buffer, (size_t)n < sizeof(buffer) ? (size_t)n : sizeof(buffer) - 1);
  return n;
}

int
PNTKDeviceOutTranslator::print(const char * inFormat, ...)
{
  va_list args;
  va_start(args, inFormat);
  int n = vprint(inFormat, args);
  va_end(args);
  return n;
}

int
PNTKDeviceOutTranslator::putc(int inCh)
{
  char c = (char)inCh;
  add(&c, 1);
  return inCh;
}

void
PNTKDeviceOutTranslator::consumeFrame(RefArg inObj, int inDepth, int indent)
{
  PrintObjectAux(inObj, indent, inDepth);
  flush();
}

void
PNTKDeviceOutTranslator::enterBreakLoop(int inLevel)
{
  flush();
  gNub->send(NTKCommand("eext"));
}

void
PNTKDeviceOutTranslator::exitBreakLoop(void)
{
  flush();
  gNub->send(NTKCommand("bext"));
}

// The name the ROM gives a function in a stack trace: "functions.<name>"
// for a global function, else nil (an anonymous code block, a closure).
// (NTKStackFrameInfo uses FindSlotName and SearchForObjectName.)
Ref
FunctionName(RefArg inFunction)
{
  if (ISNIL(inFunction))
    return NILREF;
  RefVar functions(gFunctionFrame);
  for (CObjectIterator iter(functions); !iter.done(); iter.next()) {
    if (iter.value() == inFunction && IsSymbol(iter.tag()))
      return MakeStringFromCString((std::string("functions.") + SymbolName(iter.tag())).c_str());
  }
  return NILREF;
}

// NTKStackTrace: 'fstk', an array of frames, newest first (the debug API
// counts from the oldest), each {class: 'StackFrameInfoFrame, CodeBlock:
// <name or nil>, programCounter: <pc, -1 for a native function>, receiver,
// contextFrame} (as captured from Einstein, Toolkit Protocol.md 6.1).
void
PNTKDeviceOutTranslator::stackTrace(void * interpreter)
{
  flush();
  CNSDebugAPI api((CInterpreter *)interpreter);
  ArrayIndex count = api.numStackFrames();
  RefVar frames(MakeArray(count));
  for (ArrayIndex i = 0; i < count; i++) {
    RefVar fn(api.function(i));
    bool native = NOTNIL(fn) && IsNativeFunction(fn);
    RefVar info(AllocateFrame());
    SetFrameSlot(info, MakeSymbol("class"), MakeSymbol("StackFrameInfoFrame"));
    SetFrameSlot(info, MakeSymbol("CodeBlock"), FunctionName(fn));
    SetFrameSlot(info, MakeSymbol("programCounter"),
                 MAKEINT(native || ISNIL(fn) ? -1 : (long)api.PC(i)));
    SetFrameSlot(info, MakeSymbol("receiver"), RA(NILREF));
    SetFrameSlot(info, MakeSymbol("contextFrame"), RA(NILREF));
    SetArraySlot(frames, count - 1 - i, info);
  }
  gNub->send(NTKCommand("fstk"), NTKFlatten(frames));
}

// TNTKNub::ExceptionNotify: 'estr' for a message, 'eref' for a Ref (NSOF),
// else 'eerr' with the error code. The length word counts the name and the
// payload, not the u32 count words (Toolkit Protocol.md 2.4, 6.1).
void
PNTKDeviceOutTranslator::exceptionNotify(Exception * inException)
{
  flush();
  std::string name(inException->name ? inException->name : "");
  std::vector<uint8_t> nameBytes(name.begin(), name.end());
  nameBytes.push_back(0);
  std::vector<uint8_t> data = U32((uint32_t)nameBytes.size());
  data.insert(data.end(), nameBytes.begin(), nameBytes.end());
  if (Subexception(inException->name, exMessage)) {
    std::string message(inException->data ? (const char *)inException->data : "");
    std::vector<uint8_t> size = U32((uint32_t)message.size() + 1);
    data.insert(data.end(), size.begin(), size.end());
    data.insert(data.end(), message.begin(), message.end());
    data.push_back(0);
    gNub->send(NTKCommand("estr"), data, (long)(nameBytes.size() + message.size() + 1));
  } else if (Subexception(inException->name, exRefException)) {
    RefVar value(*(RefStruct *)inException->data);
    std::vector<uint8_t> nsof = NTKFlatten(value);
    std::vector<uint8_t> size = U32((uint32_t)nsof.size());
    data.insert(data.end(), size.begin(), size.end());
    data.insert(data.end(), nsof.begin(), nsof.end());
    gNub->send(NTKCommand("eref"), data, (long)(nameBytes.size() + nsof.size()));
  } else {
    std::vector<uint8_t> error = U32((uint32_t)(long)inException->data);
    data.insert(data.end(), error.begin(), error.end());
    gNub->send(NTKCommand("eerr"), data, (long)(nameBytes.size() + 4));
  }
}

} // namespace


Ref FNTKDeviceAlive(RefArg rcvr)
{
  return MAKEBOOLEAN(gNub != nullptr && !gNub->ended);
}


Ref FNTKDeviceSend(RefArg rcvr, RefArg inObject)
{
  if (gNub != nullptr && !gNub->ended)
    gNub->send(NTKCommand("fobj"), NTKFlatten(inObject));
  return NILREF;
}


Ref FNTKDeviceGetPkgRef(RefArg rcvr, RefArg inName, RefArg inStore)
{
  if (gNub == nullptr || !IsString(inName))
    return NILREF;
  std::string name = UTF8FromString(inName);
  for (const InstalledPackage &p : gNub->installed)
    if (p.name == name)
      return MakeStringFromCString(name.c_str());
  return NILREF;
}


Ref FNTKDeviceGetPkgRefInfo(RefArg rcvr, RefArg inRef)
{
  if (gNub == nullptr || !IsString(inRef))
    return NILREF;
  std::string name = UTF8FromString(inRef);
  for (const InstalledPackage &p : gNub->installed) {
    if (p.name != name)
      continue;
    RefVar package(p.frame);
    RefVar partFrames(GetFrameSlot(package, MakeSymbol("part")));
    ArrayIndex count = IsArray(partFrames) ? Length(partFrames) : 0;
    RefVar parts(MakeArray(count));
    for (ArrayIndex i = 0; i < count; i++)
      SetArraySlot(parts, i, GetFrameSlot(GetArraySlot(partFrames, i), MakeSymbol("data")));
    RefVar info(AllocateFrame());
    SetFrameSlot(info, MakeSymbol("title"), MakeStringFromCString(name.c_str()));
    SetFrameSlot(info, MakeSymbol("size"), MAKEINT(p.size));
    SetFrameSlot(info, MakeSymbol("numParts"), MAKEINT(count));
    SetFrameSlot(info, MakeSymbol("parts"), parts);
    return info;
  }
  return NILREF;
}


#if !defined(_WIN32)
// One Toolkit session: connect, serve the desktop until it ends the
// session. 0, or 1 if no connection could be made.
static int RunDeviceSession(Nub &nub, const std::string &inTarget)
{
  // like Einstein, try again until the desktop listens (15 s)
  std::string error;
  bool started = false;
  for (int i = 0; i < 75 && !started; i++) {
    started = nub.link.start(inTarget, error);
    if (!started && error.compare(0, 14, "can't connect ") != 0)
      break;                            // not a refused connection: give up
    if (!started)
      usleep(200000);
  }
  if (!started) {
    fprintf(stderr, "newtc: -ntk-device: %s\n", error.c_str());
    return 1;
  }

  // the MNP link, then 'cnnt' (StartListener) and 'okln' with length 0
  for (int i = 0; i < 100 && !nub.linkUp && !nub.ended; i++)
    nub.pump(100);
  if (!nub.linkUp) {
    fprintf(stderr, "newtc: -ntk-device: no MNP link with %s\n", nub.link.describe().c_str());
    nub.link.stop();
    return 1;
  }
  nub.send(NTKCommand("cnnt"));
  for (int i = 0; i < 300 && nub.packets.empty() && !nub.ended; i++)
    nub.pump(100);
  if (nub.packets.empty() || nub.packets.front().command != NTKCommand("okln")
      || nub.packets.front().length != 0) {
    fprintf(stderr, "newtc: -ntk-device: the desktop didn't answer 'cnnt' with 'okln'\n");
    nub.link.stop();
    return 1;
  }
  nub.packets.pop_front();
  fprintf(stderr, "ntk-device: Toolkit connected\n");

  // Toolkit.pkg says hello: NTKSend({interpretation: 'dante, data: {}})
  RefVar hello(AllocateFrame());
  SetFrameSlot(hello, MakeSymbol("interpretation"), MakeSymbol("dante"));
  SetFrameSlot(hello, MakeSymbol("data"), AllocateFrame());
  // 'fobj' is header + command + (the NSOF size where the length goes) + NSOF
  nub.send(NTKCommand("fobj"), NTKFlatten(hello));

  // the REP with our translators, idle until the desktop ends it
  PNTKDeviceInTranslator::classInfo()->registerProtocol();
  PNTKDeviceOutTranslator::classInfo()->registerProtocol();
  gREPout->flush();
  PInTranslator *savedIn = gREPin;
  POutTranslator *savedOut = gREPout;
  gREPout = (POutTranslator *)MakeByName("POutTranslator", "PNTKDeviceOutTranslator");
  gREPin = (PInTranslator *)MakeByName("PInTranslator", "PNTKDeviceInTranslator");
  while (!nub.ended)
    REPIdle();
  gREPout->flush();
  gREPin = savedIn;
  gREPout = savedOut;

  for (int i = 0; i < 20 && nub.link.pending() > 0 && nub.link.isUp(); i++)
    usleep(100000);
  nub.link.stop();
  return 0;
}
#endif


int NTKDeviceRun(const std::string &inTarget)
{
#if defined(_WIN32)
  fprintf(stderr, "newtc: -ntk-device is not supported on Windows yet\n");
  return 1;
#else
  Nub nub;
  gNub = &nub;
  if (getenv("NEWTC_NTK_TRACE"))
    nub.link.setTrace(stderr);
  nub.link.setRole(MNPLink::kNewton);
  // like a Newton that stays on: NEWTC_NTK_DEVICE_SESSIONS sessions one
  // after the other (connecting again each time); its packages stay
  int sessions = 1;
  if (const char *count = getenv("NEWTC_NTK_DEVICE_SESSIONS"))
    sessions = atoi(count) > 1 ? atoi(count) : 1;
  int result = 0;
  for (int s = 0; s < sessions && result == 0; s++) {
    if (s == 0) {
      result = RunDeviceSession(nub, inTarget);
      continue;
    }
    // the next session: as a user taps "Connect" again, after a moment
    // (the old desktop may still be listening while it exits); a failed
    // handshake is tried again, for half a minute
    fprintf(stderr, "ntk-device: waiting for the next session\n");
    for (int attempt = 0; attempt < 30; attempt++) {
      usleep(1000000);
      nub.linkUp = false;
      nub.ended = false;
      nub.packets.clear();
      nub.reader = NTKPacketReader(false);
      nub.frameAvailable = false;
      result = RunDeviceSession(nub, inTarget);
      if (result == 0)
        break;
    }
  }
  gNub = nullptr;
  if (result == 0)
    fprintf(stderr, "ntk-device: done\n");
  return result;
#endif
}
