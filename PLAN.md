# BeebEm Android Port — Build Plan for Claude Code

**Goal:** Produce a debuggable, working Android APK that emulates a BBC Micro Model B, ported from `https://github.com/0x1337c0d3/beebem-macos`. The APK must run on a physical Samsung Galaxy S24 (arm64-v8a, Android 14/15) and on the standard Android Studio AVD emulator (x86_64). Required features: on-screen virtual BBC keyboard, and load/save of disc images (SSD/DSD/IMG/ADF).

---

## 0. Working assumptions and ground rules

The macOS port already proved out the architecture we need: the BeebEm emulator core (6502, CRTC, Video ULA, 1770/8271 disc controllers, tape, sound, ARMulator, Z80) is portable C/C++ and was lifted unchanged from beebem-linux. Only the **platform layer** (windowing, rendering, audio, dialogs, file I/O) was rewritten for Cocoa/Metal/CoreAudio. **For Android we do the same trick again:** keep the emulator core unchanged, replace the macOS platform layer with an Android one (Kotlin/Compose UI + JNI bridge + OpenGL ES 3 + AAudio + Storage Access Framework).

Stick to this rule throughout. Every time you are tempted to edit a file under `core/` (the ported emulator core), stop and ask whether the change really belongs in the platform layer instead. The fewer changes to core/, the easier future BeebEm upstream merges will be.

**Licensing — read this before writing any code.** BeebEm is GPL. The Android app and any code you derive from it must therefore be GPL-licensed too. The Acorn ROMs (OS 1.20, BASIC II, DFS) bundled in the source repo's `UserData/` are copyright Acorn/its successors — they are widely redistributed in the BBC retro community for personal use, and the upstream macOS repo ships them, but **do not publish this APK on Google Play with ROMs embedded**. For development and side-loading onto your S24 it's fine to bundle them as `assets/`. If distribution becomes a goal later, switch to a "user provides ROMs" flow (file picker on first run).

---

## 1. Phase plan and milestones

Work in these phases, in order. Do not skip ahead — each phase has an exit criterion, and the project is much harder to debug if multiple subsystems are broken simultaneously.

**Phase 1 — Project skeleton.** ✅ DONE. Empty Android Studio project that builds and launches a black Activity on the S24 and the AVD. No emulator code yet. Exit: APK installs and runs on both targets.
- Actual toolchain: AGP 9.2.0, Gradle 9.4.1, Kotlin 2.1.0 (via kotlin-compose plugin only — AGP 9.x bundles Kotlin, do NOT add kotlin-android plugin separately), compileSdk/targetSdk 36, NDK r30, CMake 4.1.2.
- Theme: android:Theme.Black.NoTitleBar (Theme.Material.* removed in API 36).

**Phase 2 — Native build pipeline.** ✅ DONE. Wire up CMake/NDK to compile the BeebEm core as a `.so`, plus a stub JNI bridge. App still shows a black Activity but logs `BeebEm core initialised` from native code. Exit: `nativeInit()` returns successfully on both targets, `arm64-v8a` and `x86_64` ABIs are both produced.
- Verified on AVD (x86_64): logcat shows "BeebEm core initialised", nativeInit returned true.
- Full JNI surface in BeebEmNative.kt; all Phase 3–7 functions stubbed in beebem_jni.cpp.
- S24 (arm64-v8a) install not yet verified — connect device and run `./gradlew :app:installDebug`.

**Phase 3 — Headless emulation.** Run the 6502 core with ROMs loaded from `assets/`, no display, no input. Verify cycles execute by reading the cycle counter from JNI after 1 second of run. Exit: cycle count is plausible (~2 million cycles/sec for a Beeb).

**Phase 4 — Video output.** OpenGL ES 3 renderer drawing the BeebEm framebuffer as a textured quad. The "BBC Computer 32K" boot banner must be visible. Exit: boot banner renders correctly on both targets at a stable 50 Hz.

**Phase 5 — Audio output.** AAudio low-latency stream wired to the SN76489 sound emulator. Exit: typing `*FX 200,3` then a `SOUND` BASIC command produces audible tones with no obvious crackling.

**Phase 6 — On-screen keyboard.** A Compose overlay rendering the full BBC keyboard layout, mapping touches to BeebEm key events through JNI. Exit: you can type `10 PRINT "HELLO"` `RUN` and see `HELLO` print.

**Phase 7 — Disc images via SAF.** Storage Access Framework integration so the user picks an `.ssd`/`.dsd`/`.img`/`.adf` file via the system file picker, the app copies it into app-private storage, and BeebEm mounts it on Drive 0. Then add save (export the modified disc image back via SAF). Exit: load Elite SSD, boot it (`SHIFT+BREAK`, mapped to a UI button), play one game, save state, save disc image back to user storage.

**Phase 8 — Polish.** Settings (sound on/off, model selection — though Model B is the only required model — speed limiter, joystick mapping if time permits), hardware keyboard support for users with one attached, touch-resize/aspect-ratio of the video pane.

A first working build (boot to BASIC prompt, on-screen keyboard, load/save disc) is achievable through Phase 7. Phase 8 is "nice to have" — descope freely.

---

## 2. Repository and project layout

Create the Android project at the **root** of a fresh repo (not inside the macOS repo). Vendor the macOS port as a git submodule or a one-time copy of `src/` so future upstream merges are tractable. Layout:

```
beebem-android/
├── README.md
├── LICENSE                          # GPL-2.0 (matching upstream)
├── settings.gradle.kts
├── build.gradle.kts                 # root
├── gradle/
├── app/
│   ├── build.gradle.kts             # Android app module
│   ├── src/main/
│   │   ├── AndroidManifest.xml
│   │   ├── java/uk/org/beebem/android/
│   │   │   ├── MainActivity.kt
│   │   │   ├── EmulatorViewModel.kt
│   │   │   ├── BeebEmNative.kt      # JNI declarations
│   │   │   ├── ui/
│   │   │   │   ├── EmulatorScreen.kt
│   │   │   │   ├── BbcKeyboard.kt   # on-screen keyboard composable
│   │   │   │   ├── ToolbarOverlay.kt
│   │   │   │   └── theme/
│   │   │   ├── render/
│   │   │   │   └── BeebGLSurfaceView.kt
│   │   │   ├── audio/
│   │   │   │   └── BeebAudioEngine.kt   # AAudio wrapper (Kotlin side, opt.)
│   │   │   └── disc/
│   │   │       └── DiscImageRepository.kt   # SAF + private storage
│   │   ├── assets/
│   │   │   └── BeebFile/BBC/        # Model B ROMs (OS12.rom, Basic2.rom, DFS-1.20.rom)
│   │   └── res/
│   └── src/main/cpp/
│       ├── CMakeLists.txt
│       ├── jni/
│       │   ├── beebem_jni.cpp       # JNI exports (nativeInit, nativeStep, nativeKeyDown…)
│       │   └── beebem_jni.h
│       ├── platform/                # The Android replacement of the Cocoa/Metal layer
│       │   ├── platform_android.cpp
│       │   ├── android_log.h        # __android_log_print helpers
│       │   ├── android_audio_aaudio.cpp
│       │   ├── android_render_gles.cpp
│       │   ├── android_assets.cpp   # AAssetManager backed file I/O for ROMs
│       │   └── android_paths.cpp    # maps "~/Library/Application Support/BeebEm"
│       └── core/                    # ← imported from beebem-macos/src; KEEP UNCHANGED
│           ├── 6502core.cpp
│           ├── beebmem.cpp
│           ├── disc8271.cpp
│           ├── disc1770.cpp
│           ├── video.cpp
│           ├── sound.cpp
│           ├── ...                  # all of the portable core
│           └── (NO Cocoa/Metal files)
```

**Key idea on the core import:** open `beebem-macos/src/` and split files into three buckets when you copy them across:

- *Pure portable core* (6502, memory, video timing, disc controllers, tape, sound emulation, save-state) → copy to `cpp/core/` verbatim. These are predominantly C++/C with no platform headers.
- *Platform shims* (anything `#include <Cocoa/...>` / `#include <Metal/...>` / `.mm` Objective-C++ files) → **do not copy**. You will write the Android equivalents in `cpp/platform/`.
- *Headers that the core includes but expect the platform to implement* (e.g. a `BeebWin`/`PlatformLayer` interface, log/error functions, file path helpers) → copy the headers to `cpp/core/`, then provide Android-side definitions in `cpp/platform/platform_android.cpp`. This is the seam.

When in doubt, try compiling. Anything that won't build cleanly with `clang++ --target=aarch64-linux-android` is platform-specific and needs a shim.

---

## 3. Toolchain and Gradle setup

Target/setup the project with these versions (current at time of writing — bump to latest stable when you start):

- **Android Gradle Plugin** 8.7+
- **Kotlin** 2.0+
- **Jetpack Compose** with Compose BOM 2024.10+
- **NDK** r27 (or whatever ships with Android Studio Ladybug+)
- **CMake** 3.22.1+ (as bundled with the NDK)
- **compileSdk = 35**, **targetSdk = 35**, **minSdk = 28** (covers S24 comfortably; 28 is a reasonable floor for AAudio + modern SAF behaviour)
- **ABIs:** `arm64-v8a` (real S24) and `x86_64` (AVD on Apple Silicon/Intel Macs and modern Linux/Windows). Skip `armeabi-v7a` and `x86` — they're not needed for the stated targets.

In `app/build.gradle.kts`:

```kotlin
android {
    namespace = "uk.org.beebem.android"
    compileSdk = 35
    defaultConfig {
        applicationId = "uk.org.beebem.android"
        minSdk = 28
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
        ndk { abiFilters += listOf("arm64-v8a", "x86_64") }
        externalNativeBuild { cmake { cppFlags += listOf("-std=c++17", "-fexceptions", "-frtti") } }
    }
    externalNativeBuild { cmake { path = file("src/main/cpp/CMakeLists.txt"); version = "3.22.1" } }
    buildFeatures { compose = true }
    packaging { jniLibs { useLegacyPackaging = false } }
}
```

In `cpp/CMakeLists.txt`, build a single `libbeebem.so` from the union of `core/`, `platform/`, and `jni/`. Use `find_library(log-lib log)`, `find_library(android-lib android)`, `find_library(aaudio-lib aaudio)`, `find_library(gles-lib GLESv3)`, `find_library(egl-lib EGL)` and link them all in. Set `add_compile_definitions(BEEBEM_ANDROID=1)` so the platform shim files can `#ifdef` cleanly.

Be ready for **GPL-vs-Play-Store friction**: BeebEm is GPL, and Play Store distribution of GPL apps is workable but has historically caused issues. For now this isn't blocking — you're side-loading to your S24 — but note it for later.

---

## 4. The C++ ↔ Kotlin JNI surface

This is the contract. Define it once in Phase 2 and resist the urge to balloon it; the macOS port shows that a small surface is enough.

```kotlin
// app/src/main/java/uk/org/beebem/android/BeebEmNative.kt
object BeebEmNative {
    init { System.loadLibrary("beebem") }

    // Lifecycle
    external fun nativeInit(assetManager: AssetManager, dataDir: String): Boolean
    external fun nativeShutdown()
    external fun nativeReset(hardReset: Boolean)

    // Per-frame: called from the GL thread. Runs ~1/50s of emulated cycles
    // and produces a frame into the supplied direct ByteBuffer (RGBA8, fixed dims).
    external fun nativeRunFrame(frameBuffer: ByteBuffer, widthOut: IntArray, heightOut: IntArray)

    // Audio: native side calls into AAudio directly; nothing exposed to Kotlin.
    // (Alternative: native fills a ring buffer that an AudioTrack pulls; only
    //  pick this if AAudio integration becomes painful.)

    // Input
    external fun nativeKeyDown(beebRow: Int, beebCol: Int)
    external fun nativeKeyUp(beebRow: Int, beebCol: Int)
    external fun nativeBreakKey(shift: Boolean)   // BREAK / SHIFT+BREAK

    // Disc
    external fun nativeMountDisc(drive: Int, path: String, writeProtect: Boolean): Boolean
    external fun nativeEjectDisc(drive: Int)
    external fun nativeIsDiscModified(drive: Int): Boolean
    external fun nativeFlushDisc(drive: Int): Boolean   // writes pending changes to backing file

    // Save state
    external fun nativeSaveState(path: String): Boolean
    external fun nativeLoadState(path: String): Boolean

    // Settings
    external fun nativeSetSoundEnabled(enabled: Boolean)
    external fun nativeSetSpeedLimit(percent: Int)   // 100 = real Beeb speed
}
```

Implementation notes that will save you time:

- **Pass the framebuffer as a direct `ByteBuffer`.** `GetDirectBufferAddress` returns a stable pointer; the renderer calls `glTexSubImage2D` on it. Do not use `GetByteArrayElements` per frame — it's a copy on most JVMs and shows up immediately in profiling.
- **Drive the emulator from the GL render thread**, not the main thread. `nativeRunFrame` runs the emulator forward by exactly one frame's worth of cycles (40,000 at 2 MHz / 50 Hz) and the renderer immediately uploads and draws. This keeps timing locked to vsync and is exactly what the macOS Metal port does.
- **Audio runs on its own thread** owned by AAudio. The native sound emulator writes samples into a lock-free SPSC ring buffer; the AAudio data callback pops from it. Do not try to pump audio from the render thread.
- **Keys are passed by BBC matrix row/column**, not by ASCII or by Android keycode. The mapping table (`row, col`) lives in Kotlin (so the on-screen keyboard layout is data-driven) and the same row/col is what BeebEm's keyboard scan matrix actually uses internally — you'll find it in `keyboard.cpp` / `BeebWin::TranslateKey`. Look there for the exact matrix and copy the constants.

---

## 5. Audio (AAudio)

Use **AAudio** in low-latency callback mode. It's been the right choice on Android for years and is what the S24 wants:

- Stream type `AAUDIO_PERFORMANCE_MODE_LOW_LATENCY`, `AAUDIO_SHARING_MODE_EXCLUSIVE` if granted, else shared.
- Sample format `AAUDIO_FORMAT_PCM_FLOAT` (or `PCM_I16` if you want to match BeebEm's internal format more closely — measure both).
- Sample rate: query the device's optimal rate via `AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE`; do not hard-code 44100 or 48000. Resample the BeebEm sound output to whatever the device wants.
- Frames-per-burst: query and round your buffer to a multiple of it. Aim for a total buffer of ~3× burst.
- The data callback must never block, allocate, log, or take a mutex held by another thread. Pop from your SPSC ring; if it's empty, output silence and bump an underrun counter. Log underruns from the render thread, not the audio thread.

Put the AAudio code entirely in `cpp/platform/android_audio_aaudio.cpp`. The Kotlin side need not know AAudio exists.

---

## 6. Video (OpenGL ES 3)

Stick with GLES 3 unless you have a reason to use Vulkan — the BeebEm framebuffer is tiny (the Beeb is at most 640×512 pixel-doubled), so this is a textured-quad workload and even Vulkan's setup overhead is wasted effort.

- A `GLSurfaceView` with `setEGLContextClientVersion(3)` and a custom renderer.
- Allocate one `GL_TEXTURE_2D` of `GL_RGBA8` sized to the BeebEm output (typically 640×512). On each frame, call `nativeRunFrame(buf)`, then `glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf)`.
- A trivial vertex+fragment shader pair. Aspect-ratio-correct the quad to the surface (letterbox black bars). Use `GL_NEAREST` filtering by default — the Beeb's 8-colour 320×256 output looks awful with bilinear smoothing. Consider an optional CRT/scanline shader later, but only after Phase 7 is solid.
- Set `setRenderMode(RENDERMODE_CONTINUOUSLY)` and rely on the system to throttle to display vsync. The S24's display is 120 Hz; emulator timing will need to either run 50 fps and let the compositor double frames, or run "as fast as it can" with a frame-pacing wall clock. Start with the former; revisit if it judders.

---

## 7. The on-screen BBC keyboard

This is the user-facing detail that will get noticed first, so spend time on the layout. The real BBC keyboard is a known thing — not QWERTY-with-extras, but very close. Reproduce its arrangement:

- Top row: function keys `f0`–`f9` plus `BREAK`.
- Then standard QWERTY with these BBC quirks: `_` and `£` keys, separate `COPY`, `:` / `*`, `;` / `+`, `↑` / `^`, `]` and `\\` keys, `ESCAPE`, `CTRL`, `SHIFT LOCK`, `CAPS LOCK`, two `SHIFT` keys, `RETURN`, cursor arrows.
- Numeric keypad on real Master 128s only — for Model B, omit it.
- `BREAK` triggers `nativeBreakKey(shift=false)`. `SHIFT+BREAK` (which boots a disc) triggers `nativeBreakKey(shift=true)`. These are not regular key matrix events; on a real BBC `BREAK` is a hardware reset line.

Implementation:

- Build it in **Compose** as a `BbcKeyboard` composable. Each key is a `Box(Modifier.pointerInput { ... detectTapGestures })` that calls `nativeKeyDown`/`nativeKeyUp` with the matrix coordinates baked into a data class.
- Make the layout data-driven: `data class BbcKey(val label: String, val row: Int, val col: Int, val widthUnits: Float = 1f, val sticky: Boolean = false)` and a list-of-rows `List<List<BbcKey>>` that you can edit in one place.
- Sticky keys (`SHIFT`, `CTRL`, `CAPS LOCK`, `SHIFT LOCK`) latch on tap, unlatch on the next non-sticky key release. Add a "lock" gesture (long-press) for users who want to hold them.
- The keyboard must be **hideable** — many users will hook up a Bluetooth keyboard to their S24 and won't want the on-screen one taking up half the display. A toggle in the top toolbar should slide it down. When hidden, route hardware keyboard `KeyEvent`s through a (separate) Android-keycode → BBC-matrix translation table.
- Test in landscape on the S24 first (that's the practical orientation), then make portrait work as a stretch.

---

## 8. Disc images — load and save via SAF

Android 14 makes raw filesystem access painful. Use the **Storage Access Framework** for everything user-facing, and copy disc images into the app's private directory (`context.filesDir`) for the emulator to operate on.

**Load flow:**

1. User taps "Load disc into Drive 0".
2. App fires `Intent(ACTION_OPEN_DOCUMENT)` with MIME `*/*` (filter by extension client-side: `.ssd`, `.dsd`, `.img`, `.adf`).
3. The returned `Uri` is opened with `contentResolver.openInputStream(uri)` and copied byte-for-byte into `${context.filesDir}/discs/<sanitised-name>.ssd`.
4. JNI: `nativeMountDisc(0, fullPath, writeProtect=false)`.
5. UI then shows a `SHIFT+BREAK` button; the user taps it to boot.

**Save flow:**

1. User taps "Save disc image" (only enabled when `nativeIsDiscModified(0)` returns true).
2. `nativeFlushDisc(0)` flushes any in-memory write cache to the private file.
3. Fire `Intent(ACTION_CREATE_DOCUMENT)`, default filename = original name with `.ssd`.
4. Copy the private file to the user-provided `Uri` via `contentResolver.openOutputStream(uri)`.

**Critical detail** — BeebEm's disc-write path writes to a path it was given. Pointing it at the SAF `Uri` directly will fail because BeebEm uses POSIX `fopen`. Always copy in, copy out. Do not be clever with FUSE or `openFileDescriptor`-and-fdopen unless Phase 7 is already working.

Auto-saving: add a setting "Auto-save disc on eject" that re-prompts SAF with the same filename when toggled on. Don't auto-save silently to user storage — modern Android disapproves.

Encapsulate all of this in `disc/DiscImageRepository.kt`.

---

## 9. ROM files — the unavoidable bit

You need the BBC OS ROM, BASIC ROM and (ideally) the DFS ROM to boot a Model B. The macOS repo bundles them at `UserData/BeebFile/BBC/`. Copy these three files into `app/src/main/assets/BeebFile/BBC/`:

- `OS12.rom` (or whichever filename `Roms.cfg` references)
- `Basic2.rom`
- `DFS-1.20.rom`

The platform shim's file-open helper (`android_assets.cpp`) checks `${context.filesDir}/BeebFile/...` first, then falls back to `assets/BeebFile/...` via `AAssetManager`. On first run, copy the asset tree into `filesDir` so users can replace ROMs later.

Also copy `Roms.cfg` (matches whichever model selected — for Model B it's the first 17 lines) and `BeebDiscs/` if the upstream repo includes any sample SSDs you can use for smoke-testing.

---

## 10. Build commands the user will actually run

In `README.md` of the new repo, document exactly:

```bash
# Build a debug APK
./gradlew :app:assembleDebug

# Install onto a connected device (S24 with USB debugging on, or running AVD)
./gradlew :app:installDebug

# Install + launch
./gradlew :app:installDebug && \
  adb shell am start -n uk.org.beebem.android/.MainActivity

# Native-only iteration (fast)
./gradlew :app:externalNativeBuildDebug
```

For the AVD: in Android Studio, use a Pixel-class device with system image API 35 (Android 15), arch `x86_64`, and **enable graphics acceleration** (Hardware - GLES 2.0). The emulator must support GLES 3 — all current AVDs do.

---

## 11. Order of operations for Claude Code

When you (Claude Code) execute this plan, work strictly bottom-up and verify each layer before moving on. Use this checklist as your TODO list and tick items off as you go:

1. Create the empty Android project with Compose, Kotlin 2.0, NDK + CMake. Verify it builds and installs on AVD.
2. Add the C++ source tree under `cpp/`, with an empty `CMakeLists.txt` that builds an empty `libbeebem.so`. Verify the `.so` ships in the APK (`unzip -l app-debug.apk | grep libbeebem`).
3. Clone/copy `beebem-macos/src/` into a temporary location. Triage every file: portable → `cpp/core/`, Cocoa/Metal → discard, ambiguous → leave in `cpp/_triage/` and review one by one. Update `CMakeLists.txt` to compile `core/`. Expect dozens of "undefined reference" errors — these are the platform-layer seams. Catalogue them.
4. For each undefined reference, add an Android implementation in `cpp/platform/`. Most fall into a few categories: logging, file paths, message boxes (no-op or `ALOGI`), screen capture (defer to Phase 8 or stub out), threading helpers (use `std::thread`).
5. Wire `nativeInit` to construct the BeebEm "main" object, load ROMs from assets, and run a few thousand cycles synchronously. Log the PC after each step. This is Phase 3's exit gate.
6. Add the GL renderer and `nativeRunFrame`. The boot banner should appear. (Phase 4.)
7. Add AAudio. (Phase 5.)
8. Add the on-screen keyboard, then verify `10 PRINT "HELLO"` `RUN`. (Phase 6.)
9. Add SAF disc loading, then disc saving. (Phase 7.)
10. Polish (Phase 8).

When you hit a wall, the answer is almost always in `beebem-macos/src/` — find the macOS file that handles the same thing and translate it to Android idioms. The Cocoa/Metal version is your reference implementation; don't reinvent.

---

## 12. Testing matrix

Before declaring victory, verify all of these:

| Scenario | AVD x86_64 | S24 arm64 |
|---|---|---|
| App launches to BBC boot banner | ✓ | ✓ |
| Sound plays on `*FX 200,3` + `SOUND` | ✓ | ✓ |
| On-screen keyboard types into BASIC | ✓ | ✓ |
| Hardware Bluetooth keyboard works | n/a | ✓ |
| Load Welcome disc, `*CAT` lists files | ✓ | ✓ |
| `SHIFT+BREAK` boots Welcome disc | ✓ | ✓ |
| Save modified disc back via SAF | ✓ | ✓ |
| Save state, kill app, restore state | ✓ | ✓ |
| Rotate device, no crash, video re-binds | n/a | ✓ |
| Background app, return, audio resumes | ✓ | ✓ |

A quick regression suite: a 30-second adb script that installs, launches, runs `10 FOR I=0 TO 9: PRINT I: NEXT` `RUN` via input events, and screenshots the result. Worth setting up early.

---

## 13. Things that will probably bite you (pre-mortem)

- **Endianness and struct packing** — the core is fine on ARM64 and x86_64 (both little-endian), but watch for `#pragma pack` differences on save-state files written on macOS vs Android. Save-state portability across platforms is a non-goal; document it as such.
- **`fopen` paths** — anywhere the core calls `fopen("BeebFile/OS12.rom", ...)` will fail on Android because there's no CWD concept the user can rely on. Wrap or `#define` the file I/O to go through `android_assets.cpp` for read paths inside the assets tree, and through `${filesDir}` for writeable paths.
- **Threading models** — beebem-linux's main loop is "do work, sleep, repeat". On Android the lifecycle is owned by the GL thread (and AAudio thread). Don't import the upstream main loop; call into the core from Android's threads instead. The macOS port already did this — copy its threading shape, not beebem-linux's.
- **App being backgrounded** — pause emulation in `onPause`, resume in `onResume`. The audio stream must be stopped, not just muted, or AAudio will spam the log with timeouts. `GLSurfaceView.onPause()` handles the GL side correctly out of the box.
- **Permissions** — modern SAF doesn't need any storage permissions in the manifest. Don't request `READ_EXTERNAL_STORAGE` — it's both unnecessary and (on Android 13+) actively wrong.
- **16 KB page size** — Android 15+ requires 16 KB-aligned native libraries on some devices and Play Store will eventually mandate it. NDK r27 produces compliant binaries by default, but if you bump compilers later, re-check.
- **The S24's foldable cousins aren't a concern** — but the S24 itself ships with One UI 6.1+ on Android 14. No specific Samsung APIs are needed.

---

## 14. What "done" looks like

A debug APK, signed with the default debug keystore, that:

- installs cleanly on a Samsung Galaxy S24 over `adb install` and on a stock Android Studio AVD (API 35, x86_64);
- launches into the BBC boot banner within 2 seconds;
- shows an on-screen BBC keyboard that can be hidden;
- loads an `.ssd` disc image picked from the user's Downloads folder;
- boots that disc with a `SHIFT+BREAK` button;
- writes the modified disc image back to user-chosen storage via SAF;
- produces sound through the device's speakers;
- does not crash on rotation, background/foreground, or app kill/restore.

That is enough to call this project complete. Anything beyond is optional.

---

## 15. References

- Source repo: `https://github.com/0x1337c0d3/beebem-macos` (especially `src/` and `docs/porting-plan.md`).
- Upstream Linux: `https://codeberg.org/chrisn/beebem-linux/` — useful when the macOS port elides something.
- Upstream Windows: `https://github.com/stardot/beebem-windows` — the canonical reference for keyboard matrix mapping, since the Windows version's `TranslateKey` is the most complete.
- BBC Micro keyboard matrix: any "BBC Micro Advanced User Guide" PDF; also documented in BeebEm's source comments.
- AAudio guide: `https://developer.android.com/ndk/guides/audio/aaudio/aaudio`.
- Storage Access Framework: `https://developer.android.com/guide/topics/providers/document-provider`.
