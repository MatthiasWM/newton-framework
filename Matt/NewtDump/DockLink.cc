/*
 File: DockLink.cc

 The Docking Protocol's packets on an MNP link (see DockLink.h).
 */

#include "DockLink.h"

#include "Communications/DES.h"

#include <chrono>
#include <cstring>
#include <random>
#include <sys/select.h>

namespace {

double Now()
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

const uint32_t kNewt = DockCommand("newt"), kDock = DockCommand("dock");

void PutLong(std::vector<uint8_t> &out, uint32_t inValue)
{
  for (int shift = 24; shift >= 0; shift -= 8)
    out.push_back((uint8_t)(inValue >> shift));
}

uint32_t GetLong(const std::vector<uint8_t> &inData, size_t inOffset)
{
  return (uint32_t)inData[inOffset] << 24 | (uint32_t)inData[inOffset + 1] << 16
       | (uint32_t)inData[inOffset + 2] << 8 | (uint32_t)inData[inOffset + 3];
}

} // namespace


std::string DockCommandName(uint32_t inCommand)
{
  std::string s;
  for (int shift = 24; shift >= 0; shift -= 8) {
    char c = (char)(inCommand >> shift);
    s += (c >= 32 && c < 127) ? c : '?';
  }
  return s;
}


uint32_t DockPacket::longData(size_t inOffset) const
{
  return data.size() >= inOffset + 4 ? GetLong(data, inOffset) : 0;
}


DockLink::DockLink(MNPLink::Role inRole)
{
  mLink.setRole(inRole);
}


DockLink::~DockLink()
{
  stop();
}


bool DockLink::start(const std::string &inTarget, std::string &outError)
{
  if (getenv("NEWTDUMP_TRACE"))
    mLink.setTrace(stderr);
  return mLink.start(inTarget, outError);
}


void DockLink::stop()
{
  mLink.stop();
  mUp = false;
}


bool DockLink::waitLinkUp(double inSeconds)
{
  double until = inSeconds < 0 ? 1e300 : Now() + inSeconds;
  while (!mUp) {
    double left = until - Now();
    if (left <= 0)
      return false;
    pump(left < 0.25 ? left : 0.25);
    if (mGone && !mUp)
      mGone = false;   // waiting for a Newton: a link that went down doesn't count
  }
  return true;
}


void DockLink::send(uint32_t inCommand, const std::vector<uint8_t> &inData)
{
  std::vector<uint8_t> packet;
  PutLong(packet, kNewt);
  PutLong(packet, kDock);
  PutLong(packet, inCommand);
  PutLong(packet, (uint32_t)inData.size());
  packet.insert(packet.end(), inData.begin(), inData.end());
  while (packet.size() % 4)
    packet.push_back(0);
  mLink.send(packet);
}


void DockLink::sendLong(uint32_t inCommand, uint32_t inValue)
{
  sendLongs(inCommand, { inValue });
}


void DockLink::sendLongs(uint32_t inCommand, const std::vector<uint32_t> &inValues)
{
  std::vector<uint8_t> data;
  for (uint32_t v : inValues)
    PutLong(data, v);
  send(inCommand, data);
}


bool DockLink::receive(DockPacket &outPacket, double inSeconds)
{
  double until = Now() + inSeconds;
  for (;;) {
    while (parse(outPacket)) {
      if (outPacket.command != DockCommand("helo"))
        return true;
    }
    if (mGone)
      return false;
    double left = until - Now();
    if (left <= 0)
      return false;
    pump(left < 0.25 ? left : 0.25);
    if (idle)
      idle(idleContext);
  }
}


// Wait up to inSeconds for MNP events and take them: data into the buffer
bool DockLink::pump(double inSeconds)
{
  int fd = mLink.notifyFd();
  fd_set readable;
  FD_ZERO(&readable);
  FD_SET(fd, &readable);
  struct timeval tv;
  tv.tv_sec = (long)inSeconds;
  tv.tv_usec = (long)((inSeconds - (double)tv.tv_sec) * 1e6);
  if (select(fd + 1, &readable, nullptr, nullptr, &tv) <= 0)
    return false;
  MNPLink::Event event;
  while (mLink.nextEvent(event)) {
    switch (event.kind) {
    case MNPLink::Event::kConnected:
      break;
    case MNPLink::Event::kLinkUp:
      mUp = true;
      mGone = false;
      mBuffer.clear();
      break;
    case MNPLink::Event::kData:
      mBuffer.insert(mBuffer.end(), event.data.begin(), event.data.end());
      break;
    case MNPLink::Event::kLinkDown:
      mUp = false;
      mGone = true;
      mWhy = event.text.empty() ? "the connection is gone" : event.text;
      break;
    }
  }
  return true;
}


// A whole packet from the buffer, if there is one. Bytes before 'newt'
// 'dock' are skipped (they shouldn't be there).
bool DockLink::parse(DockPacket &outPacket)
{
  while (mBuffer.size() >= 8 && !(GetLong(mBuffer, 0) == kNewt && GetLong(mBuffer, 4) == kDock))
    mBuffer.erase(mBuffer.begin());
  if (mBuffer.size() < 16)
    return false;
  uint32_t length = GetLong(mBuffer, 12);
  size_t padded = (length + 3) & ~(size_t)3;
  if (mBuffer.size() < 16 + padded)
    return false;
  outPacket.command = GetLong(mBuffer, 8);
  outPacket.data.assign(mBuffer.begin() + 16, mBuffer.begin() + 16 + length);
  mBuffer.erase(mBuffer.begin(), mBuffer.begin() + 16 + padded);
  return true;
}


DockNonce DockEncryptNonce(const DockNonce &inNonce, const std::string &inPassword)
{
  // the password as UniChars (code points of its UTF-8), NUL-terminated
  std::vector<UniChar> chars;
  for (size_t i = 0; i < inPassword.size(); ) {
    unsigned c = (unsigned char)inPassword[i++];
    if (c >= 0xC0 && i < inPassword.size()) {
      int extra = c >= 0xE0 ? 2 : 1;
      c &= extra == 2 ? 0x0F : 0x1F;
      while (extra-- && i < inPassword.size())
        c = (c << 6) | ((unsigned char)inPassword[i++] & 0x3F);
    }
    chars.push_back((UniChar)c);
  }
  chars.push_back(0);
  SNewtNonce key;
  DESCharToKey(chars.data(), &key);
  SNewtNonce nonce = { inNonce.hi, inNonce.lo };
  DESEncodeNonce(&key, &nonce);
  return DockNonce{ nonce.hi, nonce.lo };
}


DockNonce DockRandomNonce()
{
  std::random_device random;
  return DockNonce{ (uint32_t)random(), (uint32_t)random() };
}
