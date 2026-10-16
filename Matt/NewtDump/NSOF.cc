/*
 File: NSOF.cc

 A small NSOF reader and writer (see NSOF.h).
 */

#include "NSOF.h"

#include <cctype>
#include <cstdio>
#include <stdexcept>

namespace nsof {

enum {
  kNSImmediate = 0, kNSCharacter = 1, kNSUnicodeCharacter = 2, kNSBinaryObject = 3,
  kNSArray = 4, kNSPlainArray = 5, kNSFrame = 6, kNSSymbol = 7, kNSString = 8,
  kNSPrecedent = 9, kNSNIL = 10, kNSSmallRect = 11, kNSLargeBinary = 12
};
const int kNSOFVersion = 2;
const uint32_t kNilRef = 2, kTrueRef = 0x1A;


Ref Value::slot(const std::string &inName) const
{
  for (const auto &s : slots) {
    if (s.first.size() != inName.size())
      continue;
    bool same = true;
    for (size_t i = 0; i < inName.size() && same; i++)
      same = tolower((unsigned char)s.first[i]) == tolower((unsigned char)inName[i]);
    if (same)
      return s.second;
  }
  return nullptr;
}


std::string Value::describe(int inDepth) const
{
  switch (kind) {
  case kNil: return "nil";
  case kTrue: return "true";
  case kInteger: return std::to_string(integer);
  case kImmediate: { char b[32]; snprintf(b, sizeof(b), "#%llX", (unsigned long long)integer); return b; }
  case kCharacter: { char b[32]; snprintf(b, sizeof(b), "$\\u%04X", (unsigned)integer); return b; }
  case kSymbol: return "'" + text;
  case kString: return "\"" + text + "\"";
  case kBinary: return "<binary '" + cls + ", " + std::to_string(bytes.size()) + " bytes>";
  case kArray: {
    if (inDepth <= 0) return "[...]";
    std::string s = "[";
    for (size_t i = 0; i < items.size(); i++)
      s += (i ? ", " : "") + (items[i] ? items[i]->describe(inDepth - 1) : "?");
    return s + "]";
  }
  case kFrame: {
    if (inDepth <= 0) return "{...}";
    std::string s = "{";
    for (size_t i = 0; i < slots.size(); i++)
      s += (i ? ", " : "") + slots[i].first + ": " + (slots[i].second ? slots[i].second->describe(inDepth - 1) : "?");
    return s + "}";
  }
  }
  return "?";
}


Ref Nil() { return std::make_shared<Value>(); }
Ref True() { auto v = std::make_shared<Value>(); v->kind = Value::kTrue; return v; }
Ref Integer(int64_t inValue) { auto v = std::make_shared<Value>(); v->kind = Value::kInteger; v->integer = inValue; return v; }
Ref Symbol(const std::string &inName) { auto v = std::make_shared<Value>(); v->kind = Value::kSymbol; v->text = inName; return v; }
Ref String(const std::string &inUTF8) { auto v = std::make_shared<Value>(); v->kind = Value::kString; v->text = inUTF8; return v; }
Ref Frame(std::vector<std::pair<std::string, Ref>> inSlots) { auto v = std::make_shared<Value>(); v->kind = Value::kFrame; v->slots = std::move(inSlots); return v; }
Ref Array(std::vector<Ref> inItems) { auto v = std::make_shared<Value>(); v->kind = Value::kArray; v->cls = "array"; v->items = std::move(inItems); return v; }


/*--- Writing ---*/

namespace {

void PutXLong(std::vector<uint8_t> &out, uint32_t inValue)
{
  if (inValue < 255) {
    out.push_back((uint8_t)inValue);
  } else {
    out.push_back(0xFF);
    for (int shift = 24; shift >= 0; shift -= 8)
      out.push_back((uint8_t)(inValue >> shift));
  }
}

// UTF-8 to UTF-16 (big-endian, as a Newton's strings), with the NUL
std::vector<uint8_t> UTF16(const std::string &inUTF8)
{
  std::vector<uint8_t> out;
  for (size_t i = 0; i < inUTF8.size(); ) {
    unsigned c = (unsigned char)inUTF8[i++];
    if (c >= 0xC0 && i < inUTF8.size()) {
      int extra = c >= 0xE0 ? 2 : 1;
      c &= extra == 2 ? 0x0F : 0x1F;
      while (extra-- && i < inUTF8.size())
        c = (c << 6) | ((unsigned char)inUTF8[i++] & 0x3F);
    }
    out.push_back((uint8_t)(c >> 8));
    out.push_back((uint8_t)c);
  }
  out.push_back(0);
  out.push_back(0);
  return out;
}

// no precedents: each symbol is written where it occurs (the Newton's
// reader takes that too; our requests are small)
void WriteSymbol(std::vector<uint8_t> &out, const std::string &inName)
{
  out.push_back(kNSSymbol);
  PutXLong(out, (uint32_t)inName.size());
  out.insert(out.end(), inName.begin(), inName.end());
}

void WriteValue(std::vector<uint8_t> &out, const Ref &inValue)
{
  const Value &v = inValue ? *inValue : Value();
  switch (v.kind) {
  case Value::kNil:
    out.push_back(kNSNIL);
    break;
  case Value::kTrue:
    out.push_back(kNSImmediate);
    PutXLong(out, kTrueRef);
    break;
  case Value::kInteger:
    if (v.integer < -(1LL << 29) || v.integer > (1LL << 29) - 1)
      throw std::runtime_error("NSOF: the integer " + std::to_string(v.integer) + " doesn't fit a Newton");
    out.push_back(kNSImmediate);
    PutXLong(out, (uint32_t)(v.integer << 2));
    break;
  case Value::kImmediate:
    out.push_back(kNSImmediate);
    PutXLong(out, (uint32_t)v.integer);
    break;
  case Value::kCharacter:
    if (v.integer < 256) {
      out.push_back(kNSCharacter);
      out.push_back((uint8_t)v.integer);
    } else {
      out.push_back(kNSUnicodeCharacter);
      out.push_back((uint8_t)(v.integer >> 8));
      out.push_back((uint8_t)v.integer);
    }
    break;
  case Value::kSymbol:
    WriteSymbol(out, v.text);
    break;
  case Value::kString: {
    std::vector<uint8_t> chars = UTF16(v.text);
    out.push_back(kNSString);
    PutXLong(out, (uint32_t)chars.size());
    out.insert(out.end(), chars.begin(), chars.end());
    break;
  }
  case Value::kBinary:
    out.push_back(kNSBinaryObject);
    PutXLong(out, (uint32_t)v.bytes.size());
    WriteSymbol(out, v.cls.empty() ? "binary" : v.cls);
    out.insert(out.end(), v.bytes.begin(), v.bytes.end());
    break;
  case Value::kArray:
    if (v.cls.empty() || v.cls == "array") {
      out.push_back(kNSPlainArray);
      PutXLong(out, (uint32_t)v.items.size());
    } else {
      out.push_back(kNSArray);
      PutXLong(out, (uint32_t)v.items.size());
      WriteSymbol(out, v.cls);
    }
    for (const Ref &item : v.items)
      WriteValue(out, item);
    break;
  case Value::kFrame:
    out.push_back(kNSFrame);
    PutXLong(out, (uint32_t)v.slots.size());
    for (const auto &s : v.slots)
      WriteSymbol(out, s.first);
    for (const auto &s : v.slots)
      WriteValue(out, s.second);
    break;
  }
}

} // namespace


std::vector<uint8_t> Write(const Ref &inValue)
{
  std::vector<uint8_t> out;
  out.push_back(kNSOFVersion);
  WriteValue(out, inValue);
  return out;
}


/*--- Reading ---*/

namespace {

class Reader
{
public:
  Reader(const uint8_t *inData, size_t inSize) : mData(inData), mSize(inSize) {}

  Ref read()
  {
    int version = byte();
    if (version != 1 && version != 2)
      fail("unknown NSOF version " + std::to_string(version));
    return value();
  }

private:
  [[noreturn]] void fail(const std::string &inWhy)
  {
    throw std::runtime_error("NSOF: " + inWhy + " (at byte " + std::to_string(mPos) + ")");
  }

  uint8_t byte()
  {
    if (mPos >= mSize)
      fail("the data ends too early");
    return mData[mPos++];
  }

  uint32_t xlong()
  {
    uint32_t v = byte();
    if (v == 255) {
      v = 0;
      for (int i = 0; i < 4; i++)
        v = (v << 8) | byte();
    }
    return v;
  }

  std::vector<uint8_t> chunk(size_t inLength)
  {
    if (inLength > mSize - mPos)
      fail("an object is longer than the data");
    std::vector<uint8_t> v(mData + mPos, mData + mPos + inLength);
    mPos += inLength;
    return v;
  }

  std::string symbolName(const Ref &inSymbol)
  {
    if (!inSymbol || inSymbol->kind != Value::kSymbol)
      fail("a symbol was expected");
    return inSymbol->text;
  }

  Ref value()
  {
    uint8_t type = byte();
    switch (type) {
    case kNSImmediate: {
      uint32_t ref = xlong();
      auto v = std::make_shared<Value>();
      if ((ref & 3) == 0) {
        v->kind = Value::kInteger;
        v->integer = (int32_t)ref >> 2;
      } else if (ref == kNilRef) {
        v->kind = Value::kNil;
      } else if (ref == kTrueRef) {
        v->kind = Value::kTrue;
      } else {
        v->kind = Value::kImmediate;
        v->integer = ref;
      }
      return v;
    }
    case kNSCharacter: {
      auto v = std::make_shared<Value>();
      v->kind = Value::kCharacter;
      v->integer = byte();
      return v;
    }
    case kNSUnicodeCharacter: {
      auto v = std::make_shared<Value>();
      v->kind = Value::kCharacter;
      v->integer = byte() << 8;
      v->integer |= byte();
      return v;
    }
    case kNSBinaryObject:
    case kNSString: {
      uint32_t length = xlong();
      auto v = std::make_shared<Value>();
      mPrecedents.push_back(v);
      if (type == kNSString) {
        v->kind = Value::kString;
        std::vector<uint8_t> chars = chunk(length);
        v->text = UTF8(chars);
      } else {
        v->kind = Value::kBinary;
        v->cls = symbolName(value());
        v->bytes = chunk(length);
        if (v->cls == "string" || v->cls == "String") {
          v->kind = Value::kString;
          v->text = UTF8(v->bytes);
          v->bytes.clear();
        }
      }
      return v;
    }
    case kNSArray:
    case kNSPlainArray: {
      uint32_t count = xlong();
      auto v = std::make_shared<Value>();
      v->kind = Value::kArray;
      mPrecedents.push_back(v);
      v->cls = type == kNSArray ? symbolName(value()) : "array";
      for (uint32_t i = 0; i < count; i++)
        v->items.push_back(value());
      return v;
    }
    case kNSFrame: {
      uint32_t count = xlong();
      auto v = std::make_shared<Value>();
      v->kind = Value::kFrame;
      mPrecedents.push_back(v);
      std::vector<std::string> tags;
      for (uint32_t i = 0; i < count; i++)
        tags.push_back(symbolName(value()));
      for (uint32_t i = 0; i < count; i++)
        v->slots.emplace_back(tags[i], value());
      return v;
    }
    case kNSSymbol: {
      uint32_t length = xlong();
      std::vector<uint8_t> name = chunk(length);
      auto v = std::make_shared<Value>();
      v->kind = Value::kSymbol;
      v->text.assign(name.begin(), name.end());
      mPrecedents.push_back(v);
      return v;
    }
    case kNSPrecedent: {
      uint32_t index = xlong();
      if (index >= mPrecedents.size())
        fail("a precedent that isn't there");
      return mPrecedents[index];
    }
    case kNSNIL:
      return Nil();
    case kNSSmallRect: {
      std::vector<uint8_t> r = chunk(4);
      Ref v = Frame({{"top", Integer(r[0])}, {"left", Integer(r[1])},
                     {"bottom", Integer(r[2])}, {"right", Integer(r[3])}});
      mPrecedents.push_back(v);
      return v;
    }
    default:
      fail("an unknown object type " + std::to_string(type));
    }
  }

  // UTF-16 (big-endian) to UTF-8, up to a NUL
  static std::string UTF8(const std::vector<uint8_t> &inChars)
  {
    std::string s;
    for (size_t i = 0; i + 1 < inChars.size(); i += 2) {
      unsigned c = (unsigned)(inChars[i] << 8 | inChars[i + 1]);
      if (c == 0)
        break;
      if (c < 0x80) {
        s += (char)c;
      } else if (c < 0x800) {
        s += (char)(0xC0 | (c >> 6));
        s += (char)(0x80 | (c & 0x3F));
      } else {
        s += (char)(0xE0 | (c >> 12));
        s += (char)(0x80 | ((c >> 6) & 0x3F));
        s += (char)(0x80 | (c & 0x3F));
      }
    }
    return s;
  }

  const uint8_t *mData;
  size_t mSize;
  size_t mPos = 0;
  std::vector<Ref> mPrecedents;
};

} // namespace


Ref Read(const uint8_t *inData, size_t inSize)
{
  Reader reader(inData, inSize);
  return reader.read();
}

} // namespace nsof
