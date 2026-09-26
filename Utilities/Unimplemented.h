/*
 File: Unimplemented.h

 Built-in functions that are not implemented yet (stubs), and what happens
 when a program calls one.

 A stub is written as

   NS_STUB(FGetSortID, RefArg rcvr)
   NS_STUB(FBatteryLevel, RefArg rcvr, RefArg inBatterySelector)

 which defines the function (same name and parameters, so the ROM's table
 of built-in functions finds it as before) and registers it at startup.
 When a program calls it, StubCalled() decides by the stub mode:

   log    (default) say so once per stub, then return nil
   throw  throw a NewtonScript exception, "FGetSortID is not implemented
          yet": fails at the call (with breakOnThrows, the debugger stops
          at the calling line)
   quiet  just return nil

 NS_STUB_NIL_OK is for stubs where nil is a fine answer for now (e.g. a
 sound that isn't played): they are counted, but never logged or thrown.
 CXX_STUB() marks a C++ function without a NewtonScript name; it is only
 logged (never thrown), and the function returns its own default.

 newtc: `-stubs log|throw|quiet|report` or the environment variable
 NEWTC_STUBS (e.g. "throw,report"); report prints at exit which stubs were
 called and how often, to decide what to implement next.
 */

#ifndef UNIMPLEMENTED_H
#define UNIMPLEMENTED_H

#include "Objects.h"

#include <cstdio>
#include <string>

/** One stub: registered when the program starts (NS_STUB), or on its first
    call (CXX_STUB). */
struct StubInfo
{
  const char * name;      // the C name, e.g. "FGetSortID"
  const char * file;      // where the stub is defined
  int line;
  bool nilIsOK;           // NS_STUB_NIL_OK: never logged or thrown
  bool cxx;               // CXX_STUB: never thrown
  long calls;
  char * message;         // "... is not implemented yet" (made on first use)
  StubInfo * next;        // all stubs, in a list
  StubInfo(const char * inName, const char * inFile, int inLine, bool inNilIsOK, bool inCxx = false);
};

/** A NewtonScript stub was called: log, throw, or nothing (see above).
    Returns nil (if it doesn't throw). */
Ref StubCalled(StubInfo & info);

/** A C++ stub was called: log (unless quiet). */
void StubNoted(StubInfo & info);

enum StubMode { kStubLog, kStubThrow, kStubQuiet };
void SetStubMode(StubMode inMode);
StubMode GetStubMode(void);

/** Print the report (stubs called, most calls first) at exit. */
void SetStubReport(bool inReport);
bool GetStubReport(void);

/** Set the mode and report from a text: "log", "throw", "quiet", "report",
    or several separated by commas. False if a word is unknown. */
bool SetStubOptions(const char * inText);

/** The report: which stubs were called, how often, out of how many. */
std::string StubReport(void);

/** Where the log lines go (a line of text with a newline); stderr if NULL.
    newtc -dap sends them to the Debug Console. */
extern void (*gStubNotify)(const char * inText);

#define NS_STUB(name, ...) \
  static StubInfo name##_stub(#name, __FILE__, __LINE__, false); \
  Ref name(__VA_ARGS__) { return StubCalled(name##_stub); }

#define NS_STUB_NIL_OK(name, ...) \
  static StubInfo name##_stub(#name, __FILE__, __LINE__, true); \
  Ref name(__VA_ARGS__) { return StubCalled(name##_stub); }

#define CXX_STUB() \
  do { static StubInfo sStub(__func__, __FILE__, __LINE__, false, true); StubNoted(sStub); } while (0)

#endif // UNIMPLEMENTED_H
