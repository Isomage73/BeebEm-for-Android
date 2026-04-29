package uk.org.beebem.android

import android.content.res.AssetManager
import java.nio.ByteBuffer

object BeebEmNative {
    init { System.loadLibrary("beebem") }

    external fun nativeInit(assetManager: AssetManager, dataDir: String): Boolean
    external fun nativeShutdown()
    external fun nativeReset(hardReset: Boolean)

    external fun nativeRunFrame(frameBuffer: ByteBuffer, widthOut: IntArray, heightOut: IntArray)

    external fun nativeKeyDown(beebRow: Int, beebCol: Int)
    external fun nativeKeyUp(beebRow: Int, beebCol: Int)
    external fun nativeBreakKey(shift: Boolean)

    external fun nativeMountDisc(drive: Int, path: String, writeProtect: Boolean): Boolean
    external fun nativeEjectDisc(drive: Int)
    external fun nativeIsDiscModified(drive: Int): Boolean
    external fun nativeFlushDisc(drive: Int): Boolean

    external fun nativeSaveState(path: String): Boolean
    external fun nativeLoadState(path: String): Boolean

    external fun nativeSetSoundEnabled(enabled: Boolean)
    external fun nativeSetSpeedLimit(percent: Int)
}
