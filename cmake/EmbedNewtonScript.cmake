#
# Turn a NewtonScript source file into a C++ source file, so that newtc can
# run it without needing the .ns file at run time. Used by the CMake function
# embed_newtonscript() in CMakeLists.txt; run in script mode:
#
#   cmake -DIN=<file.ns> -DOUT=<file.cc> -DNAME=<C++ variable> -P EmbedNewtonScript.cmake
#
# The generated file defines `const EmbeddedScript NAME` (see
# Matt/EmbeddedScript.h) with the .ns file's name and its text as a byte
# array, so no character in the source can break the C++ syntax.
#

file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" hexLength)
math(EXPR byteCount "${hexLength} / 2")

# 16 bytes per line: first split the hex string into lines of 32 hex digits,
# then turn every pair of digits into "0xNN, ".
set(line32 "")
foreach(i RANGE 1 32)
  string(APPEND line32 "[0-9a-f]")
endforeach()
string(REGEX REPLACE "(${line32})" "\\1\n" hex "${hex}")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " bytes "${hex}")
string(REGEX REPLACE ", \n" ",\n  " bytes "${bytes}")

get_filename_component(fileName "${IN}" NAME)

file(WRITE "${OUT}"
"// Generated from ${fileName} by cmake/EmbedNewtonScript.cmake. Do not edit;
// edit the .ns file instead.

#include \"Matt/EmbeddedScript.h\"

static const unsigned char kSource[${byteCount} + 1] = {
  ${bytes}0x00
};

extern const EmbeddedScript ${NAME} = { \"${fileName}\", reinterpret_cast<const char *>(kSource) };
")
