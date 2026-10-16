# Compile newtdump's protocol extension (Matt/NewtDump/Newton/Extension.ns)
# with newtc into NSOF, and write it as a C++ array (kNewtDumpExtension) for
# newtdump, which sends it to the Newton ('pext').
#
#   cmake -DNEWTC=<newtc> -DIN=<Extension.ns> -DNSOF=<Extension.nsof>
#         -DOUT=<Extension.cc> -P NewtDumpExtension.cmake

execute_process (
  COMMAND "${NEWTC}" -script "${IN}" -onsof "${NSOF}"
  INPUT_FILE /dev/null
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE output
)
if (NOT result EQUAL 0)
  message (FATAL_ERROR "newtc couldn't compile ${IN}:\n${output}")
endif ()

file (READ "${NSOF}" hex HEX)
string (LENGTH "${hex}" length)
math (EXPR size "${length} / 2")
string (REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string (REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n  " bytes "${bytes}")
file (WRITE "${OUT}"
"// Generated from Matt/NewtDump/Newton/Extension.ns by cmake/NewtDumpExtension.cmake:\n"
"// the protocol extension as NSOF (newtc -onsof). Don't edit.\n\n"
"#include <cstddef>\n\n"
"extern const unsigned char kNewtDumpExtension[] = {\n  ${bytes}\n};\n"
"extern const size_t kNewtDumpExtensionSize = ${size};\n")
