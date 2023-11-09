#
# cmake -B build/Xcode  -G Xcode -DCMAKE_OSX_ARCHITECTURES=x86_64
#

cmake_minimum_required (VERSION 3.12.0 FATAL_ERROR)

project (Newton
  LANGUAGES C CXX ASM
  VERSION 2.1
)

set (CMAKE_CXX_STANDARD 17)

set ( SRCFILES
  test.cpp
  Cards/CardEvents.cc
  Compression/ArithmeticCompression/Compression.cc
  Compression/Compression.cc
  Compression/CompressionProto.s
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
  Dates.cc
  Dictionaries.cc
  ErrorString.m
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
  Graphics/Screen.cc
  Graphics/ScreenDriver.cc
  Graphics/ScreenDriverProto.s
  IA.cc
  Locales.cc
  Memory\ Manager/FakePointers.cc
  NTK/NTK.cc
  NTK/TranslatorProto.s
  NTKStubs.cc
  NewtonPackage.cc
  OS/PhysicalMemory.cc
  OS/ROMDomainManager.cc
  OS/Timers.cc
  ObjectSystem.cc
  Packages/Packages.cc
  Power.cc
  Preference.cc
  Protocols/ProtoMacro.s
  Protocols/ProtocolProto.s
  Protocols/Protocols.cc
  REP.cc
#  ROMData/forNewton/RSData.s
#  ROMData/forNewton/RSSymbols.s
#  ROMData/forNewton/RefData.s
#  ROMData/forNewton/RefSymbols.s
  Recognition/Array.cc
  Recognition/InkGeometry.cc
  Recognition/RecArea.cc
  Recognition/RecDomain.cc
  Recognition/RecObject.cc
  Recognition/RecType.cc
  Recognition/Transformations.cc
  SerialNumber.cc
  Sound/Codecs/CodecProto.s
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
  Stores/FlashProto.s
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
  Stores/PackageStore.cc
  Stores/PlainSoup.cc
  Stores/Soups.cc
  Stores/StoreDriver.cc
  Stores/StoreMonitorProto.s
  Stores/StoreObjRef.cc
  Stores/StoreProto.s
#  Stores/Store.cc
  Stores/StoreStreamObjects.cc
  Stores/StoreWrapper.cc
  Stores/Stores.cc
  Stores/Tags.cc
  Stores/UnionSoup.cc
  Stubs.cc
  Testing/TestAgent.cc
  Toolbox/Maths.cc
  Utilities/DataStuffing.cc
  Utilities/UStringUtils.c
  Utilities/Unicode.cc
  UtilityClasses/DynamicArray.cc
  UtilityClasses/List.cc
  UtilityClasses/NArray.cc
  UtilityClasses/Queues.cc
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
  Frames/MagicPointers.s
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
  Memory Manager/Handles.cc
  Memory Manager/Heap.cc
  Memory Manager/MemMgr.cc
  Memory Manager/Pointers.cc
  Memory Manager/SafeHeap.cc
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
  ROMData/RefStar.s
  ROMData/ResMaker/32bitObjects.cc
  ROMData/ResMaker/Reporting.cc
  ROMData/ResMaker/ResMaker.cc
  ROMData/ResMaker/SymbolTable.cc
  ROMData/ResMaker/UnRelocate.cc
  ROMData/forNTK/MagicPointers.s
  ROMData/forNTK/RSData.s
  ROMData/forNTK/RSSymbols.s
  ROMData/forNTK/RefData.s
  ROMData/forNTK/RefSymbols.s
  ROMData/forNewton/MagicPointers.s
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
  FILES ${SRCFILES}
)


add_executable (test ${SRCFILES})
target_compile_definitions(test PRIVATE forMac forFramework)
target_include_directories ( test PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/Frames
  ${CMAKE_SOURCE_DIR}/UtilityClasses
  ${CMAKE_SOURCE_DIR}/OS
  ${CMAKE_SOURCE_DIR}/NTK
  ${CMAKE_SOURCE_DIR}/CommAPI
  ${CMAKE_SOURCE_DIR}/Frames/Compiler
  ${CMAKE_SOURCE_DIR}/Views
  ${CMAKE_SOURCE_DIR}/Graphics
  ${CMAKE_SOURCE_DIR}/Protocols
  ${CMAKE_SOURCE_DIR}/Stores
  ${CMAKE_SOURCE_DIR}/Utilities
  ${CMAKE_SOURCE_DIR}/Recognition
  ${CMAKE_SOURCE_DIR}/Cards
  ${CMAKE_SOURCE_DIR}/Sound
  ${CMAKE_SOURCE_DIR}/Sound/Codecs
  ${CMAKE_SOURCE_DIR}/Toolbox
  ${CMAKE_SOURCE_DIR}/Packages
  ${CMAKE_SOURCE_DIR}/Compression
  ${CMAKE_SOURCE_DIR}/Memory\ Manager
  ${CMAKE_SOURCE_DIR}/Communications
  ${CMAKE_SOURCE_DIR}/Compression/LZCompression
  ${CMAKE_SOURCE_DIR}/Compression/ZippyCompression
  ${CMAKE_SOURCE_DIR}/Compression/UnicodeCompression
)

target_link_libraries ( test PRIVATE
  "-framework Cocoa"
  "-framework CoreServices"
  "-framework ApplicationServices"
  "-framework AudioUnit"
  "-framework IOKit"
)
