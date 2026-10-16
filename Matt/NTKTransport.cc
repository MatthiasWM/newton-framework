/*
 File: NTKTransport.cc
 The wire to a Newton: a TCP server (Einstein, a Newton over the network), a
 TCP client, or a serial port; POSIX and Windows. See NTKTransport.h.
 */

#include "NTKTransport.h"

#include <cerrno>
#include <climits>
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
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <atomic>
#include <string>
#include <thread>
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
// A pseudo terminal's slave (macOS /dev/ttys..., Linux /dev/pts/...),
// e.g. what /tmp/pty.BasiliskII points to
bool IsPseudoTerminal(const std::string &inDevice)
{
  char resolved[PATH_MAX];
  if (!realpath(inDevice.c_str(), resolved))
    return false;
  return strncmp(resolved, "/dev/ttys", 9) == 0 || strncmp(resolved, "/dev/pts/", 9) == 0;
}


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


/*--- TCP: Einstein's serial port connects to us (this machine only), or
       a Newton docking over the network (all interfaces) ---*/

class TCPTransport : public NTKTransport
{
public:
  TCPTransport(int inPort, bool inAllInterfaces = false) : mPort(inPort), mAll(inAllInterfaces) {}

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
    address.sin_addr.s_addr = htonl(mAll ? INADDR_ANY : INADDR_LOOPBACK);
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

  std::string describe() const override { return (mAll ? "*:" : "127.0.0.1:") + std::to_string(mPort); }

  std::string peer() const override { return mPeer.empty() ? describe() : mPeer; }

private:
  int mPort;
  bool mAll;
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
    // throw away what an earlier connection left, but not on a pseudo
    // terminal: Einstein's BasiliskII serial port (/tmp/pty.BasiliskII)
    // takes a second flush for a hung client and makes a new PTY, and our
    // fd would be dead (the next start flushes again)
    if (!IsPseudoTerminal(mDevice))
      tcflush(mPort, TCIOFLUSH);
    mAnnounced = false;
    return true;
  }

  bool reopenable() const override { return true; }

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

#else // _WIN32

/*--- Windows: Winsock, and serial ports through a reader thread ---*/

void SocketStartup()
{
  static bool started = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  (void)started;
}

int ToFd(SOCKET inSocket) { return inSocket == INVALID_SOCKET ? -1 : (int)inSocket; }

std::string SocketError(const char *inWhat)
{
  return std::string(inWhat) + " (Winsock error " + std::to_string(WSAGetLastError()) + ")";
}

bool SetNonBlocking(SOCKET inSocket)
{
  u_long on = 1;
  return ioctlsocket(inSocket, FIONBIO, &on) == 0;
}

bool WouldBlock()
{
  int error = WSAGetLastError();
  return error == WSAEWOULDBLOCK || error == WSAEINTR;
}

void CloseSocket(SOCKET &ioSocket)
{
  if (ioSocket != INVALID_SOCKET)
    closesocket(ioSocket);
  ioSocket = INVALID_SOCKET;
}


// Send all bytes on a non-blocking socket, waiting while it is full.
bool SendAll(SOCKET inSocket, const uint8_t *inData, size_t inSize)
{
  while (inSize > 0) {
    int n = ::send(inSocket, (const char *)inData, (int)(inSize > 0x10000 ? 0x10000 : inSize), 0);
    if (n > 0) {
      inData += n;
      inSize -= (size_t)n;
    } else if (n < 0 && WouldBlock()) {
      fd_set writable;
      FD_ZERO(&writable);
      FD_SET(inSocket, &writable);
      struct timeval timeout = { 5, 0 };
      if (select(0, nullptr, &writable, nullptr, &timeout) <= 0)
        return false;       // nothing could be written for 5 s
    } else {
      return false;
    }
  }
  return true;
}


NTKTransport::Event ReceiveSome(SOCKET inSocket, std::vector<uint8_t> &outData)
{
  uint8_t buffer[4096];
  int n = recv(inSocket, (char *)buffer, (int)sizeof(buffer), 0);
  if (n > 0) {
    outData.assign(buffer, buffer + n);
    return NTKTransport::kData;
  }
  if (n < 0 && WouldBlock())
    return NTKTransport::kNothing;
  return NTKTransport::kDisconnected;     // 0: closed by the peer, or an error
}


// Two connected loopback sockets (Windows has no socketpair()): [0], [1]
bool SocketPair(SOCKET outSockets[2])
{
  SocketStartup();
  outSockets[0] = outSockets[1] = INVALID_SOCKET;
  SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET)
    return false;
  struct sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  int size = sizeof(address);
  bool ok = bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0 && listen(listener, 1) == 0
         && getsockname(listener, (struct sockaddr *)&address, &size) == 0;
  if (ok) {
    outSockets[1] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ok = outSockets[1] != INVALID_SOCKET && connect(outSockets[1], (struct sockaddr *)&address, sizeof(address)) == 0;
  }
  if (ok) {
    outSockets[0] = accept(listener, nullptr, nullptr);
    ok = outSockets[0] != INVALID_SOCKET;
  }
  closesocket(listener);
  if (!ok) {
    CloseSocket(outSockets[0]);
    CloseSocket(outSockets[1]);
    return false;
  }
  int yes = 1;
  setsockopt(outSockets[1], IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(yes));
  SetNonBlocking(outSockets[0]);
  SetNonBlocking(outSockets[1]);
  return true;
}


/*--- TCP server (Winsock) ---*/

class TCPTransport : public NTKTransport
{
public:
  TCPTransport(int inPort, bool inAllInterfaces = false) : mPort(inPort), mAll(inAllInterfaces) {}

  ~TCPTransport() override
  {
    disconnect();
    CloseSocket(mListener);
  }

  bool start(std::string &outError) override
  {
    SocketStartup();
    mListener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mListener == INVALID_SOCKET) {
      outError = SocketError("can't create a socket");
      return false;
    }
    // Windows' SO_REUSEADDR lets another program take the port: exclusive use instead
    int yes = 1;
    setsockopt(mListener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&yes, sizeof(yes));
    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(mAll ? INADDR_ANY : INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)mPort);
    if (bind(mListener, (struct sockaddr *)&address, sizeof(address)) != 0 || listen(mListener, 1) != 0
     || !SetNonBlocking(mListener)) {
      outError = SocketError(("can't listen on " + describe()).c_str());
      CloseSocket(mListener);
      return false;
    }
    return true;
  }

  int fd() const override { return ToFd(mConnection != INVALID_SOCKET ? mConnection : mListener); }

  Event handleReadable(std::vector<uint8_t> &outData) override
  {
    outData.clear();
    if (mConnection == INVALID_SOCKET) {
      struct sockaddr_in address = {};
      int size = sizeof(address);
      SOCKET connection = accept(mListener, (struct sockaddr *)&address, &size);
      if (connection == INVALID_SOCKET)
        return kNothing;
      int yes = 1;
      setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(yes));
      SetNonBlocking(connection);
      char text[INET_ADDRSTRLEN] = "?";
      inet_ntop(AF_INET, &address.sin_addr, text, sizeof(text));
      mPeer = std::string(text) + ":" + std::to_string(ntohs(address.sin_port));
      mConnection = connection;
      return kConnected;
    }
    Event event = ReceiveSome(mConnection, outData);
    if (event == kDisconnected)
      disconnect();
    return event;
  }

  bool isConnected() const override { return mConnection != INVALID_SOCKET; }

  bool send(const uint8_t *inData, size_t inSize) override
  {
    if (mConnection == INVALID_SOCKET)
      return false;
    if (SendAll(mConnection, inData, inSize))
      return true;
    disconnect();
    return false;
  }

  void disconnect() override
  {
    CloseSocket(mConnection);
    mPeer.clear();
  }

  std::string describe() const override { return (mAll ? "*:" : "127.0.0.1:") + std::to_string(mPort); }

  std::string peer() const override { return mPeer.empty() ? describe() : mPeer; }

private:
  int mPort;
  bool mAll;
  SOCKET mListener = INVALID_SOCKET;
  SOCKET mConnection = INVALID_SOCKET;
  std::string mPeer;
};


/*--- TCP client (Winsock) ---*/

class TCPClientTransport : public NTKTransport
{
public:
  TCPClientTransport(int inPort) : mPort(inPort) {}
  ~TCPClientTransport() override { disconnect(); }

  bool start(std::string &outError) override
  {
    SocketStartup();
    mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mSocket == INVALID_SOCKET) {
      outError = SocketError("can't create a socket");
      return false;
    }
    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)mPort);
    if (connect(mSocket, (struct sockaddr *)&address, sizeof(address)) != 0) {
      outError = SocketError(("can't connect to " + describe()).c_str());
      disconnect();
      return false;
    }
    int yes = 1;
    setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(yes));
    SetNonBlocking(mSocket);
    return true;
  }

  int fd() const override { return ToFd(mSocket); }

  Event handleReadable(std::vector<uint8_t> &outData) override
  {
    outData.clear();
    if (mSocket == INVALID_SOCKET)
      return kNothing;
    Event event = ReceiveSome(mSocket, outData);
    if (event == kDisconnected)
      disconnect();
    return event;
  }

  bool isConnected() const override { return mSocket != INVALID_SOCKET; }

  bool send(const uint8_t *inData, size_t inSize) override
  {
    if (mSocket != INVALID_SOCKET && SendAll(mSocket, inData, inSize))
      return true;
    disconnect();
    return false;
  }

  void disconnect() override { CloseSocket(mSocket); }

  std::string describe() const override { return "127.0.0.1:" + std::to_string(mPort); }

private:
  int mPort;
  SOCKET mSocket = INVALID_SOCKET;
};


/*--- Serial (Win32): a reader thread passes the bytes on through a socket
       pair, which NTKWaitReadable can wait on; the pair closes when the
       port goes away (an adapter unplugged) ---*/

class SerialTransport : public NTKTransport
{
public:
  SerialTransport(const std::string &inDevice, long inBitRate)
    : mDevice(inDevice), mBitRate(inBitRate) {}

  ~SerialTransport() override { disconnect(); }

  bool start(std::string &outError) override
  {
    disconnect();
    // COM10 and up only open as \\.\COM10; the names are ASCII
    std::string path = mDevice.compare(0, 4, "\\\\.\\") == 0 ? mDevice : "\\\\.\\" + mDevice;
    std::wstring widePath(path.begin(), path.end());
    mPort = CreateFileW(widePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED, nullptr);
    if (mPort == INVALID_HANDLE_VALUE) {
      outError = "can't open " + mDevice + " (error " + std::to_string(GetLastError()) + ")";
      return false;
    }
    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(mPort, &dcb)) {
      outError = mDevice + " is not a serial port";
      disconnect();
      return false;
    }
    dcb.BaudRate = (DWORD)mBitRate;
    dcb.ByteSize = 8;                      // 8N1, no flow control: MNP needs none
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;
    if (!SetCommState(mPort, &dcb)) {
      outError = "can't set up " + describe() + " (error " + std::to_string(GetLastError()) + ")";
      disconnect();
      return false;
    }
    // a read returns what is there, or waits for a byte at most 200 ms
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 200;
    timeouts.WriteTotalTimeoutConstant = 5000;
    SetCommTimeouts(mPort, &timeouts);
    PurgeComm(mPort, PURGE_RXCLEAR | PURGE_TXCLEAR);
    SOCKET pair[2];
    if (!SocketPair(pair)) {
      outError = SocketError("can't make a socket pair");
      disconnect();
      return false;
    }
    mRead = pair[0];
    mWrite = pair[1];
    mAnnounced = false;
    mStopping = false;
    mReader = std::thread([this] { readPort(); });
    return true;
  }

  bool reopenable() const override { return true; }

  int fd() const override { return ToFd(mRead); }

  Event handleReadable(std::vector<uint8_t> &outData) override
  {
    outData.clear();
    if (mRead == INVALID_SOCKET)
      return kNothing;
    Event event = ReceiveSome(mRead, outData);
    if (event == kDisconnected) {       // the reader ended: the port is gone
      disconnect();
      return kDisconnected;
    }
    if (!mAnnounced && event == kData) {  // the first bytes also say "connected"
      mAnnounced = true;
      return kConnected;
    }
    return event;
  }

  bool isConnected() const override { return mPort != INVALID_HANDLE_VALUE; }

  bool send(const uint8_t *inData, size_t inSize) override
  {
    if (mPort == INVALID_HANDLE_VALUE)
      return false;
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD written = 0;
    bool ok = WriteFile(mPort, inData, (DWORD)inSize, &written, &overlapped) != 0;
    if (!ok && GetLastError() == ERROR_IO_PENDING)
      ok = GetOverlappedResult(mPort, &overlapped, &written, TRUE) != 0;
    CloseHandle(overlapped.hEvent);
    return ok && written == inSize;
  }

  void disconnect() override
  {
    if (mReader.joinable()) {
      mStopping = true;
      CancelIoEx(mPort, nullptr);
      mReader.join();
    }
    if (mPort != INVALID_HANDLE_VALUE)
      CloseHandle(mPort);
    mPort = INVALID_HANDLE_VALUE;
    CloseSocket(mRead);
    CloseSocket(mWrite);
  }

  std::string describe() const override
  {
    return mDevice + " at " + std::to_string(mBitRate) + " bps";
  }

private:
  void readPort()
  {
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    uint8_t buffer[4096];
    while (!mStopping) {
      ResetEvent(overlapped.hEvent);
      DWORD count = 0;
      if (!ReadFile(mPort, buffer, (DWORD)sizeof(buffer), &count, &overlapped)) {
        if (GetLastError() != ERROR_IO_PENDING)
          break;                          // the port is gone
        if (!GetOverlappedResult(mPort, &overlapped, &count, TRUE))
          break;                          // gone, or cancelled by disconnect()
      }
      if (count > 0 && !SendAll(mWrite, buffer, count))
        break;
    }
    CloseHandle(overlapped.hEvent);
    shutdown(mWrite, SD_SEND);            // the reading side sees the end
  }

  std::string mDevice;
  long mBitRate;
  HANDLE mPort = INVALID_HANDLE_VALUE;
  SOCKET mRead = INVALID_SOCKET;
  SOCKET mWrite = INVALID_SOCKET;
  std::thread mReader;
  std::atomic<bool> mStopping { false };
  bool mAnnounced = false;
};

#endif // _WIN32

} // namespace


int NTKWaitReadable(const int *inFds, int inCount, int inTimeoutMs, bool *outReady)
{
#if defined(_WIN32)
  // sockets only (a serial port's are a socket pair's); select() with no
  // socket at all is an error on Windows: just wait then
  SocketStartup();
  fd_set readable;
  FD_ZERO(&readable);
  int count = 0;
  for (int i = 0; i < inCount; i++) {
    outReady[i] = false;
    if (inFds[i] >= 0) {
      FD_SET((SOCKET)inFds[i], &readable);
      count++;
    }
  }
  if (count == 0) {
    if (inTimeoutMs < 0)
      return -1;
    Sleep((DWORD)inTimeoutMs);
    return 0;
  }
  struct timeval timeout = { inTimeoutMs / 1000, (inTimeoutMs % 1000) * 1000 };
  int n = select(0, &readable, nullptr, nullptr, inTimeoutMs < 0 ? nullptr : &timeout);
  if (n == SOCKET_ERROR)
    return WSAGetLastError() == WSAEINTR ? 0 : -1;
  for (int i = 0; i < inCount; i++)
    outReady[i] = inFds[i] >= 0 && FD_ISSET((SOCKET)inFds[i], &readable);
  return n;
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
  if (inTarget.compare(0, 8, "tcp-all:") == 0) {
    char *end = nullptr;
    long port = strtol(inTarget.c_str() + 8, &end, 10);
    if (*end == 0 && port > 0 && port < 65536)
      return new TCPTransport((int)port, true);
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
  outError = "unknown target \"" + inTarget + "\" (tcp, tcp:<port>, tcp-all:<port>, serial:<device>[@<bps>], tcp-client:<port>)";
  return nullptr;
}


int NTKDump(const std::string &inTarget)
{
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
}


bool NTKMakeWakePair(int outFds[2])
{
  outFds[0] = outFds[1] = -1;
#if defined(_WIN32)
  SOCKET sockets[2];
  if (!SocketPair(sockets))
    return false;
  outFds[0] = ToFd(sockets[0]);
  outFds[1] = ToFd(sockets[1]);
  return true;
#else
  if (pipe(outFds) < 0)
    return false;
  for (int i = 0; i < 2; i++)
    fcntl(outFds[i], F_SETFL, fcntl(outFds[i], F_GETFL, 0) | O_NONBLOCK);
  return true;
#endif
}


long NTKWakeRead(int inFd, void *outData, size_t inSize)
{
  if (inFd < 0)
    return -1;
#if defined(_WIN32)
  return recv((SOCKET)inFd, (char *)outData, (int)inSize, 0);
#else
  return (long)read(inFd, outData, inSize);
#endif
}


long NTKWakeWrite(int inFd, const void *inData, size_t inSize)
{
  if (inFd < 0)
    return -1;
#if defined(_WIN32)
  return ::send((SOCKET)inFd, (const char *)inData, (int)inSize, 0);
#else
  return (long)write(inFd, inData, inSize);
#endif
}


void NTKWakeClose(int inFd)
{
  if (inFd < 0)
    return;
#if defined(_WIN32)
  closesocket((SOCKET)inFd);
#else
  close(inFd);
#endif
}
