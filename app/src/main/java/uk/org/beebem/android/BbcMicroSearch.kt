package uk.org.beebem.android

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import java.util.zip.ZipInputStream

private const val BASE = "https://www.bbcmicro.co.uk"
private const val UA   = "BeebDroid/1.0 (Android; bbcmicro.co.uk search)"

data class BbcSearchResult(
    val id:        Int,
    val title:     String,
    val publisher: String,
    val year:      String,
    val coverUrl:  String?,
    val discUrl:   String?,
)

object BbcMicroSearch {

    suspend fun search(query: String): List<BbcSearchResult> = withContext(Dispatchers.IO) {
        val enc  = URLEncoder.encode(query, "UTF-8")
        val conn = URL("$BASE/index.php?search=$enc").openConnection() as HttpURLConnection
        conn.setRequestProperty("User-Agent", UA)
        conn.connectTimeout = 10_000
        conn.readTimeout    = 15_000
        val html = try {
            conn.inputStream.bufferedReader().readText()
        } finally {
            conn.disconnect()
        }
        parseResults(html)
    }

    // ── HTML parsing ──────────────────────────────────────────────────────────

    private val STRIP_TAGS = Regex("<[^>]+>")
    private fun stripHtml(s: String) = STRIP_TAGS.replace(s, "").replace("&amp;", "&").trim()

    // Matches the <a><img></a> block that opens each game entry on the page.
    private val ENTRY_START = Regex(
        """<a href="game\.php\?id=(\d+)"><img src="([^"]*)"[^>]*></a>""",
        RegexOption.IGNORE_CASE,
    )
    private val TITLE_RE = Regex("""class="row-title"><a href="[^"]*">([^<]+)</a>""", RegexOption.IGNORE_CASE)
    private val PUB_RE   = Regex("""class="row-pub">(.*?)</div>""", setOf(RegexOption.DOT_MATCHES_ALL, RegexOption.IGNORE_CASE))
    private val YEAR_RE  = Regex("""class="row-dt">.*?(\d{4})""", setOf(RegexOption.DOT_MATCHES_ALL, RegexOption.IGNORE_CASE))
    private val DISC_RE  = Regex("""href="(gameimg/discs/[^"]+)"[^>]*>Download""", RegexOption.IGNORE_CASE)

    private fun parseResults(html: String): List<BbcSearchResult> {
        val entryMatches = ENTRY_START.findAll(html).toList()
        return entryMatches.mapIndexedNotNull { i, m ->
            val blockStart = m.range.first
            val blockEnd   = if (i + 1 < entryMatches.size) entryMatches[i + 1].range.first else html.length
            val block      = html.substring(blockStart, blockEnd)

            val id    = m.groupValues[1].toIntOrNull() ?: return@mapIndexedNotNull null
            val title = TITLE_RE.find(block)?.groupValues?.get(1)?.trim() ?: return@mapIndexedNotNull null

            val imgSrc  = m.groupValues[2]
            val pubHtml = PUB_RE.find(block)?.groupValues?.get(1) ?: ""
            val year    = YEAR_RE.find(block)?.groupValues?.get(1) ?: ""
            val discRel = DISC_RE.find(block)?.groupValues?.get(1) ?: ""

            BbcSearchResult(
                id        = id,
                title     = title,
                publisher = stripHtml(pubHtml),
                year      = year,
                coverUrl  = if (imgSrc.isNotEmpty()) "$BASE/$imgSrc" else null,
                discUrl   = if (discRel.isNotEmpty()) "$BASE/$discRel" else null,
            )
        }
    }

    // ── Local file helpers ────────────────────────────────────────────────────

    private fun discsDir(context: Context) =
        File(context.filesDir, "discs").also { it.mkdirs() }

    private val DISC_EXTS = listOf("ssd", "dsd", "adfs", "img", "adf")

    fun localFile(context: Context, result: BbcSearchResult): File? {
        val dir    = discsDir(context)
        val prefix = "bbcmicro_${result.id}"
        for (ext in DISC_EXTS) {
            val f = File(dir, "$prefix.$ext")
            if (f.exists()) return f
        }
        return null
    }

    fun isDownloaded(context: Context, result: BbcSearchResult): Boolean =
        localFile(context, result) != null

    // ── Metadata sidecar (.json alongside the disc) ───────────────────────────

    private fun metaFile(context: Context, id: Int) =
        File(discsDir(context), "bbcmicro_$id.json")

    private fun saveMeta(context: Context, result: BbcSearchResult) {
        metaFile(context, result.id).writeText(
            JSONObject().apply {
                put("id",        result.id)
                put("title",     result.title)
                put("publisher", result.publisher)
                put("year",      result.year)
                put("coverUrl",  result.coverUrl ?: "")
                put("discUrl",   result.discUrl  ?: "")
            }.toString()
        )
    }

    fun loadAllDownloaded(context: Context): List<BbcSearchResult> {
        val dir = File(context.filesDir, "discs")
        if (!dir.exists()) return emptyList()
        val metaRegex = Regex("bbcmicro_(\\d+)\\.json")
        return dir.listFiles { f -> metaRegex.matches(f.name) }
            ?.mapNotNull { f ->
                try {
                    val j  = JSONObject(f.readText())
                    val id = j.getInt("id")
                    val r  = BbcSearchResult(
                        id        = id,
                        title     = j.getString("title"),
                        publisher = j.optString("publisher", ""),
                        year      = j.optString("year", ""),
                        coverUrl  = j.optString("coverUrl").takeIf { it.isNotEmpty() },
                        discUrl   = j.optString("discUrl").takeIf  { it.isNotEmpty() },
                    )
                    if (isDownloaded(context, r)) r else null
                } catch (_: Exception) { null }
            }
            ?.sortedBy { it.title.lowercase() }
            ?: emptyList()
    }

    // ── Download ──────────────────────────────────────────────────────────────

    suspend fun download(
        context:    Context,
        result:     BbcSearchResult,
        onProgress: (Float) -> Unit,
    ): File? = withContext(Dispatchers.IO) {
        val discUrl = result.discUrl ?: return@withContext null
        val dir     = discsDir(context)
        val tmp     = File(dir, "bbcmicro_${result.id}.tmp")

        try {
            val conn = URL(discUrl).openConnection() as HttpURLConnection
            conn.setRequestProperty("User-Agent", UA)
            conn.connectTimeout = 10_000
            conn.readTimeout    = 60_000

            val contentLength = conn.contentLength.toLong()
            val isZip = conn.contentType?.contains("zip", ignoreCase = true) == true
                || discUrl.endsWith(".zip", ignoreCase = true)

            var entryExt  = discUrl.substringAfterLast('.', "ssd").lowercase()
            var downloaded = 0L

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

            val target = File(dir, "bbcmicro_${result.id}.$entryExt")
            if (!tmp.renameTo(target)) {
                tmp.copyTo(target, overwrite = true)
                tmp.delete()
            }
            saveMeta(context, result)
            onProgress(1f)
            target
        } catch (_: Exception) {
            tmp.delete()
            null
        }
    }
}
