package com.motionstudio.app.data

import android.content.Context
import java.io.File

/** Project documents are the native `.msproj` JSON, stored app-privately. */
class ProjectStore(context: Context) {

    private val root: File = File(context.filesDir, "projects").apply { mkdirs() }

    fun save(name: String, json: String): File {
        val file = File(root, "${safeName(name)}.msproj")
        file.writeText(json)
        return file
    }

    fun autosave(name: String, json: String): File {
        val file = File(root, "autosave_${safeName(name)}.msproj")
        file.writeText(json)
        return file
    }

    fun read(file: File): String = if (file.exists()) file.readText() else ""

    fun projects(): List<File> =
        root.listFiles { f -> f.isFile && f.extension == "msproj" && !f.name.startsWith("autosave_") }
            ?.sortedByDescending { it.lastModified() }
            ?: emptyList()

    fun autosaveFor(name: String): File = File(root, "autosave_${safeName(name)}.msproj")

    fun delete(file: File) {
        file.delete()
    }

    fun clearAutosaves() {
        root.listFiles { f -> f.name.startsWith("autosave_") }?.forEach { it.delete() }
    }

    fun projectName(file: File): String =
        file.nameWithoutExtension.replace('_', ' ').ifBlank { "Untitled" }

    private fun safeName(name: String): String {
        val cleaned = name.trim().replace(Regex("[^A-Za-z0-9 _-]"), "_")
        return cleaned.ifBlank { "Untitled" }.take(64)
    }
}
