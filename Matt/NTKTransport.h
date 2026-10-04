/*
 File: NTKTransport.h
 The wire to a Newton for remote debugging (Matt/Toolkit Protocol.md): a
 TCP server for Einstein, whose serial port driver connects to us as a TCP
 client, or a serial port for a real Newton. Bytes only: MNP (Matt/MNP.h)
 and the Toolkit packets go on top.
 The transport doesn't block and has no thread of its own: its user waits
 on fd() for reading (with poll() or select(), together with whatever else
 it waits for) and then calls handleReadable(). A TCP transport keeps
 listening after a disconnect, so Einstein can connect again (it reconnects
 when NewtonOS opens the serial port the next time).
 Targets (newtc -ntk-dump <target>):
   tcp                    listen on 127.0.0.1:3679 (Einstein's default)
   tcp:<port>             listen on 127.0.0.1:<port>
   serial:<device>        a serial port at 38400 bps (NTK's speed)
   serial:<device>@<bps>  ... at another speed, e.g. @57600
   tcp-client:<port>      connect to 127.0.0.1:<port>, like Einstein does
                          (for newtc playing the Newton, -ntk-device)
 POSIX only for now (like -dap-server).
 */

#ifndef MATT_NTKTRANSPORT_H
#define MATT_NTKTRANSPORT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class NTKTransport
{
public:
  enum Event {
    kNothing,       // nothing happened (e.g. a read that would block)
    kConnected,     // a Newton is connected now (serial: with its first
                    // bytes, which are in outData)
    kData,          // bytes arrived (outData)
    kDisconnected   // the connection is gone (TCP: listening again)
  };

  /** A transport for a target (see above), not started yet; nullptr and an
      error message for a target it doesn't understand. */
  static NTKTransport *Create(const std::string &inTarget, std::string &outError);

  virtual ~NTKTransport() {}

  /** Listen, or open the port. False and an error message on failure. */
  virtual bool start(std::string &outError) = 0;

  /** The file descriptor to wait on for reading: the listener while no
      Newton is connected, then the connection or the port. */
  virtual int fd() const = 0;

  /** fd() is readable: accept a connection, or read what has arrived. */
  virtual Event handleReadable(std::vector<uint8_t> &outData) = 0;

  /** A serial port is "connected" as soon as it is open. */
  virtual bool isConnected() const = 0;

  /** Write all bytes (waits while the wire is busy); false if the
      connection is gone or there is none. */
  virtual bool send(const uint8_t *inData, size_t inSize) = 0;

  /** Close the connection; a TCP transport listens again. */
  virtual void disconnect() = 0;

  /** For messages: "127.0.0.1:3679", "/dev/cu.usbserial-1420 at 57600 bps". */
  virtual std::string describe() const = 0;

  /** The peer, once connected: "127.0.0.1:61031" (TCP), else describe(). */
  virtual std::string peer() const { return describe(); }
};

/** Wait until one of inCount file descriptors is readable (an fd < 0 is
    skipped), at most inTimeoutMs milliseconds (-1: no limit). outReady[i]
    says which. Returns how many are ready, 0 after the timeout, -1 on an
    error. Uses select(), not poll(): on macOS poll() "does not support
    devices" (serial ports) and misses the end of a FIFO. */
int NTKWaitReadable(const int *inFds, int inCount, int inTimeoutMs, bool *outReady);

/** newtc -ntk-dump <target>: wait for a Newton, print what arrives in hex,
    and wait again after a disconnect, until interrupted. For trying out a
    transport; 0 when interrupted, 1 if the target can't be used. */
int NTKDump(const std::string &inTarget);

#endif // MATT_NTKTRANSPORT_H
