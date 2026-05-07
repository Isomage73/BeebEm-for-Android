#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// Platform headers first — must shadow core headers via include path ordering.
#include "Windows.h"
#include "Main.h"
#include "android_internal.h"

// BeebEm core
#include "6502core.h"
#include "BeebMem.h"
#include "BeebWin.h"
#include "DiscInfo.h"
#include "DiscType.h"
#include "FileType.h"
#include "Sound.h"
#include "SysVia.h"
#include "UefState.h"

#define LOG_TAG "BeebEm"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// --------------------------------------------------------------------------
// Deferred operations — posted from any thread, applied on the GL thread at
// the start of nativeRunFrame so all BeebWin access stays single-threaded.
//
// Two independent pending slots:
//   s_pendingDisc  — latest MOUNT or EJECT (later op wins per drive)
//   s_pendingBreak — latest BREAK or SHIFT_BREAK
//
// Keeping them separate means a rapid mount→break sequence is never lost:
// both are drained in the same DrainOps() call, with the mount applied first.
// --------------------------------------------------------------------------

enum class DiscOpType { NONE, MOUNT, EJECT };

struct PendingDiscOp {
    DiscOpType type = DiscOpType::NONE;
    int        drive = 0;
    char       path[1024] = {};
    bool       writeProtect = false;
};

enum class StateOpType { NONE, SAVE, LOAD };

struct PendingStateOp {
    StateOpType type = StateOpType::NONE;
    char        path[1024] = {};
};

static pthread_mutex_t  s_opMutex      = PTHREAD_MUTEX_INITIALIZER;
static PendingDiscOp    s_pendingDisc;
static PendingStateOp   s_pendingState;
static bool             s_pendingBreak = false;
static bool             s_breakShift   = false;

// Wall-clock timing — declared here so DrainOps() can reset it after Break.
static bool   s_emuTiming    = false;
static double s_emuStartSec  = 0.0;
static double s_lastFrameSec = 0.0;

static void PostDiscOp(const PendingDiscOp &op) {
    pthread_mutex_lock(&s_opMutex);
    s_pendingDisc = op;
    pthread_mutex_unlock(&s_opMutex);
}

static void PostBreakOp(bool shift) {
    pthread_mutex_lock(&s_opMutex);
    s_pendingBreak = true;
    s_breakShift   = shift;
    pthread_mutex_unlock(&s_opMutex);
}

static void PostStateOp(StateOpType type, const char *path) {
    pthread_mutex_lock(&s_opMutex);
    s_pendingState.type = type;
    strncpy(s_pendingState.path, path, sizeof(s_pendingState.path) - 1);
    s_pendingState.path[sizeof(s_pendingState.path) - 1] = '\0';
    pthread_mutex_unlock(&s_opMutex);
}

static void DrainOps() {
    pthread_mutex_lock(&s_opMutex);
    PendingDiscOp  disc      = s_pendingDisc;
    PendingStateOp stateOp   = s_pendingState;
    bool           doBreak   = s_pendingBreak;
    bool           brkShift  = s_breakShift;
    s_pendingDisc.type  = DiscOpType::NONE;
    s_pendingState.type = StateOpType::NONE;
    s_pendingBreak      = false;
    pthread_mutex_unlock(&s_opMutex);

    if (!mainWin) return;

    // Disc op first — disc must be mounted before a SHIFT_BREAK boots it.
    if (disc.type == DiscOpType::EJECT) {
        mainWin->EjectDiscImage(disc.drive);
        LOGI("DrainOps: ejected drive=%d", disc.drive);

    } else if (disc.type == DiscOpType::MOUNT) {
        FileType type = GetFileTypeFromExtension(disc.path);
        bool ok = false;
        if (type == FileType::SSD) {
            ok = NativeFDC
                ? mainWin->Load8271DiscImage(disc.path, disc.drive, 80, DiscType::SSD)
                : mainWin->Load1770DiscImage(disc.path, disc.drive, DiscType::SSD);
        } else if (type == FileType::DSD) {
            ok = NativeFDC
                ? mainWin->Load8271DiscImage(disc.path, disc.drive, 80, DiscType::DSD)
                : mainWin->Load1770DiscImage(disc.path, disc.drive, DiscType::DSD);
        } else if (type == FileType::ADFS) {
            ok = mainWin->Load1770DiscImage(disc.path, disc.drive, DiscType::ADFS);
        } else if (type == FileType::IMG) {
            ok = mainWin->Load1770DiscImage(disc.path, disc.drive, DiscType::IMG);
        } else {
            LOGE("DrainOps: unrecognised extension: %s", disc.path);
        }
        if (ok && disc.writeProtect) mainWin->SetDiscWriteProtect(disc.drive, true);
        LOGI("DrainOps: mounted drive=%d path=%s ok=%d", disc.drive, disc.path, (int)ok);
    }

    // State save/load — happens after disc ops so a freshly-mounted disc
    // is already in the drive when a state is loaded over it.
    if (stateOp.type == StateOpType::SAVE) {
        UEFStateResult r = SaveUEFState(stateOp.path);
        LOGI("DrainOps: SaveUEFState path=%s result=%d", stateOp.path, (int)r);
    } else if (stateOp.type == StateOpType::LOAD) {
        UEFStateResult r = LoadUEFState(stateOp.path);
        LOGI("DrainOps: LoadUEFState path=%s result=%d", stateOp.path, (int)r);
        if (r == UEFStateResult::Success) {
            // Re-anchor wall-clock timing: TotalCycles has changed.
            s_emuTiming = false;
        }
    }

    // Break after disc so the newly-mounted disc is already in the drive.
    if (doBreak) {
        if (brkShift) mainWin->DoShiftBreak();
        else          mainWin->Break();
        // Force timing re-anchor on the next frame: TotalCycles is unchanged but
        // the BBC is in a completely fresh state, so resync wall-clock now.
        s_emuTiming = false;
        LOGI("DrainOps: %s", brkShift ? "ShiftBreak" : "Break");
    }
}

// --------------------------------------------------------------------------
// Wall-clock timing for emulation pacing
// --------------------------------------------------------------------------

static double MonoNow() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

// Realign s_emuStartSec so that TotalCycles matches "now" in wall-clock terms.
// Call this after any gap (GL pause, dialog overlay, etc.) to prevent the
// emulator from running at catch-up speed.
static void ResetEmuTiming(double now) {
    s_emuStartSec  = now - (double)TotalCycles / 2000000.0;
    s_emuTiming    = true;
    s_lastFrameSec = now;
}

// --------------------------------------------------------------------------
// JNI implementations
// --------------------------------------------------------------------------

extern "C" {

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeInit(
        JNIEnv *env, jobject /*obj*/,
        jobject assetManagerObj, jstring dataDirStr) {

    const char *filesDir = env->GetStringUTFChars(dataDirStr, nullptr);
    LOGI("nativeInit: filesDir=%s", filesDir);

    // Extract ROMs and config from APK assets into filesDir on first run.
    AAssetManager *mgr = AAssetManager_fromJava(env, assetManagerObj);
    if (!ExtractBeebAssets(mgr, filesDir)) {
        LOGE("Asset extraction failed");
        env->ReleaseStringUTFChars(dataDirStr, filesDir);
        return JNI_FALSE;
    }

    // Tell GetBundleResourcesPath() where the data lives.
    AndroidSetBundlePath(filesDir);

    // Set HOME to filesDir so the BEEBEM_ANDROID branch in BeebWin constructor
    // sets m_UserDataPath = filesDir/, matching where assets were extracted.
    setenv("HOME", filesDir, 1);

    env->ReleaseStringUTFChars(dataDirStr, filesDir);

    // Create and initialise the emulator.
    mainWin = new BeebWin();
    if (!mainWin->Initialise()) {
        LOGE("BeebWin::Initialise() failed");
        delete mainWin;
        mainWin = nullptr;
        return JNI_FALSE;
    }

    // Emulation is driven by nativeRunFrame() from the GL render thread.
    s_emuTiming = false;

    // Start AAudio output stream now that SoundSampleRate is known.
    AndroidAudioStart(SoundSampleRate);
    LOGI("BeebEm core initialised; audio at %u Hz", SoundSampleRate);

    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeShutdown(
        JNIEnv * /*env*/, jobject /*obj*/) {
    AndroidAudioStop();
    if (mainWin) {
        delete mainWin;
        mainWin = nullptr;
    }
    s_emuTiming = false;
    LOGI("BeebEm core shutdown");
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeReset(
        JNIEnv * /*env*/, jobject /*obj*/, jboolean hardReset) {
    if (mainWin) {
        mainWin->ResetBeebSystem(MachineType, (bool)hardReset);
    }
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeRunFrame(
        JNIEnv *env, jobject /*obj*/,
        jobject frameBuffer, jintArray widthOut, jintArray heightOut, jintArray activeWidthOut) {
    if (!mainWin || done) return JNI_FALSE;

    // Apply pending disc and break ops before running emulation cycles.
    // All BeebWin mutations happen here, on the GL thread — no other thread
    // touches mainWin directly, so no locks are needed inside BeebWin.
    DrainOps();

    // Wall-clock pacing: run exactly as many cycles as a real 2 MHz BBC would
    // have executed since the last frame. Realign the clock after any gap
    // (GL pause, dialog overlay, etc.) to prevent catch-up speed spikes.
    double now = MonoNow();
    if (!s_emuTiming) {
        ResetEmuTiming(now);
    } else if (now - s_lastFrameSec > 0.200) {
        // >200 ms since last frame — GL was paused or dialog was covering us.
        // Realign rather than running at catch-up speed.
        ResetEmuTiming(now);
    }
    s_lastFrameSec = now;

    double elapsed = now - s_emuStartSec;
    long long targetCycles = (long long)(elapsed * 2000000.0);
    long long cyclesToRun  = targetCycles - (long long)TotalCycles;
    if (cyclesToRun > 80000) cyclesToRun = 80000;
    if (cyclesToRun < 0)     cyclesToRun = 0;

    // Use a relative cycle counter so the loop exits correctly even when
    // PollHardware() fires a CycleCountWrap mid-frame (subtracts CycleCountWrap
    // from TotalCycles, resetting it to near 0).  With an absolute stopAt the
    // wrapped TotalCycles would always be < stopAt, producing an infinite loop.
    {
        int  cyclesPrev = TotalCycles;
        long long cyclesRun  = 0;
        while (cyclesRun < cyclesToRun && !done) {
            Exec6502Instruction();
            int cyclesNow = TotalCycles;
            int step = cyclesNow - cyclesPrev;
            if (step < 0) {
                // CycleCountWrap fired: TotalCycles decreased by CycleCountWrap.
                // Count the actual cycles executed and shift the timing origin
                // by the same amount so wall-clock pacing stays correct.
                step += CycleCountWrap;
                s_emuStartSec += (double)CycleCountWrap / 2000000.0;
            }
            cyclesRun  += step;
            cyclesPrev  = cyclesNow;
        }
    }

    // Dimensions are read before GetVideoBufferRGBA so they come from the
    // captured frame that is about to be consumed (consistent with the pixels).
    jint  w    = GetVideoWidth();
    jint  h    = GetVideoHeight();                     // 0 if no frame ready yet
    jint  aw   = GetVideoActiveWidth();
    jlong need = (jlong)w * BEEBEM_BITMAP_HEIGHT * 4;  // full texture size

    // Only expand and upload when a complete BBC frame is available.
    // Callers that pass a tiny probe buffer get newFrame=false and use h/w
    // to detect dimensions; the real upload path skips glTexSubImage2D
    // on frames where the BBC hasn't completed a new VSYNC.
    jboolean newFrame = JNI_FALSE;
    uint8_t *rgba = static_cast<uint8_t *>(env->GetDirectBufferAddress(frameBuffer));
    if (rgba && env->GetDirectBufferCapacity(frameBuffer) >= need) {
        newFrame = GetVideoBufferRGBA(rgba) ? JNI_TRUE : JNI_FALSE;
    }

    env->SetIntArrayRegion(widthOut,       0, 1, &w);
    env->SetIntArrayRegion(heightOut,      0, 1, &h);
    env->SetIntArrayRegion(activeWidthOut, 0, 1, &aw);
    return newFrame;
}

JNIEXPORT jlong JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeGetCycleCount(
        JNIEnv * /*env*/, jobject /*obj*/) {
    return (jlong)TotalCycles;
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeKeyDown(
        JNIEnv * /*env*/, jobject /*obj*/, jint beebRow, jint beebCol) {
    BeebKeyDown((int)beebRow, (int)beebCol);
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeKeyUp(
        JNIEnv * /*env*/, jobject /*obj*/, jint beebRow, jint beebCol) {
    BeebKeyUp((int)beebRow, (int)beebCol);
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeBreakKey(
        JNIEnv * /*env*/, jobject /*obj*/, jboolean shift) {
    if (!mainWin) return;
    // Queue so the call is applied on the GL thread, not the calling thread.
    // DoShiftBreak/Break modify complex BeebWin state; calling them while
    // Exec6502Instruction() is running on another thread corrupts that state.
    PostBreakOp((bool)shift);
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeMountDisc(
        JNIEnv *env, jobject /*obj*/,
        jint drive, jstring pathStr, jboolean writeProtect) {
    if (!mainWin) return JNI_FALSE;

    const char *path = env->GetStringUTFChars(pathStr, nullptr);
    FileType type = GetFileTypeFromExtension(path);
    bool recognised = (type == FileType::SSD || type == FileType::DSD ||
                       type == FileType::ADFS || type == FileType::IMG);
    if (recognised) {
        PendingDiscOp op;
        op.type = DiscOpType::MOUNT;
        op.drive = (int)drive;
        strncpy(op.path, path, sizeof(op.path) - 1);
        op.writeProtect = (bool)writeProtect;
        PostDiscOp(op);
        LOGI("nativeMountDisc queued drive=%d path=%s", (int)drive, path);
    } else {
        LOGE("nativeMountDisc: unrecognised extension: %s", path);
    }
    env->ReleaseStringUTFChars(pathStr, path);
    return recognised ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeEjectDisc(
        JNIEnv * /*env*/, jobject /*obj*/, jint drive) {
    PendingDiscOp op;
    op.type = DiscOpType::EJECT;
    op.drive = (int)drive;
    PostDiscOp(op);
    LOGI("nativeEjectDisc queued drive=%d", (int)drive);
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeFlushDisc(
        JNIEnv * /*env*/, jobject /*obj*/, jint /*drive*/) {
    // 8271 writes directly to the backing file; nothing to flush.
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeSaveState(
        JNIEnv *env, jobject /*obj*/, jstring pathStr) {
    if (!mainWin) return JNI_FALSE;
    const char *path = env->GetStringUTFChars(pathStr, nullptr);
    LOGI("nativeSaveState queued path=%s", path);
    PostStateOp(StateOpType::SAVE, path);
    env->ReleaseStringUTFChars(pathStr, path);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeLoadState(
        JNIEnv *env, jobject /*obj*/, jstring pathStr) {
    if (!mainWin) return JNI_FALSE;
    const char *path = env->GetStringUTFChars(pathStr, nullptr);
    LOGI("nativeLoadState queued path=%s", path);
    PostStateOp(StateOpType::LOAD, path);
    env->ReleaseStringUTFChars(pathStr, path);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeSetSoundEnabled(
        JNIEnv * /*env*/, jobject /*obj*/, jboolean /*enabled*/) {}

JNIEXPORT void JNICALL
Java_uk_org_beebem_android_BeebEmNative_nativeSetSpeedLimit(
        JNIEnv * /*env*/, jobject /*obj*/, jint /*percent*/) {}

} // extern "C"
