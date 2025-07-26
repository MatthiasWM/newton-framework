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
public:
  std::ostream &out;

  Printer(std::ostream &oStream);
  
  void Indent(int delta=0) { indent_ += delta; }
  void ClearDivider() { dividerPending_ = nullptr; }
  void Begin(int delta=0) {
    indent_ += delta;
    if (dividerPending_) { printf("%s", dividerPending_); dividerPending_ = nullptr; }
    if (indentPending_) { puts(""); for (int i=0; i<indent_; i++) printf("  "); indentPending_ = false; }
  }
  void NewLine(const char *div, int delta=0) { dividerPending_ = div; indentPending_ = true; indent_ += delta; }
  void NewLine(int delta=0) { NewLine(nullptr, delta); }
  void Divider(const char *div) { dividerPending_ = div; }  // Should this reset indentPending_?
  void End(int delta=0) { indent_ += delta; } // Should this reset textPending_ and indentPending_?
protected:
  int indent_ { 0 };
  bool indentPending_ { false };
  const char *dividerPending_ { nullptr };
};

#endif // MATT_PRINTER
