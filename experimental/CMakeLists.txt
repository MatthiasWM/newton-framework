# The Newton ROM from source with Apple's own tools (see CLAUDE.md here).
# Independent of newtc's build:
#
#   cmake -S experimental -B experimental/build
#   cmake --build experimental/build            the ROM's parts, symbols,
#                                               src/ compiled, assembler source
#   cmake --build experimental/build --target check
#                                               the probes and the whole ROM
#                                               against Apple's image
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
add_custom_target(rom ALL DEPENDS "${ROM_DIR}/symbols.json" "${ROM_DIR}/kinds.bin" "${ROM_DIR}/code.bin")

# R3d: code and literals, found by following the code from the functions
add_custom_command(
  OUTPUT "${ROM_DIR}/code.bin"
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/romcode.py" "${ROM_DIR}"
          --compare "${ROM_DIR}/kinds.bin"
  DEPENDS "${CMAKE_SOURCE_DIR}/tools/romcode.py" "${ROM_DIR}/symbols.json" "${ROM_DIR}/kinds.bin"
  COMMENT "Code and literals (tools/romcode.py)"
  VERBATIM)

# R3c: which words are instructions, from the disassembly of the ROM
set(NEWTONOS_S "${CMAKE_SOURCE_DIR}/../newtonos.s" CACHE FILEPATH
    "newtonos.s, the ROM's disassembly (git-ignored, at the repository's top)")
add_custom_command(
  OUTPUT "${ROM_DIR}/kinds.bin"
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/romkinds.py" "${NEWTONOS_S}" "${ROM_DIR}"
  DEPENDS "${CMAKE_SOURCE_DIR}/tools/romkinds.py" "${NEWTONOS_S}"
  COMMENT "Instructions and data words (tools/romkinds.py)"
  VERBATIM)

# R6: the source tree, compiled with Apple's compiler and the standard
# options; each object goes in place of its functions (romasm.py --object)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cc" "${CMAKE_SOURCE_DIR}/src/*.cp")
set(OBJECTS)
set(OBJECT_OPTIONS)
foreach(source ${SOURCES})
  file(RELATIVE_PATH rel "${CMAKE_SOURCE_DIR}/src" "${source}")
  get_filename_component(dir "${rel}" DIRECTORY)
  get_filename_component(stem "${rel}" NAME_WE)
  set(object "${CMAKE_BINARY_DIR}/src/${dir}/${stem}.o")
  add_custom_command(
    OUTPUT "${object}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/src/${dir}"
    COMMAND "${TOOLS}/ARMCpp" ---text=utf8 -c -bigend -fc -DforARM -DforQ -DQD_Gray
            -I "${INCLUDES}" -I "${CMAKE_SOURCE_DIR}/src" "${source}" -o "${object}"
    DEPENDS "${source}"
    COMMENT "ARMCpp src/${rel}"
    VERBATIM)
  list(APPEND OBJECTS "${object}")
  list(APPEND OBJECT_OPTIONS --object "${object}")
endforeach()

# R2: the ROM as assembler source (generated), assembled and linked back,
# with the compiled sources in place
set(ROMASM_DIR "${CMAKE_BINARY_DIR}/romasm")
add_custom_command(
  OUTPUT "${ROMASM_DIR}/files.txt"
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/romasm.py" "${ROM_DIR}" "${ROMASM_DIR}"
          ${OBJECT_OPTIONS}
  DEPENDS "${CMAKE_SOURCE_DIR}/tools/romasm.py" "${CMAKE_SOURCE_DIR}/tools/aof.py"
          "${ROM_DIR}/symbols.json" "${ROM_DIR}/kinds.bin" "${ROM_DIR}/code.bin" ${OBJECTS}
  COMMENT "The ROM as assembler source, compiled sources in place (tools/romasm.py)"
  VERBATIM)
add_custom_target(romasm ALL DEPENDS "${ROMASM_DIR}/files.txt")

# the checks: R0's probes, function by function; the whole image (R2, R6:
# generated source and compiled sources)
file(GLOB PROBES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/probes/*.cp")
add_custom_target(check
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/probe.py"
          --bin "${TOOLS}" --includes "${INCLUDES}" --rom "${ROM_DIR}"
          --out "${CMAKE_BINARY_DIR}/probes" ${PROBES}
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/romlink.py"
          --bin "${TOOLS}" --src "${ROMASM_DIR}" --rom "${ROM_DIR}"
          --out "${CMAKE_BINARY_DIR}/romlink"
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/aof.py" --check ${OBJECTS}
          "${CMAKE_BINARY_DIR}/romlink/abs.o"
  DEPENDS rom romasm
  COMMENT "The probes and the whole ROM against Apple's image"
  VERBATIM)

# R3b: the shift test (padding put in, two links compared; see the tool)
add_custom_target(shift
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/shifttest.py"
          --bin "${TOOLS}" --rom "${ROM_DIR}" --out "${CMAKE_BINARY_DIR}/shift"
  DEPENDS rom
  COMMENT "The shift test (tools/shifttest.py)"
  VERBATIM)

# R3g.2: the shipping ROM image (8 MB: the link, the ROM extension, the
# checksum block), from the AIF the check target links
add_custom_target(image
  COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/romimage.py"
          --aif "${CMAKE_BINARY_DIR}/romlink/rom.aif" --rom "${ROM_DIR}"
          -o "${CMAKE_BINARY_DIR}/romimage/rom.image"
  COMMENT "The shipping ROM image (tools/romimage.py; run the check target first)"
  VERBATIM)
