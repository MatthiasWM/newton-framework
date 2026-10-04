/*
 File: MNP.h
 MNP, the Microcom Networking Protocol (class 4: error correction, no
 compression), as the Newton speaks it to NTK and the Dock: frames with a
 CRC on a byte stream, numbered data frames, acknowledgements. Details and
 sources: Matt/Toolkit Protocol.md, section 5.
 MNPLink runs the link in a thread of its own, so it stays up (answering,
 acknowledging, keeping it alive) whatever the main thread does: compiling,
 running NewtonScript, serving one DAP session after another. The main
 thread waits on notifyFd() (poll/select, or an event loop) and then takes
 the events with nextEvent(); it sends with send(), which never blocks.
 Like NTK we use the simplest form: one data frame (LT) in flight, each
 acknowledged (LA) before the next; resent after 1 s without LA; an LA
 every 3 s while nothing else is sent, or the Newton drops the link.
 */

#ifndef MATT_MNP_H
#define MATT_MNP_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class NTKTransport;

/*--- Framing: SYN DLE STX <header> <data> DLE ETX <CRC low> <CRC high> ---*/

enum MNPFrameType {
  kMNPLinkRequest = 1,      // LR
  kMNPLinkDisconnect = 2,   // LD
  kMNPLinkTransfer = 4,     // LT: data
  kMNPLinkAcknowledge = 5,  // LA
  kMNPLinkAttention = 6,    // LN (not used)
  kMNPLinkAttentionAck = 7  // LNA (not used)
};

/** CRC-16/ARC as MNP uses it (polynomial 0xA001 reflected, start 0). */
uint16_t MNPCrc(const uint8_t *inData, size_t inSize, uint16_t inCrc = 0);

/** The bytes on the wire for a frame: header (starting with its own
    length) plus data, DLE doubled, CRC over header, data, and ETX. */
std::vector<uint8_t> MNPEncodeFrame(const std::vector<uint8_t> &inHeader,
                                    const uint8_t *inData = nullptr, size_t inSize = 0);

/** Finds the frames in a byte stream, which may come in any pieces. */
class MNPDecoder
{
public:
  enum Result { kMore, kFrame, kBadFrame };
  /** One byte: kFrame when a frame is complete (frame() has it: header and
      data, without framing and CRC), kBadFrame for a CRC error or a bad
      escape (the frame is dropped), else kMore. */
  Result feed(uint8_t inByte);
  const std::vector<uint8_t> &frame() const { return mFrame; }
  void reset() { mState = 0; mFrame.clear(); }
private:
  int mState = 0;
  std::vector<uint8_t> mFrame;
  uint16_t mReceivedCrc = 0;
};

/*--- The link ---*/

class MNPLink
{
public:
  struct Event {
    enum Kind {
      kConnected,   // the transport is connected (TCP accepted, serial open)
      kLinkUp,      // LR answered and acknowledged: data can flow
      kData,        // data from the Newton (one LT's worth, in order)
      kLinkDown     // the link is gone (text says why); waiting for the next
    } kind;
    std::vector<uint8_t> data;
    std::string text;
  };

  /** Which side we play: the desktop (answers the Newton's LR; the default)
      or the Newton (sends the LR, as Einstein does; for -ntk-device). */
  enum Role { kDesktop, kNewton };

  MNPLink();
  ~MNPLink();

  /** Before start(). */
  void setRole(Role inRole) { mRole = inRole; }

  /** Open the target (see NTKTransport.h) and start the link's thread.
      False and an error message if the target can't be used. */
  bool start(const std::string &inTarget, std::string &outError);

  /** End the link (an LD if it is up) and the thread. */
  void stop();

  /** Readable when events are waiting. */
  int notifyFd() const { return mNotify[0]; }

  /** The next event, if any (never blocks). Call it until it returns false
      whenever notifyFd() is readable. */
  bool nextEvent(Event &outEvent);

  /** Queue bytes for the Newton; they go out once the link is up, in LTs
      of up to 256 bytes. Data still queued when the link goes down is
      dropped. */
  void send(const uint8_t *inData, size_t inSize);
  void send(const std::vector<uint8_t> &inData) { send(inData.data(), inData.size()); }

  /** Bytes queued or in flight, not acknowledged yet. */
  size_t pending();

  /** End this link with an LD (the Newton may connect again). */
  void disconnect();

  bool isUp();

  /** Write every frame, both ways, to this file (one line each; nullptr: off). */
  void setTrace(FILE *inTrace) { mTrace = inTrace; }

  std::string describe() const;

private:
  enum State { kWaiting, kNegotiating, kUp };

  void run();
  void connected();
  void handleFrame(const std::vector<uint8_t> &inFrame);
  void sendFrame(const std::vector<uint8_t> &inHeader, const uint8_t *inData = nullptr, size_t inSize = 0);
  void sendLA();
  void sendNextLT();
  void linkDown(const std::string &inReason, bool inSendLD, bool inDropConnection);
  void post(Event::Kind inKind, const std::vector<uint8_t> &inData = {}, const std::string &inText = {});
  void trace(const char *inDirection, const std::vector<uint8_t> &inFrame);
  static double now();

  NTKTransport *mTransport = nullptr;
  std::thread mThread;
  int mNotify[2] = { -1, -1 };    // events for the main thread
  int mWake[2] = { -1, -1 };      // wakes the link's thread (send, stop)
  FILE *mTrace = nullptr;
  Role mRole = kDesktop;

  // shared with the main thread, under mLock
  std::mutex mLock;
  std::deque<Event> mEvents;
  std::deque<uint8_t> mOutgoing;
  bool mStopping = false;
  bool mDisconnectRequested = false;
  State mState = kWaiting;

  // the link's thread only
  MNPDecoder mDecoder;
  uint8_t mReceiveSeq = 0;        // the last LT received in order
  uint8_t mSendSeq = 0;           // the last LT sent
  uint8_t mCredit = 1;            // how many LTs the Newton takes (its last LA)
  std::vector<uint8_t> mInFlight; // the LT data waiting for its LA
  int mRetries = 0;
  double mRetransmitAt = 0;
  double mLastSent = 0;
  double mLRRetryAt = 0;          // Newton role: resend the LR then
  int mLRTries = 0;
};

/** newtc -ntk-mnp <target>: bring an MNP link up, print its events and the
    data that arrives (stdout), every frame (stderr), and send the lines of
    hex read from stdin. Ends at the end of stdin. */
int NTKMNPDemo(const std::string &inTarget);

#endif // MATT_MNP_H
