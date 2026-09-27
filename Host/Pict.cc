/*
 File: Pict.cc

 QuickDraw pictures of bitmaps. See Pict.h.
 */

#include "Host/Pict.h"

#include <algorithm>

namespace pict {

namespace {

int Word(const unsigned char * p) { return short((p[0] << 8) | p[1]); }

} // namespace


bool Frame(const unsigned char * inData, size_t inSize, int * outTop, int * outLeft, int * outBottom, int * outRight)
{
  if (inSize < 12)
    return false;
  *outTop = Word(inData + 2);
  *outLeft = Word(inData + 4);
  *outBottom = Word(inData + 6);
  *outRight = Word(inData + 8);
  return *outBottom > *outTop && *outRight > *outLeft;
}


bool Decode(const unsigned char * inData, size_t inSize, int * outWidth, int * outHeight, int * outRowBytes,
            std::vector<unsigned char> * outBits)
{
  int top, left, bottom, right;
  if (!Frame(inData, inSize, &top, &left, &bottom, &right) || inData[10] != 0x11 || inData[11] != 0x01)
    return false;   // not a version 1 picture
  int width = right - left, height = bottom - top, rowBytes = (width + 7) / 8;
  std::vector<unsigned char> bits(size_t(rowBytes) * height, 0);
  size_t p = 12;
  auto need = [&](size_t n) { return p + n <= inSize; };
  while (p < inSize) {
    int op = inData[p++];
    if (op == 0xFF)
      break;
    else if (op == 0x00 || op == 0x1E)          // nop, default hilite
      ;
    else if (op == 0x01) {                      // clip region: its size counts itself
      if (!need(2)) return false;
      p += size_t(Word(inData + p));
    } else if (op == 0xA0) {                    // short comment
      p += 2;
    } else if (op == 0xA1) {                    // long comment
      if (!need(4)) return false;
      p += 4 + size_t(Word(inData + p + 2));
    } else if (op == 0x90 || op == 0x98) {      // BitsRect, PackBitsRect
      if (!need(28)) return false;
      int srcRowBytes = Word(inData + p) & 0x7FFF;
      int bTop = Word(inData + p + 2), bLeft = Word(inData + p + 4), bBottom = Word(inData + p + 6);
      int sTop = Word(inData + p + 10), sLeft = Word(inData + p + 12), sBottom = Word(inData + p + 14), sRight = Word(inData + p + 16);
      int dTop = Word(inData + p + 18), dLeft = Word(inData + p + 20);
      p += 28;                                  // rowBytes, bounds, srcRect, dstRect, mode
      std::vector<unsigned char> line;
      for (int row = 0; row < bBottom - bTop; ++row) {
        line.clear();
        if (op == 0x98 && srcRowBytes >= 8) {
          size_t count;
          if (srcRowBytes > 250) { if (!need(2)) return false; count = size_t(Word(inData + p)) & 0xFFFF; p += 2; }
          else { if (!need(1)) return false; count = inData[p++]; }
          if (!need(count)) return false;
          size_t end = p + count;
          while (p < end) {                     // PackBits
            int n = inData[p++];
            if (n < 128) { line.insert(line.end(), inData + p, inData + std::min(p + n + 1, end)); p += size_t(n) + 1; }
            else if (n > 128) { if (p < end) line.insert(line.end(), size_t(257 - n), inData[p]); p++; }
          }
          p = end;
        } else {
          if (!need(size_t(srcRowBytes))) return false;
          line.assign(inData + p, inData + p + srcRowBytes);
          p += size_t(srcRowBytes);
        }
        int y = bTop + row;
        if (y < sTop || y >= sBottom)
          continue;
        for (int x = sLeft; x < sRight; ++x) {
          int bit = x - bLeft;
          if (bit < 0 || size_t(bit >> 3) >= line.size() || !(line[size_t(bit >> 3)] & (0x80 >> (bit & 7))))
            continue;
          int px = dLeft + x - sLeft - left, py = dTop + y - sTop - top;
          if (px >= 0 && px < width && py >= 0 && py < height)
            bits[size_t(py) * rowBytes + (px >> 3)] |= (0x80 >> (px & 7));
        }
      }
    } else
      return false;                             // an opcode this doesn't know
  }
  *outWidth = width;
  *outHeight = height;
  *outRowBytes = rowBytes;
  outBits->swap(bits);
  return true;
}

} // namespace pict
