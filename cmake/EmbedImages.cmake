#
# Turn the ROM's images (Host/FLTK/Images/rom_NNNN*.png, made by
# extract_rom_images.py) into a C++ source file, so that newtc has them
# without the files at run time. Run in script mode by CMakeLists.txt:
#
#   cmake -DDIR=<Host/FLTK/Images> -DOUT=<file.cc> -P EmbedImages.cmake
#
# The generated file defines the table that Host/FLTK/RomImages.cc reads:
# for each image, its magic pointer index (NNNN), its file name, and the
# PNG's bytes.
#

file(GLOB inputs "${DIR}/rom_*.png")
list(SORT inputs)

set(line32 "")
foreach(i RANGE 1 32)
  string(APPEND line32 "[0-9a-f]")
endforeach()

set(arrays "")
set(table "")
set(count 0)
foreach(png IN LISTS inputs)
  get_filename_component(fileName "${png}" NAME)
  string(REGEX MATCH "^rom_([0-9]+)" match "${fileName}")
  if(NOT match)
    continue()
  endif()
  string(REGEX REPLACE "^0+([0-9])" "\\1" index "${CMAKE_MATCH_1}")
  file(READ "${png}" hex HEX)
  string(LENGTH "${hex}" hexLength)
  math(EXPR byteCount "${hexLength} / 2")
  string(REGEX REPLACE "(${line32})" "\\1\n" hex "${hex}")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " bytes "${hex}")
  string(REGEX REPLACE ", \n" ",\n  " bytes "${bytes}")
  string(APPEND arrays "static const unsigned char kImage${count}[${byteCount}] = {\n  ${bytes}\n};\n\n")
  string(APPEND table "  { ${index}, \"${fileName}\", kImage${count}, ${byteCount} },\n")
  math(EXPR count "${count} + 1")
endforeach()

file(WRITE "${OUT}"
"// Generated from Host/FLTK/Images/rom_*.png by cmake/EmbedImages.cmake. Do
// not edit; run Host/FLTK/Images/extract_rom_images.py, or edit the PNGs.

#include \"Host/FLTK/RomImages.h\"

namespace nfl {

${arrays}extern const RomImageData kRomImages[] = {
${table}};

extern const int kRomImageCount = ${count};

} // namespace nfl
")
