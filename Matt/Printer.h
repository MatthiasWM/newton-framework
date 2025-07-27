/*
 File:    Printer.h

 This class manages textual dividers and line indentation for text
 written to a std::ostream.

 Written by:  Matt, 2025.
 */

#ifndef MATT_PRINTER
#define MATT_PRINTER 1

#include "Newton.h"

#include <ostream>


class Printer {
  int wrapAt_ { 80 };
  int wrapAfter_ { 64 };
  int indent_ { 0 };
  int line_ { 0 };
  int column_ { 0 };
  int chars_ { 0 };
  using State = struct State {
    std::string separator_ { "" };
    bool deep_ { false };
    bool suppressSeparator_ { false };
    bool prevSuppressSeparator_ { true };
    bool firstItem_ { true };
    bool itemEmpty_ { true };
  };
  std::vector<State> stack_;

  void PrintSeparator();
  void PrintNewLine(int delta = 0);
  void DoStartItem();

public:
  std::ostream &out;

  Printer(std::ostream &oStream);

  void WrapAt(int column);
  void WrapAfter(int numChars);

  void StartList(const std::string &separator, int numCharsExpected=0);
  void DeepList(const std::string &separator = "");
  void Tag();
  void Item();
  void ItemDone();
  void EndList();
  void Finalize();

  void Print(const std::string &token);
  void Print(int value);
  void Print(double value);
  void Printf(const char*, ...);

  void PrintDivider(const std::string &text);
};

#endif // MATT_PRINTER
