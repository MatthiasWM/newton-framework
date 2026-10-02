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
