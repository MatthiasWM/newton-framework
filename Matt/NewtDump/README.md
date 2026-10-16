# newtdump

Reads a Newton's ROM and the memory of its PCMCIA cards from their
physical addresses, over the Docking Protocol. Nothing has to be installed
on the Newton.

```
newtdump --target serial:/dev/cu.usbserial-0001 --baud 38400 all
newtdump --target tcp rom                  # Einstein, network client (127.0.0.1:3679)
newtdump --target pty card 0               # Einstein, serial port "BasiliskII"
newtdump info                              # the Newton and its sockets, reads nothing
```

newtdump waits for the Newton. On the Newton, start a connection: Dock (or
Connection) and "Connect". Then it shows the Newton, reads, and shows
progress with the rate and the time left. A block whose bytes are all the
same comes over as one number, so empty card memory is fast even on a serial
line.

Files (in `-o <dir>`, default the current directory):

| File | Contents |
|---|---|
| `rom.bin` | the ROM: 8 MB from physical 0 (`--rom-size`) |
| `card<n>-cis.txt` | the card's CIS, decoded (devices and size, name, maker, function) |
| `card<n>-attribute.bin` | the first 8 KB of attribute memory, as read (the CIS is in one byte of two) |
| `card<n>-common.bin` | the common memory, as big as the CIS says (`--size` if it says nothing) |
| `phys-<address>.bin` | `phys <address> <size>`: any physical memory |

If the connection drops or hangs, newtdump waits until the Newton connects
again and goes on where it was. Disconnect on the Newton stops newtdump: it
acknowledges the cancel (`opca` -> `ocaa`; the Newton waits for that),
lets the card go, and ends. A run that ends early (Disconnect, Ctrl-C, a
Newton that has to be reset) goes on with `--resume`. `newtdump --help` lists all
options.

## How it works

- The desktop side of the Docking Protocol (DockLink.cc, newtdump.cc):
  - the handshake and the password check (DES, Communications/DES.c);
  - "desktop in control" (`dsnc`);
  - a protocol extension (`pext`, command `NDmp`), as NCX installs its
    screenshot and ROM dump code.
- The extension (Newton/Extension.ns) is NewtonScript, compiled by newtc
  while building (cmake/NewtDumpExtension.cmake). It calls three ARM
  natives (Newton/PhysNatives.s), which reach the ROM only through the
  public jump table, so every ROM build works:
  - `PhysMap` maps physical memory read only and uncached, as hardware
    drivers do: a `TUPhys` (the kernel's object for a physical range) and
    a `TLicenseeVAddress` (the licensee domain, 128 MB at 0xE0000000) that
    maps it. Licensee addresses are never given back, so the extension maps
    1 MB windows once and keeps them in a global until the Newton
    restarts. A second run, or a reconnected one, uses them again.
  - `PhysRead` copies a block with 32-bit reads, or answers with the byte
    when all of them are the same.
  - `CardPower` calls `VccOn(socket, 0)` and `VccOff(socket)`. The ROM
    counts the users of a socket's power and leaves a card on while it has
    one; it powers cards down by itself otherwise.
- Physical addresses (MessagePad 2x00, eMate 300, Einstein):
  - ROM at 0;
  - PCMCIA socket n at 0x30000000 + n × 0x10000000;
  - in each socket: attribute memory at +0, common memory at +0x08000000,
    control registers at +0x0C000000.
- Before touching a socket, newtdump reads its pins (+0x0C001C00): both
  card-detect pins (0x000C) clear means a card is in it. It then powers the
  card, reads the CIS, decodes it (CIS.cc), and reads the common memory.
- A CIS is looked for in attribute memory first, and then at the start of
  common memory (some ROM cards keep it there).

The desktop side doesn't use the object system (`newton_ncx`). It needs:
- NSOF.cc, a small NSOF reader and writer;
- DockLink.cc;
- the `newton_dock` library: the MNP link and transports (Matt/MNP.cc,
  Matt/NTKTransport.cc) and DES.

## Building

```
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release --target newtdump newtdump-test-newton
```

newtdump is one of the tools (`NEWTON_BUILD_TOOLS`, on unless this is a
subproject). The build compiles the extension with newtc, so newtc is built
first. The natives' NewtonScript (Newton/PhysNatives.ns) is checked in, so the
build needs no ARM tools. After a change to PhysNatives.s, run
`Matt/tools/build_newtdump_natives.sh`, which needs `arm-none-eabi-as`.

newtdump is built and tested on macOS. On Windows, `newton_dock`'s link and
transports are still stubs, so newtdump can't connect there yet.

## Tests

`Test/newtdump/test_newtdump.py` runs newtdump against
`newtdump-test-newton`, a program that plays the Newton: the handshake and
password, and the extension's answers from image files. It covers:
- a ROM and a card, byte for byte;
- the CIS;
- the password;
- a dropped connection;
- an interrupted run and `--resume`;
- a ROM card with its CIS in common memory;
- a bus error.

Run it with the build folder: `Test/newtdump/test_newtdump.py --build
build/Release`.

The Newton's side (the natives and the extension's NewtonScript) runs only
on a Newton or in Einstein.

## To check on Einstein and on a Newton

These are the things only a real ROM can show:

1. **Mapping.** `TUPhys::Init` and `TLicenseeVAddress::Map` from the
   NewtonScript task: a refusal shows as "mapping 0x... : error ...".
   - `newtdump phys 0 4K` is the smallest test.
   - Compare `rom.bin` with Einstein's ROM file.
2. **Bus errors.** Whether a fault (e.g. a socket without power) becomes a
   NewtonScript exception the extension catches (`onexception |evt.ex|`),
   or something worse. newtdump only touches sockets whose card-detect
   pins say a card is in it.
3. **Socket numbers.** That `VccOn(n)` is socket n at 0x30000000 + n ×
   0x10000000, so 0 is the bottom slot of a 2x00. `info` shows the pins;
   after `card`, the Vcc bit should be on.
4. **CIS layout.** Attribute memory is read a byte at a time, as the ROM
   does (`TCardCISIterator::ReadCIS`; Einstein's card answers only byte
   reads). The decoder tries even, odd, and every byte, each also with the
   bytes swapped within words, and says which it found. On Einstein:
   "even bytes, bytes reversed in words".
5. **The eMate 300's addresses.** Its socket is assumed at 0x30000000
   (`--socket-base 0=<address>` to change it).
6. **Speed.** About 3.5 KB/s at 38400 bps, so the 8 MB ROM takes about
   40 minutes. 57600 bps (`--baud 57600`) is faster if the cable allows it.
   Einstein's PTY ("BasiliskII") waits 0.1 s before each read, about 2 KB/s;
   its network client (`--target tcp`) is faster.

`--target pty` doesn't flush the pseudo terminal. Einstein's BasiliskII
driver takes a second flush for a hung client and makes a new PTY. When
the port goes away anyway, newtdump opens it again (once a second), which
follows `/tmp/pty.BasiliskII` to the new one.
