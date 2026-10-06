/*
 File: NTKInspector.h
 The desktop side of NTK's Inspector: the Toolkit packets on an MNP link to
 a Newton (Matt/Toolkit Protocol.md, sections 2 to 4 and 6).
   'newt' 'ntp ' <command> <length> <data>     all words big-endian
 The Newton connects ('cnnt'; we answer 'okln'), sends text, results,
 objects, exceptions, and break loop entries and exits; we send NewtonScript
 compiled here: 'lscb' (the REP runs it like typed input and prints the
 result as text) or 'code' (the result comes back as an object), and
 packages.
 MNP runs in its own thread (Matt/MNP.h); everything here runs on the main
 thread, because objects (NSOF) live in the NewtonScript heap.
 */

#ifndef MATT_NTKINSPECTOR_H
#define MATT_NTKINSPECTOR_H

#include "Frames/Objects.h"
#include "MNP.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

/** A four character code: NTKCommand("lscb"). */
constexpr uint32_t NTKCommand(const char (&inCode)[5])
{
  return ((uint32_t)(uint8_t)inCode[0] << 24) | ((uint32_t)(uint8_t)inCode[1] << 16)
       | ((uint32_t)(uint8_t)inCode[2] << 8) | (uint32_t)(uint8_t)inCode[3];
}

std::string NTKCommandName(uint32_t inCommand);

struct NTKPacket {
  uint32_t command = 0;
  uint32_t length = 0;          // the length word as sent (see NTKPacketReader)
  std::vector<uint8_t> data;    // everything after the header
};

/** The bytes of a packet: header, length (inLength, or the data's size if
    inLength < 0), data. No padding. */
std::vector<uint8_t> NTKMakePacket(uint32_t inCommand, const uint8_t *inData = nullptr,
                                   size_t inSize = 0, long inLength = -1);

/** Finds the packets in the byte stream from the Newton. The length word
    doesn't always say how much data follows (Toolkit Protocol.md, 6.1):
    'code' (a reply) has the request's length, then u32 size + NSOF;
    'eerr' has length + 4 bytes of data, 'estr' and 'eref' length + 8. */
class NTKPacketReader
{
public:
  /** inFromNewton false: packets from the desktop, where the length word
      always says how much data follows (for newtc playing the Newton). */
  NTKPacketReader(bool inFromNewton = true) : mFromNewton(inFromNewton) {}
  void feed(const uint8_t *inData, size_t inSize);
  /** The next whole packet, if there is one. */
  bool next(NTKPacket &outPacket);
  /** Bytes that came outside a packet (dropped), for messages. */
  std::vector<uint8_t> takeJunk();
  void reset() { mBuffer.clear(); mJunk.clear(); }
private:
  bool mFromNewton;
  std::vector<uint8_t> mBuffer;
  std::vector<uint8_t> mJunk;
};

/** NSOF in memory. */
std::vector<uint8_t> NTKFlatten(RefArg inObject);
Ref NTKUnflatten(const uint8_t *inData, size_t inSize);

class NTKInspector
{
public:
  /** What the Newton says. Everything is called on the main thread from
      handleEvents(). */
  struct Listener {
    virtual ~Listener() {}
    virtual void ntkConnected(const std::string &inPeer) {}       // 'cnnt' answered
    virtual void ntkDisconnected(const std::string &inReason) {}  // link or Toolkit gone
    virtual void ntkText(const std::string &inUTF8) {}            // 'text' (CR -> LF)
    virtual void ntkResult(long inError, uint32_t inCommand) {}   // 'rslt' for inCommand
    virtual void ntkObject(const std::string &inCommand, RefArg inObject) {}  // 'fobj', 'fstk'
    virtual void ntkCodeResult(RefArg inResult) {}                // 'code' reply
    virtual void ntkException(Exception &inException) {}          // 'eerr', 'estr', 'eref'
    virtual void ntkBreakLoop(bool inEntered) {}                  // 'eext', 'bext'
    virtual void ntkOther(const NTKPacket &inPacket) {}           // anything else
  };

  NTKInspector() {}
  ~NTKInspector() { stop(); }

  /** Wait for a Newton on the target (NTKTransport.h). */
  bool start(const std::string &inTarget, std::string &outError);
  void stop();

  /** Readable when something happened: then call handleEvents(). */
  int notifyFd() const { return mLink.notifyFd(); }
  void handleEvents(Listener &inListener);

  /** The Toolkit connection is up ('cnnt' answered, not terminated). */
  bool isConnected() const { return mConnected; }
  /** The MNP link is up (after terminate(): until the Newton's LD). */
  bool linkIsUp() { return mLink.isUp(); }

  /** Send a code block: 'lscb' (REP input, output as text) ... */
  void evaluate(RefArg inCodeBlock);
  /** ... or 'code' (the result comes back to ntkCodeResult). */
  void call(RefArg inCodeBlock);
  /** 'code' calls sent and not answered yet. */
  int callsPending() const { return mCallsPending; }

  /** 'pkg ': install a package (its bytes); 'pkgX': delete one by name. */
  void loadPackage(const std::vector<uint8_t> &inPackage);
  void deletePackage(const std::string &inNameUTF8);

  /** 'term', then the MNP link ends. */
  void terminate();

  /** Call every second or so while waiting: after 15 s without a Toolkit
      packet either way, send 'stou' (30 s, the ROM's default), which the
      Newton answers. The ROM sets a 30 s idle timer on the NTK connection
      (TCMOIdleTimer: "if a connection is idle for that long, tear it
      down"), and MNP acknowledgements don't count. NEWTC_NTK_PING=<s>
      changes the 15 s (for tests). */
  void tick();

  /** Bytes not yet acknowledged by the Newton. */
  size_t pending() { return mLink.pending(); }

  std::string describe() const { return mLink.describe(); }

  MNPLink &link() { return mLink; }

private:
  void handlePacket(const NTKPacket &inPacket, Listener &inListener);
  void sendPacket(uint32_t inCommand, const std::vector<uint8_t> &inData, long inLength = -1);

  MNPLink mLink;
  NTKPacketReader mReader;
  std::deque<uint32_t> mAwaitingResult;   // commands sent that the Newton answers with 'rslt'
  bool mConnected = false;
  int mCallsPending = 0;
  double mLastTraffic = 0;              // a Toolkit packet sent or data received
};

/** newtc -ntk <target>: a terminal Inspector. Waits for a Newton; each line
    from stdin is NewtonScript, compiled here and sent with 'lscb' (like
    NTK's Inspector), or with 'code' if it starts with '=' (the result is
    printed). ":pkg <file>" uploads a package, ":pkgx <name>" deletes one.
    Prints what the Newton sends. At the end of stdin, waits for
    the answers, sends 'term', and ends. */
int NTKInspectorREPL(const std::string &inTarget);

#endif // MATT_NTKINSPECTOR_H
