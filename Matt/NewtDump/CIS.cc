/*
 File: CIS.cc

 A PCMCIA card's Card Information Structure (see CIS.h). Tuple codes and
 layouts as in the PC Card Standard (Metaformat): a code byte, a link byte
 (the length of the data), the data; 0x00 (CISTPL_NULL) has no link, 0xFF
 (CISTPL_END) ends the chain.
 */

#include "CIS.h"

#include <cstdio>
#include <tuple>

namespace {

const char *TupleName(uint8_t inCode)
{
  switch (inCode) {
  case 0x00: return "NULL";
  case 0x01: return "DEVICE";
  case 0x02: return "LONGLINK_CB";
  case 0x10: return "CHECKSUM";
  case 0x11: return "LONGLINK_A";
  case 0x12: return "LONGLINK_C";
  case 0x13: return "LINKTARGET";
  case 0x14: return "NO_LINK";
  case 0x15: return "VERS_1";
  case 0x16: return "ALTSTR";
  case 0x17: return "DEVICE_A";
  case 0x18: return "JEDEC_C";
  case 0x19: return "JEDEC_A";
  case 0x1A: return "CONFIG";
  case 0x1B: return "CFTABLE_ENTRY";
  case 0x1C: return "DEVICE_OC";
  case 0x1D: return "DEVICE_OA";
  case 0x1E: return "DEVICE_GEO";
  case 0x1F: return "DEVICE_GEO_A";
  case 0x20: return "MANFID";
  case 0x21: return "FUNCID";
  case 0x22: return "FUNCE";
  case 0x23: return "SWIL";
  case 0x40: return "VERS_2";
  case 0x41: return "FORMAT";
  case 0x42: return "GEOMETRY";
  case 0x43: return "BYTEORDER";
  case 0x44: return "DATE";
  case 0x45: return "BATTERY";
  case 0x46: return "ORG";
  case 0xFF: return "END";
  }
  return inCode >= 0x80 && inCode <= 0x8F ? "vendor" : "?";
}

const char *DeviceType(unsigned inType)
{
  static const char *kTypes[] = { "none", "mask ROM", "OTPROM", "EPROM", "EEPROM", "flash",
                                  "SRAM", "DRAM", "reserved", "reserved", "reserved", "reserved",
                                  "reserved", "function specific (I/O)", "extended", "reserved" };
  return kTypes[inType & 15];
}

const char *FunctionName(unsigned inCode)
{
  static const char *kNames[] = { "multi-function", "memory", "serial port/modem", "parallel port",
                                  "fixed disk", "video", "network", "AIMS", "SCSI", "security" };
  return inCode < 10 ? kNames[inCode] : "vendor specific";
}

std::string Hex(const std::vector<uint8_t> &inData, size_t inMax = 32)
{
  std::string s;
  char b[4];
  for (size_t i = 0; i < inData.size() && i < inMax; i++) {
    snprintf(b, sizeof(b), "%s%02X", i ? " " : "", inData[i]);
    s += b;
  }
  if (inData.size() > inMax)
    s += " ...";
  return s;
}

std::string SizeText(uint64_t inBytes)
{
  char b[48];
  if (inBytes >= 1024 * 1024 && inBytes % (1024 * 1024) == 0)
    snprintf(b, sizeof(b), "%llu MB", (unsigned long long)(inBytes >> 20));
  else if (inBytes >= 1024 && inBytes % 1024 == 0)
    snprintf(b, sizeof(b), "%llu KB", (unsigned long long)(inBytes >> 10));
  else
    snprintf(b, sizeof(b), "%llu bytes", (unsigned long long)inBytes);
  return b;
}

// CISTPL_DEVICE: device info entries, each {type, WPS, speed} [extended
// speed and type bytes] {size}; 0xFF ends. Returns the total size.
uint64_t DecodeDevices(const std::vector<uint8_t> &inData, std::vector<std::string> &ioReport)
{
  uint64_t total = 0;
  size_t i = 0;
  while (i < inData.size() && inData[i] != 0xFF) {
    uint8_t info = inData[i++];
    unsigned type = info >> 4;
    bool writeProtect = info & 0x08;
    unsigned speed = info & 0x07;
    if (speed == 7)                           // extended speed bytes
      while (i < inData.size() && (inData[i++] & 0x80)) {}
    if (type == 0x0E)                         // extended type bytes
      while (i < inData.size() && (inData[i++] & 0x80)) {}
    if (i >= inData.size())
      break;
    uint8_t sizeByte = inData[i++];
    if (sizeByte == 0xFF)
      break;
    static const uint64_t kUnits[] = { 512, 2048, 8192, 32768, 131072, 524288, 2097152, 0 };
    uint64_t size = (uint64_t)((sizeByte >> 3) + 1) * kUnits[sizeByte & 7];
    static const char *kSpeeds[] = { "", "250 ns", "200 ns", "150 ns", "100 ns", "?", "?", "extended" };
    total += size;
    ioReport.push_back("    device: " + std::string(DeviceType(type)) + ", " + SizeText(size)
                       + (speed ? ", " + std::string(kSpeeds[speed]) : "")
                       + (writeProtect ? ", write protect switch" : ""));
  }
  return total;
}

// VERS_1: major, minor, then strings (manufacturer, product, ...), each
// NUL-terminated; 0xFF ends
std::string DecodeVersion1(const std::vector<uint8_t> &inData)
{
  std::string s;
  if (inData.size() >= 2) {
    char b[24];
    snprintf(b, sizeof(b), "PC Card %u.%u", inData[0], inData[1]);
    s = b;
  }
  std::string text;
  for (size_t i = 2; i < inData.size() && inData[i] != 0xFF; i++) {
    if (inData[i] == 0) {
      if (!text.empty())
        s += ", \"" + text + "\"";
      text.clear();
    } else {
      text += (inData[i] >= 32 && inData[i] < 127) ? (char)inData[i] : '?';
    }
  }
  if (!text.empty())
    s += ", \"" + text + "\"";
  return s;
}

// One way of reading the bytes: the k-th CIS byte is raw[(start + k *
// step) ^ swap] (step 2: attribute memory, one byte in two; 1: every byte;
// swap: the byte order within a 32-bit word, as the bus delivers it)
CISInfo Parse(const std::vector<uint8_t> &inRaw, size_t inStart, size_t inStep, size_t inSwap, const char *inLayout)
{
  CISInfo info;
  info.layout = inLayout;
  std::vector<uint8_t> bytes;
  for (size_t k = 0; ; k++) {
    size_t index = (inStart + k * inStep) ^ inSwap;
    if (index >= inRaw.size())
      break;
    bytes.push_back(inRaw[index]);
  }
  size_t i = 0;
  while (i < bytes.size()) {
    uint8_t code = bytes[i++];
    if (code == 0xFF) {
      info.valid = !info.tuples.empty();
      break;
    }
    CISTuple tuple;
    tuple.code = code;
    if (code != 0x00) {
      if (i >= bytes.size())
        break;
      uint8_t link = bytes[i++];
      if (link == 0xFF) {                   // a link of 0xFF ends the chain too
        info.valid = !info.tuples.empty();
        break;
      }
      if (i + link > bytes.size())
        break;
      tuple.data.assign(bytes.begin() + (long)i, bytes.begin() + (long)(i + link));
      i += link;
    }
    // the first tuple of a CIS is a known one; garbage stops here
    if (info.tuples.empty() && code != 0x01 && code != 0x17 && code != 0x1C && code != 0x1D
        && code != 0x13 && code != 0x15 && code != 0x00 && code != 0x18 && code != 0x20)
      break;
    if (code != 0x00)
      info.tuples.push_back(tuple);
  }
  if (!info.valid)
    return info;

  for (const CISTuple &t : info.tuples) {
    char head[48];
    snprintf(head, sizeof(head), "  %02X %-14s (%zu bytes)", t.code, TupleName(t.code), t.data.size());
    info.report.push_back(head);
    switch (t.code) {
    case 0x01:   // DEVICE: common memory
      info.commonSize += DecodeDevices(t.data, info.report);
      break;
    case 0x17:   // DEVICE_A: attribute memory
      DecodeDevices(t.data, info.report);
      break;
    case 0x15:
      info.report.push_back("    " + DecodeVersion1(t.data));
      break;
    case 0x20:
      if (t.data.size() >= 4) {
        char b[64];
        snprintf(b, sizeof(b), "    manufacturer 0x%04X, card 0x%04X",
                 t.data[0] | t.data[1] << 8, t.data[2] | t.data[3] << 8);
        info.report.push_back(b);
      }
      break;
    case 0x21:
      if (!t.data.empty())
        info.report.push_back("    function: " + std::string(FunctionName(t.data[0])));
      break;
    case 0x18:
    case 0x19:
      for (size_t k = 0; k + 1 < t.data.size(); k += 2) {
        char b[64];
        snprintf(b, sizeof(b), "    JEDEC: manufacturer 0x%02X, device 0x%02X", t.data[k], t.data[k + 1]);
        info.report.push_back(b);
      }
      break;
    default:
      if (!t.data.empty())
        info.report.push_back("    " + Hex(t.data));
      break;
    }
  }
  if (info.commonSize)
    info.report.push_back("  common memory: " + SizeText(info.commonSize));
  return info;
}

// The tuples of a chain whose codes the standard knows. Read the wrong way
// (every byte where only the even ones count, the odd ones 0), a CIS is a
// longer chain than read the right way, but of codes nobody knows; and a
// real one may have a stray tuple (Einstein's VERS_1 link is 3 bytes short:
// its last letter is read as a code).
size_t KnownTuples(const CISInfo &inInfo)
{
  size_t known = 0;
  for (const CISTuple &t : inInfo.tuples)
    if (TupleName(t.code)[0] != '?')
      known++;
  return known;
}

} // namespace


CISInfo ParseCIS(const std::vector<uint8_t> &inRaw, bool inEveryByteOnly)
{
  // where the CIS bytes are depends on the bus (attribute memory: 8 bits on
  // a 16-bit bus; the Newton's byte order within a word): every way is
  // tried, the valid chain with the most known tuples wins
  struct Layout { size_t start, step; const char *name; };
  static const Layout kAttribute[] = { { 0, 2, "even bytes" }, { 1, 2, "odd bytes" }, { 0, 1, "every byte" } };
  static const Layout kCommon[] = { { 0, 1, "every byte" } };
  static const struct { size_t mask; const char *name; } kSwaps[] = {
    { 0, "" }, { 3, ", bytes reversed in words" }, { 2, ", 16-bit halves swapped" }, { 1, ", bytes swapped in halves" } };
  CISInfo best;
  for (const Layout &layout : inEveryByteOnly ? std::vector<Layout>(kCommon, kCommon + 1)
                                              : std::vector<Layout>(kAttribute, kAttribute + 3)) {
    for (const auto &swap : kSwaps) {
      std::string name = std::string(layout.name) + swap.name;
      CISInfo info = Parse(inRaw, layout.start, layout.step, swap.mask, "");
      info.layout = name;
      if (info.valid && KnownTuples(info) > KnownTuples(best))
        best = info;
    }
  }
  return best;
}
