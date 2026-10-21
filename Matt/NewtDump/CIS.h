/*
 File: CIS.h

 A PCMCIA card's Card Information Structure (CIS): the tuples in its
 attribute memory (only the even bytes count: attribute memory is 8 bits on
 a 16-bit bus) or, on some ROM cards, at the start of common memory (every
 byte). newtdump decodes the tuples that tell what a memory card is: its
 devices and their sizes, its name and maker, its function.
 */

#ifndef NEWTDUMP_CIS_H
#define NEWTDUMP_CIS_H

#include <cstdint>
#include <string>
#include <vector>

struct CISTuple
{
  uint8_t code = 0;
  std::vector<uint8_t> data;
};

struct CISInfo
{
  bool valid = false;               // a tuple chain that ends properly
  std::string layout;               // where the bytes were: "even bytes", "odd bytes", "every byte"
  std::vector<CISTuple> tuples;
  std::vector<uint8_t> bytes;       // the chain in order, up to and with its end (as Einstein keeps a CIS)
  uint64_t commonSize = 0;          // the devices of CISTPL_DEVICE, all together (0: unknown)
  std::vector<std::string> report;  // a line per tuple, for people
};

/** The CIS in raw bytes as read from attribute memory (tries even, odd, and
    every byte) or, with inEveryByteOnly, from common memory. */
CISInfo ParseCIS(const std::vector<uint8_t> &inRaw, bool inEveryByteOnly = false);

#endif // NEWTDUMP_CIS_H
