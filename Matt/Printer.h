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
    int indentDelta_ { 0 };
    bool deep_ { false };
    bool freshLine_ { false };
    bool suppressSeparator_ { false };
    bool prevSuppressSeparator_ { true };
    bool firstItem_ { true };
    bool itemEmpty_ { true };
  };
  std::vector<State> stack_;
  // debug map: a statement about to be printed records the line its first
  // token lands on (see MarkNextItem)
  int pendingMark_ { -1 };
  std::vector<std::pair<int, int>> *pendingMarks_ { nullptr };

  void PrintSeparator();
  void Write(const std::string &text);   // everything goes out here

public:
  std::ostream &out;

  Printer(std::ostream &oStream);

  void WrapAt(int column);
  void WrapAfter(int numChars);

  void OffsetIndent(int delta);
  void SetIndent(int column);

  void StartList(const std::string &separator, int numCharsExpected=0);
  void DeepList(const std::string &separator = "");
  void Tag();
  void Item();
  void ItemDone();
  void EndList();
  void Trailer();

  void DoStartItem(bool newLine = false);
  void PrintNewLine();
  void Print(const std::string &token);
  void Print(int value);
  void Print(double value);
  void Printf(const char*, ...);
  void FreshLine();

  void PrintDivider(const std::string &text);

  /** The line (1-based) the next character goes to. */
  int Line() const { return line_ + 1; }
  /** When the next item starts, append (pc, its line) to marks. */
  void MarkNextItem(int pc, std::vector<std::pair<int, int>> *marks) {
    pendingMark_ = pc; pendingMarks_ = marks; }
};

#endif // MATT_PRINTER
