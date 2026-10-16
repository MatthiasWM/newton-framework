/*
 File: DockLink.h

 The Docking Protocol's packets on an MNP link (Matt/MNP.h): 'newt' 'dock',
 a command (4 characters), the length of the data, the data, padded with
 zeros to 4 bytes ("Newton Docking Protocol.txt"). For newtdump (the
 desktop) and its test Newton (the Newton's role).
 */

#ifndef NEWTDUMP_DOCKLINK_H
#define NEWTDUMP_DOCKLINK_H

#include "Matt/MNP.h"
#include "Matt/NewtDump/NSOF.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

/** A command as a 32-bit number, 'rtdk' -> 0x7274646B. */
constexpr uint32_t DockCommand(const char (&inName)[5])
{
  return (uint32_t)(uint8_t)inName[0] << 24 | (uint32_t)(uint8_t)inName[1] << 16
       | (uint32_t)(uint8_t)inName[2] << 8 | (uint32_t)(uint8_t)inName[3];
}
std::string DockCommandName(uint32_t inCommand);

struct DockPacket
{
  uint32_t command = 0;
  std::vector<uint8_t> data;
  /** The data as a 32-bit number (big-endian), or 0 if there are fewer bytes. */
  uint32_t longData(size_t inOffset = 0) const;
};

class DockLink
{
public:
  explicit DockLink(MNPLink::Role inRole = MNPLink::kDesktop);
  ~DockLink();

  /** Open the target (Matt/NTKTransport.h: tcp, tcp:<port>, serial:<device>
      [@<bps>], tcp-client:<port>) and start MNP; false and a message if
      the target can't be used. */
  bool start(const std::string &inTarget, std::string &outError);
  void stop();
  /** For tests: as slow as a serial line at this many bits/s (MNPLink). */
  void setThrottle(long inBitsPerSecond) { mLink.setThrottle(inBitsPerSecond); }

  /** Wait until the MNP link is up (the Newton connected); false after
      inSeconds (negative: forever). */
  bool waitLinkUp(double inSeconds);
  bool isUp() const { return mUp; }

  void send(uint32_t inCommand, const std::vector<uint8_t> &inData = {});
  void sendLong(uint32_t inCommand, uint32_t inValue);
  void sendLongs(uint32_t inCommand, const std::vector<uint32_t> &inValues);
  void sendRef(uint32_t inCommand, const nsof::Ref &inValue) { send(inCommand, nsof::Write(inValue)); }

  /** The next packet; false after inSeconds without one, or when the link
      is gone (gone() says so, why() why). 'helo' packets are skipped. */
  bool receive(DockPacket &outPacket, double inSeconds);
  bool gone() const { return mGone; }
  const std::string &why() const { return mWhy; }

  /** Bytes sent but not acknowledged by the other side yet. */
  size_t pending() { return mLink.pending(); }

  /** Called while receive() waits (e.g. to show progress); may be null. */
  void (*idle)(void *) = nullptr;
  void *idleContext = nullptr;

private:
  bool pump(double inSeconds);
  bool parse(DockPacket &outPacket);

  MNPLink mLink;
  std::vector<uint8_t> mBuffer;
  bool mUp = false;
  bool mGone = false;
  std::string mWhy;
};

/** 64-bit nonces and the password check (Communications/DES.c). */
struct DockNonce { uint32_t hi = 0, lo = 0; };
/** The nonce encrypted with the password (UTF-8; empty: none). */
DockNonce DockEncryptNonce(const DockNonce &inNonce, const std::string &inPassword);
DockNonce DockRandomNonce();

#endif // NEWTDUMP_DOCKLINK_H
