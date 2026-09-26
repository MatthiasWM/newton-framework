/*
 File: Unimplemented.cc

 What happens when a program calls a built-in function that is not
 implemented yet. See Unimplemented.h.
 */

#include "Unimplemented.h"
#include "Frames/Frames.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static StubInfo * gStubs = nullptr;     // all stubs (constant-initialized, so
                                        // safe while static constructors run)
static StubMode gStubMode = kStubLog;
static bool gStubReport = false;
void (*gStubNotify)(const char * inText) = nullptr;


StubInfo::StubInfo(const char * inName, const char * inFile, int inLine, bool inNilIsOK, bool inCxx)
: name(inName), file(inFile), line(inLine), nilIsOK(inNilIsOK), cxx(inCxx),
  calls(0), message(nullptr), next(gStubs)
{
  gStubs = this;
}


static const char *
FileName(const char * inPath)
{
  const char * slash = strrchr(inPath, '/');
  return slash ? slash + 1 : inPath;
}


static const char *
Message(StubInfo & info)
{
  if (info.message == nullptr) {
    std::string text = std::string(info.name) + " is not implemented yet (stub in "
                     + FileName(info.file) + ":" + std::to_string(info.line) + ")";
    info.message = strdup(text.c_str());
  }
  return info.message;
}


static void
Log(StubInfo & info, const char * inResult)
{
  std::string text = std::string("newtc: ") + Message(info) + ", " + inResult + ".\n";
  if (gStubNotify)
    gStubNotify(text.c_str());
  else {
    fflush(stdout);   // keep the order in a terminal
    fputs(text.c_str(), stderr);
  }
}


Ref
StubCalled(StubInfo & info)
{
  ++info.calls;
  if (info.nilIsOK || gStubMode == kStubQuiet)
    return NILREF;
  if (gStubMode == kStubThrow)
    ThrowMsg(Message(info));    // keeps the pointer: the message stays
  if (info.calls == 1)
    Log(info, "returns nil");
  return NILREF;
}


void
StubNoted(StubInfo & info)
{
  ++info.calls;
  if (gStubMode != kStubQuiet && info.calls == 1)
    Log(info, "does nothing");
}


void SetStubMode(StubMode inMode) { gStubMode = inMode; }
StubMode GetStubMode(void) { return gStubMode; }


static bool gReportAtExit = false;

static void
PrintReportAtExit(void)
{
  if (gStubReport) {
    fflush(stdout);
    fputs(StubReport().c_str(), stderr);
  }
}


void
SetStubReport(bool inReport)
{
  if (inReport && !gReportAtExit) {
    atexit(PrintReportAtExit);
    gReportAtExit = true;
  }
  gStubReport = inReport;
}


bool GetStubReport(void) { return gStubReport; }


bool
SetStubOptions(const char * inText)
{
  std::string text(inText);
  size_t start = 0;
  while (start <= text.size()) {
    size_t comma = text.find(',', start);
    std::string word = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    if (word == "log")
      SetStubMode(kStubLog);
    else if (word == "throw")
      SetStubMode(kStubThrow);
    else if (word == "quiet")
      SetStubMode(kStubQuiet);
    else if (word == "report")
      SetStubReport(true);
    else if (!word.empty())
      return false;
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  return true;
}


std::string
StubReport(void)
{
  std::vector<StubInfo *> called;
  int total = 0;
  for (StubInfo * info = gStubs; info; info = info->next) {
    if (!info->cxx)
      ++total;
    if (info->calls > 0)
      called.push_back(info);
  }
  std::stable_sort(called.begin(), called.end(),
                   [](StubInfo * a, StubInfo * b) { return a->calls > b->calls; });
  char line[256];
  snprintf(line, sizeof(line), "newtc: %d of %d stubs called%s\n", (int)called.size(), total,
           called.empty() ? "." : ":");
  std::string text = line;
  for (StubInfo * info : called) {
    snprintf(line, sizeof(line), "  %8ld  %-32s %s:%d%s\n", info->calls, info->name,
             FileName(info->file), info->line,
             info->nilIsOK ? "  (nil is ok)" : info->cxx ? "  (C++)" : "");
    text += line;
  }
  return text;
}
