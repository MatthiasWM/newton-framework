

# Matt's NewtonScript debugger

## the goal

The `newtc` command line application gets a REPL debugger for NewtonScript,
based as much as possible on what is already inside NewtonScrip (`BreakLoop`)
and what the "NS Debug Tools.pkg" provides (Next, StepIn, StepOut, ...).

The REPL mode will be accessible via command line ( `-dbg` ) by expanding
the BreakLoop interface to also understand `gdb` style shortcut like `n`, `s`,
and `r`. And via DAP Debug Protocol.

## REPL

Much of REPL already comes with NewtonScript, and additional functions are in
"NS Debug Tools.pkg" that came with NTK. WE can decompile the debug tools (there
is some ARM32 code in there) and lift the features and make them part of newtc.

In a first iteration, we only look at ByteCode commands. We must keep in mind
that we want source level debugging later.

### Preserving and activating what we have

NewtonScript comes with a few basics that are extremely helpful here. The
original ROM has support for breakpoints and jumps into an interactive BreakLoop
when a breakpoint is hit. This is probably the first thing that must be reactivated
and tested. The Interpreter in ROM has a "slow interpreter loop" that manages this
that was not lifted from the ROM into this library. I have access to the disassembly,
so if needed, we can decompile it or at least look at how they did it.

More intricate debugging functions (StepIn, StepOut, etc.) came in a sepaarte
package "NS Debug Tools.pkg" that came with NTK. Note that Apple never fully
finished and tested this. "This has not yet undergone extensive official
testing, but seems to work well.". We should stick at least with the APple API,
or even with theeir implementation where it makes sense. Again, ARM32 machine
code is involved.

## Command line mode

For testing and debugging "newtc", a terminal version of REPL mode is available
via `-dbg`. In this mode, we break directly into then NewtonScript `BreakLoop`
where we can enter NewtonScript commands. This already mostly works. Adding
single letter shortcuts maked this mode usable (vs. typing StepIn(), etc. ).
If this works reliable on ByteCode level, we add support for the DAP protocol.

## DAP protocol

I want to implement a debugger interface for the "newtc" executable using the
DAP protocol with Visual Studio Code. The DAP is handled using cppdap:
https://github.com/google/cppdap . DAP mode is enabled by passing the `-dap`
flag.

DAP is to be implemented on top of a REPL debugger interface.

ByteCode debugging should map directly. How do we handle the display of the
ByteCode diassembly though?

## Source Level Debugging

### Internal Debug Information

To allow source level debugging, we must map the pair sourefile/linenumber to
the newtonscript function address/ByteCode offset (PC) - and back. We need
to find an internal representation to do this mapping and write a function
for converting in either direction.

### Generating and storing the debug map

We need to generate this map as external debug data files that can be loaded
by `newtc` when debug mode is enetered. We have three cases:

- Compiling a source file into NewtonScript directly: the compiler must
  generate the map directly, no inbetween file is needed (compile and debug)

- Compiling into a NSOF or pkg file: the compiler must write the debug
  information file alongside the NSOF/pkg file. Mapping functions could be done
  by adding an index or UID to every function that the compiler generates

- Decompiling packages: in this case, newtc load a pkg file without debug
  information and decompiles it. Here, the decompiler must geenrate the map.
  Every function has a natural index that should be repeatable after loading
  the package.

- Decompile ROM: decompiling the ROM, again, the decompiler generates the map.
  Since the ROM is stored as a single binary blob, we should be able to look
  up functions inside the blob by storing the binary offset in the map file.

To sum up, every separately loaded chunk (ROM, NSOF, pkg, compiled code) gets
its own map file. All required map files are loaded when in debug mode. An
internal map is generated. Unmapped functions will fall back to ByteCode
debugging.



