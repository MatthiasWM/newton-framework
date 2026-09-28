#
# Turn a Newton package into a C++ source file, so that a newtc app has it
# without the file at run time (Matt/EmbeddedApp.h). Run in script mode by
# CMakeLists.txt (the newtc_app target):
#
#   cmake -DIN=<app.pkg> -DOUT=<file.cc> -DNAME=<app name> -P EmbedPackage.cmake
#

set(line32 "")
foreach(i RANGE 1 32)
  string(APPEND line32 "[0-9a-f]")
endforeach()

file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" hexLength)
math(EXPR byteCount "${hexLength} / 2")
string(REGEX REPLACE "(${line32})" "\\1\n" hex "${hex}")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " bytes "${hex}")
string(REGEX REPLACE ", \n" ",\n  " bytes "${bytes}")
get_filename_component(fileName "${IN}" NAME)

file(WRITE "${OUT}"
"// Generated from ${fileName} by cmake/EmbedPackage.cmake. Do not edit.

#include \"Matt/EmbeddedApp.h\"

extern const char gEmbeddedAppName[] = \"${NAME}\";

extern const unsigned char gEmbeddedAppPackage[${byteCount}] = {
  ${bytes}
};

extern const size_t gEmbeddedAppPackageSize = ${byteCount};
")
