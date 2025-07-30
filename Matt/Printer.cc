/*
 File:    ObjectPrinter.cc

 This class manages textual dividers and line indentation for text
 written to a std::ostream.

 Written by:  Matt, 2025.
 */

#include "Matt/Printer.h"

#include <stdarg.h>
#include <cassert>

/**
 * @class Printer
 * @brief A base class for text formatting and indentation.
 *
 */

Printer::Printer(std::ostream &oStream)
: out(oStream)
{
  // TODO: remove this when everything in ObjectPrinter and Decompiler uses Printer.out .
  std::ios::sync_with_stdio(true);
  State initialState;
  stack_.push_back(initialState);
}

void Printer::WrapAt(int column)
{
  wrapAt_ = column;
}

void Printer::WrapAfter(int numChars)
{
  wrapAfter_ = numChars;
}

void Printer::Offset(int delta) {
  State &state = stack_.back();
  state.indentDelta_ += delta;
}

void Printer::SetIndent(int n) {
  State &state = stack_.back();
  state.indentDelta_ = (int)-stack_.size() + n + 1;
}

void Printer::PrintSeparator()
{
  State &state = stack_.back();
  out << state.separator_;
//  if (!state.deep_) out << " ";
}

void Printer::PrintNewLine()
{
  State &state = stack_.back();
  out << std::endl;
  int i = (int)stack_.size() + state.indentDelta_ -1;
  if (i > 0)
    for ( ; i > 0; --i) out << "  ";
}

void Printer::DoStartItem()
{
  State &state = stack_.back();
  if (!state.prevSuppressSeparator_) PrintSeparator();
  if (!state.firstItem_ && !state.deep_)
    out << " ";
  if (state.deep_)
    PrintNewLine();
  state.prevSuppressSeparator_ = state.suppressSeparator_;
  state.firstItem_ = false;
}

void Printer::StartList(const std::string &separator, int numCharsExpected)
{
  State &state = stack_.back();
  State newState;
  newState.separator_ = separator;
  newState.indentDelta_ = state.indentDelta_;
  if (numCharsExpected >= wrapAfter_) newState.deep_ = true;
  stack_.push_back(newState);
}

void Printer::DeepList(const std::string &separator)
{
  StartList(separator, wrapAfter_);
}

void Printer::Tag()
{
  State &state = stack_.back();
  state.itemEmpty_ = true;
  state.suppressSeparator_ = true;
}

void Printer::Item()
{
  State &state = stack_.back();
  state.itemEmpty_ = true;
  state.suppressSeparator_ = false;
}

void Printer::ItemDone()
{
  Item();
}

void Printer::EndList()
{
  assert(stack_.size()); // Called EndList() without calling StartList()
  stack_.pop_back();
}

void Printer::Trailer()
{
  State &state = stack_.back();
  state.indentDelta_--;
  state.firstItem_ = true;
  state.itemEmpty_ = true;
  state.prevSuppressSeparator_ = true;
}

void Printer::Print(const std::string &token)
{
  State &state = stack_.back();
  if (state.itemEmpty_) {
    DoStartItem();
    state.itemEmpty_ = false;
  }
  out << token;
}

void Printer::Print(int value)
{
  State &state = stack_.back();
  if (state.itemEmpty_) {
    DoStartItem();
    state.itemEmpty_ = false;
  }
  out << value;;
}

void Printer::Print(double value)
{
  State &state = stack_.back();
  if (state.itemEmpty_) {
    DoStartItem();
    state.itemEmpty_ = false;
  }
  out << value;;
}

void Printer::Printf(const char *format, ...) {
  State &state = stack_.back();
  if (state.itemEmpty_) {
    DoStartItem();
    state.itemEmpty_ = false;
  }
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

void Printer::PrintDivider(const std::string &text)
{
  // TODO: untested
  State &state = stack_.back();
  if (state.itemEmpty_) {
    DoStartItem();
    state.itemEmpty_ = false;
  }
  out << std::endl;
  if (text.empty()) {
    for (int i = wrapAt_; i > 0; --i) out << "-";
  } else {
    int n = (wrapAt_ - (int)text.length() - 2) / 2;
    for (int i = n; i > 0; --i) out << "-";
    out << " " << text << " ";
    for (int i = n; i > 0; --i) out << "-";
  }
  out << std::endl;
}






