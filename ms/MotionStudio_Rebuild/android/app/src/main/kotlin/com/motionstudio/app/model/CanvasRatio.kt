package com.motionstudio.app.model

/**
 * Delivery canvas shapes. Changing one of these really changes the project
 * canvas the C++ compositor renders into, which is what an editor's aspect
 * selector is supposed to do (it is not a zoom on the preview).
 */
enum class CanvasRatio(val label: String, val width: Int, val height: Int) {
    Portrait9x16("9:16", 1080, 1920),
    Landscape16x9("16:9", 1920, 1080),
    Square1x1("1:1", 1080, 1080),
    Portrait4x5("4:5", 1080, 1350),
    Cinema21x9("21:9", 2560, 1080),
    Classic4x3("4:3", 1440, 1080);

    val aspect: Float get() = width.toFloat() / height.toFloat()

    fun matches(w: Int, h: Int): Boolean {
        if (h <= 0) return false
        return kotlin.math.abs(w.toFloat() / h - aspect) < 0.01f
    }
}

/**
 * Aspect guide drawn over the preview. [Project] follows the composited frame
 * (no guide); the others show where a delivery crop would land.
 */
enum class PreviewRatio(val label: String) {
    Project("PROJECT"),
    R16x9("16:9"),
    R9x16("9:16"),
    R1x1("1:1"),
    R4x5("4:5"),
    R21x9("21:9");

    val aspect: Float?
        get() = when (this) {
            Project -> null
            R16x9 -> 16f / 9f
            R9x16 -> 9f / 16f
            R1x1 -> 1f
            R4x5 -> 4f / 5f
            R21x9 -> 21f / 9f
        }

    fun next(): PreviewRatio = entries[(ordinal + 1) % entries.size]
}
