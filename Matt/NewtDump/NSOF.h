/*
 File: NSOF.h

 A small NSOF (Newton Streamed Object Format) reader and writer for
 newtdump: what the Docking Protocol's commands carry (frames, arrays,
 integers, strings, symbols, binaries), without the NewtonScript object
 system. Version 2 streams; precedents as the ROM's reader numbers them
 (binaries, strings, arrays, frames, symbols, small rects).
 */

#ifndef NEWTDUMP_NSOF_H
#define NEWTDUMP_NSOF_H

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nsof {

struct Value;
using Ref = std::shared_ptr<Value>;

struct Value
{
  enum Kind { kNil, kTrue, kInteger, kImmediate, kCharacter, kSymbol, kString, kBinary, kArray, kFrame };
  Kind kind = kNil;
  int64_t integer = 0;            // kInteger, kImmediate (the raw Ref), kCharacter
  std::string text;               // kSymbol (as is), kString (UTF-8)
  std::string cls;                // kBinary, kArray: the class symbol
  std::vector<uint8_t> bytes;     // kBinary
  std::vector<Ref> items;         // kArray
  std::vector<std::pair<std::string, Ref>> slots;   // kFrame, in order

  /** A frame's slot (case-insensitive, as NewtonScript symbols), or nullptr. */
  Ref slot(const std::string &inName) const;
  bool isInteger() const { return kind == kInteger; }
  bool isNil() const { return kind == kNil; }
  /** For messages: a short NewtonScript-like text. */
  std::string describe(int inDepth = 2) const;
};

Ref Nil();
Ref True();
Ref Integer(int64_t inValue);
Ref Symbol(const std::string &inName);
Ref String(const std::string &inUTF8);
Ref Frame(std::vector<std::pair<std::string, Ref>> inSlots);
Ref Array(std::vector<Ref> inItems);

/** NSOF (version 2) for a value. Integers must fit a Newton (30 bits). */
std::vector<uint8_t> Write(const Ref &inValue);

/** The value in NSOF bytes; throws std::runtime_error if they aren't. */
Ref Read(const uint8_t *inData, size_t inSize);
inline Ref Read(const std::vector<uint8_t> &inData) { return Read(inData.data(), inData.size()); }

} // namespace nsof

#endif // NEWTDUMP_NSOF_H
