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

// NewtonScript functions, registered in newtc.cc:
//   LineOfPC(fn, pc) -> [file, line] or nil
//   CodeForLine(file, line) -> {line:, code: [[fn, pc], ...]} or nil
extern "C" Ref FLineOfPC(RefArg rcvr, RefArg inFn, RefArg inPC);
extern "C" Ref FCodeForLine(RefArg rcvr, RefArg inFile, RefArg inLine);

#endif // MATT_LINETABLES_H
