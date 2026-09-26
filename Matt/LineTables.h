/*
 File: LineTables.h

 Source lines for NewtonScript functions (Matt/CLAUDE.md, Phase 7).

 Compiled with -g (global dbgKeepLineNumbers), a function has a slot
 lineTable = [lineTable: file, pc, line, pc, line, ...]: the absolute path
 of the source file, then one (pc, line) pair wherever the line changes,
 sorted by pc. The code from that pc up to the next pair belongs to the
 line. Closures made from a function share its instructions and carry the
 same table.

 This module keeps a list of all functions compiled with a line table
 (the compiler reports them through gCompiledFunctionHook) and answers the
 two questions a debugger has:
   - which line is this pc of this function on? (stack frames)
   - where does the code for this line of this file start? (breakpoints)
 */

#ifndef MATT_LINETABLES_H
#define MATT_LINETABLES_H

#include "Frames/Objects.h"

#include <string>

/** Start collecting the functions the compiler makes with a line table. */
void InstallLineTables(void);

/** [file, line] for pc of fn, or nil if fn has no line table. Code before
    the first entry (a prologue) belongs to the first line. */
Ref LineOfPC(RefArg fn, long pc);

/** Where the code of line `line` of `file` starts: {line: <the line
    used>, code: [[fn, pc], ...]}, one entry per function that has code
    on that line, at the lowest pc. If the line has no code, the next line
    that has code in a function spanning `line` is used (like debuggers
    move a breakpoint to the next statement), else the next line with code
    in the file (between top-level statements, each a function of its
    own). nil if nothing matches. */
Ref CodeForLine(RefArg file, long line);

/** Line stepping (the interpreter's gDebuggerStep, checked before every
    instruction while a step is active). Starting from pc of fn in the
    frame with index depth (0 = oldest, as in CNSDebugAPI):
    - over: stop at a new statement of that function (or at a statement
      start it jumps back to: once per loop iteration), in the caller once
      it returns (or an exception leaves it), or at the next top-level
      statement; calls (recursion included) run through;
    - in: like over, and also at the first statement of a called function
      that has a line table;
    - out: in the caller once the function returns, or at the next
      top-level statement.
    Code without a line table (C++, the NS Debug Tools) is run through. Any
    stop ends the step (CancelLineStep, e.g. from a breakpoint). */
enum LineStepKind { kLineStepNone, kLineStepOver, kLineStepIn, kLineStepOut };
void StartLineStep(LineStepKind kind, RefArg fn, long pc, long depth);
void CancelLineStep(void);

/** Debug maps (.nsdbg, written by -odecompile) for code whose binary must
    not change (packages): a hash of a function's instructions, to find it
    again (FNV-1a, 16 hex digits). */
std::string InstructionsHash(RefArg fn);

/** The path from root to target as a JSON array of slot names and array
    indexes ("null" if target isn't reachable, not counting magic pointers). */
std::string PathToObject(RefArg root, RefArg target);

/** Use table as the line table of fn (and every function with the same
    instructions) without changing fn (package objects may be read-only). */
void RegisterLineTable(RefArg fn, RefArg table);

/** Load a debug map (JSON text) for the package (or other object) root:
    each function is found by the hash of its instructions (if several
    have the same bytecode, by its path) and gets the map's line table.
    Returns the number of functions matched; *outTotal: in the map. */
int LoadDebugMap(RefArg root, const std::string &json, int *outTotal);

// NewtonScript functions, registered in newtc.cc:
//   LineOfPC(fn, pc) -> [file, line] or nil
//   CodeForLine(file, line) -> {line:, code: [[fn, pc], ...]} or nil
//   StartLineStep(kind, fn, pc, depth) -> true; kind 'over, 'in, 'out;
//     resume the program (ExitBreakLoop) to run the step
extern "C" Ref FLineOfPC(RefArg rcvr, RefArg inFn, RefArg inPC);
extern "C" Ref FCodeForLine(RefArg rcvr, RefArg inFile, RefArg inLine);
extern "C" Ref FStartLineStep(RefArg rcvr, RefArg inKind, RefArg inFn, RefArg inPC, RefArg inDepth);

#endif // MATT_LINETABLES_H
