


# A new App

I want you to write a command line app based on the code in this archive that
connects to a physical Newton via serial port, TCP, or via PTY
`/tmp/pty.BasiliskII` to Einstein.

The goal is to read the contents of the ROM and of either of the PCMCIA
RAM and CIS area via Physical Memory calls (not via virtual memory, as parts
of the ROM are paged out while running).

# Protocol

Let that app connect via the Dock protocol, so no apps have to be installed
on the Newton. The Dock Protocol allows us to define new dock commands and
inject NewtonScript, including binary code, so we can call directly into the
ROM via ARM calls.

The protocol tends to hickup and even lock up. Especially if we try to transfer
8MB over a serial line.
Our tool shall read smaller blocks
of data and wait for MNP to sync, do the Newton DMA and MNP can catch up.
There shall always be a progress indicator, and an estimated time left to read
the card.

Baud rate should be a command line option for serial connections.

# ROM

We just read the 8MB from their physical address.

# PCMCIA

Make sure that the PCMCIA card is powered up and stay powered through the
process. Read the CIS and find out if the card is memory mapped, and what
the size of the card is. Output some of the CIS information that may be relevant.
Note that pure ROM cards may have their CIS in the memory area instead.

Download the contents of the PCMCIA memory area. A bonus would be some simple
compression for blocks that are all set to the same value.

# Implementation

Make this new app another CMake target. Try to include only what the app needs
to run. It will need NSOF blocks that can be precompiled using `newtc`, so
maybe we don't need to include the entire framework or original ROM code.

# Resources

This repo implements most of what we need.

Much of the docking protocol is expplained here:
'/Users/matt/dev/Newton/NewtonOS/Docking Protocol/Newton Docking Protocol.txt'

You can use newton-re to reaad the decompiled ROM code:
/Users/matt/dev/newton-re

The disassembly is here:
/Users/matt/dev/Albert/NewtonOS/newtonos.s

NCX implements a complete Newton Sync tool. It injects anew commands to read
a screen snapshot, very similar to what we plan:
/Users/matt/dev/newton-connection.git

Physical and virtaul memory map findings are here (not verified):
https://github.com/pguyot/Einstein/wiki/Memory-Map
https://github.com/pguyot/Einstein/wiki/Mem-Map-Virtual
https://github.com/pguyot/Einstein/wiki/MemPCMCIA

# Interaction

Please ask if you have questions or are unsure about detail. Other than that,
please implement the entire thing at once. No need for me to see every step on
the way.

You can use Einstein in /Users/matt/dev/Einstein.git for testing. There is
unfortunately still no working physical device, but I will get a German machine
in the next days "für weitere Tests", and some tools for repairing the other
devices.
