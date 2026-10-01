# The Newton ROM from source with Apple's own tools (see CLAUDE.md here).
# Independent of newtc's build:
#
#   cmake -S experimental -B experimental/build
#   cmake --build experimental/build            the ROM's parts and symbols
#   cmake --build experimental/build --target check
#                                               the probes against the ROM
#
# Needs Python 3 and newton-re (dparnell's, for the debug ROM image and the
# library that reads it; NEWTON_RE).

cmake_minimum_required(VERSION 3.20)
project(NewtonROM NONE)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(NEWTON_RE "${CMAKE_SOURCE_DIR}/../../newton-re" CACHE PATH
    "newton-re (its DebugRom/ and tools/newton-rom/)")
get_filename_component(NEWTON_RE "${NEWTON_RE}" ABSOLUTE)
set(ROM_IMAGE "${NEWTON_RE}/DebugRom/MP2x00 US/Senior CirrusNoDebug image")
if(NOT EXISTS "${ROM_IMAGE}")
  message(FATAL_ERROR "No debug ROM image at ${ROM_IMAGE} (set NEWTON_RE)")
endif()

set(TOOLS "${CMAKE_SOURCE_DIR}/bin")
set(INCLUDES "${CMAKE_SOURCE_DIR}/includes")
set(ROM_DIR "${CMAKE_BINARY_DIR}/rom")

# R1: the ROM's parts and symbols from Apple's linker output
add_custom_command(
  OUTPUT "${ROM_DIR}/ro.bin" "${ROM_DIR}/rw.bin" "${ROM_DIR}/rex.bin" "${ROM_DIR}/symbols.json"
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/romsyms.py" "${NEWTON_RE}" "${ROM_DIR}"
  DEPENDS "${CMAKE_SOURCE_DIR}/tools/romsyms.py" "${ROM_IMAGE}"
  COMMENT "The ROM's parts and symbols (tools/romsyms.py)"
  VERBATIM)
add_custom_target(rom ALL DEPENDS "${ROM_DIR}/symbols.json")

# R0: the probes, compiled and compared with the ROM function by function
file(GLOB PROBES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/probes/*.cp")
add_custom_target(check
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/probe.py"
          --bin "${TOOLS}" --includes "${INCLUDES}" --rom "${ROM_DIR}"
          --out "${CMAKE_BINARY_DIR}/probes" ${PROBES}
  DEPENDS rom
  COMMENT "The probes against the ROM (tools/probe.py)"
  VERBATIM)
