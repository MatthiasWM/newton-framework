/*
 File:    ObjectPrinter.cc

 This class manages textual dividers and line indentation for text
 written to a std::ostream.

 Written by:  Matt, 2025.
 */

#include "Matt/Printer.h"

#include <stdarg.h>
#include <cassert>
#include <sstream>

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

void Printer::OffsetIndent(int delta) {
  State &state = stack_.back();
  state.indentDelta_ += delta;
}

void Printer::SetIndent(int n) {
  State &state = stack_.back();
  state.indentDelta_ = (int)-stack_.size() + n + 1;
}

// All text goes out here, so the line count (Line(), debug maps) is right.
void Printer::Write(const std::string &text)
{
  out << text;
  for (char c : text)
    if (c == '\n')
      ++line_;
}

void Printer::PrintSeparator()
{
  State &state = stack_.back();
  Write(state.separator_);
//  if (!state.deep_) out << " ";
}

void Printer::PrintNewLine()
{
  State &state = stack_.back();
  Write("\n");
  out.flush();
  int i = (int)stack_.size() + state.indentDelta_ -1;
  if (i > 0)
    for ( ; i > 0; --i) Write("  ");
}

void Printer::DoStartItem(bool newLine)
{
  State &state = stack_.back();
  if (!state.freshLine_) {
    if (!state.prevSuppressSeparator_) PrintSeparator();
    if (!state.firstItem_ && !state.deep_)
      Write(" ");
  }
  if (state.deep_ || state.freshLine_ || newLine)
    PrintNewLine();
  if (pendingMarks_ != nullptr) {
    if (pendingMark_ >= 0)
      pendingMarks_->push_back({pendingMark_, Line()});
    pendingMarks_ = nullptr;
    pendingMark_ = -1;
  }
  state.prevSuppressSeparator_ = state.suppressSeparator_;
  state.firstItem_ = false;
  state.freshLine_ = false;
  state.itemEmpty_ = false;
}

void Printer::StartList(const std::string &separator, int numCharsExpected)
{
  State &state = stack_.back();

  if (state.itemEmpty_) {
    state.prevSuppressSeparator_ = state.suppressSeparator_;
    state.firstItem_ = false;
    state.freshLine_ = false;
    state.itemEmpty_ = false;
  }
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
  Write(token);
}

void Printer::Print(int value)
{
  State &state = stack_.back();
  if (state.itemEmpty_) {
    DoStartItem();
  }
  Write(std::to_string(value));
}

void Printer::Print(double value)
{
  State &state = stack_.back();
  if (state.itemEmpty_) {
    DoStartItem();
  }
  std::ostringstream text;
  text << value;
  Write(text.str());
}

void Printer::Printf(const char *format, ...) {
  State &state = stack_.back();
  if (state.itemEmpty_) {
    DoStartItem();
  }
  va_list args, argsCopy;
  va_start(args, format);
  va_copy(argsCopy, args);
  int length = vsnprintf(nullptr, 0, format, args);
  std::string text(length > 0 ? (size_t)length : 0, '\0');
  if (length > 0)
    vsnprintf(&text[0], (size_t)length + 1, format, argsCopy);
  va_end(argsCopy);
  va_end(args);
  Write(text);
}

void Printer::FreshLine()
{
  State &state = stack_.back();
  state.freshLine_ = true;
  state.itemEmpty_ = true;
}

void Printer::PrintDivider(const std::string &text)
{
  // TODO: untested
  State &state = stack_.back();
  if (state.itemEmpty_) {
    DoStartItem();
  }
  Write("\n");
  if (text.empty()) {
    for (int i = wrapAt_; i > 0; --i) Write("-");
  } else {
    int n = (wrapAt_ - (int)text.length() - 2) / 2;
    for (int i = n; i > 0; --i) Write("-");
    Write(" " + text + " ");
    for (int i = n; i > 0; --i) Write("-");
  }
  Write("\n");
}






