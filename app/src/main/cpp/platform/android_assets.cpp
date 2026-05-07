// Android asset extraction — copies BeebData/ from APK assets to filesDir.

#include <android/asset_manager.h>
#include <android/log.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define LOG_TAG "BeebEm"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool MakeDirs(const char *path) {
    char tmp[4096];
    strncpy(tmp, path, sizeof(tmp) - 1);
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static bool ExtractFile(AAssetManager *mgr, const char *assetPath,
                         const char *destPath) {
    AAsset *asset = AAssetManager_open(mgr, assetPath, AASSET_MODE_STREAMING);
    if (!asset) {
        LOGE("Cannot open asset: %s", assetPath);
        return false;
    }

    FILE *f = fopen(destPath, "wb");
    if (!f) {
        LOGE("Cannot create file: %s (%s)", destPath, strerror(errno));
        AAsset_close(asset);
        return false;
    }

    char buf[8192];
    int bytes;
    while ((bytes = AAsset_read(asset, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, (size_t)bytes, f);
    }
    fclose(f);
    AAsset_close(asset);
    return true;
}

// Extract a directory tree of assets rooted at assetDir into destRoot.
static void ExtractDir(AAssetManager *mgr, const char *assetDir,
                        const char *destRoot) {
    AAssetDir *dir = AAssetManager_openDir(mgr, assetDir);
    if (!dir) return;

    const char *filename;
    while ((filename = AAssetDir_getNextFileName(dir)) != nullptr) {
        char assetPath[4096], destPath[4096];
        if (assetDir[0])
            snprintf(assetPath, sizeof(assetPath), "%s/%s", assetDir, filename);
        else
            snprintf(assetPath, sizeof(assetPath), "%s", filename);
        snprintf(destPath, sizeof(destPath), "%s/%s", destRoot, filename);
        ExtractFile(mgr, assetPath, destPath);
    }
    AAssetDir_close(dir);
}

bool ExtractBeebAssets(AAssetManager *mgr, const char *filesDir) {
    // The asset tree under BeebData/ maps directly to the BeebEm UserData layout.
    // We extract it flat into filesDir so BeebEm's fopen calls find things at:
    //   filesDir/Roms.cfg
    //   filesDir/Preferences.cfg
    //   filesDir/BeebFile/BBC/OS12.rom  etc.

    const char *subdirs[] = {
        "BeebData",
        "BeebData/BeebFile",
        "BeebData/BeebFile/BBC",
        "BeebData/BeebState",
    };
    const char *destdirs[] = {
        filesDir,
        nullptr,  // will be built
        nullptr,
        nullptr,
    };

    // Build destination paths.
    char bbcFilePath[4096], bbcRomPath[4096], beebStatePath[4096];
    snprintf(bbcFilePath,  sizeof(bbcFilePath),  "%s/BeebFile",     filesDir);
    snprintf(bbcRomPath,   sizeof(bbcRomPath),   "%s/BeebFile/BBC", filesDir);
    snprintf(beebStatePath,sizeof(beebStatePath), "%s/BeebState",    filesDir);

    MakeDirs(bbcFilePath);
    MakeDirs(bbcRomPath);
    MakeDirs(beebStatePath);

    // Extract top-level config files.
    ExtractDir(mgr, "BeebData", filesDir);

    // Extract ROMs.
    AAssetDir *romDir = AAssetManager_openDir(mgr, "BeebData/BeebFile/BBC");
    if (romDir) {
        const char *fname;
        while ((fname = AAssetDir_getNextFileName(romDir)) != nullptr) {
            char assetPath[4096], destPath[4096];
            snprintf(assetPath, sizeof(assetPath), "BeebData/BeebFile/BBC/%s", fname);
            snprintf(destPath,  sizeof(destPath),  "%s/%s", bbcRomPath, fname);
            // Only extract if not already present (preserve user-replaced ROMs).
            struct stat st;
            if (stat(destPath, &st) != 0) {
                ExtractFile(mgr, assetPath, destPath);
            }
        }
        AAssetDir_close(romDir);
    }

    LOGI("Assets extracted to %s", filesDir);
    return true;
}

// --------------------------------------------------------------------------
// Bundle path (set from JNI, read by BeebWin::SetBeebEmPaths).
// --------------------------------------------------------------------------

static char s_bundlePath[4096] = {};

void AndroidSetBundlePath(const char *path) {
    if (path) {
        strncpy(s_bundlePath, path, sizeof(s_bundlePath) - 1);
    }
}

const char *GetBundleResourcesPath() {
    return s_bundlePath;
}
