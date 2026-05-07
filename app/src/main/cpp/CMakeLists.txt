cmake_minimum_required(VERSION 3.22.1)
project("beebem" CXX)

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/core)
set(PLAT ${CMAKE_CURRENT_SOURCE_DIR}/platform)

# ---------------------------------------------------------------------------
# Portable BeebEm core — same set as macOS CMakeLists, minus SDL/macOS files.
# ---------------------------------------------------------------------------
set(BEEBEM_CORE_SOURCES
    ${CORE}/6502core.cpp
    ${CORE}/Arm.cpp
    ${CORE}/ArmDisassembler.cpp
    ${CORE}/AtoDConv.cpp
    ${CORE}/AviWriter.cpp
    ${CORE}/Bcd.cpp
    ${CORE}/BeebMem.cpp
    ${CORE}/BeebWin.cpp
    ${CORE}/BeebWinIo.cpp
    ${CORE}/BeebWinPrefs.cpp
    ${CORE}/BeebWinSpeech.cpp
    ${CORE}/ComboBox.cpp
    ${CORE}/Csw.cpp
    ${CORE}/Debug.cpp
    ${CORE}/DebugTrace.cpp
    ${CORE}/Dialog.cpp
    ${CORE}/Disc1770.cpp
    ${CORE}/Disc8271.cpp
    ${CORE}/DiscEdit.cpp
    ${CORE}/DiscInfo.cpp
    ${CORE}/Econet.cpp
    ${CORE}/Ext1770.cpp
    ${CORE}/FileDialog.cpp
    ${CORE}/FileType.cpp
    ${CORE}/FileUtils.cpp
    ${CORE}/FolderSelectDialog.cpp
    ${CORE}/IC32Latch.cpp
    ${CORE}/Ide.cpp
    ${CORE}/IP232.cpp
    ${CORE}/JoystickController.cpp
    ${CORE}/JoystickDialog.cpp
    ${CORE}/JoystickOptions.cpp
    ${CORE}/KeyboardLinksDialog.cpp
    ${CORE}/KeyMap.cpp
    ${CORE}/ListView.cpp
    ${CORE}/Log.cpp
    ${CORE}/Master512CoPro.cpp
    ${CORE}/Model.cpp
    ${CORE}/Music5000.cpp
    ${CORE}/PALRom.cpp
    ${CORE}/Preferences.cpp
    ${CORE}/RingBuffer.cpp
    ${CORE}/RomConfigFile.cpp
    ${CORE}/Rtc.cpp
    ${CORE}/Sasi.cpp
    ${CORE}/Scsi.cpp
    ${CORE}/Serial.cpp
    ${CORE}/SerialPort.cpp
    ${CORE}/Socket.cpp
    ${CORE}/Sound.cpp
    ${CORE}/SoundStreamer.cpp
    ${CORE}/Speech.cpp
    ${CORE}/SprowCoPro.cpp
    ${CORE}/StringUtils.cpp
    ${CORE}/SysVia.cpp
    ${CORE}/TapeControlDialog.cpp
    ${CORE}/TapeMap.cpp
    ${CORE}/Teletext.cpp
    ${CORE}/Thread.cpp
    ${CORE}/TouchScreen.cpp
    ${CORE}/Tube.cpp
    ${CORE}/Uef.cpp
    ${CORE}/UefState.cpp
    ${CORE}/UserKeyboardDialog.cpp
    ${CORE}/UserPortBreakoutBox.cpp
    ${CORE}/UserPortRTC.cpp
    ${CORE}/UserVia.cpp
    ${CORE}/Via.cpp
    ${CORE}/Video.cpp
    ${CORE}/WindowUtils.cpp
    ${CORE}/Z80.cpp
    ${CORE}/Z80Disassembler.cpp
    ${CORE}/Z80Support.cpp
    ${CORE}/ARMulator/armdis.cpp
    ${CORE}/ARMulator/armemu.cpp
    ${CORE}/ARMulator/arminit.cpp
    ${CORE}/ARMulator/armstubs.cpp
    ${CORE}/ARMulator/armsupp.cpp
    ${CORE}/ARMulator/thumbemu.cpp
    ${CORE}/Hardware/Acorn1770/Acorn.cpp
    ${CORE}/Hardware/OpusDDOS/Opus.cpp
    ${CORE}/Hardware/Watford/Watford.cpp
)

# ---------------------------------------------------------------------------
# Android platform layer — replaces macos/*.mm / posix/Windows.cpp / SDL
# ---------------------------------------------------------------------------
set(BEEBEM_PLATFORM_SOURCES
    ${PLAT}/android_platform.cpp
    ${PLAT}/android_audio_aaudio.cpp
    ${PLAT}/android_windows.cpp
    ${PLAT}/android_beebpages.cpp
    ${PLAT}/android_main.cpp
    ${PLAT}/android_assets.cpp
    ${PLAT}/android_beebwin.cpp
)

# ---------------------------------------------------------------------------
# JNI bridge
# ---------------------------------------------------------------------------
set(BEEBEM_JNI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/beebem_jni.cpp
)

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
add_library(beebem SHARED
    ${BEEBEM_CORE_SOURCES}
    ${BEEBEM_PLATFORM_SOURCES}
    ${BEEBEM_JNI_SOURCES}
)

# ---------------------------------------------------------------------------
# Include paths — platform/ comes BEFORE core/ so our headers shadow the
# original posix/ and macos/ directories.
# ---------------------------------------------------------------------------
target_include_directories(beebem PRIVATE
    ${PLAT}           # Windows.h, android_internal.h, macos/MacPlatform.h …
    ${CORE}           # all core .h files
    ${CORE}/ARMulator
    ${CORE}/Hardware/Acorn1770
    ${CORE}/Hardware/OpusDDOS
    ${CORE}/Hardware/Watford
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(beebem PRIVATE
    BEEBEM_ANDROID=1
    MODE32               # required by ARMulator
    DATA_DIR=""          # used by BeebWin::SetBeebEmPaths() non-WIN32 else branch
)

# ---------------------------------------------------------------------------
# Compile options
# ---------------------------------------------------------------------------
target_compile_options(beebem PRIVATE
    -std=c++17
    -fexceptions
    -frtti
    -Wall
    -Wno-unused-parameter
    -Wno-deprecated-declarations
    # Force -O3 regardless of CMAKE_BUILD_TYPE (last -O flag wins with clang).
    # Debug-build symbols (-g) are still added by the build type; this only
    # controls the optimiser.
    -O3
    -fomit-frame-pointer
    # ABI-specific: enable NEON auto-vectorisation on arm64 and SSE4.2 on x86_64.
    $<$<STREQUAL:${CMAKE_ANDROID_ARCH_ABI},arm64-v8a>:-march=armv8-a+simd>
    $<$<STREQUAL:${CMAKE_ANDROID_ARCH_ABI},x86_64>:-msse4.2>
    # Thin LTO — enables cross-TU inlining (6502core ↔ beebem_jni hot loop).
    -flto=thin
)

target_link_options(beebem PRIVATE
    -flto=thin
)

# ---------------------------------------------------------------------------
# System libraries
# ---------------------------------------------------------------------------
find_library(log-lib     log)
find_library(android-lib android)
find_library(z-lib       z)
find_library(gles3-lib   GLESv3)
find_library(egl-lib     EGL)
find_library(aaudio-lib  aaudio)

target_link_libraries(beebem
    ${log-lib}
    ${android-lib}
    ${z-lib}
    ${gles3-lib}
    ${egl-lib}
    ${aaudio-lib}
)
