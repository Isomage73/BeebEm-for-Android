package uk.org.beebem.android

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File

object DiscManager {

    fun discsDir(context: Context): File =
        File(context.filesDir, "discs").also { it.mkdirs() }

    fun copyUriToPrivate(context: Context, uri: Uri): File? {
        val name = queryDisplayName(context, uri) ?: return null
        val safe = name.replace(Regex("[^a-zA-Z0-9._-]"), "_")
        val dest = File(discsDir(context), safe)
        return runCatching {
            context.contentResolver.openInputStream(uri)!!.use { input ->
                dest.outputStream().use { output -> input.copyTo(output) }
            }
            dest
        }.getOrNull()
    }

    fun copyPrivateToUri(context: Context, localFile: File, uri: Uri) {
        context.contentResolver.openOutputStream(uri)?.use { output ->
            localFile.inputStream().use { input -> input.copyTo(output) }
        }
    }

    private fun queryDisplayName(context: Context, uri: Uri): String? {
        context.contentResolver.query(
            uri,
            arrayOf(OpenableColumns.DISPLAY_NAME),
            null, null, null
        )?.use { cursor ->
            if (cursor.moveToFirst()) return cursor.getString(0)
        }
        return uri.lastPathSegment?.substringAfterLast('/')
    }
}
