/*
 File: RomImages.cc

 The ROM's graphics as images for FLTK. See RomImages.h.
 */

// FLTK first: the framework's headers #define names FLTK uses (OVERRIDE, ...)
#include <FL/Fl_PNG_Image.H>

#include "Host/FLTK/RomImages.h"

#include "Frames/Frames.h"
#include "Frames/Globals.h"
#include "ROMResources.h"

#include <cstring>
#include <map>
#include <utility>

namespace nfl {

namespace {

// The ROM object's size on the Newton display: a PICT's frame, an icon's
// bounds. 0 if neither.
void LogicalSize(long inMagicIndex, int * outWidth, int * outHeight)
{
  *outWidth = *outHeight = 0;
  RefVar object(MAKEMAGICPTR(inMagicIndex));
  auto word = [](const unsigned char * data, int offset) { return int(short((data[offset] << 8) | data[offset + 1])); };
  if (IsBinary(object) && Length(object) >= 10) {   // a PICT: size, then top, left, bottom, right
    const unsigned char * data = (const unsigned char *)BinaryData(object);
    *outWidth = word(data, 8) - word(data, 4);
    *outHeight = word(data, 6) - word(data, 2);
  } else if (IsFrame(object)) {
    RefVar bits(GetFrameSlot(object, SYMA(bits)));
    if (IsBinary(bits) && Length(bits) >= 16) {    // a bitmap: its bounds at 8
      const unsigned char * data = (const unsigned char *)BinaryData(bits);
      *outWidth = word(data, 14) - word(data, 10);
      *outHeight = word(data, 12) - word(data, 8);
    }
  }
}

// A copy of an image with its gray (or color) channels inverted.
Fl_Image * Inverted(Fl_Image * inImage)
{
  int d = inImage->d();
  if (inImage->count() != 1 || d < 1)
    return nullptr;
  int w = inImage->data_w(), h = inImage->data_h();
  int ld = inImage->ld() ? inImage->ld() : w * d;
  const unsigned char * src = reinterpret_cast<const unsigned char *>(inImage->data()[0]);
  unsigned char * dst = new unsigned char[size_t(w) * h * d];
  int colors = (d == 2 || d == 4) ? d - 1 : d;   // the last channel is alpha
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w * d; ++x) {
      unsigned char v = src[y * ld + x];
      dst[size_t(y) * w * d + x] = (x % d) < colors ? 255 - v : v;
    }
  Fl_RGB_Image * image = new Fl_RGB_Image(dst, w, h, d);
  image->alloc_array = 1;
  image->scale(inImage->w(), inImage->h(), 0, 1);
  return image;
}

} // namespace


Fl_Image * RomImage(long inMagicIndex, bool inInverted)
{
  static std::map<std::pair<long, bool>, Fl_Image *> sImages;
  auto key = std::make_pair(inMagicIndex, inInverted);
  auto found = sImages.find(key);
  if (found != sImages.end())
    return found->second;
  Fl_Image * image = nullptr;
  if (inInverted) {
    if (Fl_Image * plain = RomImage(inMagicIndex, false))
      image = Inverted(plain);
  } else {
    for (int i = 0; i < kRomImageCount; ++i) {
      if (kRomImages[i].magic != inMagicIndex)
        continue;
      Fl_PNG_Image * png = new Fl_PNG_Image(kRomImages[i].name, kRomImages[i].png, kRomImages[i].size);
      if (png->fail()) {
        delete png;
        png = nullptr;
      } else {
        // drawn at the ROM object's size, whatever the PNG's resolution (a
        // PNG twice as big is sharp on a Retina screen)
        int width, height;
        LogicalSize(inMagicIndex, &width, &height);
        if (width > 0 && height > 0)
          png->scale(width, height, 0, 1);
      }
      image = png;
      break;
    }
  }
  sImages[key] = image;
  return image;
}

} // namespace nfl
