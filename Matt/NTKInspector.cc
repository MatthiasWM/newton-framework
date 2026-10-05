/*
 File: NTKInspector.cc
 The desktop side of NTK's Inspector. See NTKInspector.h.
 */

#include "NTKInspector.h"
#include "NTKTransport.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "Frames/Pipes.h"
#include "Frames/StreamObjects.h"
#include "Frames/Compiler/InputStreams.h"
#include "Frames/Compiler/Compiler.h"
#include "REPTranslators.h"
#include "Utilities/Unicode.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

#if !defined(_WIN32)
#include <unistd.h>
#endif

extern void REPExceptionNotify(Exception * inException);

namespace {

const uint32_t kNewt = NTKCommand("newt");
const uint32_t kNtp = NTKCommand("ntp ");

uint32_t GetU32(const uint8_t *p)
{
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

void PutU32(std::vector<uint8_t> &out, uint32_t v)
{
  out.push_back((uint8_t)(v >> 24));
  out.push_back((uint8_t)(v >> 16));
  out.push_back((uint8_t)(v >> 8));
  out.push_back((uint8_t)v);
}

// Text from the Newton: MacRoman with CR line ends -> UTF-8 with LF.
std::string TextFromMacRoman(const uint8_t *inText, size_t inSize)
{
  std::vector<UniChar> unicode(inSize + 1);
  if (inSize > 0)
    ConvertToUnicode(inText, unicode.data(), (ArrayIndex)inSize, kMacRomanEncoding);
  std::string out;
  for (size_t i = 0; i < inSize; i++) {
    UniChar c = unicode[i];
    if (c == 0x0D)
      c = 0x0A;
    if (c < 0x80) {
      out += (char)c;
    } else if (c < 0x800) {
      out += (char)(0xC0 | (c >> 6));
      out += (char)(0x80 | (c & 0x3F));
    } else {
      out += (char)(0xE0 | (c >> 12));
      out += (char)(0x80 | ((c >> 6) & 0x3F));
      out += (char)(0x80 | (c & 0x3F));
    }
  }
  return out;
}

double Seconds()
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// A C string of at most inMax bytes (without a NUL if it ends there).
std::string CString(const uint8_t *inData, size_t inMax)
{
  size_t n = 0;
  while (n < inMax && inData[n] != 0)
    n++;
  return std::string((const char *)inData, n);
}

} // namespace


std::string NTKCommandName(uint32_t inCommand)
{
  std::string name;
  for (int shift = 24; shift >= 0; shift -= 8) {
    char c = (char)((inCommand >> shift) & 0xFF);
    name += (c >= 0x20 && c < 0x7F) ? c : '?';
  }
  return name;
}


std::vector<uint8_t> NTKMakePacket(uint32_t inCommand, const uint8_t *inData, size_t inSize, long inLength)
{
  std::vector<uint8_t> out;
  PutU32(out, kNewt);
  PutU32(out, kNtp);
  PutU32(out, inCommand);
  PutU32(out, inLength < 0 ? (uint32_t)inSize : (uint32_t)inLength);
  if (inData)
    out.insert(out.end(), inData, inData + inSize);
  return out;
}


/*--- NTKPacketReader ---*/

void NTKPacketReader::feed(const uint8_t *inData, size_t inSize)
{
  mBuffer.insert(mBuffer.end(), inData, inData + inSize);
}


bool NTKPacketReader::next(NTKPacket &outPacket)
{
  // skip to the next "newtntp " (anything before it is junk)
  static const uint8_t kStart[8] = { 'n', 'e', 'w', 't', 'n', 't', 'p', ' ' };
  size_t start = 0;
  while (start + 8 <= mBuffer.size() && memcmp(mBuffer.data() + start, kStart, 8) != 0)
    start++;
  if (start + 8 > mBuffer.size()) {
    // keep a possible partial start at the end
    size_t keep = mBuffer.size() < 7 ? mBuffer.size() : 7;
    while (keep > 0 && memcmp(mBuffer.data() + mBuffer.size() - keep, kStart, keep) != 0)
      keep--;
    start = mBuffer.size() - keep;
  }
  if (start > 0) {
    mJunk.insert(mJunk.end(), mBuffer.begin(), mBuffer.begin() + (long)start);
    mBuffer.erase(mBuffer.begin(), mBuffer.begin() + (long)start);
  }
  if (mBuffer.size() < 16)
    return false;
  uint32_t command = GetU32(mBuffer.data() + 8);
  uint32_t length = GetU32(mBuffer.data() + 12);
  size_t dataSize;
  if (!mFromNewton) {
    dataSize = length;
  } else if (command == NTKCommand("code")) {
    if (mBuffer.size() < 20)
      return false;
    dataSize = 4 + GetU32(mBuffer.data() + 16);
  } else if (command == NTKCommand("eerr")) {
    dataSize = (size_t)length + 4;
  } else if (command == NTKCommand("estr") || command == NTKCommand("eref")) {
    dataSize = (size_t)length + 8;
  } else {
    dataSize = length;
  }
  if (mBuffer.size() < 16 + dataSize)
    return false;
  outPacket.command = command;
  outPacket.length = length;
  outPacket.data.assign(mBuffer.begin() + 16, mBuffer.begin() + 16 + (long)dataSize);
  mBuffer.erase(mBuffer.begin(), mBuffer.begin() + 16 + (long)dataSize);
  return true;
}


std::vector<uint8_t> NTKPacketReader::takeJunk()
{
  std::vector<uint8_t> junk;
  junk.swap(mJunk);
  return junk;
}


/*--- NSOF in memory ---*/

std::vector<uint8_t> NTKFlatten(RefArg inObject)
{
  CPtrPipe pipe;
  CObjectWriter writer(inObject, pipe, false);
  std::vector<uint8_t> bytes(writer.size());
  pipe.init(bytes.data(), bytes.size(), false, nullptr);
  writer.write();
  return bytes;
}


Ref NTKUnflatten(const uint8_t *inData, size_t inSize)
{
  CPtrPipe pipe;
  pipe.init((void *)inData, inSize, false, nullptr);
  CObjectReader reader(pipe);
  return reader.read();
}


/*--- NTKInspector ---*/

bool NTKInspector::start(const std::string &inTarget, std::string &outError)
{
  if (getenv("NEWTC_NTK_TRACE"))
    mLink.setTrace(stderr);
  return mLink.start(inTarget, outError);
}


void NTKInspector::stop()
{
  mLink.stop();
  mConnected = false;
}


void NTKInspector::sendPacket(uint32_t inCommand, const std::vector<uint8_t> &inData, long inLength)
{
  // the Newton answers these with 'rslt' (TNTKNub::DoCommand, ProduceFrame)
  if (inCommand == NTKCommand("lscb") || inCommand == NTKCommand("code")
      || inCommand == NTKCommand("pkg ") || inCommand == NTKCommand("pkgX")
      || inCommand == NTKCommand("stou"))
    mAwaitingResult.push_back(inCommand);
  mLink.send(NTKMakePacket(inCommand, inData.data(), inData.size(), inLength));
  mLastTraffic = Seconds();
}


void NTKInspector::tick()
{
  static double interval = -1;
  if (interval < 0) {
    const char *env = getenv("NEWTC_NTK_PING");
    interval = (env && atof(env) > 0) ? atof(env) : 15.0;
  }
  if (mConnected && Seconds() - mLastTraffic > interval) {
    std::vector<uint8_t> seconds = { 0, 0, 0, 30 };
    sendPacket(NTKCommand("stou"), seconds);
  }
}


void NTKInspector::evaluate(RefArg inCodeBlock)
{
  sendPacket(NTKCommand("lscb"), NTKFlatten(inCodeBlock));
}


void NTKInspector::call(RefArg inCodeBlock)
{
  // a u32 the Newton reads and ignores, then the code block
  std::vector<uint8_t> data(4, 0);
  std::vector<uint8_t> nsof = NTKFlatten(inCodeBlock);
  data.insert(data.end(), nsof.begin(), nsof.end());
  sendPacket(NTKCommand("code"), data);
  mCallsPending++;
}


void NTKInspector::loadPackage(const std::vector<uint8_t> &inPackage)
{
  sendPacket(NTKCommand("pkg "), inPackage);
}


void NTKInspector::deletePackage(const std::string &inNameUTF8)
{
  // the name as UTF-16 (big-endian) with a NUL; ASCII and Latin-1 suffice
  std::vector<uint8_t> name;
  for (size_t i = 0; i < inNameUTF8.size(); i++) {
    unsigned c = (uint8_t)inNameUTF8[i];
    if ((c & 0xE0) == 0xC0 && i + 1 < inNameUTF8.size())
      c = ((c & 0x1F) << 6) | ((uint8_t)inNameUTF8[++i] & 0x3F);
    name.push_back((uint8_t)(c >> 8));
    name.push_back((uint8_t)c);
  }
  name.push_back(0);
  name.push_back(0);
  sendPacket(NTKCommand("pkgX"), name);
}


void NTKInspector::terminate()
{
  if (mConnected)
    sendPacket(NTKCommand("term"), {});
  mConnected = false;
  mCallsPending = 0;
}


void NTKInspector::handleEvents(Listener &inListener)
{
  MNPLink::Event event;
  while (mLink.nextEvent(event)) {
    switch (event.kind) {
      case MNPLink::Event::kConnected:
        break;
      case MNPLink::Event::kLinkUp:
        mReader.reset();
        break;
      case MNPLink::Event::kData: {
        mLastTraffic = Seconds();
        mReader.feed(event.data.data(), event.data.size());
        NTKPacket packet;
        while (mReader.next(packet))
          handlePacket(packet, inListener);
        std::vector<uint8_t> junk = mReader.takeJunk();
        if (!junk.empty()) {
          NTKPacket other;
          other.data = junk;
          inListener.ntkOther(other);
        }
        break;
      }
      case MNPLink::Event::kLinkDown:
        if (mConnected) {
          mConnected = false;
          mCallsPending = 0;
          mAwaitingResult.clear();
          inListener.ntkDisconnected(event.text);
        }
        break;
    }
  }
}


void NTKInspector::handlePacket(const NTKPacket &inPacket, Listener &inListener)
{
  const std::vector<uint8_t> &d = inPacket.data;
  uint32_t command = inPacket.command;

  if (command == NTKCommand("cnnt")) {
    mAwaitingResult.clear();
    sendPacket(NTKCommand("okln"), {});       // (also starts the idle clock)
    mConnected = true;
    mCallsPending = 0;
    inListener.ntkConnected(mLink.describe());

  } else if (command == NTKCommand("text")) {
    inListener.ntkText(TextFromMacRoman(d.data(), d.size()));

  } else if (command == NTKCommand("rslt")) {
    uint32_t forCommand = 0;
    if (!mAwaitingResult.empty()) {
      forCommand = mAwaitingResult.front();
      mAwaitingResult.pop_front();
    }
    inListener.ntkResult(d.size() >= 4 ? (long)(int32_t)GetU32(d.data()) : 0, forCommand);

  } else if (command == NTKCommand("eext") || command == NTKCommand("bext")) {
    inListener.ntkBreakLoop(command == NTKCommand("eext"));

  } else if (command == NTKCommand("term")) {
    mConnected = false;
    mCallsPending = 0;
    inListener.ntkDisconnected("the Newton ended the connection");

  } else if (command == NTKCommand("fobj") || command == NTKCommand("fstk")
          || command == NTKCommand("code")) {
    size_t offset = command == NTKCommand("code") ? 4 : 0;
    RefVar object;
    bool ok = true;
    newton_try
    {
      object = NTKUnflatten(d.data() + offset, d.size() - offset);
    }
    newton_catch_all
    {
      ok = false;
    }
    end_try;
    if (command == NTKCommand("code") && mCallsPending > 0)
      mCallsPending--;
    if (!ok)
      inListener.ntkOther(inPacket);
    else if (command == NTKCommand("code"))
      inListener.ntkCodeResult(object);
    else
      inListener.ntkObject(NTKCommandName(command), object);

  } else if ((command == NTKCommand("eerr") || command == NTKCommand("estr")
           || command == NTKCommand("eref")) && d.size() >= 8) {
    // u32 name length, the name (with its NUL), then the payload
    size_t nameLength = GetU32(d.data());
    if (4 + nameLength + 4 > d.size()) {
      inListener.ntkOther(inPacket);
      return;
    }
    std::string name = CString(d.data() + 4, nameLength);
    const uint8_t *rest = d.data() + 4 + nameLength;
    size_t restSize = d.size() - 4 - nameLength;
    Exception exception = { name.c_str(), nullptr, nullptr };
    if (command == NTKCommand("eerr")) {
      exception.data = (void *)(long)(int32_t)GetU32(rest);
      inListener.ntkException(exception);
    } else if (command == NTKCommand("estr")) {
      size_t size = GetU32(rest);
      std::string message = CString(rest + 4, size < restSize - 4 ? size : restSize - 4);
      exception.data = (void *)message.c_str();
      inListener.ntkException(exception);
    } else {
      size_t size = GetU32(rest);
      RefVar data;
      newton_try
      {
        data = NTKUnflatten(rest + 4, size < restSize - 4 ? size : restSize - 4);
      }
      newton_catch_all
      {
      }
      end_try;
      RefStruct ref(data);
      exception.data = &ref;
      inListener.ntkException(exception);
    }

  } else {
    inListener.ntkOther(inPacket);
  }
}


/*--- newtc -ntk: a terminal Inspector ---*/

namespace {

class TerminalInspector : public NTKInspector::Listener
{
public:
  int callsSent = 0;              // 'code' calls, numbered from 1 ...
  int repliesSeen = 0;            // ... and their replies, which come in order
  int quietCall = 0;              // the call whose reply isn't printed (the sync)

  void ntkConnected(const std::string &inPeer) override
  {
    callsSent = repliesSeen = quietCall = 0;
    printf("ntk: connected (%s)\n", inPeer.c_str());
  }
  void ntkDisconnected(const std::string &inReason) override
  {
    printf("ntk: disconnected (%s)\n", inReason.c_str());
  }
  void ntkText(const std::string &inUTF8) override
  {
    fputs(inUTF8.c_str(), stdout);
  }
  void ntkResult(long inError, uint32_t inCommand) override
  {
    if (inError != 0)
      printf("ntk: error %ld\n", inError);
  }
  void ntkObject(const std::string &inCommand, RefArg inObject) override
  {
    printf("ntk: %s ", inCommand.c_str());
    fflush(stdout);
    PrintObject(inObject, 0);
    gREPout->print("\n");
    gREPout->flush();
  }
  void ntkCodeResult(RefArg inResult) override
  {
    if (++repliesSeen == quietCall)
      return;
    fflush(stdout);
    PrintObject(inResult, 0);
    gREPout->print("\n");
    gREPout->flush();
  }
  void ntkException(Exception &inException) override
  {
    fflush(stdout);
    REPExceptionNotify(&inException);
    gREPout->flush();
  }
  void ntkBreakLoop(bool inEntered) override
  {
    printf("ntk: break loop %s\n", inEntered ? "entered" : "left");
  }
  void ntkOther(const NTKPacket &inPacket) override
  {
    if (inPacket.command == 0)
      printf("ntk: %zu bytes outside a packet:", inPacket.data.size());
    else
      printf("ntk: '%s' length %u:", NTKCommandName(inPacket.command).c_str(), inPacket.length);
    for (size_t i = 0; i < inPacket.data.size() && i < 32; i++)
      printf(" %02x", inPacket.data[i]);
    printf(inPacket.data.size() > 32 ? " ...\n" : "\n");
  }
};


// Compile a line of NewtonScript: one code block per statement. A
// compile error is printed (like the REPL does) and gives none.
std::vector<RefVar> CompileLine(const std::string &inLine)
{
  std::vector<RefVar> blocks;
  RefVar src = MakeStringFromCString(inLine.c_str());
  CStringInputStream stream(src);
  CCompiler compiler(&stream, true);
  newton_try
  {
    while (!stream.end()) {
      RefVar block = compiler.compile();
      if (NOTNIL(block))
        blocks.push_back(block);
    }
  }
  newton_catch_all
  {
    REPExceptionNotify(CurrentException());
    gREPout->flush();
    blocks.clear();
  }
  end_try;
  return blocks;
}

} // namespace


int NTKInspectorREPL(const std::string &inTarget)
{
#if defined(_WIN32)
  fprintf(stderr, "newtc: -ntk is not supported on Windows yet\n");
  return 1;
#else
  NTKInspector inspector;
  TerminalInspector terminal;
  std::string error;
  if (!inspector.start(inTarget, error)) {
    fprintf(stderr, "newtc: -ntk: %s\n", error.c_str());
    return 1;
  }
  printf("ntk: waiting for a Newton on %s\n", inspector.describe().c_str());
  fflush(stdout);

  std::deque<std::string> lines;      // waiting for the connection
  std::string partial;
  bool inputOpen = true;
  bool syncSent = false;
  bool terminated = false;
  double terminatedAt = 0;
  for (;;) {
    int fds[2] = { inspector.notifyFd(), inputOpen ? 0 : -1 };
    bool ready[2];
    if (NTKWaitReadable(fds, 2, 200, ready) < 0)
      break;
    if (ready[0])
      inspector.handleEvents(terminal);
    inspector.tick();
    if (ready[1]) {
      char buffer[4096];
      ssize_t n = read(0, buffer, sizeof(buffer));
      if (n <= 0) {
        inputOpen = false;
        if (!partial.empty())
          lines.push_back(partial);
        partial.clear();
      } else {
        partial.append(buffer, (size_t)n);
        size_t end;
        while ((end = partial.find('\n')) != std::string::npos) {
          lines.push_back(partial.substr(0, end));
          partial.erase(0, end + 1);
        }
      }
    }
    // send what was typed, once the Newton is there
    while (inspector.isConnected() && !lines.empty()) {
      std::string line = lines.front();
      lines.pop_front();
      if (line.compare(0, 5, ":pkg ") == 0) {         // upload a package
        std::string path = line.substr(5);
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) {
          printf("ntk: can't read %s\n", path.c_str());
          continue;
        }
        std::vector<uint8_t> package;
        uint8_t chunk[4096];
        size_t n;
        while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
          package.insert(package.end(), chunk, chunk + n);
        fclose(f);
        inspector.loadPackage(package);
        continue;
      }
      if (line.compare(0, 6, ":pkgx ") == 0) {        // delete a package by name
        inspector.deletePackage(line.substr(6));
        continue;
      }
      bool asCode = !line.empty() && line[0] == '=';
      for (RefVar &block : CompileLine(asCode ? line.substr(1) : line)) {
        if (asCode) {
          inspector.call(block);
          terminal.callsSent++;
        } else
          inspector.evaluate(block);
      }
    }
    // the end of the input: wait for the answers (a last 'code' call comes
    // back after everything sent before it), then 'term'
    if (!inputOpen && lines.empty()) {
      if (!inspector.isConnected() && !terminated && !syncSent)
        break;                        // nothing to do, nobody there
      if (inspector.isConnected() && !syncSent) {
        std::vector<RefVar> sync = CompileLine("nil");
        inspector.call(sync[0]);
        terminal.quietCall = ++terminal.callsSent;
        syncSent = true;
      } else if (inspector.isConnected() && inspector.callsPending() == 0 && !terminated) {
        inspector.terminate();
        terminated = true;
        terminatedAt = Seconds();
      }
      if (terminated && (inspector.pending() == 0
                         || Seconds() - terminatedAt > 3.0))
        break;
      if (syncSent && !inspector.isConnected() && !terminated)
        break;                        // the Newton went away meanwhile
    }
    fflush(stdout);
  }
  usleep(300000);                     // let the Newton's LD arrive
  inspector.handleEvents(terminal);
  inspector.stop();
  printf("ntk: done\n");
  fflush(stdout);
  return 0;
#endif
}
