/*
 File: NTKTransport.cc
 The wire to a Newton: a TCP server (Einstein) or a serial port. See
 NTKTransport.h.
 */

#include "NTKTransport.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {

const int kEinsteinPort = 3679;     // Einstein's default for its TCP client
const int kNTKBitRate = 38400;      // what Toolkit.pkg's connection uses

#if !defined(_WIN32)

bool SetNonBlocking(int inFd)
{
  int flags = fcntl(inFd, F_GETFL, 0);
  return flags >= 0 && fcntl(inFd, F_SETFL, flags | O_NONBLOCK) == 0;
}


// Write all bytes to a non-blocking fd, waiting while it is full.
bool WriteAll(int inFd, const uint8_t *inData, size_t inSize)
{
  while (inSize > 0) {
    ssize_t n = write(inFd, inData, inSize);
    if (n > 0) {
      inData += n;
      inSize -= (size_t)n;
    } else if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
      fd_set writable;
      FD_ZERO(&writable);
      FD_SET(inFd, &writable);
      struct timeval timeout = { 5, 0 };
      if (select(inFd + 1, nullptr, &writable, nullptr, &timeout) <= 0)
        return false;       // nothing could be written for 5 s
    } else {
      return false;
    }
  }
  return true;
}


// Read what is there: kData, kNothing (would block), or kDisconnected.
NTKTransport::Event ReadSome(int inFd, std::vector<uint8_t> &outData)
{
  uint8_t buffer[4096];
  ssize_t n = read(inFd, buffer, sizeof(buffer));
  if (n > 0) {
    outData.assign(buffer, buffer + n);
    return NTKTransport::kData;
  }
  if (n < 0 && (errno == EAGAIN || errno == EINTR))
    return NTKTransport::kNothing;
  return NTKTransport::kDisconnected;     // 0: closed by the peer, or an error
}


/*--- TCP: Einstein's serial port connects to us ---*/

class TCPTransport : public NTKTransport
{
public:
  TCPTransport(int inPort) : mPort(inPort) {}

  ~TCPTransport() override
  {
    disconnect();
    if (mListener >= 0)
      close(mListener);
  }

  bool start(std::string &outError) override
  {
    mListener = socket(AF_INET, SOCK_STREAM, 0);
    if (mListener < 0) {
      outError = std::string("can't create a socket: ") + strerror(errno);
      return false;
    }
    int yes = 1;
    setsockopt(mListener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // this machine only
    address.sin_port = htons((uint16_t)mPort);
    if (bind(mListener, (struct sockaddr *)&address, sizeof(address)) < 0
     || listen(mListener, 1) < 0
     || !SetNonBlocking(mListener)) {
      outError = "can't listen on " + describe() + ": " + strerror(errno);
      close(mListener);
      mListener = -1;
      return false;
    }
    signal(SIGPIPE, SIG_IGN);     // a peer that goes away is a disconnect
    return true;
  }

  int fd() const override { return mConnection >= 0 ? mConnection : mListener; }

  Event handleReadable(std::vector<uint8_t> &outData) override
  {
    outData.clear();
    if (mConnection < 0) {
      struct sockaddr_in address = {};
      socklen_t size = sizeof(address);
      int connection = accept(mListener, (struct sockaddr *)&address, &size);
      if (connection < 0)
        return kNothing;
      int yes = 1;
      setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
      SetNonBlocking(connection);
      char text[INET_ADDRSTRLEN] = "?";
      inet_ntop(AF_INET, &address.sin_addr, text, sizeof(text));
      mPeer = std::string(text) + ":" + std::to_string(ntohs(address.sin_port));
      mConnection = connection;
      return kConnected;
    }
    Event event = ReadSome(mConnection, outData);
    if (event == kDisconnected)
      disconnect();
    return event;
  }

  bool isConnected() const override { return mConnection >= 0; }

  bool send(const uint8_t *inData, size_t inSize) override
  {
    if (mConnection < 0)
      return false;
    if (WriteAll(mConnection, inData, inSize))
      return true;
    disconnect();
    return false;
  }

  void disconnect() override
  {
    if (mConnection >= 0) {
      close(mConnection);
      mConnection = -1;
      mPeer.clear();
    }
  }

  std::string describe() const override { return "127.0.0.1:" + std::to_string(mPort); }

  std::string peer() const override { return mPeer.empty() ? describe() : mPeer; }

private:
  int mPort;
  int mListener = -1;
  int mConnection = -1;
  std::string mPeer;
};


/*--- TCP client: newtc playing the Newton connects to a desktop ---*/

class TCPClientTransport : public NTKTransport
{
public:
  TCPClientTransport(int inPort) : mPort(inPort) {}
  ~TCPClientTransport() override { disconnect(); }

  bool start(std::string &outError) override
  {
    mSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (mSocket < 0) {
      outError = std::string("can't create a socket: ") + strerror(errno);
      return false;
    }
    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)mPort);
    if (connect(mSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
      outError = "can't connect to " + describe() + ": " + strerror(errno);
      disconnect();
      return false;
    }
    int yes = 1;
    setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    SetNonBlocking(mSocket);
    signal(SIGPIPE, SIG_IGN);
    return true;
  }

  int fd() const override { return mSocket; }

  Event handleReadable(std::vector<uint8_t> &outData) override
  {
    outData.clear();
    if (mSocket < 0)
      return kNothing;
    Event event = ReadSome(mSocket, outData);
    if (event == kDisconnected)
      disconnect();
    return event;
  }

  bool isConnected() const override { return mSocket >= 0; }

  bool send(const uint8_t *inData, size_t inSize) override
  {
    if (mSocket >= 0 && WriteAll(mSocket, inData, inSize))
      return true;
    disconnect();
    return false;
  }

  void disconnect() override
  {
    if (mSocket >= 0) {
      close(mSocket);
      mSocket = -1;
    }
  }

  std::string describe() const override { return "127.0.0.1:" + std::to_string(mPort); }

private:
  int mPort;
  int mSocket = -1;
};


/*--- Serial: a real Newton ---*/

bool SpeedFor(long inBitRate, speed_t &outSpeed)
{
  switch (inBitRate) {
    case 9600: outSpeed = B9600; return true;
    case 19200: outSpeed = B19200; return true;
    case 38400: outSpeed = B38400; return true;
    case 57600: outSpeed = B57600; return true;
    case 115200: outSpeed = B115200; return true;
    case 230400: outSpeed = B230400; return true;
    default: return false;
  }
}


class SerialTransport : public NTKTransport
{
public:
  SerialTransport(const std::string &inDevice, long inBitRate)
    : mDevice(inDevice), mBitRate(inBitRate) {}

  ~SerialTransport() override { disconnect(); }

  bool start(std::string &outError) override
  {
    speed_t speed;
    if (!SpeedFor(mBitRate, speed)) {
      outError = "unsupported speed " + std::to_string(mBitRate)
               + " (9600, 19200, 38400, 57600, 115200, 230400)";
      return false;
    }
    // O_NONBLOCK: don't wait for carrier detect, and read without waiting
    mPort = open(mDevice.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (mPort < 0) {
      outError = "can't open " + mDevice + ": " + strerror(errno);
      return false;
    }
    ioctl(mPort, TIOCEXCL);       // nobody else may open it while we use it
    struct termios options;
    if (tcgetattr(mPort, &options) < 0) {
      outError = mDevice + " is not a serial port: " + strerror(errno);
      disconnect();
      return false;
    }
    mOriginal = options;
    mHaveOriginal = true;
    cfmakeraw(&options);
    options.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
    options.c_cflag |= CS8 | CREAD | CLOCAL;       // 8N1, ignore modem lines
#if defined(CRTSCTS)
    options.c_cflag &= ~CRTSCTS;                   // MNP needs no flow control
#endif
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    if (tcsetattr(mPort, TCSANOW, &options) < 0) {
      outError = "can't set up " + describe() + ": " + strerror(errno);
      disconnect();
      return false;
    }
    tcflush(mPort, TCIOFLUSH);
    mAnnounced = false;
    return true;
  }

  int fd() const override { return mPort; }

  Event handleReadable(std::vector<uint8_t> &outData) override
  {
    outData.clear();
    if (mPort < 0)
      return kNothing;
    Event event = ReadSome(mPort, outData);
    if (event == kDisconnected) {       // the adapter was unplugged
      disconnect();
      return kDisconnected;
    }
    if (!mAnnounced && event == kData) {  // the first bytes also say "connected"
      mAnnounced = true;
      return kConnected;
    }
    return event;
  }

  bool isConnected() const override { return mPort >= 0; }

  bool send(const uint8_t *inData, size_t inSize) override
  {
    return mPort >= 0 && WriteAll(mPort, inData, inSize);
  }

  void disconnect() override
  {
    if (mPort >= 0) {
      if (mHaveOriginal)
        tcsetattr(mPort, TCSANOW, &mOriginal);
      close(mPort);
      mPort = -1;
    }
  }

  std::string describe() const override
  {
    return mDevice + " at " + std::to_string(mBitRate) + " bps";
  }

private:
  std::string mDevice;
  long mBitRate;
  int mPort = -1;
  bool mAnnounced = false;
  bool mHaveOriginal = false;
  struct termios mOriginal = {};
};

#endif // !_WIN32

} // namespace


int NTKWaitReadable(const int *inFds, int inCount, int inTimeoutMs, bool *outReady)
{
#if defined(_WIN32)
  return -1;
#else
  fd_set readable;
  FD_ZERO(&readable);
  int maxFd = -1;
  for (int i = 0; i < inCount; i++) {
    outReady[i] = false;
    if (inFds[i] >= 0) {
      FD_SET(inFds[i], &readable);
      if (inFds[i] > maxFd)
        maxFd = inFds[i];
    }
  }
  struct timeval timeout = { inTimeoutMs / 1000, (inTimeoutMs % 1000) * 1000 };
  int n = select(maxFd + 1, &readable, nullptr, nullptr, inTimeoutMs < 0 ? nullptr : &timeout);
  if (n < 0)
    return errno == EINTR ? 0 : -1;
  for (int i = 0; i < inCount; i++)
    outReady[i] = inFds[i] >= 0 && FD_ISSET(inFds[i], &readable);
  return n;
#endif
}


NTKTransport *NTKTransport::Create(const std::string &inTarget, std::string &outError)
{
#if defined(_WIN32)
  outError = "-ntk is not supported on Windows yet";
  return nullptr;
#else
  if (inTarget == "tcp")
    return new TCPTransport(kEinsteinPort);
  if (inTarget.compare(0, 4, "tcp:") == 0) {
    char *end = nullptr;
    long port = strtol(inTarget.c_str() + 4, &end, 10);
    if (*end == 0 && port > 0 && port < 65536)
      return new TCPTransport((int)port);
    outError = "bad port in \"" + inTarget + "\"";
    return nullptr;
  }
  if (inTarget.compare(0, 11, "tcp-client:") == 0) {
    char *end = nullptr;
    long port = strtol(inTarget.c_str() + 11, &end, 10);
    if (*end == 0 && port > 0 && port < 65536)
      return new TCPClientTransport((int)port);
    outError = "bad port in \"" + inTarget + "\"";
    return nullptr;
  }
  if (inTarget.compare(0, 7, "serial:") == 0) {
    std::string device = inTarget.substr(7);
    long bitRate = kNTKBitRate;
    size_t at = device.rfind('@');
    if (at != std::string::npos) {
      char *end = nullptr;
      bitRate = strtol(device.c_str() + at + 1, &end, 10);
      if (*end != 0 || bitRate <= 0) {
        outError = "bad speed in \"" + inTarget + "\"";
        return nullptr;
      }
      device.resize(at);
    }
    if (device.empty()) {
      outError = "no device in \"" + inTarget + "\"";
      return nullptr;
    }
    return new SerialTransport(device, bitRate);
  }
  outError = "unknown target \"" + inTarget + "\" (tcp, tcp:<port>, serial:<device>[@<bps>], tcp-client:<port>)";
  return nullptr;
#endif
}


int NTKDump(const std::string &inTarget)
{
#if defined(_WIN32)
  fprintf(stderr, "newtc: -ntk-dump is not supported on Windows yet\n");
  return 1;
#else
  std::string error;
  NTKTransport *transport = NTKTransport::Create(inTarget, error);
  if (!transport || !transport->start(error)) {
    fprintf(stderr, "newtc: -ntk-dump: %s\n", error.c_str());
    delete transport;
    return 1;
  }
  bool serial = inTarget.compare(0, 7, "serial:") == 0;
  printf("ntk: %s %s\n", serial ? "opened" : "listening on", transport->describe().c_str());
  fflush(stdout);
  std::vector<uint8_t> data;
  for (;;) {
    int fd = transport->fd();
    bool ready;
    if (NTKWaitReadable(&fd, 1, -1, &ready) < 0)
      break;
    if (!ready)
      continue;
    switch (transport->handleReadable(data)) {
      case NTKTransport::kConnected:
        printf("ntk: connected (%s)\n", transport->peer().c_str());
        break;
      case NTKTransport::kDisconnected:
        printf("ntk: disconnected%s\n", serial ? "" : ", listening again");
        if (serial) {
          fflush(stdout);
          delete transport;
          return 0;
        }
        break;
      default:
        break;
    }
    if (!data.empty()) {
      printf("ntk: <- %zu bytes:", data.size());
      for (uint8_t b : data)
        printf(" %02x", b);
      printf("\n");
    }
    fflush(stdout);
  }
  delete transport;
  return 0;
#endif
}
