package uk.org.beebem.android

import android.content.Context
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object StateManager {
    private const val DIR_NAME = "states"
    private const val EXT = ".uefstate"
    private val DATE_FMT = SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US)

    fun statesDir(context: Context): File =
        File(context.filesDir, DIR_NAME).also { it.mkdirs() }

    fun generatePath(context: Context): String =
        File(statesDir(context), DATE_FMT.format(Date()) + EXT).absolutePath

    fun listStates(context: Context): List<File> =
        statesDir(context)
            .listFiles { f -> f.name.endsWith(EXT) }
            ?.sortedByDescending { it.lastModified() }
            ?: emptyList()

    fun delete(file: File) { file.delete() }

    fun displayName(file: File): String =
        file.nameWithoutExtension.replace('_', ' ')
}
