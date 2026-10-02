# Bugs in the ROM

Bugs found in Apple's code while rebuilding it from source (R6), so they can
be fixed later. The sources reproduce the ROM byte for byte, so each bug is
in the source as it is in the ROM; a fix changes the image. Each entry says
where it is (ROM address, function, our source), what the code does, what
it was probably meant to do, and how sure we are.

Status: **confirmed** (the code cannot do what it seems meant to) or
**possible** (depends on how other code behaves; to check).

## B1 Flash driver: erase-suspend test is always false (confirmed)

- ROM: `T28F016_SA_SVDriver::StartReadingArray`, 0x203F98..0x203FD8
  (MP2x00 US 2.1, 717006).
- Source: `src/Stores/T28F016_SA_SVDriver.cc`, `StartReadingArray`, the
  test after the suspend command (`none = !lanes`).
- What it does: to read the flash while an erase is under way, the driver
  suspends the erase (command 0xB0), waits for the chip, and tests the
  status register for "erase suspended" (bit 6, 0x40 in each lane):
  `(!lanes & status & lanes & 0x40404040)`. `!lanes` is 0 for any lanes the
  driver uses (it is 1 only when there are no lanes), so the whole
  expression is always 0: the code always takes the branch for "erase
  suspended" and later resumes it (`DoneReadingArray` sends 0xD0).
- Probably meant: `!(status & lanes & 0x40404040)`: the `!` applied to the
  whole test, not to `lanes`. Then, when the erase had already finished
  before the suspend took effect (bit 6 clear), the driver would mark the
  erase as no longer under way and not suspended, and set the flag that
  makes the next block command send its confirm (0xD0) twice.
- Effect: after an erase finishes while it is being suspended, the driver
  still believes it suspended it and later sends a resume (0xD0) to a chip
  that has nothing to resume, and keeps `fEraseUnderway` set. Whether the
  chips mind a stray 0xD0 is not known; the window is small (the erase must
  end between the suspend command and the status read).
- Fix: `if (!(*address & lanes & 0x40404040))` with the two branches
  swapped as the code has them (or the intended meaning checked against
  the chip's data sheet first).

## B2 ExtendedGestalt: a failed second allocation is not noticed (possible)

- ROM: `ExtendedGestalt`, 0x201CE4..0x201DDC.
- Source: `src/OS/SystemNatives.cc`, `ExtendedGestalt`, the block after
  `malloc(size)`.
- What it does: it calls Gestalt with a block of `gParmBlockSize` bytes; if
  Gestalt reports a larger size, it frees the block, allocates a larger one
  and calls Gestalt again. If that second `malloc` fails, `parmBlock` is
  NULL but `err` still holds the first call's result; if that was `noErr`,
  it calls `ConstructReturnValue(NULL, ...)`, which reads from address 0,
  then `free(NULL)`.
- Depends on: whether Gestalt returns `noErr` when the block is too small
  (and only reports the size needed). If it returns an error then, the
  second call's failure path is harmless (`err` is set, the result is nil).
- Fix: `if (parmBlock != NULL) err = ...; else err = kError_No_Memory;` (or
  skip to the free when `parmBlock` is NULL).

## B3 FMinimumBatteryCheck: busy loop when the battery status fails (possible)

- ROM: `FMinimumBatteryCheck`, 0x2019A0..0x201A0C.
- Source: `src/OS/SystemNatives.cc`, `FMinimumBatteryCheck`.
- What it does: it loops until the battery is good enough or the Newton is
  on AC power, sleeping (`SleepUntilNextWakeup`) while it is not. When
  `GetBatteryStatus` fails, it neither sleeps nor leaves the loop: it asks
  again at once, for as long as the power manager keeps failing.
- Depends on: whether `GetBatteryStatus` can fail for long (the power
  manager not running, an RPC error).
- Fix: leave the loop (or sleep) when the status cannot be read.

## B4 Fax decoder: a byte is lost after the ring runs empty (confirmed)

- ROM: `TT4FaxLine::GetNextBit`, 0x204940..0x204A00.
- Source: `src/Communications/Fax/T4FaxLine.cc`, `GetNextBit`, the first
  test (`fGet++`).
- What it does: `fGet` is meant to point at the byte last read: before
  reading, the decoder moves it on unless the ring is empty (`fGet ==
  fPut` and the writer not a lap ahead), then throws
  `exFaxBufOverrunException` if the ring is empty. But in two states
  `fGet` points at a byte not yet read: after `Reset` (both at the
  ring's start), and after an underflow (the reader moved onto `fPut`,
  then threw). The next byte `AppendTo` puts there is then skipped: the
  reader moves past it before reading.
- Effect: the first byte received after `Init`/`Reset`, and the first
  byte received after every underflow, are never decoded. After an
  underflow `DecodeLine` (with `inCatchOverrun`) gives up the line and
  `SkipPastEOL` looks for the next end of line, which hides most of it;
  a lost byte at a page's start is likely in the leading end of line
  or fill. A lost byte inside a line spoils that line.
- Fix: keep `fGet` at the next byte to read (test for empty before
  reading, move on after), or set a flag when `fGet` points at an unread
  byte (after `Reset` and before throwing).

## B5 Fax decoder: DecodeLine's end-of-page test can never be true (possible)

- ROM: `TT4FaxLine::DecodeLine`, 0x204BC4..0x204CEC.
- Source: `src/Communications/Fax/T4FaxLine.cc`, `DecodeLine`, the test
  after the loop (`inLineBytes == 0`).
- What it does: lines without pixels (an end of line right away) are
  skipped, up to 7 in a row (`tries`). Afterwards, if the bytes decoded
  are not the line's size, the result is `tries >= 6 && inLineBytes ==
  0`: the size the caller passes, which is never 0, so the result is
  always false there.
- Probably meant: `outBytes == 0` (only empty lines: the end of the page,
  RTC, six ends of line), so that the end of a page is told apart from a
  bad line.
- Depends on: what the fax tool does with the result (it may find the
  page's end another way).
- Fix: `tries >= 6 && outBytes == 0`, after checking the callers.

## B6 TAgentReporter: the base class's destructor runs twice (harmless)

- ROM: `TAgentReporter::~TAgentReporter`, 0x206898..0x2068D4.
- Source: `src/Testing/AgentReporter.cc`, the destructor.
- What it does: its body calls `TTestReporter::~TTestReporter()`
  explicitly; the compiler then calls it again, as it does for every base
  class at the end of a destructor.
- Effect: none today: `TTestReporter`'s destructor (0x22B46C) does
  nothing but free the object when asked to (flag 1), and both calls pass
  0. It would free or release things twice if that destructor ever did
  more.
- Fix: leave the body empty.

## B7 TAgentReporter::AgentReportStatus: copies 224 bytes from a string of any length (harmless)

- ROM: `TAgentReporter::AgentReportStatus`, 0x206948..0x206B8C, the
  default case.
- Source: `src/Testing/AgentReporter.cc`, `BlockMove(name, buf, ...)`.
- What it does: for a status it does not know, it sends the test's name
  as the message, copied with `BlockMove(name, buf, 224)`: 224 bytes
  whatever the name's length (the empty string "" when the name is
  NULL).
- Effect: it reads past the end of the name; the copy ends with the
  name's terminating zero, so the message is right, unless the name is
  longer than 223 characters (then it is not terminated) or lies within
  224 bytes of the end of mapped memory.
- Fix: `strncpy(buf, name, sizeof(buf) - 1)` and a terminating zero.
