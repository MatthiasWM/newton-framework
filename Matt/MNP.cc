/*
 File: MNP.cc
 MNP for the link to a Newton. See MNP.h and Matt/Toolkit Protocol.md,
 section 5.
 */

#include "MNP.h"
#include "NTKTransport.h"

#include <cerrno>
#include <chrono>
#include <cstring>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

const uint8_t kSYN = 0x16, kDLE = 0x10, kSTX = 0x02, kETX = 0x03;

const size_t kMaxLTData = 256;      // N401 with "data phase optimisation"
const double kAckTimeout = 1.0;     // resend an LT after this (NTX's T401)
const int kMaxRetries = 10;
const double kKeepAlive = 3.0;      // an LA after this much quiet (NTX's T403)

// Our answer to the Newton's LR (unixnpi, NewtonInspector, NTX): window
// k = 1, N401 = 64, data phase optimisation (LTs up to 256 bytes, short LT
// and LA headers); no compression, no speed change.
const std::vector<uint8_t> kDesktopLR = {
  0x17, kMNPLinkRequest, 0x02,
  0x01, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF,
  0x02, 0x01, 0x02,
  0x03, 0x01, 0x01,
  0x04, 0x02, 0x40, 0x00,
  0x08, 0x01, 0x03
};

// The Newton's LR (Einstein, ROM 717006, captured 2026-10-05): k = 8,
// N401 = 64, data phase optimisation, MNP 5 compression offered, no speed
// change. Used when newtc plays the Newton.
const std::vector<uint8_t> kNewtonLR = {
  0x20, kMNPLinkRequest, 0x02,
  0x01, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF,
  0x02, 0x01, 0x02,
  0x03, 0x01, 0x08,
  0x04, 0x02, 0x40, 0x00,
  0x08, 0x01, 0x03,
  0x09, 0x01, 0x01,
  0x0E, 0x04, 0x03, 0x04, 0x00, 0xFA
};
const int kMaxLRTries = 4;          // Einstein gives up after four LRs

const char *FrameName(uint8_t inType)
{
  switch (inType) {
    case kMNPLinkRequest: return "LR";
    case kMNPLinkDisconnect: return "LD";
    case kMNPLinkTransfer: return "LT";
    case kMNPLinkAcknowledge: return "LA";
    case kMNPLinkAttention: return "LN";
    case kMNPLinkAttentionAck: return "LNA";
    default: return "??";
  }
}

} // namespace


/*--- Framing ---*/

uint16_t MNPCrc(const uint8_t *inData, size_t inSize, uint16_t inCrc)
{
  uint16_t crc = inCrc;
  for (size_t i = 0; i < inSize; i++) {
    crc ^= inData[i];
    for (int bit = 0; bit < 8; bit++)
      crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
  }
  return crc;
}


std::vector<uint8_t> MNPEncodeFrame(const std::vector<uint8_t> &inHeader, const uint8_t *inData, size_t inSize)
{
  std::vector<uint8_t> out = { kSYN, kDLE, kSTX };
  uint16_t crc = 0;
  auto add = [&](const uint8_t *bytes, size_t count) {
    for (size_t i = 0; i < count; i++) {
      out.push_back(bytes[i]);
      if (bytes[i] == kDLE)
        out.push_back(kDLE);
    }
    crc = MNPCrc(bytes, count, crc);
  };
  add(inHeader.data(), inHeader.size());
  if (inData)
    add(inData, inSize);
  out.push_back(kDLE);
  out.push_back(kETX);
  crc = MNPCrc(&kETX, 1, crc);
  out.push_back((uint8_t)(crc & 0xFF));
  out.push_back((uint8_t)(crc >> 8));
  return out;
}


MNPDecoder::Result MNPDecoder::feed(uint8_t inByte)
{
  switch (mState) {
    case 0:                                 // looking for SYN
      if (inByte == kSYN)
        mState = 1;
      return kMore;
    case 1:                                 // SYN, now DLE
      mState = (inByte == kDLE) ? 2 : (inByte == kSYN ? 1 : 0);
      return kMore;
    case 2:                                 // SYN DLE, now STX
      if (inByte == kSTX) {
        mFrame.clear();
        mState = 3;
      } else {
        mState = (inByte == kSYN) ? 1 : 0;
      }
      return kMore;
    case 3:                                 // header and data
      if (inByte == kDLE)
        mState = 4;
      else
        mFrame.push_back(inByte);
      return kMore;
    case 4:                                 // after a DLE
      if (inByte == kDLE) {                 // a doubled DLE is one data byte
        mFrame.push_back(kDLE);
        mState = 3;
        return kMore;
      }
      if (inByte == kETX) {
        mState = 5;
        return kMore;
      }
      mState = 0;                           // a bad escape: drop the frame
      return kBadFrame;
    case 5:                                 // CRC, low byte
      mReceivedCrc = inByte;
      mState = 6;
      return kMore;
    case 6: {                               // CRC, high byte
      mReceivedCrc |= (uint16_t)(inByte << 8);
      mState = 0;
      uint16_t crc = MNPCrc(mFrame.data(), mFrame.size());
      crc = MNPCrc(&kETX, 1, crc);
      return (crc == mReceivedCrc && mFrame.size() >= 2) ? kFrame : kBadFrame;
    }
  }
  mState = 0;
  return kMore;
}


/*--- The link ---*/

MNPLink::MNPLink() {}


MNPLink::~MNPLink()
{
  stop();
}


double MNPLink::now()
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}


std::string MNPLink::describe() const
{
  return mTransport ? mTransport->describe() : std::string("(no target)");
}


bool MNPLink::start(const std::string &inTarget, std::string &outError)
{
#if defined(_WIN32)
  outError = "-ntk is not supported on Windows yet";
  return false;
#else
  mTransport = NTKTransport::Create(inTarget, outError);
  if (!mTransport || !mTransport->start(outError)) {
    delete mTransport;
    mTransport = nullptr;
    return false;
  }
  if (pipe(mNotify) < 0 || pipe(mWake) < 0) {
    outError = std::string("can't create a pipe: ") + strerror(errno);
    return false;
  }
  for (int fd : { mNotify[0], mNotify[1], mWake[0], mWake[1] })
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  mStopping = false;
  mThread = std::thread([this] { run(); });
  return true;
#endif
}


void MNPLink::stop()
{
#if !defined(_WIN32)
  if (mThread.joinable()) {
    {
      std::lock_guard<std::mutex> lock(mLock);
      mStopping = true;
    }
    uint8_t b = 's';
    (void)!write(mWake[1], &b, 1);
    mThread.join();
  }
  delete mTransport;
  mTransport = nullptr;
  for (int *fd : { &mNotify[0], &mNotify[1], &mWake[0], &mWake[1] }) {
    if (*fd >= 0)
      close(*fd);
    *fd = -1;
  }
#endif
}


bool MNPLink::nextEvent(Event &outEvent)
{
#if !defined(_WIN32)
  uint8_t buffer[64];
  while (read(mNotify[0], buffer, sizeof(buffer)) > 0)
    ;                           // drain first, then take: no wake-up is lost
#endif
  std::lock_guard<std::mutex> lock(mLock);
  if (mEvents.empty())
    return false;
  outEvent = std::move(mEvents.front());
  mEvents.pop_front();
  return true;
}


void MNPLink::send(const uint8_t *inData, size_t inSize)
{
  {
    std::lock_guard<std::mutex> lock(mLock);
    mOutgoing.insert(mOutgoing.end(), inData, inData + inSize);
  }
#if !defined(_WIN32)
  uint8_t b = 'd';
  (void)!write(mWake[1], &b, 1);
#endif
}


size_t MNPLink::pending()
{
  std::lock_guard<std::mutex> lock(mLock);
  return mOutgoing.size() + mInFlight.size();
}


void MNPLink::disconnect()
{
  {
    std::lock_guard<std::mutex> lock(mLock);
    mDisconnectRequested = true;
  }
#if !defined(_WIN32)
  uint8_t b = 'x';
  (void)!write(mWake[1], &b, 1);
#endif
}


bool MNPLink::isUp()
{
  std::lock_guard<std::mutex> lock(mLock);
  return mState == kUp;
}


void MNPLink::post(Event::Kind inKind, const std::vector<uint8_t> &inData, const std::string &inText)
{
  {
    std::lock_guard<std::mutex> lock(mLock);
    mEvents.push_back(Event{ inKind, inData, inText });
  }
#if !defined(_WIN32)
  uint8_t b = 'e';
  (void)!write(mNotify[1], &b, 1);
#endif
}


void MNPLink::trace(const char *inDirection, const std::vector<uint8_t> &inFrame)
{
  if (!mTrace)
    return;
  uint8_t type = inFrame.size() > 1 ? inFrame[1] : 0;
  fprintf(mTrace, "mnp: %s %s", inDirection, FrameName(type));
  if (type == kMNPLinkTransfer && inFrame.size() >= 3)
    fprintf(mTrace, " seq %d, %zu bytes", inFrame[2], inFrame.size() - (size_t)inFrame[0] - 1);
  else if (type == kMNPLinkAcknowledge && inFrame.size() >= 4)
    fprintf(mTrace, " seq %d credit %d", inFrame[2], inFrame[3]);
  fprintf(mTrace, ":");
  for (uint8_t b : inFrame)
    fprintf(mTrace, " %02x", b);
  fprintf(mTrace, "\n");
  fflush(mTrace);
}


void MNPLink::sendFrame(const std::vector<uint8_t> &inHeader, const uint8_t *inData, size_t inSize)
{
  if (mTrace) {
    std::vector<uint8_t> whole(inHeader);
    if (inData)
      whole.insert(whole.end(), inData, inData + inSize);
    trace("->", whole);
  }
  std::vector<uint8_t> wire = MNPEncodeFrame(inHeader, inData, inSize);
  mTransport->send(wire.data(), wire.size());
  mLastSent = now();
}


void MNPLink::sendLA()
{
  sendFrame({ 0x03, kMNPLinkAcknowledge, mReceiveSeq, 1 });
}


void MNPLink::sendNextLT()
{
  if (!mInFlight.empty() || mCredit == 0)
    return;
  {
    std::lock_guard<std::mutex> lock(mLock);
    if (mState != kUp || mOutgoing.empty())
      return;
    size_t count = mOutgoing.size() < kMaxLTData ? mOutgoing.size() : kMaxLTData;
    mInFlight.assign(mOutgoing.begin(), mOutgoing.begin() + (long)count);
    mOutgoing.erase(mOutgoing.begin(), mOutgoing.begin() + (long)count);
  }
  mSendSeq = (uint8_t)(mSendSeq + 1);
  mRetries = 0;
  sendFrame({ 0x02, kMNPLinkTransfer, mSendSeq }, mInFlight.data(), mInFlight.size());
  mRetransmitAt = now() + kAckTimeout;
}


void MNPLink::linkDown(const std::string &inReason, bool inSendLD, bool inDropConnection)
{
  bool wasActive;
  {
    std::lock_guard<std::mutex> lock(mLock);
    wasActive = mState != kWaiting;
    mState = kWaiting;
    mOutgoing.clear();
  }
  if (inSendLD && mTransport->isConnected())
    sendFrame({ 0x04, kMNPLinkDisconnect, 0x01, 0x01, 0xFF });  // reason: user
  mInFlight.clear();
  mDecoder.reset();
  if (inDropConnection && mTransport->isConnected())
    mTransport->disconnect();
  if (wasActive || inDropConnection)
    post(Event::kLinkDown, {}, inReason);
}


void MNPLink::handleFrame(const std::vector<uint8_t> &inFrame)
{
  trace("<-", inFrame);
  size_t headerSize = (size_t)inFrame[0] + 1;
  switch (inFrame[1]) {
    case kMNPLinkRequest: {
      if (mRole == kNewton) {
        // The desktop's answer to our LR: confirm it, the link is up.
        bool negotiating;
        {
          std::lock_guard<std::mutex> lock(mLock);
          negotiating = mState == kNegotiating;
          mState = kUp;
        }
        mReceiveSeq = 0;
        mSendSeq = 0;
        mCredit = 1;
        mInFlight.clear();
        sendFrame({ 0x03, kMNPLinkAcknowledge, 0, 1 });
        if (negotiating)
          post(Event::kLinkUp);
        break;
      }
      // The Newton (re)starts the link: answer with our LR, wait for its LA.
      // (It resends its LR about once a second until it gets an answer.)
      {
        std::lock_guard<std::mutex> lock(mLock);
        mState = kNegotiating;
        mOutgoing.clear();
      }
      mReceiveSeq = 0;
      mSendSeq = 0;
      mCredit = 1;
      mInFlight.clear();
      sendFrame(kDesktopLR);
      break;
    }
    case kMNPLinkAcknowledge: {
      uint8_t seq = inFrame.size() > 2 ? inFrame[2] : 0;
      uint8_t credit = inFrame.size() > 3 ? inFrame[3] : 1;
      bool up;
      {
        std::lock_guard<std::mutex> lock(mLock);
        up = mState == kUp;
        if (mState == kNegotiating && mRole == kNewton)
          break;                            // waiting for the desktop's LR
        if (mState == kNegotiating)
          mState = kUp;
      }
      if (!up) {
        mCredit = credit;
        post(Event::kLinkUp);
        break;
      }
      mCredit = credit;
      if (!mInFlight.empty() && seq == mSendSeq)
        mInFlight.clear();                  // acknowledged
      else if (!mInFlight.empty())
        mRetransmitAt = 0;                  // an LA for the one before: resend now
      break;
    }
    case kMNPLinkTransfer: {
      if (inFrame.size() < 3)
        break;
      uint8_t seq = inFrame[2];
      if (seq == (uint8_t)(mReceiveSeq + 1)) {
        mReceiveSeq = seq;
        sendLA();
        if (inFrame.size() > headerSize)
          post(Event::kData, std::vector<uint8_t>(inFrame.begin() + (long)headerSize, inFrame.end()));
      } else {
        sendLA();       // resent (our LA got lost) or out of order: say what we have
      }
      break;
    }
    case kMNPLinkDisconnect:
      linkDown("the Newton disconnected", false, false);
      break;
    default:
      break;                                // LN, LNA: not used by the Newton here
  }
}


void MNPLink::connected()
{
  mDecoder.reset();
  post(Event::kConnected, {}, mTransport->peer());
  if (mRole == kNewton) {
    {
      std::lock_guard<std::mutex> lock(mLock);
      mState = kNegotiating;
    }
    mLRTries = 1;
    sendFrame(kNewtonLR);
    mLRRetryAt = now() + 1.0;
  }
}


void MNPLink::run()
{
#if !defined(_WIN32)
  std::vector<uint8_t> data;
  if (mTransport->isConnected())        // a TCP client is connected from the start
    connected();
  for (;;) {
    bool disconnectRequested;
    {
      std::lock_guard<std::mutex> lock(mLock);
      if (mStopping)
        break;
      disconnectRequested = mDisconnectRequested;
      mDisconnectRequested = false;
    }
    if (disconnectRequested)
      linkDown("disconnected by newtc", true, false);

    sendNextLT();

    // the next deadline: a retransmission or a keep-alive
    double t = now();
    double deadline = t + 1.0;
    bool up = isUp();
    if (up && !mInFlight.empty() && mRetransmitAt < deadline)
      deadline = mRetransmitAt;
    if (up && mLastSent + kKeepAlive < deadline)
      deadline = mLastSent + kKeepAlive;
    if (mRole == kNewton && !up && mLRRetryAt > t && mLRRetryAt < deadline)
      deadline = mLRRetryAt;
    int timeout = deadline > t ? (int)((deadline - t) * 1000) + 1 : 0;

    int fds[2] = { mWake[0], mTransport->fd() };
    bool ready[2];
    int n = NTKWaitReadable(fds, 2, timeout, ready);
    if (n < 0)
      break;
    if (ready[0]) {
      uint8_t buffer[64];
      while (read(mWake[0], buffer, sizeof(buffer)) > 0)
        ;
    }
    if (ready[1]) {
      switch (mTransport->handleReadable(data)) {
        case NTKTransport::kConnected:
          connected();
          break;
        case NTKTransport::kDisconnected:
          linkDown("the connection is gone", false, true);
          break;
        default:
          break;
      }
      for (uint8_t b : data) {
        MNPDecoder::Result r = mDecoder.feed(b);
        if (r == MNPDecoder::kFrame)
          handleFrame(mDecoder.frame());
        else if (r == MNPDecoder::kBadFrame && isUp())
          sendLA();                         // the Newton resends what we lack
      }
    }

    // timers
    t = now();
    if (mRole == kNewton && t >= mLRRetryAt) {
      bool negotiating;
      {
        std::lock_guard<std::mutex> lock(mLock);
        negotiating = mState == kNegotiating;
      }
      if (negotiating) {
        if (++mLRTries > kMaxLRTries) {
          linkDown("no answer to the LR", false, true);
        } else {
          sendFrame(kNewtonLR);
          mLRRetryAt = t + 1.0;
        }
      }
    }
    if (isUp() && !mInFlight.empty() && t >= mRetransmitAt) {
      if (++mRetries > kMaxRetries) {
        linkDown("no acknowledgement from the Newton", true, false);
      } else {
        sendFrame({ 0x02, kMNPLinkTransfer, mSendSeq }, mInFlight.data(), mInFlight.size());
        mRetransmitAt = t + kAckTimeout;
      }
    }
    if (isUp() && t >= mLastSent + kKeepAlive)
      sendLA();
  }
  if (isUp())
    linkDown("newtc stopped", true, false);
#endif
}


/*--- newtc -ntk-mnp ---*/

int NTKMNPDemo(const std::string &inTarget)
{
#if defined(_WIN32)
  fprintf(stderr, "newtc: -ntk-mnp is not supported on Windows yet\n");
  return 1;
#else
  MNPLink link;
  link.setTrace(stderr);
  std::string error;
  if (!link.start(inTarget, error)) {
    fprintf(stderr, "newtc: -ntk-mnp: %s\n", error.c_str());
    return 1;
  }
  printf("mnp: waiting on %s\n", link.describe().c_str());
  fflush(stdout);
  std::string line;
  bool inputOpen = true;
  while (inputOpen) {
    int fds[2] = { link.notifyFd(), 0 };
    bool ready[2];
    if (NTKWaitReadable(fds, 2, -1, ready) < 0)
      break;
    MNPLink::Event event;
    while (link.nextEvent(event)) {
      switch (event.kind) {
        case MNPLink::Event::kConnected:
          printf("mnp: connected (%s)\n", event.text.c_str());
          break;
        case MNPLink::Event::kLinkUp:
          printf("mnp: link up\n");
          break;
        case MNPLink::Event::kData:
          printf("mnp: <- %zu bytes:", event.data.size());
          for (uint8_t b : event.data)
            printf(" %02x", b);
          printf("\n");
          break;
        case MNPLink::Event::kLinkDown:
          printf("mnp: link down (%s)\n", event.text.c_str());
          break;
      }
    }
    if (ready[1]) {
      char buffer[1024];
      ssize_t n = read(0, buffer, sizeof(buffer));
      if (n <= 0) {
        inputOpen = false;
      } else {
        line.append(buffer, (size_t)n);
        size_t end;
        while ((end = line.find('\n')) != std::string::npos) {
          std::vector<uint8_t> bytes;
          const char *s = line.c_str();
          char *next = nullptr;
          for (;;) {
            unsigned long value = strtoul(s, &next, 16);
            if (next == s || next > line.c_str() + end)
              break;
            bytes.push_back((uint8_t)value);
            s = next;
          }
          line.erase(0, end + 1);
          if (!bytes.empty()) {
            printf("mnp: -> %zu bytes queued\n", bytes.size());
            link.send(bytes);
          }
        }
      }
    }
    fflush(stdout);
  }
  // let what is queued go out before ending (a few seconds at most)
  for (int i = 0; i < 50 && link.pending() > 0 && link.isUp(); i++)
    usleep(100000);
  link.stop();
  printf("mnp: stopped\n");
  return 0;
#endif
}
