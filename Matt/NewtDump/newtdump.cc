/*
 File: newtdump.cc

 newtdump: read a Newton's ROM and the memory of its PCMCIA cards, from
 their physical addresses, over the Docking Protocol: nothing to install
 on the Newton. The user starts a connection on the Newton (Dock or
 Connection: "Connect"); newtdump answers as the desktop, installs a
 protocol extension ('pext', Matt/NewtDump/Newton/Extension.ns) whose ARM
 code maps physical memory read only (TUPhys, TLicenseeVAddress) and keeps
 a card powered (VccOn), and asks for the memory a block at a time, so MNP
 and the Newton keep up on a slow serial line. A block whose bytes are all
 the same comes back as one number. The connection may drop or hang:
 newtdump waits for the Newton to connect again and goes on where it was;
 --resume continues an earlier run's files.

 Physical addresses (MessagePad 2x00, eMate 300, and Einstein): the ROM is
 8 MB at 0; PCMCIA socket n at 0x30000000 + n * 0x10000000, with the
 attribute memory (CIS) at +0, the common memory at +0x08000000, the
 socket's control registers at +0x0C000000 (+0x1C00: the pins, card detect
 in 0x000C; +0x2800: 0x0020 Vcc on).
 */

#include "Matt/NewtDump/CIS.h"
#include "Matt/NewtDump/DockLink.h"
#include "Matt/NewtDump/NSOF.h"

#include <chrono>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

extern const unsigned char kNewtDumpExtension[];   // Extension.ns as NSOF (CMake)
extern const size_t kNewtDumpExtensionSize;

namespace {

const char *kVersion = "1.0";

/*--- Machines ---*/

struct Machine
{
  const char *name;
  uint32_t type;              // NewtonInfo's fMachineType (Gestalt)
  int sockets;
  uint32_t romBase;
  uint32_t romSize;
};

const Machine kMachines[] = {
  { "MessagePad 2000/2100", 0x10003000, 2, 0, 8 << 20 },
  { "eMate 300", 0x10004000, 1, 0, 8 << 20 },
};

const uint32_t kSocketBase0 = 0x30000000, kSocketStride = 0x10000000;
const uint32_t kAttributeOffset = 0, kCommonOffset = 0x08000000, kControlOffset = 0x0C000000;
const uint32_t kRegPins = 0x1C00, kRegVcc = 0x2800, kRegVoltage = 0x2C00;
const uint32_t kWindow = 1 << 20;                 // mapped at a time (licensee space is never given back)
const uint32_t kPage = 4096;

/*--- Options ---*/

struct Options
{
  std::string target = "tcp";
  long baud = 38400;
  std::string output = ".";
  uint32_t block = 2048;
  long pauseMs = 0;
  bool resume = false;
  std::string password;
  uint64_t cardSize = 0;
  uint32_t romSize = 0;
  std::string machine;                              // "", mp2x00, emate
  std::map<int, uint32_t> socketBases;
  bool verbose = false;
  std::vector<std::string> command;
};

void Usage()
{
  fprintf(stderr,
"newtdump %s: read a Newton's ROM and PCMCIA cards over the Docking Protocol\n"
"\n"
"usage: newtdump [options] <command>\n"
"\n"
"commands:\n"
"  rom                   the ROM (8 MB from physical 0) -> rom.bin\n"
"  card [<socket>]       PCMCIA cards (all, or one socket): the CIS (decoded:\n"
"                        card<n>-cis.txt), the attribute memory\n"
"                        (card<n>-attribute.bin), the common memory\n"
"                        (card<n>-common.bin)\n"
"  all                   the ROM and all cards\n"
"  phys <addr> <size>    physical memory (multiples of 4 KB; 0x... or decimal)\n"
"                        -> phys-<addr>.bin\n"
"  info                  the Newton and its sockets; reads nothing\n"
"\n"
"On the Newton, start a connection (Dock or Connection, \"Connect\") after\n"
"newtdump says it is waiting. Nothing has to be installed on the Newton.\n"
"\n"
"options:\n"
"  -t, --target <t>      tcp (default: 127.0.0.1:3679, Einstein's network client),\n"
"                        tcp:<port>, serial:<device>, pty (Einstein's\n"
"                        /tmp/pty.BasiliskII)\n"
"  -b, --baud <bps>      serial speed (default 38400)\n"
"  -o, --output <dir>    where the files go (default: the current directory)\n"
"  --block <bytes>       bytes per request (default 2048; 256 to 16384, a power of 2)\n"
"  --pause <ms>          wait after each block (default 0)\n"
"  --resume              go on with files an earlier run didn't finish\n"
"  --password <p>        the Newton's password, if it has one\n"
"  --size <bytes>        a card's common memory size, if its CIS doesn't say\n"
"  --rom-size <bytes>    the ROM's size (default 8 MB)\n"
"  --machine <m>         mp2x00 or emate (default: what the Newton says it is)\n"
"  --socket-base <n>=<addr>  socket n's physical base (default 0x30000000 + n * 0x10000000)\n"
"  -v, --verbose         every request and answer\n"
"\n"
"Sizes and addresses: decimal, 0x hex, or with K or M (e.g. 2M).\n", kVersion);
}

uint64_t ParseNumber(const std::string &inText, const char *inWhat)
{
  char *end = nullptr;
  uint64_t value = strtoull(inText.c_str(), &end, 0);
  if (end == inText.c_str())
    throw std::runtime_error(std::string(inWhat) + ": not a number: " + inText);
  if (*end == 'K' || *end == 'k') { value <<= 10; end++; }
  else if (*end == 'M' || *end == 'm') { value <<= 20; end++; }
  if (*end)
    throw std::runtime_error(std::string(inWhat) + ": not a number: " + inText);
  return value;
}

std::string SizeText(double inBytes)
{
  char b[32];
  if (inBytes >= 1024 * 1024)
    snprintf(b, sizeof(b), "%.1f MB", inBytes / (1024 * 1024));
  else if (inBytes >= 1024)
    snprintf(b, sizeof(b), "%.1f KB", inBytes / 1024);
  else
    snprintf(b, sizeof(b), "%.0f bytes", inBytes);
  return b;
}

std::string Hex32(uint32_t inValue)
{
  char b[16];
  snprintf(b, sizeof(b), "0x%08X", inValue);
  return b;
}

double Now()
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

volatile sig_atomic_t gInterrupted = 0;
void OnInterrupt(int) { gInterrupted = 1; }

/*--- Progress: one line, with the rate and the time left ---*/

class Progress
{
public:
  void start(const std::string &inWhat, uint64_t inTotal, uint64_t inDone)
  {
    mWhat = inWhat;
    mTotal = inTotal;
    mDone = mStartDone = inDone;
    mStart = mLast = Now();
    mRate = 0;
    mSamples.clear();
    mSaved = 0;
    show(true);
  }
  void add(uint64_t inBytes, bool inUniform)
  {
    mDone += inBytes;
    if (inUniform)
      mSaved += inBytes;
    show(false);
  }
  void finish(bool inComplete)
  {
    show(true);
    double seconds = Now() - mStart;
    fprintf(stderr, "\n%s: %s in %.0f s%s%s\n", mWhat.c_str(), SizeText((double)(mDone - mStartDone)).c_str(),
            seconds, mSaved ? (", " + SizeText((double)mSaved) + " sent as one byte (all the same)").c_str() : "",
            inComplete ? "" : " (not finished)");
  }
private:
  void show(bool inNow)
  {
    double now = Now();
    if (!inNow && now - mLast < 0.25)
      return;
    mLast = now;
    mSamples.push_back({ now, mDone });
    while (mSamples.size() > 2 && now - mSamples.front().first > 20)   // the last 20 s
      mSamples.erase(mSamples.begin());
    double rate = 0;
    if (mSamples.size() >= 2 && mSamples.back().first > mSamples.front().first)
      rate = (double)(mSamples.back().second - mSamples.front().second)
           / (mSamples.back().first - mSamples.front().first);
    double fraction = mTotal ? (double)mDone / (double)mTotal : 1;
    const int kWidth = 24;
    int filled = (int)(fraction * kWidth);
    std::string bar(kWidth, '-');
    for (int i = 0; i < filled && i < kWidth; i++)
      bar[i] = '#';
    std::string left = "";
    if (rate > 0 && mDone < mTotal) {
      long seconds = (long)((double)(mTotal - mDone) / rate);
      char b[48];
      snprintf(b, sizeof(b), "%ld:%02ld:%02ld left", seconds / 3600, seconds / 60 % 60, seconds % 60);
      left = b;
    }
    fprintf(stderr, "\r%-10s %3.0f%% [%s] %s of %s  %s/s  %s   ", mWhat.c_str(), fraction * 100, bar.c_str(),
            SizeText((double)mDone).c_str(), SizeText((double)mTotal).c_str(), SizeText(rate).c_str(), left.c_str());
    fflush(stderr);
  }
  std::string mWhat;
  uint64_t mTotal = 0, mDone = 0, mStartDone = 0, mSaved = 0;
  double mStart = 0, mLast = 0, mRate = 0;
  std::vector<std::pair<double, uint64_t>> mSamples;
};

/*--- The session with the Newton ---*/

struct ConnectionLost : std::runtime_error
{
  using std::runtime_error::runtime_error;
};

/** The user ended the session on the Newton (Disconnect: 'opca', or 'disc'). */
struct Cancelled : std::runtime_error
{
  using std::runtime_error::runtime_error;
};

class Session
{
public:
  explicit Session(const Options &inOptions) : mOptions(inOptions) {}

  void open()
  {
    std::string target = mOptions.target;
    if (target == "pty")
      target = "serial:/tmp/pty.BasiliskII";
    if (target.rfind("serial:", 0) == 0 && target.find('@') == std::string::npos)
      target += "@" + std::to_string(mOptions.baud);
    std::string error;
    if (!mLink.start(target, error))
      throw std::runtime_error(error);
    mTarget = target;
  }

  /** Wait for the Newton and dock: the handshake, the password, the
      extension. inAgain: after a lost connection. */
  void connect(bool inAgain)
  {
    fprintf(stderr, "%s on %s. On the Newton: Dock (or Connection), \"Connect\".\n",
            inAgain ? "Waiting for the Newton to connect again" : "Waiting for a Newton", mTarget.c_str());
    for (;;) {
      if (gInterrupted)
        throw std::runtime_error("interrupted");
      if (!mLink.waitLinkUp(0.5))
        continue;
      try {
        handshake();
        break;
      } catch (ConnectionLost &e) {
        fprintf(stderr, "The connection broke while docking (%s); waiting again.\n", e.what());
      }
    }
    mMaps.clear();   // the Newton keeps its mappings; they are asked for again (cheap)
    nsof::Ref hello = request(nsof::Frame({{ "op", nsof::Symbol("hello") }}));
    if (mOptions.verbose)
      fprintf(stderr, "extension: %s\n", hello->describe().c_str());
  }

  void close()
  {
    if (mLink.isUp() && !mDisconnected) {
      mLink.send(DockCommand("disc"));
      double until = Now() + 2;
      while (mLink.pending() > 0 && Now() < until) {
        DockPacket p;
        mLink.receive(p, 0.1);
      }
    }
    mLink.stop();
  }

  const Machine &machine() const { return mMachine; }
  uint32_t socketBase(int inSocket) const
  {
    auto it = mOptions.socketBases.find(inSocket);
    return it != mOptions.socketBases.end() ? it->second : kSocketBase0 + (uint32_t)inSocket * kSocketStride;
  }
  std::string newtonName() const { return mName; }
  std::string describeNewton() const { return mDescription; }

  /** A request to the extension; its answer. ConnectionLost if the
      connection is gone or nothing came back in time. */
  nsof::Ref request(const nsof::Ref &inRequest, double inSeconds = 60)
  {
    if (mOptions.verbose)
      fprintf(stderr, "-> %s\n", inRequest->describe().c_str());
    mLink.sendRef(DockCommand("NDmp"), inRequest);
    DockPacket p;
    for (;;) {
      if (!mLink.receive(p, inSeconds))
        throw ConnectionLost(mLink.gone() ? mLink.why() : "the Newton didn't answer in time");
      if (p.command == DockCommand("NDrs"))
        break;
      checkCancel(p);
      if (mOptions.verbose)
        fprintf(stderr, "(ignored: '%s')\n", DockCommandName(p.command).c_str());
    }
    // an integer comes as a 4-byte long, not NSOF (TDocker::WriteCommand,
    // ROM 0x00095B64: "integers as longs"); anything else is NSOF (WriteRef)
    nsof::Ref answer = p.data.size() == 4 ? nsof::Integer((int32_t)p.longData(0)) : nsof::Read(p.data);
    if (mOptions.verbose)
      fprintf(stderr, "<- %s\n", answer->describe().c_str());
    return answer;
  }

  /** The bytes at a physical address (a multiple of 4, not across a 1 MB
      window); sets ioUniform if they all were the same. */
  std::vector<uint8_t> read(uint32_t inAddress, uint32_t inLength, bool &outUniform, bool inBytes = false)
  {
    uint32_t window = inAddress & ~(kWindow - 1);
    int map = mapWindow(window, kWindow);
    std::vector<std::pair<std::string, nsof::Ref>> slots = {
      { "op", nsof::Symbol("read") }, { "map", nsof::Integer(map) },
      { "offset", nsof::Integer(inAddress - window) }, { "length", nsof::Integer(inLength) } };
    if (inBytes)              // byte reads (attribute memory answers only those)
      slots.push_back({ "bytes", nsof::True() });
    nsof::Ref answer = request(nsof::Frame(slots));
    outUniform = false;
    if (answer->kind == nsof::Value::kInteger) {
      outUniform = true;
      return std::vector<uint8_t>(inLength, (uint8_t)answer->integer);
    }
    if (answer->kind == nsof::Value::kBinary && answer->bytes.size() == inLength)
      return answer->bytes;
    throw std::runtime_error("reading " + Hex32(inAddress) + ": " + errorText(answer));
  }

  /** A 32-bit register (a small mapping of its own). */
  uint32_t readRegister(uint32_t inAddress)
  {
    uint32_t page = inAddress & ~(kPage - 1);
    int map = mapWindow(page, kPage);
    nsof::Ref answer = request(nsof::Frame({{ "op", nsof::Symbol("read") }, { "map", nsof::Integer(map) },
                                            { "offset", nsof::Integer(inAddress - page) },
                                            { "length", nsof::Integer(4) }}));
    if (answer->kind == nsof::Value::kInteger)
      return (uint32_t)(answer->integer & 0xFF) * 0x01010101u;
    if (answer->kind == nsof::Value::kBinary && answer->bytes.size() == 4)
      return (uint32_t)answer->bytes[0] << 24 | (uint32_t)answer->bytes[1] << 16
           | (uint32_t)answer->bytes[2] << 8 | answer->bytes[3];
    throw std::runtime_error("reading the register " + Hex32(inAddress) + ": " + errorText(answer));
  }

  /** Card power: VccOn (on) or VccOff. */
  long power(int inSocket, bool inOn)
  {
    // switching off after a cancel: the Newton may not answer any more
    nsof::Ref answer = request(nsof::Frame({{ "op", nsof::Symbol("power") }, { "socket", nsof::Integer(inSocket) },
                                            { "on", inOn ? nsof::True() : nsof::Nil() }}), mCancelled ? 5 : 60);
    nsof::Ref result = answer->slot("result");
    if (!result || !result->isInteger())
      throw std::runtime_error("card power: " + errorText(answer));
    return (long)result->integer;
  }

private:
  int mapWindow(uint32_t inBase, uint32_t inSize)
  {
    uint64_t key = (uint64_t)inBase << 32 | inSize;
    auto it = mMaps.find(key);
    if (it != mMaps.end())
      return it->second;
    nsof::Ref answer = request(nsof::Frame({{ "op", nsof::Symbol("map") }, { "page", nsof::Integer(inBase / kPage) },
                                            { "pages", nsof::Integer(inSize / kPage) }}));
    nsof::Ref map = answer->slot("map");
    if (!map || !map->isInteger())
      throw std::runtime_error("mapping " + Hex32(inBase) + " (" + SizeText(inSize) + "): " + errorText(answer));
    mMaps[key] = (int)map->integer;
    return (int)map->integer;
  }

  // The user tapped Disconnect on the Newton: it sends 'opca' and throws
  // away everything until the desktop acknowledges ('ocaa') or ends the
  // session (ROM 0x00094450); or it just ends it ('disc')
  void checkCancel(const DockPacket &inPacket)
  {
    if (inPacket.command == DockCommand("opca") || inPacket.command == DockCommand("opcn")) {
      mLink.send(DockCommand("ocaa"));
      mCancelled = true;
      throw Cancelled("cancelled on the Newton");
    }
    if (inPacket.command == DockCommand("disc")) {
      mCancelled = true;
      mDisconnected = true;
      throw Cancelled("the Newton ended the connection");
    }
  }

public:
  bool cancelled() const { return mCancelled; }

private:
  static std::string errorText(const nsof::Ref &inAnswer)
  {
    nsof::Ref error = inAnswer ? inAnswer->slot("error") : nullptr;
    nsof::Ref name = inAnswer ? inAnswer->slot("name") : nullptr;
    std::string s = "error";
    if (error && error->isInteger())
      s += " " + std::to_string(error->integer);
    if (name && name->kind == nsof::Value::kString)
      s += " (" + name->text + ")";
    if (!error)
      s = "unexpected answer " + (inAnswer ? inAnswer->describe() : std::string("nil"));
    return s;
  }

  DockPacket expect(uint32_t inCommand, double inSeconds = 30)
  {
    DockPacket p;
    for (;;) {
      if (!mLink.receive(p, inSeconds))
        throw ConnectionLost(mLink.gone() ? mLink.why() : "no '" + DockCommandName(inCommand) + "' from the Newton");
      if (p.command == inCommand)
        return p;
      checkCancel(p);
      if (mOptions.verbose)
        fprintf(stderr, "(while waiting for '%s': '%s')\n", DockCommandName(inCommand).c_str(),
                DockCommandName(p.command).c_str());
    }
  }

  // The Docking Protocol's session setup (Dante: 2.x ROMs)
  void handshake()
  {
    const uint32_t kSettingUpSession = 1, kDanteProtocolVersion = 10, kMacDesktop = 0;
    expect(DockCommand("rtdk"), 3600);
    mLink.sendLong(DockCommand("dock"), kSettingUpSession);

    DockPacket name = expect(DockCommand("name"));
    uint32_t infoLength = name.longData(0);
    std::vector<uint32_t> info;
    for (size_t i = 4; i + 4 <= 4 + infoLength && i + 4 <= name.data.size(); i += 4)
      info.push_back(name.longData(i));
    mName.clear();
    for (size_t i = 4 + infoLength; i + 1 < name.data.size(); i += 2) {
      unsigned c = (unsigned)(name.data[i] << 8 | name.data[i + 1]);
      if (c == 0)
        break;
      mName += c < 128 ? (char)c : '?';
    }
    uint32_t machineType = info.size() > 2 ? info[2] : 0;
    uint32_t romVersion = info.size() > 3 ? info[3] : 0;
    uint32_t ramSize = info.size() > 5 ? info[5] : 0;
    if (romVersion < 0x00020000)
      throw std::runtime_error("this Newton runs NewtonOS 1 (ROM version " + Hex32(romVersion)
                               + "): newtdump needs NewtonOS 2");
    chooseMachine(machineType);
    char text[256];
    snprintf(text, sizeof(text), "%s: %s (machine type %s), ROM version %u.%u, %s RAM",
             mName.empty() ? "a Newton" : mName.c_str(), mMachine.name, Hex32(machineType).c_str(),
             romVersion >> 16, romVersion & 0xFFFF, SizeText(ramSize).c_str());
    mDescription = text;

    DockNonce ours = DockRandomNonce();
    std::vector<uint8_t> desktopInfo;
    for (uint32_t v : { kDanteProtocolVersion, kMacDesktop, ours.hi, ours.lo, kSettingUpSession, 0u })
      for (int shift = 24; shift >= 0; shift -= 8)
        desktopInfo.push_back((uint8_t)(v >> shift));
    std::vector<uint8_t> apps = nsof::Write(nsof::Array({ nsof::Frame({
      { "name", nsof::String("newtdump") }, { "id", nsof::Integer(2) }, { "version", nsof::Integer(1) },
      { "doesAuto", nsof::True() } }) }));
    desktopInfo.insert(desktopInfo.end(), apps.begin(), apps.end());
    mLink.send(DockCommand("dinf"), desktopInfo);

    DockPacket newtonInfo = expect(DockCommand("ninf"));
    DockNonce theirs{ newtonInfo.longData(4), newtonInfo.longData(8) };
    mLink.sendLong(DockCommand("wicn"), 0);
    expect(DockCommand("dres"));
    mLink.sendLong(DockCommand("stim"), 90);

    DockNonce expected = DockEncryptNonce(ours, mOptions.password);
    for (int attempt = 0; ; attempt++) {
      DockPacket pass = expect(DockCommand("pass"), 300);
      if (pass.longData(0) == expected.hi && pass.longData(4) == expected.lo)
        break;
      if (attempt >= 2) {
        mLink.sendLong(DockCommand("dres"), (uint32_t)-28022);
        throw std::runtime_error("the password doesn't match (--password)");
      }
      mLink.send(DockCommand("pwbd"));
    }
    DockNonce answer = DockEncryptNonce(theirs, mOptions.password);
    mLink.sendLongs(DockCommand("pass"), { answer.hi, answer.lo });

    // the desktop runs the session; our extension
    mLink.send(DockCommand("dsnc"));
    std::vector<uint8_t> extension;
    uint32_t id = DockCommand("NDmp");
    for (int shift = 24; shift >= 0; shift -= 8)
      extension.push_back((uint8_t)(id >> shift));
    extension.insert(extension.end(), kNewtDumpExtension, kNewtDumpExtension + kNewtDumpExtensionSize);
    mLink.send(DockCommand("pext"), extension);
    DockPacket result = expect(DockCommand("dres"));
    int32_t error = (int32_t)result.longData(0);
    if (error != 0 && error != -28020)   // -28020: already registered (a session before)
      throw std::runtime_error("the Newton didn't take newtdump's extension (error " + std::to_string(error) + ")");
    fprintf(stderr, "Connected: %s.\n", mDescription.c_str());
  }

  void chooseMachine(uint32_t inType)
  {
    if (!mOptions.machine.empty()) {
      mMachine = mOptions.machine == "emate" ? kMachines[1] : kMachines[0];
      return;
    }
    for (const Machine &m : kMachines)
      if (m.type == inType) {
        mMachine = m;
        return;
      }
    throw std::runtime_error("unknown machine type " + Hex32(inType)
                             + ": newtdump knows the MessagePad 2x00 and the eMate 300 (--machine to try one)");
  }

  const Options &mOptions;
  DockLink mLink;
  std::string mTarget;
  std::string mName, mDescription;
  Machine mMachine = kMachines[0];
  std::map<uint64_t, int> mMaps;
  bool mCancelled = false;
  bool mDisconnected = false;     // the Newton sent 'disc': none from us
public:
  bool disconnectedByNewton() const { return mDisconnected; }
};

/*--- Downloads ---*/

std::string OutputPath(const Options &inOptions, const std::string &inName)
{
  return inOptions.output + "/" + inName;
}

/** Read inSize bytes from inAddress into a file, a block at a time, with
    progress; reconnects when the connection breaks. False if interrupted.
    inBytes: byte reads instead of 32-bit ones (attribute memory). */
bool Download(Session &ioSession, const Options &inOptions, const std::string &inWhat, uint32_t inAddress,
              uint64_t inSize, const std::string &inPath, bool inBytes = false)
{
  uint64_t start = 0;
  if (inOptions.resume) {
    struct stat st;
    if (stat(inPath.c_str(), &st) == 0)
      start = std::min<uint64_t>((uint64_t)st.st_size, inSize) / inOptions.block * inOptions.block;
  }
  FILE *f = fopen(inPath.c_str(), start ? "r+b" : "wb");
  if (!f)
    throw std::runtime_error("can't write " + inPath);
  if (start) {
    fseek(f, (long)start, SEEK_SET);
    fprintf(stderr, "%s: going on at %s (--resume)\n", inPath.c_str(), SizeText((double)start).c_str());
  }
  Progress progress;
  progress.start(inWhat, inSize, start);
  uint64_t done = start;
  while (done < inSize && !gInterrupted) {
    uint32_t address = inAddress + (uint32_t)done;
    uint32_t length = (uint32_t)std::min<uint64_t>(inOptions.block, inSize - done);
    uint32_t toWindowEnd = kWindow - (address & (kWindow - 1));
    if (length > toWindowEnd)
      length = toWindowEnd;
    bool uniform = false;
    std::vector<uint8_t> data;
    try {
      data = ioSession.read(address, length, uniform, inBytes);
    } catch (ConnectionLost &e) {
      progress.finish(false);
      fprintf(stderr, "The connection is lost (%s) at %s.\n", e.what(), Hex32(address).c_str());
      fflush(f);
      ioSession.connect(true);
      progress.start(inWhat, inSize, done);
      continue;
    } catch (Cancelled &) {
      fclose(f);
      progress.finish(false);
      fprintf(stderr, "%s isn't complete: --resume goes on from %s.\n", inPath.c_str(),
              SizeText((double)done).c_str());
      throw;
    }
    fwrite(data.data(), 1, data.size(), f);
    done += length;
    progress.add(length, uniform);
    if (inOptions.pauseMs > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(inOptions.pauseMs));
    }
  }
  fclose(f);
  progress.finish(done >= inSize);
  if (done < inSize)
    fprintf(stderr, "%s isn't complete: --resume goes on from %s.\n", inPath.c_str(), SizeText((double)done).c_str());
  return done >= inSize;
}

void ReadROM(Session &ioSession, const Options &inOptions)
{
  uint32_t size = inOptions.romSize ? inOptions.romSize : ioSession.machine().romSize;
  Download(ioSession, inOptions, "ROM", ioSession.machine().romBase, size, OutputPath(inOptions, "rom.bin"));
}

/** A card's pins: card detect (both clear: inserted), voltage sense, write
    protect. */
std::string DescribePins(uint32_t inPins)
{
  std::string s = (inPins & 0x000C) == 0 ? "a card" : ((inPins & 0x000C) == 0x000C ? "no card" : "a card, not all the way in");
  if ((inPins & 0x000C) == 0) {
    bool vs1 = inPins & 0x0010, vs2 = inPins & 0x0020;
    s += vs1 && vs2 ? ", 5 V" : (vs2 && !vs1) || (!vs1 && !vs2) ? ", 3.3 V" : ", X.X V (not supported)";
    if (inPins & 0x0200)
      s += ", write protected";
  }
  return s;
}

void ReadCard(Session &ioSession, const Options &inOptions, int inSocket, bool inOnlyInfo)
{
  uint32_t base = ioSession.socketBase(inSocket);
  uint32_t pins = ioSession.readRegister(base + kControlOffset + kRegPins);
  printf("Socket %d (%s): %s (pins %s)\n", inSocket, Hex32(base).c_str(), DescribePins(pins).c_str(),
         Hex32(pins & 0xFFFF).c_str());
  fflush(stdout);
  if ((pins & 0x000C) != 0 || inOnlyInfo)
    return;

  long on = ioSession.power(inSocket, true);
  uint32_t vcc = ioSession.readRegister(base + kControlOffset + kRegVcc);
  uint32_t voltage = ioSession.readRegister(base + kControlOffset + kRegVoltage);
  printf("  power: VccOn %ld, Vcc %s (%s)\n", on, (vcc & 0x0020) ? "on" : "off",
         (voltage & 0x0003) == 1 ? "5 V" : (voltage & 0x0003) == 2 ? "3.3 V" : "?");
  std::string prefix = "card" + std::to_string(inSocket);
  try {
    // the CIS: attribute memory (its first 8 KB hold 4 KB of CIS bytes),
    // read a byte at a time as the ROM does (TCardCISIterator::ReadCIS)
    const uint32_t kAttributeRead = 8192;
    std::string attributePath = OutputPath(inOptions, prefix + "-attribute.bin");
    Download(ioSession, inOptions, prefix + " attr", base + kAttributeOffset, kAttributeRead, attributePath, true);
    if (gInterrupted)
      throw std::runtime_error("interrupted");
    std::vector<uint8_t> attribute(kAttributeRead);
    if (FILE *f = fopen(attributePath.c_str(), "rb")) {
      attribute.resize(fread(attribute.data(), 1, attribute.size(), f));
      fclose(f);
    }
    CISInfo cis = ParseCIS(attribute);
    std::string where = "attribute memory";
    if (!cis.valid) {
      // a ROM card may have its CIS at the start of common memory
      bool uniform;
      std::vector<uint8_t> common = ioSession.read(base + kCommonOffset, 4096, uniform);
      cis = ParseCIS(common, true);
      where = "common memory";
    }
    std::vector<std::string> lines;
    if (cis.valid) {
      lines.push_back("CIS of the card in socket " + std::to_string(inSocket) + " (in " + where + ", "
                      + cis.layout + "):");
      lines.insert(lines.end(), cis.report.begin(), cis.report.end());
    } else {
      lines.push_back("No CIS found on the card in socket " + std::to_string(inSocket)
                      + " (attribute memory, start of common memory).");
    }
    for (const std::string &l : lines)
      printf("%s\n", l.c_str());
    fflush(stdout);
    if (FILE *f = fopen(OutputPath(inOptions, prefix + "-cis.txt").c_str(), "w")) {
      fprintf(f, "%s\n", ioSession.describeNewton().c_str());
      for (const std::string &l : lines)
        fprintf(f, "%s\n", l.c_str());
      fclose(f);
    }

    uint64_t size = inOptions.cardSize ? inOptions.cardSize : cis.commonSize;
    if (size == 0)
      throw std::runtime_error("the card's size is unknown (no CIS device entry): give it with --size");
    if (size > 64 << 20)
      throw std::runtime_error("the card says it has " + SizeText((double)size) + ", more than a socket's 64 MB");
    Download(ioSession, inOptions, prefix + " mem", base + kCommonOffset, size,
             OutputPath(inOptions, prefix + "-common.bin"));
  } catch (...) {
    // let the card go (VccOff), unless the Newton has ended the session
    if (!ioSession.disconnectedByNewton())
      try { ioSession.power(inSocket, false); } catch (...) {}
    throw;
  }
  ioSession.power(inSocket, false);
}

} // namespace


int main(int argc, char **argv)
{
  Options options;
  try {
    for (int i = 1; i < argc; i++) {
      std::string a = argv[i];
      auto value = [&](const char *inName) -> std::string {
        if (i + 1 >= argc)
          throw std::runtime_error(std::string(inName) + " needs a value");
        return argv[++i];
      };
      if (a == "-h" || a == "--help") { Usage(); return 0; }
      else if (a == "-t" || a == "--target") options.target = value("--target");
      else if (a == "-b" || a == "--baud") options.baud = (long)ParseNumber(value("--baud"), "--baud");
      else if (a == "-o" || a == "--output") options.output = value("--output");
      else if (a == "--block") options.block = (uint32_t)ParseNumber(value("--block"), "--block");
      else if (a == "--pause") options.pauseMs = (long)ParseNumber(value("--pause"), "--pause");
      else if (a == "--resume") options.resume = true;
      else if (a == "--password") options.password = value("--password");
      else if (a == "--size") options.cardSize = ParseNumber(value("--size"), "--size");
      else if (a == "--rom-size") options.romSize = (uint32_t)ParseNumber(value("--rom-size"), "--rom-size");
      else if (a == "--machine") {
        options.machine = value("--machine");
        if (options.machine != "mp2x00" && options.machine != "emate")
          throw std::runtime_error("--machine: mp2x00 or emate");
      }
      else if (a == "--socket-base") {
        std::string v = value("--socket-base");
        size_t eq = v.find('=');
        if (eq == std::string::npos)
          throw std::runtime_error("--socket-base: <socket>=<address>");
        options.socketBases[(int)ParseNumber(v.substr(0, eq), "--socket-base")] =
          (uint32_t)ParseNumber(v.substr(eq + 1), "--socket-base");
      }
      else if (a == "-v" || a == "--verbose") options.verbose = true;
      else if (!a.empty() && a[0] == '-') throw std::runtime_error("unknown option " + a + " (--help)");
      else options.command.push_back(a);
    }
    if (options.block < 256 || options.block > 16384 || (options.block & (options.block - 1)))
      throw std::runtime_error("--block: a power of 2 from 256 to 16384");
    if (options.romSize % kPage || options.cardSize % kPage)
      throw std::runtime_error("--rom-size and --size: multiples of 4 KB");
    if (options.command.empty()) {
      Usage();
      return 1;
    }
  } catch (std::exception &e) {
    fprintf(stderr, "newtdump: %s\n", e.what());
    return 1;
  }

  const std::string &cmd = options.command[0];
  if (cmd != "rom" && cmd != "card" && cmd != "all" && cmd != "phys" && cmd != "info") {
    fprintf(stderr, "newtdump: unknown command %s (--help)\n", cmd.c_str());
    return 1;
  }
  { std::error_code err; std::filesystem::create_directories(options.output, err); }

  signal(SIGINT, OnInterrupt);
#if defined(SIGPIPE)
  signal(SIGPIPE, SIG_IGN);
#endif
  Session session(options);
  int result = 0;
  try {
    session.open();
    session.connect(false);
    printf("%s\n", session.describeNewton().c_str());
    fflush(stdout);
    if (cmd == "rom" || cmd == "all")
      ReadROM(session, options);
    if ((cmd == "card" || cmd == "all" || cmd == "info") && !gInterrupted) {
      int sockets = session.machine().sockets;
      if (cmd == "card" && options.command.size() > 1) {
        int socket = (int)ParseNumber(options.command[1], "card");
        if (socket < 0 || socket >= sockets)
          throw std::runtime_error("this Newton has sockets 0 to " + std::to_string(sockets - 1));
        ReadCard(session, options, socket, false);
      } else {
        for (int s = 0; s < sockets && !gInterrupted; s++)
          ReadCard(session, options, s, cmd == "info");
      }
    }
    if (cmd == "phys") {
      if (options.command.size() < 3)
        throw std::runtime_error("phys <address> <size>");
      uint64_t address = ParseNumber(options.command[1], "phys"), size = ParseNumber(options.command[2], "phys");
      if (address % kPage || size % kPage || address + size > 0x100000000ULL)
        throw std::runtime_error("phys: address and size multiples of 4 KB, within 4 GB");
      char name[48];
      snprintf(name, sizeof(name), "phys-%08" PRIX64 ".bin", address);
      Download(session, options, "phys", (uint32_t)address, size, OutputPath(options, name));
    }
    if (gInterrupted) {
      fprintf(stderr, "Interrupted.\n");
      result = 1;
    }
  } catch (Cancelled &e) {
    fprintf(stderr, "Stopped: %s.\n", e.what());
    result = 1;
  } catch (std::exception &e) {
    fprintf(stderr, "\nnewtdump: %s\n", e.what());
    result = 1;
  }
  session.close();
  return result;
}
