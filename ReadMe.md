# Newton framework
The Newton OS as libraries and tools.
Well, enough of it to enable the Newton Connection and Newton Toolkit apps, and `newtc`, a NewtonScript compiler,
interpreter and debugger.


## NOTES ON THE NEWTON SOURCE
Let's make no bones about it, the source is derived from close inspection of the symbols included in the Newton ROM v2.2,
the header files distributed with the Newton C++ Tools and Lantern DDKs, and the Newton Programmer's Guide, etc.
It's mostly faithful to the original, even down to things that really have no place on another platform.
The most obvious change is in renaming classes having a T prefix (Apple's Pascal heritage) to a C prefix (because we are now in a C world).
Comments are (I hope) sprinkled adequately, but there is no overall design document, hence these notes.


## BUILDING
The build uses CMake (3.15 or later) and a C++20 compiler. A program that links the libraries can use C++17.

```
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release --target newtc newtdump
```

What CMake builds:

| Target | What it is | Systems |
|---|---|---|
| `newton_ncx` | Static library for NCX (Newton Connection): objects, NewtonScript (interpreter, compiler, decompiler), NSOF, stores, compression, packages, the ROM's data, QuickDraw drawing. Started with `InitObjectSystemCore()`. | Linux, Windows, macOS |
| `newton_dock` | Static library: the MNP link, serial and TCP transports, DES. On Windows the link and transports are stubs; framing, CRC and DES work. | Linux, Windows, macOS |
| `newtc` | NewtonScript compiler, interpreter and debugger (VS Code through DAP; also on a Newton or Einstein over the Toolkit protocol). Writes packages. `newtc -h` lists its options. | macOS |
| `newtdump` | Reads a Newton's ROM and PCMCIA cards over the Docking Protocol (Matt/NewtDump/README.md). `newtdump-test-newton` plays the Newton for its tests. | macOS |
| `NewtPlay` | Runs Newton packages as a Mac app. Needs `-DNEWTC_USES_FLTK=ON`; `Matt/tools/build_newtplay.sh` builds the release. Not built by default. | macOS |
| `ResMaker` | Makes the ROM data (C) from a ROM image (see ROM Data below). Not built by default. | macOS |
| `newton_ncx_test`, `newton_dock_test` | The libraries' tests (`ctest`). | Linux, Windows, macOS |
| `MessagePad` | Simon's MP2100 on the Mac. It doesn't link at the moment; Intel Macs only. | — |

The libraries are always built. GitHub Actions builds and tests them on Linux (x64, arm64; gcc), Windows (x64, arm64; MSVC) and macOS (universal); see `.github/workflows/newton_ncx.yml`. The tools are built and tested on macOS; other systems haven't been tried yet.

Options:

* `NEWTON_BUILD_TOOLS`: newtc, newtdump, ResMaker, NewtPlay. On by default, off when this is a subproject.
* `NEWTON_BUILD_TESTS`: the libraries' tests. On by default, off when this is a subproject.
* `NEWTON_SANITIZE`: AddressSanitizer in Debug builds. On by default, off when this is a subproject; not with MSVC.
* `NEWTC_USES_FLTK`: newtc opens Newton views in windows. FLTK is fetched from GitHub. Off by default.
* `NEWTON_NCX_FLTK`: `newton_ncx` draws with FLTK, on FLTK's current surface; without it, drawing does nothing. On if the parent project has `fltk::fltk` or `NEWTC_USES_FLTK` is on.
* `NEWTC_UBSAN`: the undefined behavior sanitizer in newtc. On by default; release builds turn it off.
* `NEWTON_BUILD_MESSAGEPAD`: on for a top-level build on an Intel Mac.

To build only the libraries and their tests, for example on Linux or Windows:

```
cmake -S . -B build -DNEWTON_BUILD_TOOLS=OFF -DNEWTON_BUILD_TESTS=ON
cmake --build build --config RelWithDebInfo --target newton_ncx_test newton_dock_test --parallel 4
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

On Linux, FLTK needs the X11 development packages (the list is in the workflow). Use a fixed number of jobs (`--parallel 4`): on its own, `--parallel` starts an unlimited `make -j`.

As a subproject, NCX does this:

```cmake
include (FetchContent)
FetchContent_Declare (newton
  GIT_REPOSITORY https://github.com/MatthiasWM/newton-framework.git
  GIT_TAG dap_to_toolbox
)
FetchContent_MakeAvailable (newton)
target_link_libraries (myapp PRIVATE newton_ncx newton_dock)
```

`-DFETCHCONTENT_SOURCE_DIR_NEWTON=<path>` uses a local checkout instead of downloading one. `Test/cmake/test_subproject.py` tests this setup.

`cmake -G Xcode` makes an Xcode project. `Newton.xcodeproj` is the original Xcode project; it isn't maintained.


## TESTS
* `ctest` in the build folder runs the libraries' tests.
* `Test/dbg/run_dbg_tests.py` covers newtc: the compiler, the interpreter, DAP, packages and FLTK views. It runs about 130 cases from `Test/dbg/cases`. `--newtc <path>` selects the binary (default `build/VSCode/newtc`, a Debug build with FLTK). The other `Test/dbg/test_*.py` scripts test more of newtc.
* `Test/ntk/test_*.py`: MNP, the Toolkit protocol, and remote debugging, against newtc playing the Newton (`-ntk-device`).
* `Test/newtdump/test_newtdump.py --build <build folder>`: newtdump against `newtdump-test-newton`.
* `Test/romdata/test_resmaker.py`: regenerates the ROM data with ResMaker and compares it with the repository.
* `Test/cmake/test_subproject.py`: the subproject build. With `--build-tools`, it builds the tools there too.
* `Test/run_corpus.py`: decompiles a collection of packages (`Test/pkglist.txt`). `--compare` checks a run against an earlier run's manifest (`Test/corpus_results`).


## POINTS OF INTEREST

### ROM Data
NewtonScript ROM data is extracted from a ROM image file by ResMaker (the CMake target `ResMaker`). ResMaker writes it as C source: `ROMData/forNewton` for newtc and NCX, `ROMData/forNTK` for NTK.
This means we have access to all NewtonScript symbols, frames, functions, magic pointers, etc from the ROM (good) but they can not be changed (bad).
The goal is to recreate the NewtonScript sources to those assets so they can be built into a package by NTK and extracted by ResMaker into C source for linkage into the C world.
Never edit the generated files; change ResMaker instead. To run it, in `ROMData`:
`ResMaker rom.image -s RefStarSource.txt -sym 717006.cfunctions -o forNewton`. For NTK, add `-via ../Toolkit.exp -via ../Editor.exp` and use `-o forNTK`.

### Native functions
The Newton.exp file (Toolkit.exp for the NTK framework) defines native functions in the same way as the Newton C++ Tools for MPW.

### Virtual Memory
Some MMU functions have been implemented as an aid to understanding Newton memory management,
but there is of course no actual page faulting which leads to some memory management issues (stacks don't grow, VBOs aren't loaded when needed, etc).

### Memory Allocation
The original OS mixes NewPtr, malloc and free (and sometimes calls the wrong function to free that memory).
Here, all allocation (well, there are some exceptions) is done with a call to NewPtr/FreePtr (which replaces DisposPtr).
Memory is actually allocated by malloc/free (see FakePointers.cc) or new/delete.

### Stores
A flash store has been implemented.
This is used for VBOs as well as soup storage. It uses a memory-mapped file, so it is persistent (`Utilities/HostMemory.cc` does the mapping on each system).
It would be interesting to know whether the implementation is faithful enough to read a real Newton store image.

### Recognisers
Most high-level recogniser classes have been implemented, and there is a tap recogniser, but it needs more of the view system to be working to do anything useful.

### View System
QuickDraw calls are drawn through a few primitives in `Graphics/QDHost.cc`. They use FLTK when it is linked; otherwise they draw nothing. There is no Quartz or Cocoa code.
newtc's views open in FLTK windows (`Host/FLTK`).

### NewtonScript Compiler & Interpreter
The original NewtonScript compiler was built using yacc and flex, defined in NewtonScript.y and NewtonScript.l.
newtc compiles to the bytecode the Newton runs. It also decompiles packages and debugs NewtonScript: locally, or on a Newton or Einstein (`Matt/Toolkit Protocol.md`).

### Multitasking
Task switching is performed without using host platform tasks.
Each Newton task has its own registers, stack and data area which are switched by pseudo-SWI using assembler.
Only the MessagePad target uses task switching.

### Protocols
Protocol classes are built using inline assembler and hand-mangled function names.
Back in the day this process was automated, of course, but I never had the time or inclination to write the tools to do it.

### Communications
`newton_dock` has the MNP link, the serial and TCP transports, and DES for the Docking Protocol's password. newtc uses them for the Toolkit protocol; newtdump and NCX use them for docking.
On Windows the link and the transports are stubs for now.

### MPW Tools
The REx tool builds a valid ROM extension. Other MPW tools are work in progress.
