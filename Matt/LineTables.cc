/*
 File: LineTables.cc

 Source lines for NewtonScript functions. See LineTables.h.
 */

#include "Matt/LineTables.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"
#include "Frames/Interpreter.h"
#include "Frames/Iterators.h"
#include "Frames/Compiler/Compiler.h"
#include "ROMResources.h"

#include <climits>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

Ref gFunctions = NILREF;   // every function compiled with a line table (GC root)
Ref gRegistered = NILREF;  // [[instructions, table], ...] from debug maps (GC root)
Ref gCacheInstructions = NILREF;  // the last registry lookup (GC roots)
Ref gCacheTable = NILREF;

void RememberFunction(RefArg inFunction)
{
  RefVar functions(gFunctions);
  AddArraySlot(functions, inFunction);
  gFunctions = functions;
}

// The line table of fn, or nil: its own (compiled with -g), else one
// registered for its instructions (a debug map, -nsdbg).
Ref TableOf(RefArg fn)
{
  if (!IsFrame(fn))
    return NILREF;
  Ref table = GetFrameSlot(fn, MakeSymbol("lineTable"));
  if (IsArray(table) && Length(table) >= 3)
    return table;
  if (ISNIL(gRegistered) || Length(gRegistered) == 0)
    return NILREF;
  Ref instructions = GetFrameSlot(fn, SYMA(instructions));
  if (ISNIL(instructions))
    return NILREF;
  if (EQ(instructions, gCacheInstructions))
    return gCacheTable;
  for (ArrayIndex i = 0, n = Length(gRegistered); i < n; ++i) {
    Ref pair = GetArraySlot(gRegistered, i);
    if (EQ(GetArraySlot(pair, 0), instructions)) {
      gCacheInstructions = instructions;
      gCacheTable = GetArraySlot(pair, 1);
      return gCacheTable;
    }
  }
  return NILREF;
}

long EntryPC(RefArg table, ArrayIndex i)   { return RINT(GetArraySlot(table, 1 + 2 * i)); }
long EntryLine(RefArg table, ArrayIndex i) { return RINT(GetArraySlot(table, 2 + 2 * i)); }

// A path in canonical form (symbolic links resolved), for comparing the
// file names in line tables with those a debugger client sends.
std::string CanonicalPath(const std::string &path)
{
  char resolved[PATH_MAX];
  if (!path.empty() && realpath(path.c_str(), resolved) != nullptr)
    return resolved;
  return path;
}

/*------------------------------------------------------------------------------
  Where a pc is in a line table: the line, and whether a line's code starts
  exactly at pc (a statement start). False if fn has no line table.
------------------------------------------------------------------------------*/

bool LineAt(RefArg fn, long pc, long *outLine, bool *outAtStart)
{
  RefVar table(TableOf(fn));
  if (ISNIL(table))
    return false;
  ArrayIndex count = (Length(table) - 1) / 2;
  ArrayIndex low = 0, high = count;
  while (low < high) {
    ArrayIndex middle = (low + high) / 2;
    if (EntryPC(table, middle) <= pc)
      low = middle + 1;
    else
      high = middle;
  }
  ArrayIndex entry = (low == 0) ? 0 : low - 1;
  *outLine = EntryLine(table, entry);
  *outAtStart = (low > 0 && EntryPC(table, entry) == pc);
  return true;
}

/*------------------------------------------------------------------------------
  Line stepping. The interpreter asks StepCheck() before every instruction
  (gDebuggerStep) while a step is active.
------------------------------------------------------------------------------*/

LineStepKind gStepKind = kLineStepNone;
Ref gStepInstructions = NILREF;   // the start function's code (GC root)
long gStepDepth = 0;              // the start frame's index
long gStepLine = 0;               // the start line
long gStepLastPC = 0;             // the last pc seen in the start function
long gStepStartPC = 0;
bool gStepResuming = false;       // not back at the start point yet

bool StepCheck(RefArg fn, long pc, long depth)
{
  if (gStepKind == kLineStepNone)
    return false;
  bool sameFunction = IsFrame(fn)
    && EQ(GetFrameSlot(fn, SYMA(instructions)), gStepInstructions);
  if (gStepResuming) {
    // The debugger's own code (the break loop, the request handler) runs in
    // deeper frames until the program is back where it stopped: nothing to
    // check there. Back in the start frame, the instruction we stopped at
    // may be checked once more (a stop at a breakpoint) or not (a stop from
    // this check): skip it, then check as usual.
    if (depth > gStepDepth)
      return false;
    gStepResuming = false;
    if (depth == gStepDepth && sameFunction && pc == gStepStartPC)
      return false;
  }
  long line;
  bool atStart;
  bool hasLines = LineAt(fn, pc, &line, &atStart);
  bool stop = false;

  if (depth < gStepDepth)
    // left the start function (return, exception): stop in the caller,
    // wherever it continues; past code without lines (C++, the tools)
    stop = hasLines;
  else if (depth == gStepDepth && !sameFunction)
    // the next top-level statement (each is a function of its own)
    stop = hasLines && atStart;
  else if (depth == gStepDepth) {
    // in the start function: a new statement, or back to a statement
    // start (a loop)
    if (gStepKind != kLineStepOut)
      stop = atStart && (line != gStepLine || pc <= gStepLastPC);
    gStepLastPC = pc;
  }
  else if (gStepKind == kLineStepIn)
    // deeper: a called function, at its first statement
    stop = hasLines && atStart;

  if (stop)
    CancelLineStep();
  return stop;
}

} // namespace


void StartLineStep(LineStepKind kind, RefArg fn, long pc, long depth)
{
  long line;
  bool atStart;
  if (!LineAt(fn, pc, &line, &atStart))
    line = 0;
  gStepKind = kind;
  gStepInstructions = IsFrame(fn) ? GetFrameSlot(fn, SYMA(instructions)) : NILREF;
  gStepDepth = depth;
  gStepLine = line;
  gStepLastPC = pc;
  gStepStartPC = pc;
  gStepResuming = true;
  gDebuggerStep = StepCheck;
}


void CancelLineStep(void)
{
  gStepKind = kLineStepNone;
  gStepInstructions = NILREF;
  gDebuggerStep = NULL;
}


Ref FStartLineStep(RefArg rcvr, RefArg inKind, RefArg inFn, RefArg inPC, RefArg inDepth)
{
  LineStepKind kind = EQ(inKind, MakeSymbol("in")) ? kLineStepIn
                    : EQ(inKind, MakeSymbol("out")) ? kLineStepOut
                    : kLineStepOver;
  if (!ISINT(inPC) || !ISINT(inDepth))
    return NILREF;
  StartLineStep(kind, inFn, RINT(inPC), RINT(inDepth));
  return TRUEREF;
}


void InstallLineTables(void)
{
  if (ISNIL(gFunctions)) {
    gFunctions = MakeArray(0);
    AddGCRoot(&gFunctions);
    AddGCRoot(&gStepInstructions);
    gRegistered = MakeArray(0);
    AddGCRoot(&gRegistered);
    AddGCRoot(&gCacheInstructions);
    AddGCRoot(&gCacheTable);
  }
  gCompiledFunctionHook = RememberFunction;
}


Ref LineOfPC(RefArg fn, long pc)
{
  RefVar table(TableOf(fn));
  if (ISNIL(table))
    return NILREF;
  // the last entry whose pc is <= pc
  ArrayIndex count = (Length(table) - 1) / 2;
  ArrayIndex low = 0, high = count;
  while (low < high) {
    ArrayIndex middle = (low + high) / 2;
    if (EntryPC(table, middle) <= pc)
      low = middle + 1;
    else
      high = middle;
  }
  // code before the first entry (e.g. copying arguments a closure uses
  // into the argFrame) belongs to the first line
  ArrayIndex entry = (low == 0) ? 0 : low - 1;
  RefVar result(MakeArray(2));
  SetArraySlot(result, 0, GetArraySlot(table, 0));
  SetArraySlot(result, 1, MAKEINT(EntryLine(table, entry)));
  return result;
}


Ref CodeForLine(RefArg file, long line)
{
  if (ISNIL(gFunctions) || !IsString(file))
    return NILREF;
  std::string wanted = CanonicalPath(UTF8FromString(file));
  std::map<std::string, bool> sameFile;   // file name -> is it `file`?

  // the functions of this file
  RefVar functions(MakeArray(0));
  for (ArrayIndex i = 0, n = Length(gFunctions); i < n; ++i) {
    RefVar fn(GetArraySlot(gFunctions, i));
    RefVar table(TableOf(fn));
    if (ISNIL(table))
      continue;
    RefVar name(GetArraySlot(table, 0));
    if (!IsString(name))
      continue;
    std::string nameText = UTF8FromString(name);
    auto known = sameFile.find(nameText);
    bool matches;
    if (known != sameFile.end())
      matches = known->second;
    else
      matches = sameFile[nameText] = (CanonicalPath(nameText) == wanted);
    if (matches)
      AddArraySlot(functions, fn);
  }

  // The line to use: `line` if some function has code there, else the
  // next line with code in a function whose lines span `line`, else (a
  // line between top-level statements, each a function of its own) the
  // next line with code in the file.
  long useLine = LONG_MAX, nextInFile = LONG_MAX;
  for (ArrayIndex i = 0, n = Length(functions); i < n; ++i) {
    RefVar table(TableOf(GetArraySlot(functions, i)));
    ArrayIndex count = (Length(table) - 1) / 2;
    long first = LONG_MAX, last = LONG_MIN, next = LONG_MAX;
    for (ArrayIndex e = 0; e < count; ++e) {
      long entryLine = EntryLine(table, e);
      if (entryLine < first) first = entryLine;
      if (entryLine > last) last = entryLine;
      if (entryLine >= line && entryLine < next) next = entryLine;
    }
    if (line >= first && line <= last && next < useLine)
      useLine = next;
    if (next < nextInFile)
      nextInFile = next;
  }
  if (useLine == LONG_MAX)
    useLine = nextInFile;
  if (useLine == LONG_MAX)
    return NILREF;

  // where each function's code for that line starts
  RefVar code(MakeArray(0));
  for (ArrayIndex i = 0, n = Length(functions); i < n; ++i) {
    RefVar fn(GetArraySlot(functions, i));
    RefVar table(TableOf(fn));
    ArrayIndex count = (Length(table) - 1) / 2;
    for (ArrayIndex e = 0; e < count; ++e)
      if (EntryLine(table, e) == useLine) {
        RefVar entry(MakeArray(2));
        SetArraySlot(entry, 0, fn);
        SetArraySlot(entry, 1, MAKEINT(EntryPC(table, e)));
        AddArraySlot(code, entry);
        break;
      }
  }
  RefVar result(AllocateFrame());
  SetFrameSlot(result, MakeSymbol("line"), MAKEINT(useLine));
  SetFrameSlot(result, MakeSymbol("code"), code);
  return result;
}


Ref FLineOfPC(RefArg rcvr, RefArg inFn, RefArg inPC)
{
  return ISINT(inPC) ? LineOfPC(inFn, RINT(inPC)) : NILREF;
}


Ref FCodeForLine(RefArg rcvr, RefArg inFile, RefArg inLine)
{
  return ISINT(inLine) ? CodeForLine(inFile, RINT(inLine)) : NILREF;
}


/*------------------------------------------------------------------------------
  Debug maps (.nsdbg) for code whose binary must not change (packages).
------------------------------------------------------------------------------*/

std::string InstructionsHash(RefArg fn)
{
  Ref instructions = IsFrame(fn) ? GetFrameSlot(fn, SYMA(instructions)) : NILREF;
  if (!IsBinary(instructions))
    return "";
  // FNV-1a, 64 bits
  uint64_t hash = 0xcbf29ce484222325ULL;
  const unsigned char *bytes = (const unsigned char *)BinaryData(instructions);
  for (ArrayIndex i = 0, n = Length(instructions); i < n; ++i) {
    hash ^= bytes[i];
    hash *= 0x100000001b3ULL;
  }
  char text[17];
  snprintf(text, sizeof(text), "%016llx", (unsigned long long)hash);
  return text;
}


void RegisterLineTable(RefArg fn, RefArg table)
{
  RefVar pair(MakeArray(2));
  SetArraySlot(pair, 0, GetFrameSlot(fn, SYMA(instructions)));
  SetArraySlot(pair, 1, table);
  RefVar registered(gRegistered);
  AddArraySlot(registered, pair);
  gRegistered = registered;
  RememberFunction(fn);   // for CodeForLine
}


namespace {

// Every NewtonScript function reachable from obj (not through magic
// pointers into the ROM). Allocates nothing on the NewtonScript heap while
// walking, so the visited set of Refs stays valid.
void CollectFunctions(Ref obj, std::set<Ref> &visited, std::vector<RefVar> &found)
{
  if (ISMAGICPTR(obj) || !ISREALPTR(obj) || !(IsFrame(obj) || IsArray(obj)))
    return;
  if (!visited.insert(obj).second)
    return;
  if (IsFrame(obj) && !ISNIL(GetFrameSlot(obj, SYMA(instructions)))
   && IsBinary(GetFrameSlot(obj, SYMA(instructions))))
    found.push_back(RefVar(obj));
  RefVar objVar(obj);
  CObjectIterator iter(objVar, false);
  for ( ; !iter.done(); iter.next())
    CollectFunctions(iter.value(), visited, found);
}

// The path from obj to target (slot names, array indexes), depth first, not
// through magic pointers. Allocates nothing while walking.
bool FindPath(Ref obj, Ref target, std::set<Ref> &visited, std::vector<Ref> &path)
{
  if (EQ(obj, target))
    return true;
  if (ISMAGICPTR(obj) || !ISREALPTR(obj) || !(IsFrame(obj) || IsArray(obj)))
    return false;
  if (!visited.insert(obj).second)
    return false;
  RefVar objVar(obj);
  CObjectIterator iter(objVar, false);
  for ( ; !iter.done(); iter.next()) {
    path.push_back(iter.tag());
    if (FindPath(iter.value(), target, visited, path))
      return true;
    path.pop_back();
  }
  return false;
}

// Follow a path (JSON array: slot names, array indexes) from root.
Ref ObjectAtPath(RefArg root, RefArg path)
{
  RefVar obj(root);
  if (!IsArray(path))
    return NILREF;
  for (ArrayIndex i = 0, n = Length(path); i < n; ++i) {
    Ref step = GetArraySlot(path, i);
    if (ISINT(step) && IsArray(obj) && RINT(step) >= 0 && (ArrayIndex)RINT(step) < Length(obj))
      obj = GetArraySlot(obj, RINT(step));
    else if (IsString(step) && IsFrame(obj))
      obj = GetFrameSlot(obj, MakeSymbol(UTF8FromString(step).c_str()));
    else
      return NILREF;
  }
  // a binCFunction frame with NewtonScript code: the decompiler printed bcFunc
  if (IsFrame(obj) && NOTNIL(GetFrameSlot(obj, MakeSymbol("bcFunc"))))
    obj = GetFrameSlot(obj, MakeSymbol("bcFunc"));
  return obj;
}

} // namespace


std::string PathToObject(RefArg root, RefArg target)
{
  std::set<Ref> visited;
  std::vector<Ref> path;
  if (!FindPath(root, target, visited, path))
    return "null";
  std::string json = "[";
  for (size_t i = 0; i < path.size(); ++i) {
    if (i > 0)
      json += ",";
    if (ISINT(path[i]))
      json += std::to_string(RINT(path[i]));
    else if (IsSymbol(path[i]))
      json += QuoteJSON(SymbolName(path[i]));
    else
      json += "null";
  }
  return json + "]";
}


int LoadDebugMap(RefArg root, const std::string &json, int *outTotal)
{
  RefVar map(ParseJSON(json.data(), json.size()));
  RefVar source(GetFrameSlot(map, MakeSymbol("source")));
  RefVar entries(GetFrameSlot(map, MakeSymbol("functions")));
  if (!IsArray(entries))
    ThrowMsg("not a debug map (no \"functions\")");

  // the functions of the package, by hash
  std::set<Ref> visited;
  std::vector<RefVar> functions;
  CollectFunctions(root, visited, functions);
  std::map<std::string, std::vector<size_t>> byHash;
  for (size_t i = 0; i < functions.size(); ++i)
    byHash[InstructionsHash(functions[i])].push_back(i);

  int matched = 0;
  ArrayIndex count = Length(entries);
  for (ArrayIndex e = 0; e < count; ++e) {
    RefVar entry(GetArraySlot(entries, e));
    RefVar hash(GetFrameSlot(entry, MakeSymbol("hash")));
    RefVar lines(GetFrameSlot(entry, MakeSymbol("lines")));
    if (!IsString(hash) || !IsArray(lines))
      continue;
    auto candidates = byHash.find(UTF8FromString(hash));
    if (candidates == byHash.end())
      continue;
    // the same bytecode in several places: the path decides
    RefVar fn;
    if (candidates->second.size() == 1)
      fn = functions[candidates->second[0]];
    else {
      RefVar atPath(ObjectAtPath(root, GetFrameSlot(entry, MakeSymbol("path"))));
      for (size_t index : candidates->second)
        if (EQ(functions[index], atPath))
          fn = atPath;
    }
    if (ISNIL(fn))
      continue;
    RefVar table(AllocateArray(MakeSymbol("lineTable"), 1));
    SetArraySlot(table, 0, source);
    for (ArrayIndex i = 0, n = Length(lines); i < n; ++i)
      AddArraySlot(table, GetArraySlot(lines, i));
    RegisterLineTable(fn, table);
    ++matched;
  }
  if (outTotal)
    *outTotal = (int)count;
  return matched;
}
