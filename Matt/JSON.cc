/*
 File: JSON.cc

 JSON <-> NewtonScript objects. See JSON.h.
 */

#include "Matt/JSON.h"

#include "Frames/Frames.h"
#include "Frames/Iterators.h"
#include "Frames/RefMemory.h"
#include "Frames/Strings.h"
#include "ROMResources.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// NewtonScript integers have kRefValueBits bits (62 on 64-bit hosts, 30 on a
// Newton), like the compiler's integer literals; larger JSON integers become
// reals.
const long kMaxNewtonInt = (1L << (kRefValueBits - 1)) - 1;
const long kMinNewtonInt = -(1L << (kRefValueBits - 1));

// Nesting limit, so that deep or cyclic structures throw instead of
// overflowing the stack.
const int kMaxDepth = 256;

// ThrowMsg keeps the pointer, so the message must stay valid.
char gJSONError[256];

[[noreturn]] void Fail(const char *what, size_t position)
{
  snprintf(gJSONError, sizeof(gJSONError), "JSON: %s at offset %zu", what, position);
  ThrowMsg(gJSONError);
  for (;;) { }  // ThrowMsg does not return
}

[[noreturn]] void FailWrite(const char *what)
{
  snprintf(gJSONError, sizeof(gJSONError), "JSON: %s", what);
  ThrowMsg(gJSONError);
  for (;;) { }
}


/*------------------------------------------------------------------------------
  Parser: recursive descent over the UTF-8 text.
------------------------------------------------------------------------------*/

class Parser
{
public:
  Parser(const char *text, size_t length) : p(text), start(text), end(text + length) { }

  Ref parseDocument()
  {
    skipSpace();
    RefVar value(parseValue(0));
    skipSpace();
    if (p != end)
      Fail("unexpected text after the value", offset());
    return value;
  }

private:
  const char *p, *start, *end;

  size_t offset() const { return (size_t)(p - start); }

  void skipSpace()
  {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      ++p;
  }

  bool consume(const char *word)
  {
    size_t n = strlen(word);
    if ((size_t)(end - p) >= n && memcmp(p, word, n) == 0) {
      p += n;
      return true;
    }
    return false;
  }

  Ref parseValue(int depth)
  {
    if (depth > kMaxDepth)
      Fail("nested too deeply", offset());
    if (p >= end)
      Fail("unexpected end of text", offset());
    switch (*p) {
      case '{': return parseObject(depth);
      case '[': return parseArray(depth);
      case '"': {
        std::u16string s = parseString();
        return MakeString((const UniChar *)s.c_str());
      }
      case 't': if (consume("true")) return TRUEREF; break;
      case 'f': if (consume("false")) return NILREF; break;
      case 'n': if (consume("null")) return NILREF; break;
      default:
        if (*p == '-' || (*p >= '0' && *p <= '9'))
          return parseNumber();
        break;
    }
    Fail("unexpected character", offset());
  }

  Ref parseObject(int depth)
  {
    ++p;  // '{'
    RefVar frame(AllocateFrame());
    skipSpace();
    if (p < end && *p == '}') { ++p; return frame; }
    for (;;) {
      skipSpace();
      if (p >= end || *p != '"')
        Fail("expected a key string", offset());
      size_t keyOffset = offset();
      std::u16string key = parseString();
      std::string asciiKey;
      for (char16_t ch : key) {
        if (ch < 0x20 || ch > 0x7E)
          Fail("a key must be printable ASCII (it becomes a symbol)", keyOffset);
        asciiKey += (char)ch;
      }
      if (asciiKey.empty())
        Fail("empty key", keyOffset);
      skipSpace();
      if (p >= end || *p != ':')
        Fail("expected ':'", offset());
      ++p;
      skipSpace();
      RefVar value(parseValue(depth + 1));
      SetFrameSlot(frame, MakeSymbol(asciiKey.c_str()), value);
      skipSpace();
      if (p < end && *p == ',') { ++p; continue; }
      if (p < end && *p == '}') { ++p; return frame; }
      Fail("expected ',' or '}'", offset());
    }
  }

  Ref parseArray(int depth)
  {
    ++p;  // '['
    RefVar array(MakeArray(0));
    skipSpace();
    if (p < end && *p == ']') { ++p; return array; }
    for (;;) {
      skipSpace();
      RefVar value(parseValue(depth + 1));
      AddArraySlot(array, value);
      skipSpace();
      if (p < end && *p == ',') { ++p; continue; }
      if (p < end && *p == ']') { ++p; return array; }
      Fail("expected ',' or ']'", offset());
    }
  }

  unsigned hex4()
  {
    if (end - p < 4)
      Fail("incomplete \\u escape", offset());
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
      char c = *p++;
      v <<= 4;
      if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
      else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
      else Fail("bad \\u escape", offset());
    }
    return v;
  }

  // Append one Unicode code point as UTF-16.
  static void append(std::u16string &s, unsigned cp)
  {
    if (cp >= 0x10000) {
      cp -= 0x10000;
      s += (char16_t)(0xD800 + (cp >> 10));
      s += (char16_t)(0xDC00 + (cp & 0x3FF));
    } else {
      s += (char16_t)cp;
    }
  }

  // Decode one UTF-8 sequence starting at p.
  unsigned utf8()
  {
    unsigned char c = (unsigned char)*p;
    int extra;
    unsigned cp;
    if (c < 0x80) { ++p; return c; }
    else if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
    else Fail("bad UTF-8", offset());
    size_t at = offset();
    ++p;
    for (int i = 0; i < extra; ++i) {
      if (p >= end || (((unsigned char)*p) & 0xC0) != 0x80)
        Fail("bad UTF-8", at);
      cp = (cp << 6) | (((unsigned char)*p++) & 0x3F);
    }
    return cp;
  }

  std::u16string parseString()
  {
    ++p;  // '"'
    std::u16string s;
    for (;;) {
      if (p >= end)
        Fail("unterminated string", offset());
      char c = *p;
      if (c == '"') { ++p; return s; }
      if ((unsigned char)c < 0x20)
        Fail("control character in string", offset());
      if (c != '\\') { append(s, utf8()); continue; }
      ++p;
      if (p >= end)
        Fail("unterminated string", offset());
      char e = *p++;
      switch (e) {
        case '"': s += u'"'; break;
        case '\\': s += u'\\'; break;
        case '/': s += u'/'; break;
        case 'b': s += u'\b'; break;
        case 'f': s += u'\f'; break;
        case 'n': s += u'\n'; break;
        case 'r': s += u'\r'; break;
        case 't': s += u'\t'; break;
        case 'u': {
          unsigned cp = hex4();
          if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
            const char *save = p;
            p += 2;
            unsigned low = hex4();
            if (low >= 0xDC00 && low <= 0xDFFF)
              cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            else
              p = save;  // not a pair: keep the lone high surrogate
          }
          append(s, cp);
          break;
        }
        default:
          Fail("bad escape in string", offset() - 1);
      }
    }
  }

  Ref parseNumber()
  {
    const char *numStart = p;
    bool isInteger = true;
    if (*p == '-') ++p;
    if (p >= end || !(*p >= '0' && *p <= '9'))
      Fail("bad number", offset());
    while (p < end && *p >= '0' && *p <= '9') ++p;
    if (p < end && *p == '.') {
      isInteger = false;
      ++p;
      if (p >= end || !(*p >= '0' && *p <= '9'))
        Fail("bad number", offset());
      while (p < end && *p >= '0' && *p <= '9') ++p;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
      isInteger = false;
      ++p;
      if (p < end && (*p == '+' || *p == '-')) ++p;
      if (p >= end || !(*p >= '0' && *p <= '9'))
        Fail("bad number", offset());
      while (p < end && *p >= '0' && *p <= '9') ++p;
    }
    std::string text(numStart, (size_t)(p - numStart));
    if (isInteger) {
      errno = 0;
      long v = strtol(text.c_str(), nullptr, 10);
      if (errno == 0 && v >= kMinNewtonInt && v <= kMaxNewtonInt)
        return MAKEINT(v);
    }
    return MakeReal(strtod(text.c_str(), nullptr));
  }
};


/*------------------------------------------------------------------------------
  Writer
------------------------------------------------------------------------------*/

void WriteString(std::string &out, const UniChar *s, size_t length)
{
  out += '"';
  for (size_t i = 0; i < length; ++i) {
    UniChar ch = s[i];
    switch (ch) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (ch < 0x20 || ch > 0x7E) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04X", (unsigned)ch);
          out += buf;
        } else {
          out += (char)ch;
        }
    }
  }
  out += '"';
}

void WriteASCII(std::string &out, const char *s)
{
  std::u16string u;
  for (; *s; ++s) u += (char16_t)(unsigned char)*s;
  WriteString(out, (const UniChar *)u.c_str(), u.size());
}

void WriteValue(std::string &out, RefArg obj, int depth)
{
  if (depth > kMaxDepth)
    FailWrite("nested too deeply (a cycle?)");
  Ref r = obj;
  if (ISNIL(r)) { out += "false"; return; }
  if (r == TRUEREF) { out += "true"; return; }
  if (ISINT(r)) { out += std::to_string(RINT(r)); return; }
  if (ISCHAR(r)) {
    UniChar ch = RCHAR(r);
    WriteString(out, &ch, 1);
    return;
  }
  if (IsSymbol(r)) { WriteASCII(out, SymbolName(r)); return; }
  if (IsString(r)) {
    UniChar *s = GetUString(obj);
    size_t length = 0;
    while (s[length]) ++length;
    WriteString(out, s, length);
    return;
  }
  if (IsReal(r)) {
    double d = CDouble(r);
    if (!std::isfinite(d)) { out += "null"; return; }
    // shortest form that reads back as the same double
    char buf[32];
    for (int precision = 15; precision <= 17; ++precision) {
      snprintf(buf, sizeof(buf), "%.*g", precision, d);
      if (strtod(buf, nullptr) == d)
        break;
    }
    out += buf;
    return;
  }
  if (IsFrame(r)) {
    out += '{';
    bool first = true;
    CObjectIterator iter(obj, false);
    for ( ; !iter.done(); iter.next()) {
      if (!first) out += ',';
      first = false;
      WriteASCII(out, SymbolName(iter.tag()));
      out += ':';
      RefVar value(iter.value());
      WriteValue(out, value, depth + 1);
    }
    out += '}';
    return;
  }
  if (IsArray(r)) {
    out += '[';
    ArrayIndex count = Length(obj);
    for (ArrayIndex i = 0; i < count; ++i) {
      if (i > 0) out += ',';
      RefVar value(GetArraySlot(obj, i));
      WriteValue(out, value, depth + 1);
    }
    out += ']';
    return;
  }
  FailWrite("can't convert this object (not a frame, array, string, number, symbol, character, true, or nil)");
}

} // namespace


Ref ParseJSON(const char *text, size_t length)
{
  Parser parser(text, length);
  return parser.parseDocument();
}


std::string ToJSON(RefArg obj)
{
  std::string out;
  WriteValue(out, obj, 0);
  return out;
}


std::string QuoteJSON(const std::string &text)
{
  std::u16string u;
  for (unsigned char ch : text) u += (char16_t)ch;
  std::string out;
  WriteString(out, (const UniChar *)u.c_str(), u.size());
  return out;
}


std::string UTF8FromString(RefArg str)
{
  UniChar *s = GetUString(str);
  std::string utf8;
  for (size_t i = 0; s[i]; ++i) {
    unsigned cp = s[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
      ++i;
    }
    if (cp < 0x80) utf8 += (char)cp;
    else if (cp < 0x800) { utf8 += (char)(0xC0 | (cp >> 6)); utf8 += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { utf8 += (char)(0xE0 | (cp >> 12)); utf8 += (char)(0x80 | ((cp >> 6) & 0x3F)); utf8 += (char)(0x80 | (cp & 0x3F)); }
    else { utf8 += (char)(0xF0 | (cp >> 18)); utf8 += (char)(0x80 | ((cp >> 12) & 0x3F)); utf8 += (char)(0x80 | ((cp >> 6) & 0x3F)); utf8 += (char)(0x80 | (cp & 0x3F)); }
  }
  return utf8;
}


/*------------------------------------------------------------------------------
  NewtonScript functions
------------------------------------------------------------------------------*/

// JSONParse(string): the object the JSON text describes.
Ref FJSONParse(RefArg rcvr, RefArg inString)
{
  if (!IsString(inString))
    ThrowBadTypeWithFrameData(kNSErrNotAString, inString);
  // NewtonScript strings are UTF-16: convert to UTF-8 for the parser
  std::string utf8 = UTF8FromString(inString);
  return ParseJSON(utf8.data(), utf8.size());
}


// JSONStringify(object): the object as compact JSON text.
Ref FJSONStringify(RefArg rcvr, RefArg inObject)
{
  std::string json = ToJSON(inObject);
  return MakeStringFromCString(json.c_str());   // pure ASCII
}
