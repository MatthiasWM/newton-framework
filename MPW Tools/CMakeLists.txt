# CMakeLists.txt for MPW Tools (converted from REx.xcodeproj)
# ROM Extension Creator and other Newton development tools

# Created by Claude AI, untested!

cmake_minimum_required(VERSION 3.20)

# Set C++ standard to match Xcode project (gnu++0x = C++11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)  # Enable GNU extensions

# Set macOS deployment target to match Xcode project
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11)
endif()

# Compiler flags to match Xcode project settings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")

# Header search paths
include_directories(
    ${CMAKE_SOURCE_DIR}  # Main newton-framework root
    ${CMAKE_CURRENT_SOURCE_DIR}/REx  # REx headers
)

# REx - ROM Extension Creator
set(REX_SOURCES
    REx/REx.cc
    REx/Lexer.cc
    REx/Options.cc
    REx/ClauseHandlers.cc
    REx/Chunk.cc
    REx/PackageAccessor.cc
    REx/PatchTable.cc
    REx/CRC16.cc
    REx/Reporting.cc
)

set(REX_HEADERS
    REx/Lexer.h
    REx/Options.h
    REx/Chunk.h
    REx/CRC16.h
    REx/PackageAccessor.h
    REx/PatchTable.h
    REx/Reporting.h
    REx/ClauseHandlers.h
    REx/RExArray.h
    REx/NewtonKit.h
)

add_executable(REx ${REX_SOURCES} ${REX_HEADERS})

# Set output directory
set_target_properties(REx PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Pram - NSOF file creator from text config
add_executable(Pram Pram/Pram.cc)
set_target_properties(Pram PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Packer - Newton package creator
set(PACKER_SOURCES
    Packer/Packer.cc
    Packer/Relocation.cc
    Packer/StringTable.cc
    Packer/Reporting.cc
)

add_executable(Packer ${PACKER_SOURCES})
set_target_properties(Packer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# AIFtoNTK - Convert AIF files to NTK format
add_executable(AIFtoNTK AIFtoNTK/AIFtoNTK.cc)
set_target_properties(AIFtoNTK PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# MachOtoNTK - Convert Mach-O files to NTK format
add_executable(MachOtoNTK AIFtoNTK/MachOtoNTK.cc)
set_target_properties(MachOtoNTK PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Link CoreServices framework for AIFtoNTK and MachOtoNTK on macOS
if(APPLE)
    find_library(CORE_SERVICES_FRAMEWORK CoreServices)
    if(CORE_SERVICES_FRAMEWORK)
        target_link_libraries(AIFtoNTK ${CORE_SERVICES_FRAMEWORK})
        target_link_libraries(MachOtoNTK ${CORE_SERVICES_FRAMEWORK})
    endif()
endif()

# ProtocolGenTool - Generate protocol files
add_executable(ProtocolGenTool ProtocolGenTool/ProtocolGenTool.cc)
set_target_properties(ProtocolGenTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install targets
install(TARGETS REx Pram Packer AIFtoNTK MachOtoNTK ProtocolGenTool
    RUNTIME DESTINATION bin
)

# Copy configuration files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/REx.config
               ${CMAKE_BINARY_DIR}/bin/REx.config COPYONLY)

# Copy test configuration for development
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Test/TestREx.config
               ${CMAKE_BINARY_DIR}/bin/TestREx.config COPYONLY)