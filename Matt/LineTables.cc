/*
 File: LineTables.cc

 Source lines for NewtonScript functions. See LineTables.h.
 */

#include "Matt/LineTables.h"
#include "Matt/JSON.h"

#include "Frames/Frames.h"
#include "Frames/Compiler/Compiler.h"

#include <climits>
#include <cstdlib>
#include <map>
#include <string>

namespace {

Ref gFunctions = NILREF;   // every function compiled with a line table (GC root)

void RememberFunction(RefArg inFunction)
{
  RefVar functions(gFunctions);
  AddArraySlot(functions, inFunction);
  gFunctions = functions;
}

// The line table of fn, or nil.
Ref TableOf(RefArg fn)
{
  if (!IsFrame(fn))
    return NILREF;
  Ref table = GetFrameSlot(fn, MakeSymbol("lineTable"));
  return (IsArray(table) && Length(table) >= 3) ? table : NILREF;
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

} // namespace


void InstallLineTables(void)
{
  if (ISNIL(gFunctions)) {
    gFunctions = MakeArray(0);
    AddGCRoot(&gFunctions);
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
