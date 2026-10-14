/*
 File: PNG.cc

 Newton bitmaps and icons from PNG files (plan 4.6): MakeBitmapFromPNG and
 MakeIconFromPNG, for scripts that build packages. PNG files are read with
 stb_image (Matt/ThirdParty), so this works in every newtc build, also
 without FLTK.

 Newton hardware shows black and white, or at most 16 grays: a bitmap is
 1 bit deep (`bits`, black is 1), and a 4-bit version (16 grays, 0 is white,
 15 black) goes into `colorData` (Apple's name; NewtonOS 2.x uses it where
 the screen has grays). A `mask` (1 = the pixel is part of the image) comes
 from the PNG's alpha. Each binary starts with a 16-byte header: 4 bytes 0,
 the bytes per row (rows are padded to 32 bits), 2 bytes 0, then the bounds
 (top, left, bottom, right), as in the packages NTK made.
 */

#include "Matt/PNG.h"

#include "Objects.h"
#include "Frames.h"
#include "Arrays.h"
#include "Globals.h"
#include "ROMResources.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_FAILURE_USERMSG
#include "Matt/ThirdParty/stb_image.h"

#include <string>
#include <vector>

extern std::string FindScriptFile(RefArg inName);   // Frames/Compiler/Compiler.cc

namespace {

const long kNewtcErrReadFile = -1000001;      // ErrorString.cc: "Can't read the file"
const long kNewtcErrArgument = -1000002;      // ErrorString.cc: "Wrong argument"

[[noreturn]] void Fail(long inError, const std::string &inMessage)
{
  RefVar fr(AllocateFrame());
  SetFrameSlot(fr, SYMA(errorCode), MAKEINT(inError));
  SetFrameSlot(fr, SYMA(value), MakeStringFromCString(inMessage.c_str()));
  ThrowRefException(exFramesData, fr);
  for (;;) {}
}

struct Options
{
  int depth = 1;            // 1, or 4 (also the 1-bit version, for B&W Newtons)
  bool dither = false;      // 1 bit: Floyd-Steinberg instead of a threshold
  int threshold = 128;      // 1 bit: a gray below it is black
  int alphaThreshold = 128; // mask: alpha at least this is part of the image
  enum { kAuto, kNone, kPicture, kCalculate } mask = kAuto;   // NTK's mask choices
};

int IntOption(RefArg inOptions, const char *inName, int inDefault, int inMin, int inMax, const char *inFunction)
{
  RefVar value(GetFrameSlot(inOptions, MakeSymbol(inName)));
  if (ISNIL(value))
    return inDefault;
  if (!ISINT(value) || RINT(value) < inMin || RINT(value) > inMax)
    Fail(kNewtcErrArgument, std::string(inFunction) + ": " + inName + " must be an integer from "
         + std::to_string(inMin) + " to " + std::to_string(inMax));
  return int(RINT(value));
}

Options ReadOptions(RefArg inOptions, bool inIcon, const char *inFunction)
{
  Options o;
  if (ISNIL(inOptions))
    return o;
  if (!IsFrame(inOptions))
    Fail(kNewtcErrArgument, std::string(inFunction) + ": the options must be a frame, e.g. {depth: 4}");
  o.depth = IntOption(inOptions, "depth", 1, 1, 4, inFunction);
  if (o.depth != 1 && o.depth != 4)
    Fail(kNewtcErrArgument, std::string(inFunction) + ": depth must be 1 or 4 (Newtons show at most 16 grays)");
  o.dither = NOTNIL(GetFrameSlot(inOptions, MakeSymbol("dither")));
  o.threshold = IntOption(inOptions, "threshold", 128, 0, 256, inFunction);
  o.alphaThreshold = IntOption(inOptions, "alphaThreshold", 128, 1, 255, inFunction);
  RefVar mask(GetFrameSlot(inOptions, MakeSymbol("mask")));
  if (EQ(mask, MakeSymbol("none")))
    o.mask = Options::kNone;
  else if (EQ(mask, MakeSymbol("picture")))
    o.mask = Options::kPicture;
  else if (EQ(mask, MakeSymbol("calculate")))
    o.mask = Options::kCalculate;
  else if (NOTNIL(mask))
    Fail(kNewtcErrArgument, std::string(inFunction) + ": mask must be 'none, 'picture or 'calculate (NTK's No Mask, Use Picture, Calculate Mask)");
  return o;
}

long RowBytes(long inWidth, int inDepth)
{
  return ((inWidth * inDepth + 31) / 32) * 4;
}

// A binary with the 16-byte header and zeroed rows.
Ref NewBits(const char *inClass, long inWidth, long inHeight, int inDepth, unsigned char **outRows, long *outRowBytes)
{
  long rowBytes = RowBytes(inWidth, inDepth);
  RefVar bits(AllocateBinary(MakeSymbol(inClass), 16 + rowBytes * inHeight));
  unsigned char *p = (unsigned char *)BinaryData(bits);
  memset(p, 0, 16 + rowBytes * inHeight);
  p[4] = (unsigned char)(rowBytes >> 8);
  p[5] = (unsigned char)rowBytes;
  p[12] = (unsigned char)(inHeight >> 8);   // top 0, left 0, bottom, right
  p[13] = (unsigned char)inHeight;
  p[14] = (unsigned char)(inWidth >> 8);
  p[15] = (unsigned char)inWidth;
  *outRows = p + 16;
  *outRowBytes = rowBytes;
  return bits;
}

// The argument: a path, or a frame with the path (file) and the options.
Ref MakeFromPNG(RefArg inArg, bool inIcon, const char *inFunction)
{
  RefVar file(inArg), options;
  if (IsFrame(inArg)) {
    file = GetFrameSlot(inArg, MakeSymbol("file"));
    options = inArg;
  }
  if (!IsString(file))
    Fail(kNewtcErrArgument, std::string(inFunction) + ": a PNG file's path, or a frame with it in file: "
         + inFunction + "({file: \"icon.png\", depth: 4})");
  Options o = ReadOptions(options, inIcon, inFunction);
  std::string path = FindScriptFile(file);
  int width = 0, height = 0, channels = 0;
  unsigned char *rgba = stbi_load(path.c_str(), &width, &height, &channels, 4);
  if (rgba == nullptr)
    Fail(kNewtcErrReadFile, path + " (" + stbi_failure_reason() + ")");
  if (width > 32767 || height > 32767) {
    stbi_image_free(rgba);
    Fail(kNewtcErrReadFile, path + " is too big for a Newton bitmap");
  }
  // the gray of each pixel over white (0 black, 255 white), and its alpha
  std::vector<int> gray(size_t(width) * height);
  std::vector<unsigned char> alpha(size_t(width) * height);
  bool hasAlpha = false;
  for (size_t i = 0; i < gray.size(); ++i) {
    const unsigned char *px = rgba + 4 * i;
    int y = (299 * px[0] + 587 * px[1] + 114 * px[2] + 500) / 1000;
    gray[i] = (y * px[3] + 255 * (255 - px[3]) + 127) / 255;
    alpha[i] = px[3];
    if (px[3] != 255)
      hasAlpha = true;
  }
  stbi_image_free(rgba);

  unsigned char *rows;
  long rowBytes;
  // 1 bit: black below the threshold, or dithered (the error spread on)
  RefVar bits(NewBits("bits", width, height, 1, &rows, &rowBytes));
  std::vector<int> work(gray);
  std::vector<bool> isBlack(gray.size());
  for (long y = 0; y < height; ++y)
    for (long x = 0; x < width; ++x) {
      int g = work[y * width + x];
      bool black = g < o.threshold;
      isBlack[y * width + x] = black;
      if (black)
        rows[y * rowBytes + x / 8] |= (unsigned char)(0x80 >> (x % 8));
      if (o.dither) {
        int err = g - (black ? 0 : 255);
        if (x + 1 < width) work[y * width + x + 1] += err * 7 / 16;
        if (y + 1 < height) {
          if (x > 0) work[(y + 1) * width + x - 1] += err * 3 / 16;
          work[(y + 1) * width + x] += err * 5 / 16;
          if (x + 1 < width) work[(y + 1) * width + x + 1] += err * 1 / 16;
        }
      }
    }

  RefVar result(AllocateFrame());
  RefVar bounds(AllocateFrame());
  SetFrameSlot(bounds, SYMA(left), MAKEINT(0));
  SetFrameSlot(bounds, SYMA(top), MAKEINT(0));
  SetFrameSlot(bounds, SYMA(right), MAKEINT(width));
  SetFrameSlot(bounds, SYMA(bottom), MAKEINT(height));
  SetFrameSlot(result, SYMA(bounds), bounds);
  SetFrameSlot(result, SYMA(bits), bits);

  // the mask, as NTK's choices: Use Picture (the alpha; all of the image
  // without one), Calculate Mask (in each row, from the left and from the
  // right edge, outside until the first black pixel), No Mask. The default:
  // the picture's when the PNG has transparency, else a bitmap has none and
  // an icon a calculated one.
  int mode = o.mask;
  if (mode == Options::kAuto)
    mode = hasAlpha ? Options::kPicture : (inIcon ? Options::kCalculate : Options::kNone);
  if (mode != Options::kNone) {
    RefVar mask(NewBits("mask", width, height, 1, &rows, &rowBytes));
    for (long y = 0; y < height; ++y) {
      long first = 0, last = width - 1;     // calculated: the row's black span
      if (mode == Options::kCalculate) {
        while (first < width && !isBlack[y * width + first]) first++;
        while (last >= first && !isBlack[y * width + last]) last--;
      }
      for (long x = 0; x < width; ++x) {
        bool inside = mode == Options::kCalculate ? (x >= first && x <= last)
                                                  : alpha[y * width + x] >= o.alphaThreshold;
        if (inside)
          rows[y * rowBytes + x / 8] |= (unsigned char)(0x80 >> (x % 8));
      }
    }
    SetFrameSlot(result, SYMA(mask), mask);
  }

  // 16 grays: 0 white, 15 black, two pixels a byte (the left one high)
  if (o.depth == 4) {
    RefVar cbits(NewBits("cbits", width, height, 4, &rows, &rowBytes));
    for (long y = 0; y < height; ++y)
      for (long x = 0; x < width; ++x) {
        int level = 15 - (gray[y * width + x] * 15 + 127) / 255;
        rows[y * rowBytes + x / 2] |= (unsigned char)(x % 2 ? level : level << 4);
      }
    RefVar entry(AllocateFrame());
    SetFrameSlot(entry, MakeSymbol("cbits"), cbits);
    SetFrameSlot(entry, MakeSymbol("bitDepth"), MAKEINT(4));
    RefVar colorData(MakeArray(1));
    SetArraySlot(colorData, 0, entry);
    SetFrameSlot(result, MakeSymbol("colorData"), colorData);
  }
  return result;
}

} // namespace


/*
 MakeBitmapFromPNG(file): a bitmap frame {bounds, bits, mask, colorData} for
 a view (e.g. clPictureView's icon). `file` is the PNG file's path (found
 as Load finds files), or a frame with the path in `file` and options (all
 optional): depth 1 (default) or 4 (16 grays in colorData, plus the 1-bit
 bits), dither true (1 bit), threshold (0-256, default 128: grays below it
 are black), mask (NTK's choices: 'none, 'picture: from the alpha, all of
 the image without one, 'calculate: in each row outside from either edge
 until the first black pixel; default 'picture when the PNG has
 transparency, else none), alphaThreshold (1-255, default 128).
 */
Ref FMakeBitmapFromPNG(RefArg inRcvr, RefArg inFile)
{
  return MakeFromPNG(inFile, false, "MakeBitmapFromPNG");
}


/*
 MakeIconFromPNG(file): as MakeBitmapFromPNG, with a mask always unless
 mask: 'none (an icon's shape): 'picture when the PNG has transparency,
 else 'calculate.
 */
Ref FMakeIconFromPNG(RefArg inRcvr, RefArg inFile)
{
  return MakeFromPNG(inFile, true, "MakeIconFromPNG");
}
