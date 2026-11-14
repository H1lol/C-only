package com.motionstudio.app.editor

import android.content.Context
import com.motionstudio.app.native.NativeEngine
import java.io.File

/**
 * Unpacks the bundled title fonts from assets/text once and points the
 * native rasteriser at the folder.
 *
 * stb_truetype needs a real file path, so the assets are copied to
 * `filesDir/fonts` on first use. Every font is SIL OFL 1.1; the licences ship
 * in `third_party/fonts/titles/`.
 */
object TextFonts {

    /** Families bundled with the app, in menu order. */
    val families: List<Pair<String, String>> = listOf(
        "anton" to "Anton",
        "bebas" to "Bebas Neue",
        "archivo" to "Archivo Black",
        "oswald" to "Oswald",
        "playfair" to "Playfair Display",
        "lobster" to "Lobster",
    )

    @Volatile
    private var ready = false

    @Synchronized
    fun ensure(context: Context): Boolean {
        val dir = File(context.filesDir, "fonts")
        if (!ready || !File(dir, "anton.ttf").exists()) {
            if (!dir.exists() && !dir.mkdirs()) return false
            families.forEach { (family, _) ->
                val out = File(dir, "$family.ttf")
                if (out.exists() && out.length() > 0) return@forEach
                try {
                    context.assets.open("text/$family.ttf").use { input ->
                        out.outputStream().use { output -> input.copyTo(output) }
                    }
                } catch (_: Exception) {
                    // A missing asset must not stop the editor from opening.
                }
            }
            ready = true
        }
        NativeEngine.nativeTextSetFontDir(dir.absolutePath)
        return true
    }
}
