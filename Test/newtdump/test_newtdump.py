#!/usr/bin/env python3
"""
newtdump against newtdump-test-newton (a Newton played by a program: the
Docking Protocol's handshake and password, the protocol extension's
requests answered from image files). Checks:

  - all: the ROM and a card come out as they were (byte for byte), blocks
    that are all one value too (sent as one number); the CIS decoded (an
    SRAM card's size, its name); the empty socket says so
  - the password: the right one docks, a wrong one is refused
  - the connection drops in the middle: newtdump waits, docks again, and
    goes on; the files are complete
  - an interrupted run (Ctrl-C) and --resume: the file is complete
  - a ROM card with its CIS at the start of common memory
  - Einstein's card image (.pccard): --pccard while reading a card, and the
    pccard command from the files of an earlier run (no Newton)
  - a card smaller than --size: a bus error, reported, no crash

The Newton's side (the ARM natives, the extension's NewtonScript) runs only
on a Newton or Einstein (Matt/NewtDump/README.md).

Usage: Test/newtdump/test_newtdump.py [--build build/VSCode]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import os
import random
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def rom_image(size):
    rng = random.Random(42)
    data = bytearray(rng.getrandbits(8) for _ in range(size))
    data[0x4000:0x8000] = b"\x00" * 0x4000          # blocks of one value
    data[0x10000:0x11000] = b"\xFF" * 0x1000
    return bytes(data)


def cis_bytes(size_code=0x3A, strings=(b"Apple", b"Test SRAM 64K")):
    """CISTPL_DEVICE (SRAM, 64 KB), VERS_1, MANFID, FUNCID (memory), END."""
    tuples = bytearray()
    tuples += bytes([0x01, 3, 0x62, size_code, 0xFF])        # SRAM, 200 ns; size; end
    vers = bytearray([4, 1]) + b"\x00".join(strings) + b"\x00\xFF"
    tuples += bytes([0x15, len(vers)]) + vers
    tuples += bytes([0x20, 4, 0x34, 0x12, 0x78, 0x56])
    tuples += bytes([0x21, 2, 1, 0])
    tuples += bytes([0xFF])
    return bytes(tuples)


def attribute_image(cis, size=8192):
    """The CIS in the even bytes (attribute memory: 8 bits on a 16-bit bus)."""
    data = bytearray(b"\xFF" * size)
    for i, b in enumerate(cis):
        data[2 * i] = b
    return bytes(data)


def pccard_image(data, cis, name, card_type):
    """Einstein's .pccard (TLinearCard::ImageInfo): data, CIS, name, and ten
    big-endian words plus "TLinearCard\\0" at the end."""
    name = name.encode() + b"\x00"
    info = [len(name), len(data) + len(cis), 0, len(data) + len(cis), len(cis), len(data), len(data), 0,
            card_type, 1]
    return data + cis + name + b"".join(w.to_bytes(4, "big") for w in info) + b"TLinearCard\x00"


class Run:
    """newtdump (listening) and the test Newton (connecting to it)."""

    def __init__(self, build, out, args, newton_args, port=None):
        self.port = port or free_port()
        self.newtdump = subprocess.Popen(
            [str(build / "newtdump"), "--target", "tcp:%d" % self.port, "-o", str(out)] + args,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.DEVNULL, text=True)
        self.err = []
        self.waiting = threading.Event()
        threading.Thread(target=self._read_err, daemon=True).start()
        self.waiting.wait(10)
        self.newton = subprocess.Popen(
            [str(build / "newtdump-test-newton"), "--target", "tcp-client:%d" % self.port] + newton_args,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.DEVNULL, text=True)

    def _read_err(self):
        buf = ""
        while True:
            c = self.newtdump.stderr.read(1)
            if not c:
                break
            buf += c
            if "Waiting for a Newton" in buf:
                self.waiting.set()
            if c in "\n\r":
                self.err.append(buf)
                buf = ""
        self.err.append(buf)
        self.waiting.set()

    def wait(self, timeout=120):
        try:
            out = self.newtdump.stdout.read()
            self.newtdump.wait(timeout)
        except subprocess.TimeoutExpired:
            self.newtdump.kill()
            out = ""
        try:
            newton_out, newton_err = self.newton.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            self.newton.kill()
            newton_out, newton_err = self.newton.communicate()
        time.sleep(0.1)
        return self.newtdump.returncode, out, "".join(self.err), newton_err


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default=str(REPO / "build" / "VSCode"))
    args = ap.parse_args()
    build = Path(args.build)
    checks = []
    logs = []

    def check(name, ok, log=""):
        checks.append((name, bool(ok)))
        if not ok:
            logs.append("--- %s ---\n%s" % (name, log))

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        rom = rom_image(256 * 1024)
        (tmp / "rom.img").write_bytes(rom)
        cis = cis_bytes()
        (tmp / "attr.img").write_bytes(attribute_image(cis))
        rng = random.Random(7)
        common = bytearray(rng.getrandbits(8) for _ in range(64 * 1024))
        common[0x2000:0x3000] = b"\x00" * 0x1000
        (tmp / "common.img").write_bytes(bytes(common))
        images = ["--rom", str(tmp / "rom.img"), "--attribute0", str(tmp / "attr.img"),
                  "--common0", str(tmp / "common.img")]

        # all: the ROM and the card
        out = tmp / "all"
        code, stdout, stderr, newton = Run(build, out, ["--rom-size", "256K", "all"], images).wait()
        log = stdout + stderr + newton
        check("all: newtdump ends well", code == 0, log)
        check("... the ROM, byte for byte", (out / "rom.bin").exists() and (out / "rom.bin").read_bytes() == rom, log)
        check("... uniform blocks sent as one byte", "sent as one byte" in stderr, log)
        check("... the card's common memory, byte for byte",
              (out / "card0-common.bin").exists() and (out / "card0-common.bin").read_bytes() == bytes(common), log)
        cis_text = (out / "card0-cis.txt").read_text() if (out / "card0-cis.txt").exists() else ""
        check("... the CIS: an SRAM device of 64 KB, its name, maker, function",
              "SRAM, 64 KB" in cis_text and '"Test SRAM 64K"' in cis_text and "manufacturer 0x1234" in cis_text
              and "function: memory" in cis_text and "even bytes" in cis_text, log + cis_text)
        check("... socket 0 has a card, socket 1 none",
              "Socket 0 (0x30000000): a card" in stdout and "Socket 1 (0x40000000): no card" in stdout, log)
        check("... the Newton described", "Test Newton: MessagePad 2000/2100" in stdout, log)

        # the pccard command: Einstein's image from the files above, no Newton
        result = subprocess.run([str(build / "newtdump"), "-o", str(out), "pccard"], capture_output=True,
                                text=True, stdin=subprocess.DEVNULL, timeout=30)
        image = (out / "card0.pccard").read_bytes() if (out / "card0.pccard").exists() else b""
        check("pccard: card0.pccard from the files (data, the CIS in order, the CIS's name, SRAM)",
              result.returncode == 0 and image == pccard_image(bytes(common), cis, "Apple Test SRAM 64K", 6),
              result.stdout + result.stderr)

        # the password
        out = tmp / "password"
        code, stdout, stderr, newton = Run(build, out, ["--rom-size", "16K", "--password", "secret", "rom"],
                                           images + ["--password", "secret"]).wait()
        check("password: the right one docks", code == 0 and (out / "rom.bin").read_bytes() == rom[:16384],
              stdout + stderr + newton)
        out = tmp / "badpassword"
        code, stdout, stderr, newton = Run(build, out, ["--rom-size", "16K", "--password", "wrong", "rom"],
                                           images + ["--password", "secret"]).wait(timeout=60)
        check("... a wrong one is refused", code != 0 and "password" in stderr, stdout + stderr + newton)

        # the connection drops after 20 reads: newtdump docks again and goes on
        out = tmp / "drop"
        code, stdout, stderr, newton = Run(build, out, ["--rom-size", "128K", "rom"],
                                           images + ["--drop-after", "20"]).wait()
        check("drop: newtdump waits, docks again, the ROM is complete",
              code == 0 and "connection is lost" in stderr and "connect again" in stderr
              and (out / "rom.bin").read_bytes() == rom[:128 * 1024], stdout + stderr + newton)

        # Disconnect on the Newton in the middle: 'opca' answered with 'ocaa',
        # newtdump stops (no waiting, no reconnecting), the card powered off
        out = tmp / "cancel"
        started = time.time()
        code, stdout, stderr, newton = Run(build, out, ["card", "0"], images + ["--cancel-after", "8"]).wait(timeout=60)
        check("cancelled on the Newton: acknowledged ('ocaa'), newtdump stops at once",
              code == 1 and "Stopped: cancelled on the Newton" in stderr and "--resume" in stderr
              and "cancel acknowledged ('ocaa')" in newton and time.time() - started < 20,
              stdout + stderr + newton)

        # interrupted (as with Ctrl-C), then --resume
        out = tmp / "resume"
        run = Run(build, out, ["--rom-size", "256K", "rom"], images + ["--delay", "20"])
        time.sleep(1.5)
        run.newtdump.send_signal(signal.SIGINT)
        code, stdout, stderr, newton = run.wait()
        partial = (out / "rom.bin").stat().st_size if (out / "rom.bin").exists() else 0
        check("interrupted: a partial file, and the hint", code != 0 and 0 < partial < len(rom)
              and "--resume" in stderr, stdout + stderr + newton)
        code, stdout, stderr, newton = Run(build, out, ["--rom-size", "256K", "--resume", "rom"], images).wait()
        check("... --resume completes it", code == 0 and "going on at" in stderr
              and (out / "rom.bin").read_bytes() == rom, stdout + stderr + newton)

        # a ROM card: no CIS in attribute memory, at the start of common memory
        rom_card = bytearray(cis_bytes(0x3A, (b"Maker", b"ROM Card"))) + bytes(64 * 1024)
        rom_card = rom_card[:64 * 1024]
        rom_card[0x8000:0x9000] = bytes(range(256)) * 16
        (tmp / "romcard.img").write_bytes(bytes(rom_card))
        (tmp / "noattr.img").write_bytes(b"\xFF" * 8192)
        out = tmp / "romcard"
        code, stdout, stderr, newton = Run(build, out, ["card", "0"],
                                           ["--attribute0", str(tmp / "noattr.img"),
                                            "--common0", str(tmp / "romcard.img")]).wait()
        check("ROM card: the CIS in common memory, the card read",
              code == 0 and "in common memory, every byte" in stdout and '"ROM Card"' in stdout
              and (out / "card0-common.bin").read_bytes() == bytes(rom_card), stdout + stderr + newton)

        # Einstein's linear card: its default CIS (an Intel 2 MB flash card),
        # byte addresses as Einstein maps them (TLinearCard::ReadAttrB: the
        # byte at A is CIS[(A / 2) ^ 1])
        einstein_cis = bytes([0x01, 0x03, 0x52, 0x06, 0xFF, 0x1E, 0x06, 0x02, 0x11, 0x01, 0x01, 0x03, 0x01,
                              0x18, 0x02, 0x89, 0xA2, 0x15, 0x0C, 0x04, 0x01]) + b"intel\x00FLASH\x00\xFF" + b"\xFF"
        attr = bytearray(8192)
        for a in range(len(attr)):
            k = (a // 2) ^ 1
            attr[a] = einstein_cis[k] if k < len(einstein_cis) else 0
        (tmp / "einstein-attr.img").write_bytes(bytes(attr))
        flash = bytes(random.Random(3).getrandbits(8) for _ in range(256 * 1024))
        (tmp / "einstein-common.img").write_bytes(flash)
        out = tmp / "einstein"
        code, stdout, stderr, newton = Run(build, out, ["--size", "256K", "--pccard", "--name", "Flash",
                                                         "card", "0"],
                                           ["--attribute0", str(tmp / "einstein-attr.img"),
                                            "--common0", str(tmp / "einstein-common.img")]).wait()
        check("Einstein's card: its CIS found (word-swapped even bytes), 2 MB flash, Intel",
              code == 0 and "even bytes, bytes reversed in words" in stdout and "flash, 2 MB" in stdout
              and "JEDEC: manufacturer 0x89" in stdout and (out / "card0-common.bin").read_bytes() == flash,
              stdout + stderr + newton)
        # its image: the CIS as Einstein keeps it (the chain up to its end), the flash type
        image = (out / "card0.pccard").read_bytes() if (out / "card0.pccard").exists() else b""
        check("... --pccard: the card as Einstein had it",
              image == pccard_image(flash, einstein_cis[:-1], "Flash", 5), stdout + stderr + newton)

        # --size beyond the card: a bus error, reported
        out = tmp / "buserror"
        code, stdout, stderr, newton = Run(build, out, ["--size", "128K", "card", "0"], images).wait()
        check("a card smaller than --size: the error reported, no crash",
              code == 1 and "error -10035" in stderr and "evt.ex.bus" in stderr, stdout + stderr + newton)

    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if logs:
        print("\n".join(l[-4000:] for l in logs))
    return 0 if all(ok for _, ok in checks) else 1


if __name__ == "__main__":
    sys.exit(main())
