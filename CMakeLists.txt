#
# cmake -B build/Xcode  -G Xcode -DCMAKE_OSX_ARCHITECTURES=x86_64
#
# Always: the newton_ncx library (the object system, for NCX).
# As the top-level project: newtc and the other tools (NEWTON_BUILD_TOOLS),
# the tests (NEWTON_BUILD_TESTS), AddressSanitizer in Debug builds
# (NEWTON_SANITIZE), MessagePad on an Intel Mac (NEWTON_BUILD_MESSAGEPAD).
# As a subproject (add_subdirectory, FetchContent; e.g. NCX): all of these off
# unless the parent sets them, so nothing but what the parent asks for is
# built or fetched.
#

# 3.15: option() keeps a value the parent project set as a normal variable
# (CMP0077); FLTK needs 3.15 too.
cmake_minimum_required (VERSION 3.15)

project (Newton
  LANGUAGES C CXX
  VERSION 2.1
)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set (NEWTON_TOP_LEVEL ON)
else ()
  set (NEWTON_TOP_LEVEL OFF)
endif ()
if (APPLE AND NEWTON_TOP_LEVEL AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  set (newton_messagepad_default ON)
else ()
  set (newton_messagepad_default OFF)   # its kernel and protocols are x86 assembler
endif ()
option (NEWTON_BUILD_TOOLS "Build newtc, newtdump, ResMaker, NewtPlay" ${NEWTON_TOP_LEVEL})
option (NEWTON_SANITIZE "AddressSanitizer in Debug builds of the framework's targets" ${NEWTON_TOP_LEVEL})
option (NEWTON_BUILD_MESSAGEPAD "Build MessagePad (Intel Macs only)" ${newton_messagepad_default})

set (CMAKE_CXX_STANDARD 20)
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)

# AddressSanitizer in all Debug builds (all targets of this directory):
# reports use-after-free, out-of-bounds access and similar memory bugs where
# they happen. A parent project that turns it on must link its programs with
# -fsanitize=address too.
if (NEWTON_SANITIZE AND NOT MSVC)
  set (ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${ASAN_FLAGS}")
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${ASAN_FLAGS}")
  set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif ()

set ( SRCFILES
  newtc.cc
  Dates.cc
  Dictionaries.cc
  ErrorString.cc
  IA.cc
  Locales.cc
  NTKStubs.cc
  NewtonPackage.cc
  ObjectSystem.cc
  ObjectSystemCore.cc
  Power.cc
  Preference.cc
  REP.cc
  SerialNumber.cc
  Stubs.cc
  Matt/BookWriter.cc
  Matt/PackageWriter.cc
  Matt/Printer.cc
  Matt/ObjectPrinter.cc
  Matt/Decompiler.cc
  Matt/AST.cc
  Matt/ASTAdmin.cc
  Matt/ASTDataFlow.cc
  Matt/ASTControlFlow.cc
  Matt/ASTControlFlowHelper.cc
  Matt/ASTPattern.cc
  Matt/ASTControlFlowPatterns.cc
  Matt/PDFGen/pdfgen.c
  Cards/CardEvents.cc
  Compression/ArithmeticCompression/Compression.cc
  Compression/Compression.cc
  Compression/CompressionProto.cc
# Compression/CompressionProto.s
  Compression/LZCompression/CopyOffset_bin.cc
  Compression/LZCompression/LZCompressionData-j.cc
  Compression/LZCompression/LZCompressor.cc
  Compression/LZCompression/LZDecompressor.cc
  Compression/LZCompression/LZStoreCompander.cc
  Compression/LZCompression/PushPopBits.cc
  Compression/QDCompression.cc
  Compression/Relocation.cc
  Compression/SimpleStoreCompander.cc
  Compression/StoreCompanderWrapper.cc
  Compression/UnicodeCompression/Compression.cc
  Compression/ZippyCompression/Compression.cc
  Frames/Arrays.cc
  Frames/CObjectBinaries.cc
  Frames/Compiler/Codegen.cc
  Frames/Compiler/Compiler.cc
  Frames/Compiler/CompilerSupport.cc
  Frames/Compiler/InputStreams.cc
  Frames/Compiler/Lexer.cc
  Frames/Compiler/Parser.cc
  Frames/Compiler/y.tab.c
  Frames/DebugAPI.cc
  Frames/Decompiler.cc
  Frames/Exceptions.cc
  Frames/Frames.cc
  Frames/Globals.cc
  Frames/Interpreter.cc
  Frames/Iterators.cc
  Frames/LargeBinaries.cc
  Frames/Lookup.cc
  Frames/MagicPointers.cc
  Frames/NewtonScript.cc
  Frames/ObjectHeap.cc
  Frames/ObjectPrinter.cc
  Frames/Objects.cc
  Frames/Pipes.cc
  Frames/RefIO.cc
  Frames/RefMemory.cc
  Frames/RefVar.cc
  Frames/RichStrings.cc
  Frames/ScriptInit.cc
  Frames/SortedArrays.cc
  Frames/StreamObjects.cc
  Frames/Strings.cc
  Frames/Symbols.cc
  Graphics/DrawImage.cc
  Graphics/DrawInk.cc
  Graphics/DrawShape.cc
  Graphics/DrawText.cc
  Graphics/Geometry.cc
  Graphics/Ink.cc
  Graphics/InkCompr.c
  Graphics/QDDrawing.cc
  Graphics/QDHost.cc
  Graphics/Screen.cc
  Graphics/ScreenDriver.cc
  Graphics/ScreenDriverProto.cc
#  Graphics/ScreenDriverProto.s
  Memory\ Manager/FakePointers.cc
  NTK/NTK.cc
  NTK/TranslatorProto.cc
#  NTK/TranslatorProto.s
  OS/PhysicalMemory.cc
  OS/ROMDomainManager.cc
  OS/Timers.cc
  Packages/Packages.cc
  Protocols/ProtoMacro.cc
#  Protocols/ProtoMacro.s
  Protocols/ProtocolProto.cc
#  Protocols/ProtocolProto.s
  Protocols/Protocols.cc
  ROMData/forNewton/ROMData.c
  ROMData/forNewton/ROMFunctions.c
  ROMData/forNewton/ROMSymbols.c
  Recognition/Array.cc
  Recognition/InkGeometry.cc
  Recognition/RecArea.cc
  Recognition/RecDomain.cc
  Recognition/RecObject.cc
  Recognition/RecType.cc
  Recognition/Transformations.cc
  Sound/Codecs/CodecProto.cc
#  Sound/Codecs/CodecProto.s
  Sound/Codecs/DTMFCodec.cc
  Sound/Sound.cc
  Stores/CachedReadStore.cc
  Stores/Cursors.cc
  Stores/Entries.cc
  Stores/FaultBlocks.cc
  Stores/Flash.cc
  Stores/FlashBlock.cc
  Stores/FlashCache.cc
  Stores/FlashDriver.cc
  Stores/FlashIterator.cc
  Stores/FlashProto.cc
#  Stores/FlashProto.s
  Stores/FlashRange.cc
  Stores/FlashStore.cc
  Stores/FlashTracker.cc
  Stores/Hints.cc
  Stores/Indexes.cc
  Stores/LargeObjectStore.cc
  Stores/LargeObjects.cc
  Stores/MuxStore.cc
  Stores/NodeCache.cc
  Stores/PSSManager.cc
  Stores/HostStore.cc
  Stores/PackageStore.cc
  Stores/PlainSoup.cc
  Stores/Soups.cc
  Stores/StoreDriver.cc
  Stores/StoreMonitorProto.cc
#  Stores/StoreMonitorProto.s
  Stores/StoreObjRef.cc
  Stores/StoreProto.cc
#  Stores/StoreProto.s
#  Stores/Store.cc
  Stores/StoreStreamObjects.cc
  Stores/StoreWrapper.cc
  Stores/Stores.cc
  Stores/Tags.cc
  Stores/UnionSoup.cc
  Testing/TestAgent.cc
  Toolbox/Maths.cc
  Utilities/DataStuffing.cc
  Utilities/Unimplemented.cc
  Utilities/HostMemory.cc
  Utilities/UStringUtils.c
  Utilities/Unicode.cc
  UtilityClasses/DynamicArray.cc
  UtilityClasses/List.cc
  UtilityClasses/NArray.cc
  UtilityClasses/Queues.cc
)

set ( HEADERFILES
  ROMData/forNewton/ROMObjects.h
  ROMData/forNewton/ROMSymbols.h
  NewtConfig.h
  Newton.h
  NewtonDebug.h
  NewtonErrors.h
  NewtonExceptions.h
  NewtonMemory.h
  NewtonPackage.h
  NewtonTime.h
  NewtonTypes.h
  NewtonWidgets.h
  Matt/BookWriter.h
  Matt/PackageWriter.h
  Matt/Printer.h
  Matt/ObjectPrinter.h
  Matt/Decompiler.h
  Matt/AST.h
  Matt/ASTAdmin.h
  Matt/ASTDataFlow.h
  Matt/ASTControlFlow.h
  Matt/ASTControlFlowHelper.h
  Matt/ASTPattern.h
  Matt/PDFGen/pdfgen.h
  Cards/CardEvents.h
  Compression/Compression.h
  Compression/LZCompression/CopyOffset_bin.h
  Compression/LZCompression/LZCompressionData-j.h
  Compression/LZCompression/LZCompressor.h
  Compression/LZCompression/LZDecompressor.h
  Compression/LZCompression/LZStoreCompander.h
  Compression/LZCompression/PushPopBits.h
  Compression/QDCompression.h
  Compression/Relocation.h
  Compression/SimpleStoreCompander.h
  Compression/StoreCompander.h
  Compression/UnicodeCompression/UnicodeCompression.h
  Compression/ZippyCompression/ZippyStoreDecompression.h
  Frames/Arrays.h
  Frames/CObjectBinaries.h
  Frames/Compiler/Compiler.h
  Frames/Compiler/InputStreams.h
  Frames/Compiler/y.tab.h
  Frames/DebugAPI.h
  Frames/Frames.h
  Frames/Funcs.h
  Frames/Globals.h
  Frames/Interpreter.h
  Frames/Iterators.h
  Frames/LargeBinaries.h
  Frames/Lookup.h
  Frames/MagicPointers.h
  Frames/NewtonScript.h
  Frames/ObjectHeap.h
  Frames/Objects.h
  Frames/Pipes.h
  Frames/RefIO.h
  Frames/RefMemory.h
  Frames/RefVar.h
  Frames/RichStrings.h
  Frames/StreamObjects.h
  Frames/Strings.h
  Frames/Symbols.h
  Graphics/DrawShape.h
  Graphics/DrawText.h
  Graphics/Geometry.h
  Graphics/Ink.h
  Graphics/QDDrawing.h
  Graphics/QDHost.h
  Graphics/Screen.h
  Graphics/ScreenDriver.h
  Memory\ Manager/MemMgr.h
  NTK/NTK.h
  OS/PhysicalMemory.h
  OS/Timers.h
  Packages/PackageEvents.h
  Packages/PackageManager.h
  Packages/PackageParts.h
  Packages/PackageTypes.h
  Packages/PartHandler.h
  Packages/PartPipe.h
  Protocols/Protocols.h
  Recognition/InkGeometry.h
  Recognition/RecArea.h
  Recognition/RecDomain.h
  Recognition/RecObject.h
  Recognition/Transformations.h
  Sound/Codecs/DTMFCodec.h
  Sound/Sound.h
  Stores/CachedReadStore.h
  Stores/Cursors.h
  Stores/Entries.h
  Stores/FaultBlocks.h
  Stores/Flash.h
  Stores/FlashBlock.h
  Stores/FlashCache.h
  Stores/FlashDriver.h
  Stores/FlashIterator.h
  Stores/FlashRange.h
  Stores/FlashStore.h
  Stores/FlashTracker.h
  Stores/Hints.h
  Stores/Indexes.h
  Stores/LargeObjectStore.h
  Stores/LargeObjects.h
  Stores/MuxStore.h
  Stores/NodeCache.h
  Stores/PSSManager.h
  Stores/PackageStore.h
  Stores/PlainSoup.h
  Stores/Soups.h
  Stores/StoreDriver.h
  Stores/StoreStreamObjects.h
  Stores/StoreWrapper.h
  Stores/Stores.h
  Stores/UnionSoup.h
  Testing/TestAgent.h
  Toolbox/Maths.h
  Utilities/DataStuffing.h
  Utilities/Unimplemented.h
  Utilities/HostMemory.h
  Utilities/UStringUtils.h
  Utilities/Unicode.h
  UtilityClasses/DynamicArray.h
  UtilityClasses/List.h
  UtilityClasses/NArray.h
)

SET ( UNUSED_SRC_FILES
  Cards/CardServer.cc
  CommAPI/CMService.cc
  CommAPI/CommOptions.cc
  CommServices/MNPService.cc
  Communications/CMWorld.cc
  Communications/CommManager.cc
  Communications/CommTool.cc
  Communications/CommToolProto.s
  Communications/CommToolProtocol.cc
  Communications/DES.c
  Communications/Docker.cc
  Communications/DockerDynArray.cc
  Communications/Endpoint.cc
  Communications/EndpointClient.cc
  Communications/EndpointEvents.cc
  Communications/EndpointProto.s
  Communications/EzEndpoint.cc
  Communications/LocalTalkTool.cc
  Communications/MNPOptions.cc
  Communications/MNPTool.cc
  Communications/OptionArray.cc
  Communications/SerialEndpoint.cc
  Communications/SerialOptions.cc
  Communications/SerialTool.cc
  Compression/CompressionImpl.s
#  Frames/Compiler/Compiler (original).cc
#  Frames/Compiler/CompilerSupport (original).cc
  Frames/Funcs.cc
  Graphics/NewtQD.cc
  Graphics/Quartz.cc
  Graphics/Regions.cc
  Graphics/ScreenBits.cc
  Graphics/ScreenDriverImpl.s
  Graphics/Shapes.cc
  MPW Tools/AIFtoNTK/AIFtoNTK-0.cc
  MPW Tools/AIFtoNTK/AIFtoNTK.cc
  MPW Tools/AIFtoNTK/MachOtoNTK.cc
  MPW Tools/Packer/Packer.cc
  MPW Tools/Packer/Relocation.cc
  MPW Tools/Packer/Reporting.cc
  MPW Tools/Packer/StringTable.cc
  MPW Tools/Pram/Pram.cc
  MPW Tools/ProtocolGenTool/impl.s
  MPW Tools/ProtocolGenTool/macro.s
  MPW Tools/REx/CRC16.cc
  MPW Tools/REx/Chunk.cc
  MPW Tools/REx/ClauseHandlers.cc
  MPW Tools/REx/Lexer.cc
  MPW Tools/REx/Options.cc
  MPW Tools/REx/PackageAccessor.cc
  MPW Tools/REx/PatchTable.cc
  MPW Tools/REx/REx.cc
  MPW Tools/REx/Reporting.cc
  MPW Tools/REx/Research.cc
  Memory\ Manager/Handles.cc
  Memory\ Manager/Heap.cc
  Memory\ Manager/MemMgr.cc
  Memory\ Manager/Pointers.cc
  Memory\ Manager/SafeHeap.cc
  NTK/TranslatorImpl.s
  OS/AppWorld.cc
  OS/Boot.cc
  OS/Environment.cc
  OS/Gestalt.cc
  OS/Glue.cc
  OS/Interrupt.cc
  OS/KernelMonitor.cc
  OS/KernelObjects.cc
  OS/KernelPhys.cc
  OS/KernelPorts.cc
  OS/KernelSemaphore.cc
  OS/KernelTasks.cc
  OS/MMU.c
  OS/Marshaling.cc
  OS/MemArch.cc
  OS/MemObjManager.cc
  OS/NameServer.cc
  OS/NewtWorld.cc
  OS/PageManager.cc
  OS/PowerManager.cc
  OS/RAM.cc
  OS/ROMExtension.cc
  OS/SWInterrupt.cc
  OS/Scheduler.cc
  OS/Semaphore.cc
  OS/SerialTool.cc
  OS/SharedMem.cc
  OS/StackManager.cc
  OS/SystemEvents.cc
  OS/UserDomain.cc
  OS/UserMonitor.cc
  OS/UserObjects.cc
  OS/UserPorts.cc
  OS/UserSharedMem.cc
  OS/UserTasks.cc
  OS/VirtualMemory.cc
  OS/asm-ARM/SWIBoot.s
  OS/asm-ARM/SWIGlue.s
  OS/asm-ARM/SWIHandler.s
  OS/asm-LP64/SWIGlue.s
  OS/asm-LP64/SWIHandler.s
  OS/asm-PPC/SWIGlue.s
  OS/asm-PPC/SWIHandler.s
  OS/asm-i386/SWIGlue.s
  OS/asm-i386/SWIHandler.s
  Packages/PackageManager.cc
  Packages/PartHandler.cc
  Packages/Parts.cc
  Platform/MPInterface.cc
  Platform/Platform.cc
  Platform/PlatformDriverProto.s
  Platform/Voyager.cc
  Printing/Printing.cc
  Protocols/ProtocolImpl.s
  QD/Quartz.cc
  ROMData/32bitObjectPrinter.cc
  ROMData/ResMaker/32bitObjects.cc
  ROMData/ResMaker/Reporting.cc
  ROMData/ResMaker/ResMaker.cc
  ROMData/ResMaker/SymbolTable.cc
  ROMData/ResMaker/UnRelocate.cc
  ROMData/forNTK/ROMData.c
  ROMData/forNTK/ROMFunctions.c
  ROMData/forNTK/ROMSymbols.c
  Recognition/Arbiter.cc
  Recognition/ClickRecognizer.cc
  Recognition/Controller.cc
  Recognition/EdgeListRecognizer.cc
  Recognition/Inker.cc
  Recognition/Journal.cc
  Recognition/RecStroke.cc
  Recognition/RecUnit.cc
  Recognition/Recognition.cc
  Recognition/Recognizers.cc
  Recognition/SIUnit.cc
  Recognition/ShapeRecognizer.cc
  Recognition/Stroke.cc
  Recognition/StrokeRecognizer.cc
  Recognition/Stroker.cc
  Recognition/Tablet.cc
  Recognition/TabletBuffer.cc
  Recognition/TabletDriver.cc
  Recognition/TabletDriverImpl.s
  Recognition/TabletDriverProto.s
  Recognition/Unit.cc
  Recognition/WordRecognizer.cc
  Sound/Codecs/CodecImpl.s
  Sound/Codecs/GSMCodec.cc
  Sound/Codecs/IMACodec.cc
  Sound/Codecs/MuLawCodec.cc
  Sound/Codecs/gsm-1.0-pl12/src/add.c
  Sound/Codecs/gsm-1.0-pl12/src/code.c
  Sound/Codecs/gsm-1.0-pl12/src/debug.c
  Sound/Codecs/gsm-1.0-pl12/src/decode.c
  Sound/Codecs/gsm-1.0-pl12/src/gsm_create.c
  Sound/Codecs/gsm-1.0-pl12/src/gsm_decode.c
  Sound/Codecs/gsm-1.0-pl12/src/gsm_destroy.c
  Sound/Codecs/gsm-1.0-pl12/src/gsm_encode.c
  Sound/Codecs/gsm-1.0-pl12/src/gsm_explode.c
  Sound/Codecs/gsm-1.0-pl12/src/gsm_implode.c
  Sound/Codecs/gsm-1.0-pl12/src/gsm_option.c
  Sound/Codecs/gsm-1.0-pl12/src/gsm_print.c
  Sound/Codecs/gsm-1.0-pl12/src/long_term.c
  Sound/Codecs/gsm-1.0-pl12/src/lpc.c
  Sound/Codecs/gsm-1.0-pl12/src/preprocess.c
  Sound/Codecs/gsm-1.0-pl12/src/rpe.c
  Sound/Codecs/gsm-1.0-pl12/src/short_term.c
  Sound/Codecs/gsm-1.0-pl12/src/table.c
  Sound/Codecs/gsm-1.0-pl12/src/toast.c
  Sound/Codecs/gsm-1.0-pl12/src/toast_alaw.c
  Sound/Codecs/gsm-1.0-pl12/src/toast_audio.c
  Sound/Codecs/gsm-1.0-pl12/src/toast_lin.c
  Sound/Codecs/gsm-1.0-pl12/src/toast_ulaw.c
  Sound/Codecs/gsm-1.0-pl12/voipster/gsm_decode.c
  Sound/Codecs/gsm-1.0-pl12/voipster/gsm_encode.c
  Sound/Codecs/gsm-1.0-pl12/voipster/gsm_state.c
  Sound/SoundChannel.cc
  Sound/SoundDriver.cc
  Sound/SoundDriverProto.s
  Sound/SoundServer.cc
  Sound/UserSoundChannel.cc
  SplashScreenProto.s
  Startup.cc
  Stores/FlashImpl.s
  Stores/HackStore.cc
  Stores/Indexes copy.cc
  Stores/Store.cc
  Stores/StoreImpl.s
  Stores/StoreMonitorImpl.s
  Stores/StorePackages.cc
  Testing/CommServer.cc
  Toolbox/CompMath.c
  UtilityClasses/Buffer.cc
  UtilityClasses/BufferList.cc
  UtilityClasses/BufferPipe.cc
  UtilityClasses/BufferSegment.cc
  UtilityClasses/CircleBuf.cc
  UtilityClasses/EndpointPipe.cc
  UtilityClasses/Events.cc
  UtilityClasses/MemObject.cc
  UtilityClasses/MemoryPipe.cc
  UtilityClasses/TaskSafeRingBuffer.cc
  UtilityClasses/TimerQueue.cc
  UtilityClasses/TraceEvents.cc
  Views/Alerts.cc
  Views/Animation.cc
  Views/Application.cc
  Views/BusyBox.cc
  Views/DataView.cc
  Views/Debug.cc
  Views/DragAndDrop.cc
  Views/GaugeView.cc
  Views/KeyboardView.cc
  Views/Librarian.cc
  Views/ModalViews.cc
  Views/Notebook.cc
  Views/ParagraphView.cc
  Views/PickView.cc
  Views/PictureView.cc
  Views/PolygonView.cc
  Views/Responder.cc
  Views/RootView.cc
  Views/SplashScreen.cc
  Views/SplashScreenProto.s
  Views/TextView.cc
  Views/View.cc
  Views/ViewObject.cc
  Views/ViewStubs.cc
  Views/ViewUtils.cc
  main.m
)

source_group(
  TREE "${CMAKE_CURRENT_SOURCE_DIR}"
  PREFIX "Source Files"
  FILES ${SRCFILES} ${HEADERFILES}
)


# Build a NewtonScript source file into a target (see Matt/EmbeddedScript.h):
# the .ns file becomes a C++ file that defines `const EmbeddedScript <name>`,
# regenerated whenever the .ns file changes.
function (embed_newtonscript target nsfile name)
  set (out "${CMAKE_CURRENT_BINARY_DIR}/embedded/${name}.cc")
  add_custom_command (
    OUTPUT "${out}"
    COMMAND ${CMAKE_COMMAND} -DIN=${nsfile} -DOUT=${out} -DNAME=${name}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedNewtonScript.cmake
    DEPENDS "${nsfile}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedNewtonScript.cmake"
    COMMENT "Embedding NewtonScript ${nsfile}"
  )
  target_sources (${target} PRIVATE "${out}" "${nsfile}")
  set_source_files_properties ("${nsfile}" PROPERTIES HEADER_FILE_ONLY TRUE)
endfunction ()

#------------------------------------------------------------------------------
#   FLTK, for newton_ncx's drawing (NEWTON_NCX_FLTK) and newtc
#   (NEWTC_USES_FLTK): the parent project's if it has added FLTK (fltk::fltk),
#   else fetched from GitHub (master) and built here, as static libraries:
#   only the libraries, no FLUID, tools, tests, examples, or docs.
#------------------------------------------------------------------------------
macro (newton_use_fltk)
  if (NOT TARGET fltk::fltk)
    include (FetchContent)
    set (FLTK_BUILD_SHARED_LIBS OFF)
    set (FLTK_BUILD_FLUID OFF)
    set (FLTK_BUILD_FLTK_OPTIONS OFF)
    set (FLTK_BUILD_TEST OFF)
    set (FLTK_BUILD_EXAMPLES OFF)
    set (FLTK_BUILD_FORMS OFF)
    set (FLTK_BUILD_GL OFF)
    set (FLTK_BUILD_HTML_DOCS OFF)
    set (FLTK_BUILD_PDF_DOCS OFF)
    FetchContent_Declare (fltk
      GIT_REPOSITORY https://github.com/fltk/fltk.git
      GIT_TAG master
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable (fltk)
    message (STATUS "Newton: FLTK from ${fltk_SOURCE_DIR}")
  endif ()
endmacro ()

if (TARGET fltk::fltk OR NEWTC_USES_FLTK)
  set (newton_ncx_fltk_default ON)
else ()
  set (newton_ncx_fltk_default OFF)
endif ()
option (NEWTON_NCX_FLTK "newton_ncx draws with FLTK (Graphics/QDHost.cc); FLTK: the parent project's, else fetched" ${newton_ncx_fltk_default})

#------------------------------------------------------------------------------
#   newton_ncx: the object system as a static library, for NCX (Newton
#   Connection): objects, NewtonScript (interpreter, compiler), NSOF, stores,
#   compression, packages, Unicode, the ROM's data; and the QuickDraw
#   functions NCX draws with (shapes, bitmaps, pictures, text, ink:
#   Graphics/), on FLTK's current surface with NEWTON_NCX_FLTK, else drawing
#   nothing. Initialized with InitObjectSystemCore() (ObjectSystemCore.cc).
#   No ROM functions (ROMFunctions.c: views, sound, ...), no NTK protocol, no
#   debugger. The sources are what a program that uses all of this links
#   (measured with the linker, NCX 0.3, 0.5); a file added here must build
#   on Windows, Linux and macOS.
#------------------------------------------------------------------------------
set ( NEWTON_NCX_SRCFILES
  Compression/ArithmeticCompression/Compression.cc
  Compression/Compression.cc
  Compression/CompressionProto.cc
  Compression/LZCompression/CopyOffset_bin.cc
  Compression/LZCompression/LZCompressionData-j.cc
  Compression/LZCompression/LZCompressor.cc
  Compression/LZCompression/LZDecompressor.cc
  Compression/LZCompression/LZStoreCompander.cc
  Compression/LZCompression/PushPopBits.cc
  Compression/Relocation.cc
  Compression/SimpleStoreCompander.cc
  Compression/StoreCompanderWrapper.cc
  Compression/UnicodeCompression/Compression.cc
  Compression/ZippyCompression/Compression.cc
  Dates.cc
  Dictionaries.cc
  ErrorString.cc
  Frames/Arrays.cc
  Frames/CObjectBinaries.cc
  Frames/Compiler/Codegen.cc
  Frames/Compiler/Compiler.cc
  Frames/Compiler/CompilerSupport.cc
  Frames/Compiler/InputStreams.cc
  Frames/Compiler/Lexer.cc
  Frames/Compiler/Parser.cc
  Frames/Compiler/y.tab.c
  Frames/DebugAPI.cc
  Frames/Decompiler.cc
  Frames/Exceptions.cc
  Frames/Frames.cc
  Frames/Globals.cc
  Frames/Interpreter.cc
  Frames/Iterators.cc
  Frames/LargeBinaries.cc
  Frames/Lookup.cc
  Frames/MagicPointers.cc
  Frames/NewtonScript.cc
  Frames/ObjectHeap.cc
  Frames/ObjectPrinter.cc
  Frames/Objects.cc
  Frames/Pipes.cc
  Frames/RefIO.cc
  Frames/RefMemory.cc
  Frames/RefVar.cc
  Frames/RichStrings.cc
  Frames/ScriptInit.cc
  Frames/StreamObjects.cc
  Frames/Strings.cc
  Frames/Symbols.cc
  Graphics/DrawImage.cc
  Graphics/DrawInk.cc
  Graphics/DrawShape.cc
  Graphics/DrawText.cc
  Graphics/Geometry.cc
  Graphics/Ink.cc
  Graphics/InkCompr.c
  Graphics/QDDrawing.cc
  Graphics/QDHost.cc
  Host/Pict.cc
  IA.cc
  Locales.cc
  "Memory Manager/FakePointers.cc"
  NTK/TranslatorProto.cc
  NewtonPackage.cc
  OS/PhysicalMemory.cc
  OS/ROMDomainManager.cc
  OS/Timers.cc
  ObjectSystemCore.cc
  Packages/Packages.cc
  Preference.cc
  Protocols/ProtocolProto.cc
  Protocols/Protocols.cc
  REP.cc
  ROMData/forNewton/ROMData.c
  ROMData/forNewton/ROMSymbols.c
  Recognition/Array.cc
  Recognition/RecObject.cc
  SerialNumber.cc
  Stores/CachedReadStore.cc
  Stores/Cursors.cc
  Stores/Entries.cc
  Stores/FaultBlocks.cc
  Stores/Flash.cc
  Stores/FlashBlock.cc
  Stores/FlashCache.cc
  Stores/FlashDriver.cc
  Stores/FlashIterator.cc
  Stores/FlashRange.cc
  Stores/FlashStore.cc
  Stores/FlashTracker.cc
  Stores/Hints.cc
  Stores/HostStore.cc
  Stores/Indexes.cc
  Stores/LargeObjectStore.cc
  Stores/LargeObjects.cc
  Stores/MuxStore.cc
  Stores/NodeCache.cc
  Stores/PSSManager.cc
  Stores/PackageStore.cc
  Stores/PlainSoup.cc
  Stores/Soups.cc
  Stores/StoreDriver.cc
  Stores/StoreMonitorProto.cc
  Stores/StoreObjRef.cc
  Stores/StoreProto.cc
  Stores/StoreStreamObjects.cc
  Stores/StoreWrapper.cc
  Stores/Stores.cc
  Stores/Tags.cc
  Stores/UnionSoup.cc
  Toolbox/Maths.cc
  Utilities/HostMemory.cc
  Utilities/UStringUtils.c
  Utilities/Unicode.cc
  Utilities/Unimplemented.cc
  UtilityClasses/DynamicArray.cc
  UtilityClasses/List.cc
  UtilityClasses/NArray.cc
)
add_library (newton_ncx STATIC ${NEWTON_NCX_SRCFILES})
target_include_directories (newton_ncx PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Frames
  ${CMAKE_CURRENT_SOURCE_DIR}/UtilityClasses
  ${CMAKE_CURRENT_SOURCE_DIR}/OS
  ${CMAKE_CURRENT_SOURCE_DIR}/NTK
  ${CMAKE_CURRENT_SOURCE_DIR}/CommAPI
  ${CMAKE_CURRENT_SOURCE_DIR}/Frames/Compiler
  ${CMAKE_CURRENT_SOURCE_DIR}/Views
  ${CMAKE_CURRENT_SOURCE_DIR}/Graphics
  ${CMAKE_CURRENT_SOURCE_DIR}/Protocols
  ${CMAKE_CURRENT_SOURCE_DIR}/Stores
  ${CMAKE_CURRENT_SOURCE_DIR}/Utilities
  ${CMAKE_CURRENT_SOURCE_DIR}/Recognition
  ${CMAKE_CURRENT_SOURCE_DIR}/Cards
  ${CMAKE_CURRENT_SOURCE_DIR}/Sound
  ${CMAKE_CURRENT_SOURCE_DIR}/Sound/Codecs
  ${CMAKE_CURRENT_SOURCE_DIR}/Toolbox
  ${CMAKE_CURRENT_SOURCE_DIR}/Packages
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression
  "${CMAKE_CURRENT_SOURCE_DIR}/Memory Manager"
  ${CMAKE_CURRENT_SOURCE_DIR}/Communications
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression/LZCompression
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression/ZippyCompression
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression/UnicodeCompression
)
# the headers depend on these, so NCX compiles with them too
target_compile_definitions (newton_ncx PUBLIC
  forNTK forMac forFramework hasGlobalConstantFunctions=1 hasPureFunctionSupport=1
  NEWTC_HOST_STORE=1)
if (APPLE)
  target_link_libraries (newton_ncx PUBLIC "-framework CoreFoundation" "-framework IOKit")   # SerialNumber.cc
endif ()
if (NEWTON_NCX_FLTK)
  newton_use_fltk ()
  target_compile_definitions (newton_ncx PRIVATE NEWTON_QD_FLTK=1)
  target_link_libraries (newton_ncx PUBLIC fltk::fltk fltk::images)
endif ()
# the ROM data (ROMData.c) uses C99 designated initializers
set_target_properties (newton_ncx PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
if (MSVC)
  # a pointer cast to a smaller integer (long is 32 bits on 64-bit Windows)
  # is an error, not a warning that truncates silently
  target_compile_options (newton_ncx PRIVATE /we4311 /we4302 /bigobj)
  target_compile_definitions (newton_ncx PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif ()

# The library's test: links only newton_ncx, built as C++17 like NCX
# (Test/ncx/newton_ncx_test.cc; run by Test/ncx/test_newton_ncx.py and ctest).
option (NEWTON_BUILD_TESTS "Build the framework's tests" ${NEWTON_TOP_LEVEL})
if (NEWTON_BUILD_TESTS)
  enable_testing ()
  add_executable (newton_ncx_test Test/ncx/newton_ncx_test.cc)
  target_link_libraries (newton_ncx_test PRIVATE newton_ncx)
  if (NEWTON_NCX_FLTK)
    target_compile_definitions (newton_ncx_test PRIVATE NEWTON_QD_FLTK=1)   # checks the pixels
  endif ()
  set_target_properties (newton_ncx_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  add_test (NAME newton_ncx COMMAND newton_ncx_test ${CMAKE_CURRENT_SOURCE_DIR}/ROMData/NTKFunctions.newtonpkg)
endif ()

#------------------------------------------------------------------------------
#   newton_dock: the wire to a Newton, for NCX, newtc, and newtdump (NCX
#   0.6): the transports (Matt/NTKTransport.h: TCP server and client,
#   serial), MNP (Matt/MNP.h: framing, CRC, the link in a thread of its
#   own), and the Dock's DES (Communications/DES.h). No framework headers,
#   C++17. Headers by their path from here ("Matt/MNP.h"). POSIX and
#   Windows (Winsock; a serial port through a reader thread: NCX 3.1).
#------------------------------------------------------------------------------
add_library (newton_dock STATIC
  Matt/MNP.cc Matt/MNP.h Matt/NTKTransport.cc Matt/NTKTransport.h
  Communications/DES.c Communications/DES.h)
target_include_directories (newton_dock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties (newton_dock PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
find_package (Threads REQUIRED)
target_link_libraries (newton_dock PUBLIC Threads::Threads)
if (WIN32)
  target_link_libraries (newton_dock PUBLIC ws2_32)    # Winsock (Matt/NTKTransport.cc)
endif ()
if (MSVC)
  target_compile_options (newton_dock PRIVATE /we4311 /we4302)
  target_compile_definitions (newton_dock PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif ()

# Its test (Test/dock/newton_dock_test.cc; Test/dock/test_newton_dock.py
# and ctest): framing, CRC, DES, and two links talking over TCP.
if (NEWTON_BUILD_TESTS)
  add_executable (newton_dock_test Test/dock/newton_dock_test.cc)
  target_link_libraries (newton_dock_test PRIVATE newton_dock)
  set_target_properties (newton_dock_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  add_test (NAME newton_dock COMMAND newton_dock_test)
endif ()


# A target with newtc's sources and settings (for NewtPlay below), plus
# more sources.
function (newtc_like target)
  get_target_property (sources newtc SOURCES)
  add_executable (${target} MACOSX_BUNDLE EXCLUDE_FROM_ALL ${sources} ${ARGN})
  foreach (property INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS COMPILE_FEATURES
                    LINK_LIBRARIES LINK_OPTIONS CXX_STANDARD CXX_STANDARD_REQUIRED CXX_EXTENSIONS)
    get_target_property (value newtc ${property})
    if (value)
      set_property (TARGET ${target} PROPERTY ${property} "${value}")
    endif ()
  endforeach ()
endfunction ()


#------------------------------------------------------------------------------
#   The tools: newtc, NewtPlay, newtdump, ResMaker (NEWTON_BUILD_TOOLS)
#------------------------------------------------------------------------------
if (NEWTON_BUILD_TOOLS)

# newtc's and newtdump's version: a tag v<version> releases them
# (.github/workflows/tools.yml checks that the two agree)
set (NEWTON_TOOLS_VERSION "0.4.0")

add_executable (newtc ${SRCFILES} ${HEADERFILES})
target_compile_definitions (newtc PRIVATE NEWTON_TOOLS_VERSION="${NEWTON_TOOLS_VERSION}")

target_sources (newtc PRIVATE Matt/EmbeddedScript.cc Matt/EmbeddedScript.h Matt/PNG.cc Matt/PNG.h Matt/JSON.cc Matt/JSON.h Matt/DAP.cc Matt/DAP.h Matt/NTKInspector.cc Matt/NTKInspector.h Matt/NTKDevice.cc Matt/NTKDevice.h Matt/NTKRemote.cc Matt/NTKRemote.h Matt/LineTables.cc Matt/LineTables.h Matt/EventLoop.cc Matt/EventLoop.h Matt/TestWindow.cc Matt/TestWindow.h Host/Root.cc Host/Root.h Host/Views.cc Host/Views.h Host/ViewMethods.cc Host/ViewMethods.h Host/Graphics.cc Host/Graphics.h Host/Pen.cc Host/Pen.h Host/Shapes.cc Host/Shapes.h Host/Pict.cc Host/Pict.h Host/Timers.cc Host/Timers.h)
target_link_libraries (newtc PRIVATE newton_dock)   # MNP and the transports (-ntk-*, -dap-server)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/NSDebugTools.ns gNSDebugToolsScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/NSDebugToolsInstall.ns gNSDebugToolsInstallScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/NSDShortCuts.ns gNSDShortCutsScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/DAP.ns gDAPScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/DAPRemote.ns gDAPRemoteScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/Agent.ns gAgentScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/Remote.ns gRemoteScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/Connect.ns gConnectScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/AgentNatives.ns gAgentNativesScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Debugger/DebugToolsPackage.ns gDebugToolsPackageScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Platform/Newton21.ns gPlatformScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Platform/NTKBuild.ns gNTKBuildScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Platform/NewtApp.ns gNewtAppScript)
embed_newtonscript (newtc ${CMAKE_CURRENT_SOURCE_DIR}/Matt/Platform/PlatformFile.ns gPlatformFileScript)

# Testing is decompiling and recompiling builds the same object tree
# ./testdec '/Users/matt/dev/Einstein/Sample Code/Application Design/Altered States-6/Using NTK Layout/Altered States.pkg'
# ./build/Xcode/Debug/newtc -script /tmp/newtc_a -debug bc -decompile > /tmp/newtc_c

set_property (TARGET newtc
  PROPERTY XCODE_SCHEME_ARGUMENTS
#    "-nos2"
#    "-pkglist /Users/matt/dev/a.list"
#    "-pkglist /Users/matt/dev/unna.list"
#    "-debug ast"
#    "-debug bc"
#    "-trap part.0.data.theForm.ResetFSM._proto.ObjectToString"
#    "-hello"
##    "-script '/Users/matt/dev/Newton/NTK/Platforms/Newton 2.0 Defs'"
##    "-script /Users/matt/dev/a.txt"
##    "-script /tmp/newtc_a"
##    "-script '/Users/matt/dev/Einstein/Sample Code/Application Design/Altered States-6/Using NTK Layout/Altered States.text'"
##    "-pkg '/Users/matt/dev/Einstein/Sample Code/Sound/SoundStudio-2/SoundStudio.pkg'"
#    "-pkg '/Users/matt/dev/Einstein/Sample Code/Application Design/ChezDTS-2/ChezDTS.pkg'"
    "-pkg '/Users/matt/Azureus/unna2/books/Fiction_Poetry/Shakespeare/A Midsummer Nights Dream.pkg'"
#    "-script /Users/matt/dev/adjusto.txt"
#    "-pkg /Users/matt/hello.pkg"
#    "-pkg /Users/matt/dev/Gauges.pkg"
#    "-pkg /Users/matt/dev/Einstein/loop.ntk.pkg"
#    "-pkg /Users/matt/Azureus/unna2/games/Copycat1.04/CopyCat.pkg"
#    "-nsof /Users/matt/dev/test.nsof"
#    "-script /Users/matt/dev/test.ns"
#    "-run /Users/matt/dev/test.ns"
#    "-nos2"
#    "-s \"1+1;\""
#    "-r \"func() print([1, 2, 3, 4]);\""
#    "-opkg /Users/matt/dev/Einstein/loop.new.pkg"
#    "-onsof /Users/matt/dev/test.out.nsof"
#    "-opkg /Users/matt/hello.pkg"
    "-print"
#    "-decompile"
#    "-stats"
#    "-help"
    "-ohtml ~/dev/test.html"
)

target_compile_definitions(newtc PRIVATE forNTK forMac forFramework hasGlobalConstantFunctions=1 hasPureFunctionSupport=1)
# the internal store keeps its objects in memory and a file (Stores/HostStore.h)
target_compile_definitions(newtc PRIVATE NEWTC_HOST_STORE=1)
# UBSan links a runtime library from the toolchain, so a newtc built with it
# only runs where that library is: the binary bundled with VSNewt is built
# with -DNEWTC_UBSAN=OFF (Matt/tools/build_vsnewt_newtc.sh).
option(NEWTC_UBSAN "Build newtc with the undefined behavior sanitizer" ON)
if (NEWTC_UBSAN AND NOT MSVC)
  target_compile_options(newtc PUBLIC -fsanitize=undefined)
  target_link_options(newtc PUBLIC -fsanitize=undefined)
endif()
target_include_directories (newtc PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Frames
  ${CMAKE_CURRENT_SOURCE_DIR}/UtilityClasses
  ${CMAKE_CURRENT_SOURCE_DIR}/OS
  ${CMAKE_CURRENT_SOURCE_DIR}/NTK
  ${CMAKE_CURRENT_SOURCE_DIR}/CommAPI
  ${CMAKE_CURRENT_SOURCE_DIR}/Frames/Compiler
  ${CMAKE_CURRENT_SOURCE_DIR}/Views
  ${CMAKE_CURRENT_SOURCE_DIR}/Graphics
  ${CMAKE_CURRENT_SOURCE_DIR}/Protocols
  ${CMAKE_CURRENT_SOURCE_DIR}/Stores
  ${CMAKE_CURRENT_SOURCE_DIR}/Utilities
  ${CMAKE_CURRENT_SOURCE_DIR}/Recognition
  ${CMAKE_CURRENT_SOURCE_DIR}/Cards
  ${CMAKE_CURRENT_SOURCE_DIR}/Sound
  ${CMAKE_CURRENT_SOURCE_DIR}/Sound/Codecs
  ${CMAKE_CURRENT_SOURCE_DIR}/Toolbox
  ${CMAKE_CURRENT_SOURCE_DIR}/Packages
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression
  ${CMAKE_CURRENT_SOURCE_DIR}/Memory\ Manager
  ${CMAKE_CURRENT_SOURCE_DIR}/Communications
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression/LZCompression
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression/ZippyCompression
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression/UnicodeCompression
)

if (APPLE)
  target_link_libraries (newtc PRIVATE
    "-framework Cocoa"
    "-framework CoreServices"
    "-framework ApplicationServices"
    "-framework AudioUnit"
    "-framework IOKit"
  )
endif ()

# MSVC: M_PI and friends; no min/max macros from windows.h (FLTK includes it)
if (MSVC)
  target_compile_definitions (newtc PRIVATE _USE_MATH_DEFINES NOMINMAX)
endif ()

# Line editing and history in the break loop, if the system has libedit
# (macOS always has it; Linux if installed). Without it, newtc reads plain
# lines from stdin. Not a required dependency.
find_path (EDITLINE_INCLUDE_DIR editline/readline.h)
find_library (EDITLINE_LIBRARY edit)
if (EDITLINE_INCLUDE_DIR AND EDITLINE_LIBRARY)
  message (STATUS "newtc: line editing with libedit (${EDITLINE_LIBRARY})")
  target_compile_definitions (newtc PRIVATE HAVE_LIBEDIT=1)
  target_include_directories (newtc PRIVATE ${EDITLINE_INCLUDE_DIR})
  target_link_libraries (newtc PRIVATE ${EDITLINE_LIBRARY})
else ()
  message (STATUS "newtc: no libedit, plain line input")
endif ()

# FLTK (experimental, off by default): fetched from GitHub (master) and
# built as part of this project, as static libraries, and linked into
# newtc. Code that uses it checks NEWTC_USES_FLTK. With the option off,
# nothing is fetched.
option(NEWTC_USES_FLTK "Build newtc with FLTK (fetched from GitHub)" OFF)
if (NEWTC_USES_FLTK)
  if (CMAKE_VERSION VERSION_LESS 3.15)
    message (FATAL_ERROR "NEWTC_USES_FLTK needs CMake 3.15 or later (FLTK)")
  endif ()
  newton_use_fltk ()
  message (STATUS "newtc: with FLTK")
  target_compile_definitions (newtc PRIVATE NEWTC_USES_FLTK=1)
  # the Newton views on FLTK (Host/FLTK/, namespace nfl)
  target_sources (newtc PRIVATE
    Host/FLTK/Links.cc Host/FLTK/Links.h
    Host/FLTK/FloatNGo.cc Host/FLTK/FloatNGo.h
    Host/FLTK/Widgets.cc Host/FLTK/Widgets.h Host/FLTK/Pen.cc Host/FLTK/Pen.h
    Host/FLTK/Drawing.cc Host/FLTK/Drawing.h Host/FLTK/InvertMode.cc Host/FLTK/Popup.cc Host/FLTK/Popup.h Host/FLTK/Boxtypes.cc Host/FLTK/Boxtypes.h Host/FLTK/Fonts.cc Host/FLTK/Fonts.h
  )
  target_link_libraries (newtc PRIVATE fltk::fltk fltk::images)
  # the ROM's graphics, lifted into PNGs (Host/FLTK/Images), compiled in
  file (GLOB rom_images CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Host/FLTK/Images/rom_*.png")
  set (rom_images_cc "${CMAKE_CURRENT_BINARY_DIR}/generated/RomImageData.cc")
  add_custom_command (
    OUTPUT "${rom_images_cc}"
    COMMAND ${CMAKE_COMMAND} -DDIR=${CMAKE_CURRENT_SOURCE_DIR}/Host/FLTK/Images -DOUT=${rom_images_cc}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedImages.cmake
    DEPENDS ${rom_images} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedImages.cmake"
    COMMENT "Embedding the ROM's images"
  )
  target_sources (newtc PRIVATE Host/FLTK/RomImages.cc Host/FLTK/RomImages.h "${rom_images_cc}")
endif ()

# NewtPlay (Phase 11, Matt/NewtPlay.h): newtc as a Mac app that runs Newton
# packages, NewtPlay.app. Built with FLTK on macOS, not by default:
#   cmake --build <build> --target NewtPlay   (Matt/tools/build_newtplay.sh)
set (NEWTPLAY_VERSION "0.1" CACHE STRING "NewtPlay's version")
if (NEWTC_USES_FLTK AND APPLE)
  set (newtplay_icns "${CMAKE_CURRENT_BINARY_DIR}/generated/NewtPlay.icns")
  add_custom_command (
    OUTPUT "${newtplay_icns}"
    COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_CURRENT_SOURCE_DIR}/Resources/NewtPlay.png -DOUT=${newtplay_icns}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/generated/NewtPlayIcon -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MakeIcns.cmake
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/NewtPlay.png" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MakeIcns.cmake"
    COMMENT "NewtPlay's icon"
  )
  set_source_files_properties ("${newtplay_icns}" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/NewtPlay.png"
                               PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  newtc_like (NewtPlay Matt/NewtPlay.cc Matt/NewtPlayMake.cc Matt/NewtPlay.h "${newtplay_icns}" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/NewtPlay.png")
  target_compile_definitions (NewtPlay PRIVATE NEWTC_NEWTPLAY=1 NEWTPLAY_VERSION="${NEWTPLAY_VERSION}")
  set_target_properties (NewtPlay PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NewtPlayInfo.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.newton-framework.NewtPlay"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${NEWTPLAY_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${NEWTPLAY_VERSION}"
  )
  add_custom_command (TARGET NewtPlay POST_BUILD
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:NewtPlay>"
    COMMENT "Signing NewtPlay.app (ad hoc)"
  )
endif ()

# newtdump (Matt/NewtDump): reads a Newton's ROM and PCMCIA cards over the
# Docking Protocol. Small: no Newton framework, only the MNP link, the
# transports, DES, and its own NSOF. Its protocol extension (Newton code) is
# compiled by newtc while building (cmake/NewtDumpExtension.cmake).
# newtdump-test-newton plays the Newton for its tests (Test/newtdump).
set (newtdump_extension "${CMAKE_CURRENT_BINARY_DIR}/generated/NewtDumpExtension.cc")
add_custom_command (
  OUTPUT "${newtdump_extension}"
  COMMAND ${CMAKE_COMMAND} -DNEWTC=$<TARGET_FILE:newtc>
          -DIN=${CMAKE_CURRENT_SOURCE_DIR}/Matt/NewtDump/Newton/Extension.ns
          -DNSOF=${CMAKE_CURRENT_BINARY_DIR}/generated/NewtDumpExtension.nsof
          -DOUT=${newtdump_extension} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NewtDumpExtension.cmake
  DEPENDS newtc "${CMAKE_CURRENT_SOURCE_DIR}/Matt/NewtDump/Newton/Extension.ns"
          "${CMAKE_CURRENT_SOURCE_DIR}/Matt/NewtDump/Newton/PhysNatives.ns"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NewtDumpExtension.cmake"
  COMMENT "Compiling newtdump's protocol extension with newtc"
)
set (newtdump_common Matt/NewtDump/NSOF.cc Matt/NewtDump/NSOF.h Matt/NewtDump/DockLink.cc
     Matt/NewtDump/DockLink.h)
add_executable (newtdump Matt/NewtDump/newtdump.cc Matt/NewtDump/CIS.cc Matt/NewtDump/CIS.h
                ${newtdump_common} "${newtdump_extension}")
target_compile_definitions (newtdump PRIVATE NEWTON_TOOLS_VERSION="${NEWTON_TOOLS_VERSION}")
add_executable (newtdump-test-newton Matt/NewtDump/FakeNewton.cc ${newtdump_common})
foreach (target newtdump newtdump-test-newton)
  target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  set_target_properties (${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  target_link_libraries (${target} PRIVATE newton_dock)   # MNP, the transports, DES
endforeach ()

# ResMaker (ROMData/ResMaker): reads the ROM's NewtonScript objects out of
# ROMData/rom.image and writes them as source for the object system
# (ROMData/forNewton, ROMData/forNTK). A tool, not built by default:
#   cmake --build <build> --target ResMaker
# It links newtc's object system (for NTKFunctions.newtonpkg), so it runs
# with the ROM data the repo has and writes new data.
get_target_property (resmaker_sources newtc SOURCES)
list (FILTER resmaker_sources EXCLUDE REGEX "(^|/)newtc\\.cc$")
add_executable (ResMaker EXCLUDE_FROM_ALL ${resmaker_sources}
  ROMData/ResMaker/ResMaker.cc ROMData/ResMaker/32bitObjects.cc
  ROMData/ResMaker/SymbolTable.cc ROMData/ResMaker/SymbolTable.h
  ROMData/ResMaker/Reporting.cc ROMData/ResMaker/Reporting.h
  ROMData/ResMaker/UnRelocate.cc ROMData/ResMaker/NewtcStubs.cc)
foreach (property INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS COMPILE_FEATURES
                  LINK_LIBRARIES LINK_OPTIONS CXX_STANDARD CXX_STANDARD_REQUIRED CXX_EXTENSIONS)
  get_target_property (value newtc ${property})
  if (value)
    set_property (TARGET ResMaker PROPERTY ${property} "${value}")
  endif ()
endforeach ()

endif ()   # NEWTON_BUILD_TOOLS

set ( MP_SRC_FILES
  Views/DragAndDrop.cc
  Views/Application.cc
  OS/AppWorld.cc
  Recognition/Array.cc
  Frames/Arrays.cc
  OS/Boot.cc
  Views/BusyBox.cc
  Communications/CommManager.cc
  UtilityClasses/DynamicArray.cc
  OS/Environment.cc
  Protocols/ProtoMacro.s
  Memory\ Manager/FakePointers.cc
  UtilityClasses/Events.cc
  Frames/Exceptions.cc
  Frames/Frames.cc
  OS/Gestalt.cc
  Frames/Globals.cc
  Frames/Compiler/InputStreams.cc
  OS/Interrupt.cc
  NTK/TranslatorProto.s
  Frames/Iterators.cc
  OS/KernelMonitor.cc
  Communications/Endpoint.cc
  OS/KernelObjects.cc
  OS/KernelPhys.cc
  OS/KernelPorts.cc
  Sound/SoundChannel.cc
  OS/KernelSemaphore.cc
  Communications/EndpointProto.s
  OS/KernelTasks.cc
  Sound/Codecs/MuLawCodec.cc
  Views/Librarian.cc
  UtilityClasses/List.cc
  Frames/Lookup.cc
  Protocols/ProtocolProto.s
  Frames/MagicPointers.cc
  main.m
  Stores/FlashProto.s
  Toolbox/Maths.cc
  OS/MemArch.cc
  UtilityClasses/MemObject.cc
  OS/MemObjManager.cc
  OS/NameServer.cc
  OS/NewtWorld.cc
  Views/Notebook.cc
  Frames/ObjectHeap.cc
  Frames/ObjectPrinter.cc
  Communications/DES.c
  Frames/Objects.cc
  OS/PageManager.cc
  Views/PictureView.cc
  Stores/FlashDriver.cc
  UtilityClasses/Queues.cc
  Recognition/Recognition.cc
  Stores/StoreObjRef.cc
  Frames/RefMemory.cc
  Frames/RefVar.cc
  Views/Responder.cc
  Frames/Compiler/Codegen.cc
  Frames/RichStrings.cc
  OS/asm-LP64/SWIHandler.s
  Views/RootView.cc
  OS/Scheduler.cc
  Sound/SoundDriverProto.s
  OS/Semaphore.cc
  OS/SharedMem.cc
  OS/StackManager.cc
  Frames/Strings.cc
  Frames/Symbols.cc
  OS/SystemEvents.cc
  Recognition/Tablet.cc
  UtilityClasses/TimerQueue.cc
  OS/Timers.cc
  Frames/Compiler/Parser.cc
  Utilities/Unicode.cc
  OS/UserMonitor.cc
  OS/UserObjects.cc
  OS/UserPorts.cc
  OS/UserSharedMem.cc
  OS/UserTasks.cc
  Utilities/HostMemory.cc
  Utilities/UStringUtils.c
  Views/View.cc
  Views/ViewObject.cc
  Views/ViewUtils.cc
  OS/VirtualMemory.cc
  OS/ROMExtension.cc
  Protocols/Protocols.cc
  OS/MMU.c
  OS/PhysicalMemory.cc
  Preference.cc
  Platform/MPController.mm
  Locales.cc
  Frames/Interpreter.cc
  Views/TextView.cc
  Views/Debug.cc
  Graphics/Shapes.cc
  Views/ModalViews.cc
  Recognition/RecObject.cc
  Recognition/RecArea.cc
  Recognition/RecDomain.cc
  Stores/FlashIterator.cc
  Stores/Flash.cc
  Frames/Compiler/Lexer.cc
  Stores/FlashRange.cc
  Recognition/RecType.cc
  OS/asm-LP64/SWIGlue.s
  Frames/LargeBinaries.cc
  Frames/Compiler/Compiler.cc
  Recognition/Stroke.cc
  REP.cc
  Frames/Compiler/y.tab.c
  Frames/Compiler/CompilerSupport.cc
  Communications/CommToolProtocol.cc
  Platform/Platform.cc
  OS/SWInterrupt.cc
  Sound/Codecs/IMACodec.cc
  Stores/StoreMonitorProto.s
  NTK/NTK.cc
  Sound/Codecs/GSMCodec.cc
  Compression/Compression.cc
  Recognition/Inker.cc
  CommAPI/CommOptions.cc
  Printing/Printing.cc
  Frames/ScriptInit.cc
  Frames/StreamObjects.cc
  Packages/Packages.cc
  Frames/Pipes.cc
  Stores/StoreWrapper.cc
  Stores/PackageStore.cc
  OS/ROMDomainManager.cc
  Stores/CachedReadStore.cc
  Stores/FaultBlocks.cc
  Stores/LargeObjectStore.cc
  Stores/Stores.cc
  Compression/UnicodeCompression/Compression.cc
  Compression/ArithmeticCompression/Compression.cc
  Compression/ZippyCompression/Compression.cc
  Compression/LZCompression/CopyOffset_bin.cc
  Compression/LZCompression/LZCompressionData-j.cc
  Compression/LZCompression/LZCompressor.cc
  Stores/MuxStore.cc
  Compression/LZCompression/LZDecompressor.cc
  Compression/LZCompression/LZStoreCompander.cc
  Compression/LZCompression/PushPopBits.cc
  Compression/StoreCompanderWrapper.cc
  Stores/LargeObjects.cc
  Frames/NewtonScript.cc
  Frames/RefIO.cc
  Graphics/DrawImage.cc
  Graphics/DrawInk.cc
  Graphics/DrawShape.cc
  Graphics/DrawText.cc
  Graphics/InkCompr.c
  Views/SplashScreen.cc
  ObjectSystem.cc
  ObjectSystemCore.cc
  Memory\ Manager/Heap.cc
  Memory\ Manager/SafeHeap.cc
  Stores/FlashBlock.cc
  Memory\ Manager/MemMgr.cc
  Sound/Sound.cc
  Sound/Codecs/DTMFCodec.cc
  Sound/UserSoundChannel.cc
  Compression/SimpleStoreCompander.cc
  Communications/DockerDynArray.cc
  UtilityClasses/Buffer.cc
  UtilityClasses/BufferList.cc
  ROMData/forNewton/ROMSymbols.c
  UtilityClasses/BufferPipe.cc
  Stores/FlashStore.cc
  UtilityClasses/BufferSegment.cc
  UtilityClasses/CircleBuf.cc
  Frames/CObjectBinaries.cc
  Sound/Codecs/CodecProto.s
  Compression/CompressionProto.s
  Stores/Cursors.cc
  Dates.cc
  Stores/FlashCache.cc
  Dictionaries.cc
  Stores/Entries.cc
  ErrorString.cc
  Stores/Hints.cc
  IA.cc
  Stubs.cc
  Stores/Indexes.cc
  Graphics/Ink.cc
  OS/Marshaling.cc
  Recognition/InkGeometry.cc
  Recognition/Journal.cc
  UtilityClasses/MemoryPipe.cc
  Stores/NodeCache.cc
  OS/UserDomain.cc
  Communications/OptionArray.cc
  Stores/PlainSoup.cc
  Compression/QDCompression.cc
  Graphics/QDDrawing.cc
  Graphics/Geometry.cc
  Recognition/RecStroke.cc
  Recognition/RecUnit.cc
  Recognition/SIUnit.cc
  Stores/Soups.cc
  Stores/StoreDriver.cc
  Stores/StoreProto.s
  ROMData/forNewton/ROMData.c
  ROMData/forNewton/ROMFunctions.c
  Stores/StoreStreamObjects.cc
  Recognition/Stroker.cc
  Recognition/TabletBuffer.cc
  Stores/Tags.cc
  Testing/TestAgent.cc
  Recognition/Transformations.cc
  Stores/UnionSoup.cc
  Recognition/TabletDriver.cc
  Recognition/TabletDriverProto.s
  UtilityClasses/NArray.cc
  Compression/Relocation.cc
  Views/Animation.cc
  UtilityClasses/EndpointPipe.cc
  Communications/CommTool.cc
  Graphics/ScreenDriverProto.s
  Graphics/ScreenDriver.cc
  Cards/CardEvents.cc
  SerialNumber.cc
  Graphics/Screen.cc
  CommAPI/CMService.cc
  SplashScreenProto.s
  Communications/CommToolProto.s
  Packages/PackageManager.cc
  Recognition/Controller.cc
  Sound/SoundServer.cc
  Graphics/Regions.cc
  Sound/SoundDriver.cc
  Platform/PlatformDriverProto.s
  Views/ViewStubs.cc
  Views/KeyboardView.cc
  Views/ParagraphView.cc
  Views/GaugeView.cc
  Power.cc
  Frames/SortedArrays.cc
  Platform/Voyager.cc
  Startup.cc
  UtilityClasses/TraceEvents.cc
  Cards/CardServer.cc
  Packages/PartHandler.cc
  Views/Alerts.cc
  Stores/PSSManager.cc
  OS/PowerManager.cc
  Packages/Parts.cc
  Communications/CMWorld.cc
  Platform/MPInterface.cc
  Recognition/Recognizers.cc
  Views/PickView.cc
  Frames/Decompiler.cc
  Stores/FlashTracker.cc
  NewtonPackage.cc
  Recognition/Unit.cc
  Recognition/WordRecognizer.cc
  Recognition/ShapeRecognizer.cc
  Recognition/Arbiter.cc
  Communications/SerialOptions.cc
  Recognition/EdgeListRecognizer.cc
  Recognition/StrokeRecognizer.cc
  Recognition/ClickRecognizer.cc
  UtilityClasses/TaskSafeRingBuffer.cc
  Utilities/DataStuffing.cc
  Utilities/Unimplemented.cc
  Frames/DebugAPI.cc
  Views/DataView.cc
)

set ( MP_HEADER_FILES
  CommServices/MNPService.h
  Power/CardPower.h
  Power.h
  MPW\ Tools/ProtocolGenTool/impl.h
  MPW\ Tools/REx/NewtonKit.h
  MPW\ Tools/REx/RExArray.h
  MPW\ Tools/REx/CRC16.h
  MPW\ Tools/REx/Chunk.h
  MPW\ Tools/REx/Reporting.h
  MPW\ Tools/REx/PatchTable.h
  MPW\ Tools/REx/Options.h
  MPW\ Tools/REx/PackageAccessor.h
  MPW\ Tools/REx/ClauseHandlers.h
  MPW\ Tools/REx/Lexer.h
  MPW\ Tools/Packer/NewtonKit.h
  MPW\ Tools/Packer/Allocation.h
  MPW\ Tools/Packer/Reporting.h
  MPW\ Tools/Packer/AIF.h
  MPW\ Tools/Packer/Relocation.h
  MPW\ Tools/Packer/StringTable.h
  REPTranslators.h
  NewtonWidgets.h
  ConfigGlobal.h
  historyCollectorMACRO.h
  HAL/DelayTimer.h
  HAL/HALOptions.h
  HAL/SerialChipV2.h
  HAL/RegisterRdWr.h
  HAL/SerialChip.h
  HAL/SerialChipRegistry.h
  NewtonTypes.h
  Dictionaries.h
  Cards/CardEvents.h
  Platform/MPController.h
  Platform/Platform.h
  Printing/PrintErrors.h
  Printing/DriverCallbacks.h
  Printing/DotDrivers.h
  Printing/ConfigPrinting.h
  Printing/PrintTypes.h
  Lantern/LanternNSEvents.f.h
  Lantern/LanternTypes.h
  Lantern/LanternNS.f.h
  Lantern/LanternErrors.h
  Lantern/LanternEvents.h
  SerialNumber.h
  NewtonTime.h
  NewtonDebug.h
  Stores/StorePackages.h
  Stores/FlashTracker.h
  Stores/PSSManager.h
  Stores/StoreMonitor.h
  Stores/PackageStore.h
  Stores/FlashRange.h
  Stores/Soups.h
  Stores/Stores.h
  Stores/LargeObjects.h
  Stores/CachedReadStore.h
  Stores/Indexes.h
  Stores/FlashCache.h
  Stores/FlashDriver.h
  Stores/LargeObjectStore.h
  Stores/MuxStore.h
  Stores/Hints.h
  Stores/FlashBlock.h
  Stores/NStore.h
  Stores/PSSTypes.h
  Stores/StoreWrapper.h
  Stores/Cursors.h
  Stores/Flash.h
  Stores/FlashStore.h
  Stores/NodeCache.h
  Stores/PlainSoup.h
  Stores/StoreRootObjects.h
  Stores/StoreDriver.h
  Stores/StoreObjRef.h
  Stores/Entries.h
  Stores/FlashIterator.h
  Stores/StoreStreamObjects.h
  Stores/UnionSoup.h
  Stores/Store.h
  Stores/FaultBlocks.h
  Frames/ObjectHeap.h
  Frames/NewtGlobals.h
  Frames/RefIO.h
  Frames/Symbols.h
  Frames/NewtonScript.h
  Frames/DebugAPI.h
  Frames/StreamObjects.h
  Frames/RSSymbols.h
  Frames/RefMemory.h
  Frames/RefStack.h
  Frames/Interpreter.h
  Frames/RefVar.h
  Frames/RSData.h
  Frames/PrecedentsForIO.h
  Frames/Globals.h
  Frames/Objects.h
  Frames/Strings.h
  Frames/LargeBinaries.h
  Frames/MagicPointers.h
  Frames/DeveloperNotification.h
  Frames/Arrays.h
  Frames/ConfigFrames.h
  Frames/ObjHeader.h
  Frames/Pipes.h
  Frames/Lookup.h
  Frames/Funcs.h
  Frames/CObjectBinaries.h
  Frames/FakeFrames.h
  Frames/Ref32.h
  Frames/Iterators.h
  Frames/FrameDefs.h
  Frames/Frames.h
  Frames/Compiler/Compiler.h
  Frames/Compiler/Compiler-AST.h
  Frames/Compiler/InputStreams.h
  Frames/Compiler/Tokens.h
  Frames/Compiler/Opcodes.h
  Frames/Compiler/y.tab.h
  Frames/ROMResources.h
  Frames/RichStrings.h
  NewtonErrors.h
  HammerConfigBits.h
  UtilityClasses/DynamicArray.h
  UtilityClasses/DoubleQ.h
  UtilityClasses/EndpointPipe.h
  UtilityClasses/EventHandler.h
  UtilityClasses/MemObject.h
  UtilityClasses/ListLoop.h
  UtilityClasses/Events.h
  UtilityClasses/MemoryPipe.h
  UtilityClasses/ItemComparer.h
  UtilityClasses/TimerQueue.h
  UtilityClasses/List.h
  UtilityClasses/Buffer.h
  UtilityClasses/SingleQ.h
  UtilityClasses/ListIterator.h
  UtilityClasses/TaskSafeRingBuffer.h
  UtilityClasses/BufferPipe.h
  UtilityClasses/NArray.h
  UtilityClasses/ArrayIterator.h
  UtilityClasses/TraceEvents.h
  UtilityClasses/ItemTester.h
  UtilityClasses/BufferList.h
  UtilityClasses/CircleBuf.h
  UtilityClasses/BufferSegment.h
  NewtonExceptions.h
  Utilities/DataStuffing.h
  Utilities/Unimplemented.h
  Utilities/UnicodePrivate.h
  Utilities/Unicode.h
  Utilities/Sorting.h
  Utilities/HostMemory.h
  Utilities/UStringUtils.h
  Testing/CommServer.h
  Testing/TestAgent.h
  NSOF.h
  NTK/NTKProtocol.h
  NTK/protoProtoEditor.h
  NTK/NTK.h
  ObjectSystem.h
  NewtConfig.h
  Toolbox/CompMath.h
  Toolbox/FixedMath.h
  Toolbox/Maths.h
  Packages/PackageManager.h
  Packages/PackageParts.h
  Packages/PackageEvents.h
  Packages/PartHandler.h
  Packages/PartPipe.h
  Packages/PackageTypes.h
  Compression/UnicodeCompression/UnicodeCompression.h
  Compression/LZCompression/CopyOffset_bin.h
  Compression/LZCompression/LZDecompressor.h
  Compression/LZCompression/PushPopBits.h
  Compression/LZCompression/LZCompressionData-j.h
  Compression/LZCompression/LZCompressor.h
  Compression/LZCompression/LZStoreCompander.h
  Compression/Compression.h
  Compression/ZippyCompression/ZippyStoreDecompression.h
  Compression/StoreCompander.h
  Compression/Relocation.h
  Compression/SimpleStoreCompander.h
  Compression/QDCompression.h
  Dates.h
  NewtonMemory.h
  PCMCIA/CardDefines.h
  PCMCIA/ConfigPCMCIA.h
  PCMCIA/CardPCMCIA.h
  PCMCIA/CardHandler.h
  PCMCIA/CardSocket.h
  OS/UserTasks.h
  OS/Timers.h
  OS/PageManager.h
  OS/Semaphore.h
  OS/KernelTypes.h
  OS/NewtonGestalt.h
  OS/SystemEvents.h
  OS/NewtWorld.h
  OS/MemArch.h
  OS/UserPorts.h
  OS/RAMInfo.h
  OS/SharedTypes.h
  OS/RDM.h
  OS/UserPhys.h
  OS/HammerIO.h
  OS/TaskGlobals.h
  OS/SWI.h
  OS/Scheduler.h
  OS/AppWorld.h
  OS/OSErrors.h
  OS/SharedMem.h
  OS/KernelMonitor.h
  OS/UserMonitor.h
  OS/KernelPhys.h
  OS/KernelTasks.h
  OS/MemoryLanes.h
  OS/PowerManager.h
  OS/MMU.h
  OS/LongTime.h
  OS/KernelPorts.h
  OS/ROMExtension.h
  OS/StackManager.h
  OS/Environment.h
  OS/ConfigOS600.h
  OS/UserGlobals.h
  OS/KernelObjects.h
  OS/KernelGlobals.h
  OS/UserObjects.h
  OS/KernelSemaphore.h
  OS/VirtualMemory.h
  OS/Interrupt.h
  OS/CompactState.h
  OS/PhysicalMemory.h
  OS/UserSharedMem.h
  OS/ImportExportItems.h
  OS/UserDomain.h
  OS/MemObjManager.h
  OS/NameServer.h
  Recognition/WordRecognizer.h
  Recognition/ClickRecognizer.h
  Recognition/Recognizers.h
  Recognition/Arbiter.h
  Recognition/RecUnit.h
  Recognition/RecArea.h
  Recognition/Transformations.h
  Recognition/Unit.h
  Recognition/RecDomain.h
  Recognition/TabletDriver.h
  Recognition/Stroke.h
  Recognition/RecStroke.h
  Recognition/Controller.h
  Recognition/SIUnit.h
  Recognition/TabletBuffer.h
  Recognition/InkGeometry.h
  Recognition/ShapeRecognizer.h
  Recognition/RecObject.h
  Recognition/TabletTypes.h
  Recognition/Journal.h
  Recognition/Tablet.h
  Recognition/InkTypes.h
  Recognition/EdgeListRecognizer.h
  Recognition/StrokeCentral.h
  Recognition/StrokeRecognizer.h
  Recognition/Recognition.h
  Recognition/Inker.h
  Recognition/Stroker.h
  Debugger.h
  Newton.h
  PSS/CompactState.h
  PSS/StoreDriver.h
  NewtonPackage.h
  Graphics/Regions.h
  Graphics/Ink.h
  Graphics/NewtQD.h
  Graphics/Quartz.h
  Graphics/QDTypes.h
  Graphics/Drawing.h
  Graphics/ConfigQD.h
  Graphics/ScreenDriver.h
  Graphics/QDGeometry.h
  Graphics/QDDrawing.h
  Graphics/QDPatterns.h
  Graphics/Paths.h
  Graphics/TextStyles.h
  Graphics/DrawShape.h
  Graphics/PrivateMacTypes.h
  Graphics/Screen.h
  Graphics/DrawText.h
  Graphics/Experimental.h
  Graphics/NewtonQD.h
  Graphics/Geometry.h
  Views/PictureView.h
  Views/Animation.h
  Views/ParagraphView.h
  Views/ErrorNotify.h
  Views/ViewStubs.h
  Views/DataView.h
  Views/SplashScreen.h
  Views/Application.h
  Views/PickView.h
  Views/View.h
  Views/RootView.h
  Views/BusyBox.h
  Views/Modal.h
  Views/Clipboard.h
  Views/DragAndDrop.h
  Views/Alerts.h
  Views/ViewObject.h
  Views/KeyboardKeys.h
  Views/KeyboardView.h
  Views/Librarian.h
  Views/ViewFlags.h
  Views/Responder.h
  Views/GaugeView.h
  Views/Notebook.h
  Views/TextView.h
  Sound/Sound.h
  Sound/UserSoundChannel.h
  Sound/Codecs/MuLawCodec.h
  Sound/Codecs/DTMFCodec.h
  Sound/Codecs/IMACodec.h
  Sound/Codecs/SoundCodec.h
  Sound/Codecs/GSMCodec.h
  Sound/SoundServer.h
  Sound/SoundErrors.h
  Sound/SoundDriver.h
  CommAPI/Endpoint.h
  CommAPI/CommManagerInterface.h
  CommAPI/CommOptions.h
  CommAPI/CommAddresses.h
  CommAPI/CMWorld.h
  CommAPI/CommServices.h
  CommAPI/UserAbort.h
  CommAPI/CMService.h
  CommAPI/Transport.h
  CommAPI/OptionArray.h
  Protocols/Protocols.h
  REP.h
  Communications/SerialEndpoint.h
  Communications/EndpointEvents.h
  Communications/DockProtocol.h
  Communications/SerialOptions.h
  Communications/PMuxService.h
  Communications/PCECallBack.h
  Communications/CommToolProtocolV2.h
  Communications/CRC.h
  Communications/EndpointClient.h
  Communications/CommTool.h
  Communications/PConnectionEnd.h
  Communications/DockerErrors.h
  Communications/PMuxCallBack.h
  Communications/CommErrors.h
  Communications/PMuxTool.h
  Communications/DES.h
  Communications/MNPTool.h
  Communications/MNPOptions.h
  Communications/ConfigCommunications.h
  Communications/Docker.h
  Communications/SerialTool.h
  Communications/CommToolOptions.h
  Communications/CommToolProtocol.h
  Memory\ Manager/SafeHeap.h
  Memory\ Manager/MemMgr.h
  Preference.h
  Locales.h
  ROMData/RSSymbols.h
  ROMData/RSData.h
  ROMData/ResMaker/Reporting.h
  ROMData/ResMaker/SymbolTable.h
)

#------------------------------------------------------------------------------
#   MessagePad (NEWTON_BUILD_MESSAGEPAD): its kernel and protocols are x86
#   assembler, so it builds on Intel Macs only.
#------------------------------------------------------------------------------
if (NEWTON_BUILD_MESSAGEPAD)

enable_language (ASM)
add_executable ( MessagePad ${MP_SRC_FILES} ${MP_HEADER_FILES})
target_compile_definitions( MessagePad PRIVATE forMac forDarkStar )
target_include_directories ( MessagePad PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Frames
  ${CMAKE_CURRENT_SOURCE_DIR}/UtilityClasses
  ${CMAKE_CURRENT_SOURCE_DIR}/OS
  ${CMAKE_CURRENT_SOURCE_DIR}/NTK
  ${CMAKE_CURRENT_SOURCE_DIR}/CommAPI
  ${CMAKE_CURRENT_SOURCE_DIR}/Frames/Compiler
  ${CMAKE_CURRENT_SOURCE_DIR}/Views
  ${CMAKE_CURRENT_SOURCE_DIR}/Graphics
  ${CMAKE_CURRENT_SOURCE_DIR}/Protocols
  ${CMAKE_CURRENT_SOURCE_DIR}/Stores
  ${CMAKE_CURRENT_SOURCE_DIR}/Utilities
  ${CMAKE_CURRENT_SOURCE_DIR}/Recognition
  ${CMAKE_CURRENT_SOURCE_DIR}/Cards
  ${CMAKE_CURRENT_SOURCE_DIR}/Sound
  ${CMAKE_CURRENT_SOURCE_DIR}/Platform
  ${CMAKE_CURRENT_SOURCE_DIR}/Sound/Codecs
  ${CMAKE_CURRENT_SOURCE_DIR}/Toolbox
  ${CMAKE_CURRENT_SOURCE_DIR}/Testing
  ${CMAKE_CURRENT_SOURCE_DIR}/Packages
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression
  ${CMAKE_CURRENT_SOURCE_DIR}/Memory\ Manager
  ${CMAKE_CURRENT_SOURCE_DIR}/Communications
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression/LZCompression
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression/ZippyCompression
  ${CMAKE_CURRENT_SOURCE_DIR}/Compression/UnicodeCompression
  ${CMAKE_CURRENT_SOURCE_DIR}/Sound/Codecs/gsm-1.0-pl12/inc/
)

target_link_libraries ( MessagePad PRIVATE
  "-framework Cocoa"
  "-framework CoreServices"
  "-framework ApplicationServices"
  "-framework AudioUnit"
  "-framework IOKit"
)

endif ()   # NEWTON_BUILD_MESSAGEPAD
