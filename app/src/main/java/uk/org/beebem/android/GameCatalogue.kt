package uk.org.beebem.android

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.ZipInputStream

private const val CATALOGUE_URL = "http://www.stairwaytohell.com/beebdroid.json"

data class GameEntry(
    val key: String,
    val title: String,
    val publisher: String,
    val coverUrl: String?,
    val diskUrl: String?,
)

object GameCatalogue {

    suspend fun fetch(): List<GameEntry> = withContext(Dispatchers.IO) {
        val conn = URL(CATALOGUE_URL).openConnection() as HttpURLConnection
        conn.connectTimeout = 10_000
        conn.readTimeout = 15_000
        try {
            val raw = conn.inputStream.bufferedReader().readText()
            // The feed uses bare (unquoted) disk: keys — normalise before parsing.
            val text = raw.replace(Regex("""(?<!")\bdisk:"""), "\"disk\":")
            val arr = JSONArray(text)
            (0 until arr.length()).map { i ->
                val obj = arr.getJSONObject(i)
                GameEntry(
                    key       = obj.getString("k"),
                    title     = obj.getString("t"),
                    publisher = obj.optString("pub", ""),
                    coverUrl  = obj.optString("cover").takeIf { it.isNotEmpty() },
                    diskUrl   = obj.optString("disk").takeIf { it.isNotEmpty() },
                )
            }
        } finally {
            conn.disconnect()
        }
    }

    private val DISC_EXTENSIONS = listOf("ssd", "dsd", "adfs", "img", "adf")

    fun localFile(context: Context, game: GameEntry): File? {
        val dir = File(context.filesDir, "discs")
        for (ext in DISC_EXTENSIONS) {
            val f = File(dir, "${game.key}.$ext")
            if (f.exists()) return f
        }
        return null
    }

    fun isInstalled(context: Context, game: GameEntry): Boolean =
        localFile(context, game) != null

    suspend fun download(
        context: Context,
        game: GameEntry,
        onProgress: (Float) -> Unit,
    ): File? = withContext(Dispatchers.IO) {
        val diskUrl = game.diskUrl ?: return@withContext null
        val discsDir = File(context.filesDir, "discs").also { it.mkdirs() }
        val tmp = File(discsDir, "${game.key}.tmp")

        try {
            val conn = URL(diskUrl).openConnection() as HttpURLConnection
            conn.connectTimeout = 10_000
            conn.readTimeout = 60_000
            val contentLength = conn.contentLength.toLong()
            val isZip = conn.contentType?.contains("zip", ignoreCase = true) == true
                || diskUrl.endsWith(".zip", ignoreCase = true)

            var downloaded = 0L
            var entryExt = diskUrl.substringAfterLast('.', "ssd").lowercase()

            conn.inputStream.use { raw ->
                if (isZip) {
                    ZipInputStream(raw).use { zip ->
                        val entry = zip.nextEntry ?: return@withContext null
                        entryExt = entry.name.substringAfterLast('.', "ssd").lowercase()
                        tmp.outputStream().use { out ->
                            val buf = ByteArray(8192)
                            var n: Int
                            while (zip.read(buf).also { n = it } >= 0) {
                                out.write(buf, 0, n)
                                downloaded += n
                                if (contentLength > 0) onProgress(downloaded.toFloat() / contentLength)
                            }
                        }
                    }
                } else {
                    tmp.outputStream().use { out ->
                        val buf = ByteArray(8192)
                        var n: Int
                        while (raw.read(buf).also { n = it } >= 0) {
                            out.write(buf, 0, n)
                            downloaded += n
                            if (contentLength > 0) onProgress(downloaded.toFloat() / contentLength)
                        }
                    }
                }
            }
            conn.disconnect()

            val target = File(discsDir, "${game.key}.$entryExt")
            if (!tmp.renameTo(target)) {
                tmp.copyTo(target, overwrite = true)
                tmp.delete()
            }
            onProgress(1f)
            target
        } catch (_: Exception) {
            tmp.delete()
            null
        }
    }
}
