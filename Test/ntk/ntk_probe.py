#!/usr/bin/env python3
"""
ntk_probe.py: a minimal NTK Inspector (desktop side) for capturing and
testing the Toolkit protocol (Matt/Toolkit Protocol.md, step 1.5).

It listens on 127.0.0.1:3679, where Einstein's serial port (TCP client
driver) connects when the Newton opens it ("Connect Inspector" in the
Toolkit app). It speaks MNP (stop-and-wait, k = 1) and the Toolkit packets,
answers 'cnnt' with 'okln', and logs every byte both ways.

Commands come from a script file (or stdin with -i), one per line, run
after the Toolkit connection is up:

  lscb <file.nsof>      send an NSOF code block with 'lscb' (REP input)
  code <file.nsof>      send it with 'code' (u32 0 + NSOF; result as NSOF)
  src <NewtonScript>    compile 'func() begin <src> end' with newtc, send as lscb
  csrc <NewtonScript>   the same, sent as code
  src1 / srcg <NS>      like src, compiled with -nos1 (like NTK) / with -g
  pkg <file.pkg>        upload a package ('pkg ', 1 s pause after the header)
  pkgx <name>           delete a package by name ('pkgX')
  raw <hex bytes>       send bytes as they are (inside an LT)
  waitfor <cmd> [secs]  wait for a Toolkit packet from the Newton (default 30 s)
  wait <secs>           just wait (packets are still logged)
  term                  send 'term'
  ld                    send an MNP LD (disconnect)
  # ...                 comment

Usage:
  ntk_probe.py [--port 3679] [--log FILE] [--newtc PATH] [-i] [script]
"""

import argparse
import os
import queue
import select
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))

SYN, DLE, STX, ETX = 0x16, 0x10, 0x02, 0x03
LR, LD, LT, LA, LN, LNA = 1, 2, 4, 5, 6, 7
FRAME_NAMES = {LR: "LR", LD: "LD", LT: "LT", LA: "LA", LN: "LN", LNA: "LNA"}

# The desktop's LR (unixnpi, NewtonInspector, NTX): k = 1, N401 = 64,
# data phase optimisation (LTs up to 256 bytes, short LT/LA headers).
DESKTOP_LR = bytes([
    0x17, LR, 0x02,
    0x01, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF,
    0x02, 0x01, 0x02,
    0x03, 0x01, 0x01,
    0x04, 0x02, 0x40, 0x00,
    0x08, 0x01, 0x03,
])
MAX_LT_DATA = 256
LA_TIMEOUT = 1.0        # resend an LT without LA after this (NTX's T401)
LT_RETRIES = 10
KEEPALIVE = 3.0         # resend the last LA when quiet (NTX's T403)


def crc16(data, crc=0):
    """CRC-16/ARC (poly 0xA001 reflected, start 0), as MNP uses it."""
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def hexs(data, limit=None):
    if limit is not None and len(data) > limit:
        return data[:limit].hex(" ") + " ... (%d bytes)" % len(data)
    return data.hex(" ")


class Log:
    def __init__(self, path):
        self.t0 = time.time()
        self.file = open(path, "w") if path else None
        self.lock = threading.Lock()

    def __call__(self, text):
        line = "%8.3f %s" % (time.time() - self.t0, text)
        with self.lock:
            print(line, flush=True)
            if self.file:
                self.file.write(line + "\n")
                self.file.flush()


class MNP:
    """Stop-and-wait MNP over a socket. Received data goes to on_data()."""

    def __init__(self, sock, log, on_data):
        self.sock = sock
        self.log = log
        self.on_data = on_data
        self.write_lock = threading.Lock()
        self.rx_seq = 0             # last LT received in order
        self.tx_seq = 0             # last LT sent
        self.acked = threading.Condition()
        self.last_ack = None
        self.connected = threading.Event()
        self.closed = threading.Event()
        self.last_send = time.time()
        self.outq = queue.Queue()
        self._state = 0
        self._frame = bytearray()
        self._crc_bytes = bytearray()

    # --- sending ---

    def _send_frame(self, header, data=b""):
        body = header + data
        out = bytearray([SYN, DLE, STX])
        for b in body:
            out.append(b)
            if b == DLE:
                out.append(DLE)
        out += bytes([DLE, ETX])
        crc = crc16(body + bytes([ETX]))
        out += bytes([crc & 0xFF, crc >> 8])
        kind = FRAME_NAMES.get(header[1], "?%d" % header[1])
        detail = ""
        if header[1] == LT:
            detail = " seq %d, %d bytes: %s" % (header[2], len(data), hexs(data))
        elif header[1] == LA:
            detail = " seq %d credit %d" % (header[2], header[3])
        self.log("-> MNP %s%s   [wire %s]" % (kind, detail, hexs(bytes(out), 48)))
        with self.write_lock:
            self.sock.sendall(bytes(out))
            self.last_send = time.time()

    def send_la(self):
        self._send_frame(bytes([0x03, LA, self.rx_seq, 1]))

    def send_ld(self):
        self._send_frame(bytes([0x04, LD, 0x01, 0x01, 0xFF]))

    def send(self, data, wait=True):
        """Queue bytes for the sender thread. With wait, block until they are
        acknowledged. The reader thread must not wait: it is the one that
        receives the LAs."""
        done = threading.Event()
        job = {"data": data, "done": done, "error": None}
        self.outq.put(job)
        if wait:
            done.wait()
            if job["error"]:
                raise job["error"]

    def sender(self):
        while not self.closed.is_set():
            try:
                job = self.outq.get(timeout=0.5)
            except queue.Empty:
                continue
            try:
                self._send_now(job["data"])
            except Exception as e:
                job["error"] = e
                self.log("   MNP send failed: %s" % e)
            job["done"].set()

    def _send_now(self, data):
        """Send bytes as LTs, one at a time, each waiting for its LA."""
        for i in range(0, len(data), MAX_LT_DATA):
            chunk = data[i:i + MAX_LT_DATA]
            self.tx_seq = (self.tx_seq + 1) & 0xFF
            for attempt in range(LT_RETRIES):
                with self.acked:
                    self.last_ack = None
                self._send_frame(bytes([0x02, LT, self.tx_seq]), chunk)
                with self.acked:
                    self.acked.wait_for(lambda: self.last_ack == self.tx_seq or self.closed.is_set(),
                                        LA_TIMEOUT)
                    if self.last_ack == self.tx_seq:
                        break
                if self.closed.is_set():
                    raise IOError("link closed")
                self.log("   MNP no LA for LT %d, resending (%d)" % (self.tx_seq, attempt + 1))
            else:
                raise IOError("LT %d never acknowledged" % self.tx_seq)

    # --- receiving ---

    def feed(self, data):
        """Unframe received bytes (state machine like NTX's unframe)."""
        for b in data:
            st = self._state
            if st == 0:
                self._state = 1 if b == SYN else 0
            elif st == 1:
                self._state = 2 if b == DLE else (1 if b == SYN else 0)
            elif st == 2:
                if b == STX:
                    self._frame = bytearray()
                    self._state = 3
                else:
                    self._state = 0
            elif st == 3:
                if b == DLE:
                    self._state = 4
                else:
                    self._frame.append(b)
            elif st == 4:
                if b == DLE:
                    self._frame.append(DLE)
                    self._state = 3
                elif b == ETX:
                    self._crc_bytes = bytearray()
                    self._state = 5
                else:
                    self.log("<- MNP bad escape 10 %02x, frame dropped" % b)
                    self._state = 0
            elif st == 5:
                self._crc_bytes.append(b)
                if len(self._crc_bytes) == 2:
                    self._state = 0
                    got = self._crc_bytes[0] | (self._crc_bytes[1] << 8)
                    want = crc16(bytes(self._frame) + bytes([ETX]))
                    if got != want:
                        self.log("<- MNP CRC error (got %04x, want %04x): %s"
                                 % (got, want, hexs(bytes(self._frame), 64)))
                        if self.connected.is_set():
                            self.send_la()
                    else:
                        self._frame_in(bytes(self._frame))

    def _frame_in(self, f):
        if len(f) < 2:
            self.log("<- MNP short frame %s" % hexs(f))
            return
        hlen, kind = f[0], f[1]
        name = FRAME_NAMES.get(kind, "?%d" % kind)
        if kind == LR:
            self.log("<- MNP LR %s" % hexs(f))
            self.rx_seq = 0
            self.tx_seq = 0
            self._send_frame(DESKTOP_LR)
        elif kind == LA:
            self.log("<- MNP LA seq %d credit %d" % (f[2], f[3] if len(f) > 3 else -1))
            if not self.connected.is_set():
                self.connected.set()
                self.log("   MNP link up")
            with self.acked:
                self.last_ack = f[2]
                self.acked.notify_all()
        elif kind == LT:
            seq = f[2]
            data = f[hlen + 1:]
            if not self.connected.is_set():
                self.connected.set()
            if seq == ((self.rx_seq + 1) & 0xFF):
                self.rx_seq = seq
                self.log("<- MNP LT seq %d, %d bytes: %s" % (seq, len(data), hexs(data)))
                self.send_la()
                self.on_data(data)
            elif seq == self.rx_seq:
                self.log("<- MNP LT seq %d again (resent), acked, not delivered" % seq)
                self.send_la()
            else:
                self.log("<- MNP LT seq %d out of order (expected %d)" % (seq, (self.rx_seq + 1) & 0xFF))
                self.send_la()
        elif kind == LD:
            self.log("<- MNP LD %s" % hexs(f))
            self.closed.set()
        else:
            self.log("<- MNP %s %s" % (name, hexs(f)))

    def keepalive(self):
        while not self.closed.is_set():
            time.sleep(0.5)
            if self.connected.is_set() and time.time() - self.last_send > KEEPALIVE:
                self.send_la()


class Toolkit:
    """The Toolkit packet layer: 'newt' 'ntp ' <cmd> <length> <data>."""

    def __init__(self, log, newtc, outdir):
        self.log = log
        self.newtc = newtc
        self.outdir = outdir
        self.buf = bytearray()
        self.mnp = None
        self.packets = queue.Queue()
        self.count = 0

    def send(self, cmd, data=b"", length=None, wait=True):
        if length is None:
            length = len(data)
        pkt = b"newtntp " + cmd + struct.pack(">I", length) + data
        self.log("-> NTK %s length %d%s" % (cmd.decode("latin-1"), length,
                                           (": " + hexs(data, 48)) if data else ""))
        self.mnp.send(pkt, wait)

    def feed(self, data):
        self.buf += data
        while True:
            pkt = self._parse()
            if pkt is None:
                return
            self._handle(*pkt)

    def _parse(self):
        b = self.buf
        if len(b) < 16:
            return None
        if b[:8] != b"newtntp ":
            i = b.find(b"newtntp ", 1)
            junk = bytes(b[:i if i > 0 else len(b)])
            self.log("<- NTK %d bytes outside a packet: %s" % (len(junk), hexs(junk, 64)))
            del b[:len(junk)]
            return None
        cmd = bytes(b[8:12])
        length = struct.unpack(">I", b[12:16])[0]
        if cmd == b"code":
            # the length is an echo of the request's; the data is u32 size + NSOF
            if len(b) < 20:
                return None
            size = struct.unpack(">I", b[16:20])[0]
            total = 20 + size
        elif cmd == b"eerr":
            # the length field doesn't count the u32 words: nameLen (+4)
            total = 16 + length + 4
        elif cmd in (b"estr", b"eref"):
            # nameLen and msgLen/size words not counted (+8)
            total = 16 + length + 8
        else:
            total = 16 + length
        if len(b) < total:
            return None
        data = bytes(b[16:total])
        del b[:total]
        return cmd, length, data

    def _nsof(self, label, nsof):
        self.count += 1
        path = os.path.join(self.outdir, "obj%03d_%s.nsof" % (self.count, label))
        with open(path, "wb") as f:
            f.write(nsof)
        text = ""
        if self.newtc:
            try:
                r = subprocess.run([self.newtc, "-nsof", path, "-decompile"],
                                   capture_output=True, text=True, timeout=10)
                text = (r.stdout + r.stderr).strip()
                text = text.replace("DefineGlobalConstant(", "").strip()
            except Exception as e:
                text = "(newtc failed: %s)" % e
        return "%s (%d bytes NSOF, %s)%s" % (label, len(nsof), os.path.basename(path),
                                             ("\n" + text) if text else "")

    def _handle(self, cmd, length, data):
        c = cmd.decode("latin-1")
        if cmd == b"cnnt":
            self.log("<- NTK cnnt length %d" % length)
            self.send(b"okln", wait=False)     # we are on the reader thread
        elif cmd == b"text":
            t = data.decode("mac_roman").replace("\r", "\n")
            self.log("<- NTK text %r" % t)
        elif cmd == b"rslt":
            err = struct.unpack(">i", data[:4])[0] if len(data) >= 4 else None
            self.log("<- NTK rslt %s (length %d)" % (err, length))
        elif cmd in (b"fobj", b"fstk"):
            self.log("<- NTK %s %s" % (c, self._nsof(c, data)))
        elif cmd == b"code":
            self.log("<- NTK code length %d (echo) %s" % (length, self._nsof("code", data[4:])))
        elif cmd in (b"eerr", b"estr", b"eref"):
            n = struct.unpack(">I", data[:4])[0]
            name = data[4:4 + n].rstrip(b"\0").decode("latin-1")
            rest = data[4 + n:]
            if cmd == b"eerr":
                self.log("<- NTK eerr %s error %d" % (name, struct.unpack(">i", rest[:4])[0]))
            elif cmd == b"estr":
                m = struct.unpack(">I", rest[:4])[0]
                self.log("<- NTK estr %s message %r" % (name, rest[4:4 + m].rstrip(b"\0").decode("mac_roman")))
            else:
                m = struct.unpack(">I", rest[:4])[0]
                self.log("<- NTK eref %s %s" % (name, self._nsof("eref", rest[4:4 + m])))
        else:
            self.log("<- NTK %s length %d%s" % (c, length, (": " + hexs(data, 64)) if data else ""))
        self.packets.put(c)

    def wait_for(self, cmd, timeout):
        end = time.time() + timeout
        while True:
            left = end - time.time()
            if left <= 0:
                self.log("   waitfor %s: timed out" % cmd)
                return False
            try:
                got = self.packets.get(timeout=left)
            except queue.Empty:
                continue
            if got.strip() == cmd.strip():
                return True


def compile_src(newtc, src, outdir, n, flags=()):
    path = os.path.join(outdir, "src%03d.nsof" % n)
    r = subprocess.run([newtc] + list(flags) + ["-s", "func() begin %s end" % src, "-onsof", path],
                       capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(path):
        raise RuntimeError("newtc could not compile %r: %s" % (src, r.stdout + r.stderr))
    with open(path, "rb") as f:
        return f.read()


def run_command(line, tk, mnp, log, args, n):
    parts = line.split(None, 1)
    op = parts[0].lower()
    arg = parts[1] if len(parts) > 1 else ""
    log("== %s" % line)
    if op == "lscb":
        tk.send(b"lscb", open(arg, "rb").read())
    elif op == "code":
        tk.send(b"code", b"\0\0\0\0" + open(arg, "rb").read())
    elif op == "src":
        tk.send(b"lscb", compile_src(args.newtc, arg, args.outdir, n))
    elif op == "csrc":
        tk.send(b"code", b"\0\0\0\0" + compile_src(args.newtc, arg, args.outdir, n))
    elif op == "src1":     # compiled for NOS 1 (a 'CodeBlock frame, like NTK sends)
        tk.send(b"lscb", compile_src(args.newtc, arg, args.outdir, n, ["-nos1"]))
    elif op == "srcg":     # compiled with -g (DebuggerInfo, line table)
        tk.send(b"lscb", compile_src(args.newtc, arg, args.outdir, n, ["-g"]))
    elif op == "pkg":
        pkg = open(arg, "rb").read()
        log("-> NTK pkg  length %d (header, then 1 s pause, then the package)" % len(pkg))
        mnp.send(b"newtntp pkg " + struct.pack(">I", len(pkg)))
        time.sleep(1.0)
        mnp.send(pkg)
    elif op == "pkgx":
        name = arg.encode("utf-16-be") + b"\0\0"
        tk.send(b"pkgX", name)
    elif op == "raw":
        data = bytes.fromhex(arg)
        log("-> raw %s" % hexs(data))
        mnp.send(data)
    elif op == "waitfor":
        w = arg.split()
        tk.wait_for(w[0], float(w[1]) if len(w) > 1 else 30.0)
    elif op == "wait":
        time.sleep(float(arg))
    elif op == "term":
        tk.send(b"term")
    elif op == "ld":
        mnp.send_ld()
    else:
        log("   unknown command %r" % op)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("script", nargs="?", help="command script (default: none, just log)")
    p.add_argument("--port", type=int, default=3679)
    p.add_argument("--log", help="also write the log to this file")
    p.add_argument("--newtc", default=os.path.join(REPO, "build", "Release", "newtc"))
    p.add_argument("--outdir", help="where to keep NSOF files (default: a temp dir)")
    p.add_argument("-i", "--interactive", action="store_true", help="read commands from stdin")
    p.add_argument("--fifo", help="read commands from this named pipe (created if needed; "
                                  "send with: echo 'csrc 2+3' > FIFO)")
    args = p.parse_args()
    args.outdir = args.outdir or tempfile.mkdtemp(prefix="ntk_probe_")
    os.makedirs(args.outdir, exist_ok=True)
    if not os.path.exists(args.newtc):
        args.newtc = None
    log = Log(args.log)

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", args.port))
    srv.listen(1)
    log("listening on 127.0.0.1:%d (NSOF files in %s)" % (args.port, args.outdir))
    sock, peer = srv.accept()
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    log("connected from %s:%d" % peer)

    tk = Toolkit(log, args.newtc, args.outdir)
    mnp = MNP(sock, log, tk.feed)
    tk.mnp = mnp

    def reader():
        try:
            while not mnp.closed.is_set():
                r, _, _ = select.select([sock], [], [], 0.5)
                if r:
                    data = sock.recv(4096)
                    if not data:
                        log("socket closed by Einstein")
                        mnp.closed.set()
                        break
                    mnp.feed(data)
        except Exception as e:
            log("reader stopped: %r" % e)
            mnp.closed.set()

    threading.Thread(target=reader, daemon=True).start()
    threading.Thread(target=mnp.keepalive, daemon=True).start()
    threading.Thread(target=mnp.sender, daemon=True).start()

    lines = []
    if args.script:
        with open(args.script) as f:
            lines = [l.rstrip("\n") for l in f]
    if lines or args.interactive or args.fifo:
        if not tk.wait_for("cnnt", 900):
            log("no 'cnnt' from the Newton")
        time.sleep(0.5)
    n = 0
    try:
        for line in lines:
            if mnp.closed.is_set():
                break
            if line.strip() and not line.strip().startswith("#"):
                n += 1
                run_command(line.strip(), tk, mnp, log, args, n)
        if args.fifo:
            if not os.path.exists(args.fifo):
                os.mkfifo(args.fifo)
            log("reading commands from %s" % args.fifo)
            while not mnp.closed.is_set():
                with open(args.fifo) as f:          # blocks until a writer opens it
                    for line in f:
                        if mnp.closed.is_set():
                            break
                        if line.strip() and not line.strip().startswith("#"):
                            n += 1
                            try:
                                run_command(line.strip(), tk, mnp, log, args, n)
                            except Exception as e:
                                log("   failed: %s" % e)
        elif args.interactive:
            for line in sys.stdin:
                if mnp.closed.is_set():
                    break
                if line.strip() and not line.strip().startswith("#"):
                    n += 1
                    try:
                        run_command(line.strip(), tk, mnp, log, args, n)
                    except Exception as e:
                        log("   failed: %s" % e)
        else:
            while not mnp.closed.is_set():
                time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    except Exception as e:
        log("stopped: %s" % e)
    log("done")
    sock.close()


if __name__ == "__main__":
    main()
