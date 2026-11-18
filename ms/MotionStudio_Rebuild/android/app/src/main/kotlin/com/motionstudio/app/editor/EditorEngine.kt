package com.motionstudio.app.editor

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.motionstudio.app.audio.NativeAudioPlayer
import com.motionstudio.app.data.ProjectStore
import com.motionstudio.app.diag.Diagnostics
import com.motionstudio.app.model.ProjectDoc
import com.motionstudio.app.native.NativeEngine
import java.io.File

/** Styling for a title clip; mirrors ms::TextStyle one field at a time. */
/** One node in the comp network (mirrors ms::GraphNode). op matches ms::NodeOp. */
data class CompNode(
    val id: Int,
    val op: Int,
    val enabled: Boolean,
    val x: Float,
    val y: Float,
)

data class CompEdge(val from: Int, val to: Int, val slot: Int)

/** A knob link: `param` on `node` follows `srcParam` on `src` (Natron semantics). */
data class CompLink(val node: Int, val param: String, val src: Int, val srcParam: String)

/** One insert on the master audio chain. kind mirrors ms::AudioFxKind. */
data class AudioInsert(val index: Int, val kind: Int, val enabled: Boolean)

val AUDIO_INSERT_NAMES = listOf("Gain", "Parametric EQ", "Compressor", "Delay", "Reverb", "Limiter")

/** One numeric knob a node exposes, with the range the UI should offer. */
data class Knob(
    val name: String,
    val label: String,
    val min: Double,
    val max: Double,
    val default: Double,
)

object CompOps {
    const val READ = 0
    const val WRITE = 1
    const val MERGE = 2
    const val BLUR = 3
    const val GRADE = 4
    const val TRANSFORM = 5
    const val CONSTANT = 6
    const val VIEWER = 7
    const val SATURATION = 8
    const val INVERT = 9
    const val OVER = 10

    fun name(op: Int): String = when (op) {
        READ -> "Read"
        WRITE -> "Write"
        MERGE -> "Merge"
        BLUR -> "Blur"
        GRADE -> "Grade"
        TRANSFORM -> "Transform"
        CONSTANT -> "Constant"
        VIEWER -> "Viewer"
        SATURATION -> "Saturate"
        INVERT -> "Invert"
        OVER -> "Over"
        else -> "Node"
    }

    /**
     * Knobs per node kind. Kept here rather than in the engine so the UI and the
     * link picker agree on names, ranges and defaults.
     */
    fun knobs(op: Int): List<Knob> = when (op) {
        READ -> listOf(Knob("offset", "Offset sec", -30.0, 30.0, 0.0))
        BLUR -> listOf(Knob("radius", "Radius", 0.0, 64.0, 4.0))
        GRADE -> listOf(
            Knob("exposure", "Exposure", -2.0, 2.0, 0.0),
            Knob("contrast", "Contrast", 0.2, 2.5, 1.0),
        )
        SATURATION -> listOf(Knob("amount", "Amount", 0.0, 3.0, 1.0))
        MERGE -> listOf(Knob("mix", "Mix", 0.0, 1.0, 0.5))
        TRANSFORM -> listOf(
            Knob("translate_x", "Pos X px", -2000.0, 2000.0, 0.0),
            Knob("translate_y", "Pos Y px", -2000.0, 2000.0, 0.0),
            Knob("scale", "Scale", 0.05, 4.0, 1.0),
            Knob("rotate", "Rotate deg", -180.0, 180.0, 0.0),
            Knob("opacity", "Opacity", 0.0, 1.0, 1.0),
        )
        // The Constant's colour is separate; this number is its value output.
        CONSTANT -> listOf(Knob("value", "Number", -10.0, 10.0, 0.0))
        else -> emptyList()
    }

    fun capacity(op: Int): Int = when (op) {
        MERGE, OVER -> 2
        CONSTANT, READ, VIEWER -> 1
        else -> 1
    }
}

data class TitleOptions(
    val font: String = "anton",
    val size: Double = 140.0,
    val color: String = "#FFFFFF",
    val align: Int = 1,
    val anchor: Int = 1,
    val outline: Double = 0.0,
    val outlineColor: String = "#000000",
    val shadow: Double = 0.0,
    val lineSpacing: Double = 1.12,
    val tracking: Double = 0.0,
    val boxWidth: Double = 0.0,
    val uppercase: Boolean = false,
)

/**
 * Owns one native editing session and mirrors it into Compose state.
 *
 * The C++ engine is the source of truth for the document, timeline and render
 * output; this class only drives it and caches the JSON view.
 */
class EditorEngine(
    private val store: ProjectStore,
    private val autosaveEnabled: () -> Boolean,
) {

    var doc: ProjectDoc by mutableStateOf(ProjectDoc.empty())
        private set

    var time by mutableDoubleStateOf(0.0)
        private set

    var playing by mutableStateOf(false)
        private set

    var dirty by mutableStateOf(false)
        private set

    var saving by mutableStateOf(false)
        private set

    var status by mutableStateOf("Ready")
        private set

    var lastError: String? by mutableStateOf(null)
        private set

    var selectedTrackId by mutableIntStateOf(-1)
        private set

    var selectedClipId by mutableIntStateOf(-1)
        private set

    var currentFile: File? = null
        private set

    /** Name of the active .cube look, or null when the preview is ungraded. */
    var lookName: String? by mutableStateOf(null)
        private set

    var lookStrength: Float by mutableFloatStateOf(1f)
        private set

    // Master grade state, mirrored for the UI.
    var gradeExposure by mutableFloatStateOf(0f)
        private set
    var gradeContrast by mutableFloatStateOf(1f)
        private set
    var gradeSaturation by mutableFloatStateOf(1f)
        private set
    var gradeTemperature by mutableFloatStateOf(0f)
        private set
    var gradeLift by mutableFloatStateOf(0f)
        private set
    var gradeGamma by mutableFloatStateOf(1f)
        private set
    var gradeGain by mutableFloatStateOf(1f)
        private set
    var gradeActive by mutableStateOf(false)
        private set

    private val audio = NativeAudioPlayer()
    private var lastAutosaveAt = 0L

    val fps: Double get() = if (doc.fps > 1.0) doc.fps else 30.0
    val duration: Double get() = doc.timelineEnd
    val hasProject: Boolean get() = doc.tracks.isNotEmpty() || doc.media.isNotEmpty()

    fun openNew(name: String, width: Int, height: Int, fps: Double, duration: Double) {
        lastError = null
        val ok = NativeEngine.nativeNewProject(name, width, height, fps)
        if (!ok) {
            lastError = "Could not create the project"
            return
        }
        currentFile = null
        reload()
        // The engine owns the duration; apply the requested one by trimming the
        // sequence end after creation.
        status = "New project"
        dirty = false
        time = 0.0
        selectedTrackId = doc.tracks.firstOrNull()?.id ?: -1
        selectedClipId = -1
        if (duration > 0) NativeEngine.nativeSeek(0.0)
    }

    fun openFile(file: File): Boolean {
        lastError = null
        val json = store.read(file)
        if (json.isBlank()) {
            lastError = "That project file is empty"
            return false
        }
        if (!NativeEngine.nativeProjectLoad(json)) {
            lastError = "The native engine rejected this project file"
            return false
        }
        currentFile = file
        reload()
        time = 0.0
        selectedTrackId = doc.tracks.firstOrNull()?.id ?: -1
        selectedClipId = -1
        dirty = false
        status = "Opened ${file.nameWithoutExtension}"
        Diagnostics.note("openFile ${file.name} (${doc.tracks.size} tracks)")
        return true
    }

    /** Lets the UI report progress (import, export) through the status line. */

    // ---- Comp network -------------------------------------------------------
    var compNodes by mutableStateOf<List<CompNode>>(emptyList())
        private set
    var compEdges by mutableStateOf<List<CompEdge>>(emptyList())
        private set
    var compActive by mutableStateOf(false)
        private set
    var compSink by mutableIntStateOf(0)
        private set
    var compLinks by mutableStateOf<List<CompLink>>(emptyList())
        private set

    /** Re-reads the native graph. Cheap: a handful of ints, no pixel work. */
    fun refreshComp() {
        val nodes = ArrayList<CompNode>()
        val buf = IntArray(5)
        for (i in 0 until NativeEngine.nativeCompNodeCount()) {
            if (NativeEngine.nativeCompNodeAt(i, buf)) {
                nodes += CompNode(buf[0], buf[1], buf[2] != 0, buf[3].toFloat(), buf[4].toFloat())
            }
        }
        compNodes = nodes
        val edges = ArrayList<CompEdge>()
        val eb = IntArray(3)
        for (i in 0 until NativeEngine.nativeCompEdgeCount()) {
            if (NativeEngine.nativeCompEdgeAt(i, eb)) edges += CompEdge(eb[0], eb[1], eb[2])
        }
        compEdges = edges
        compActive = NativeEngine.nativeCompActive()
        compSink = NativeEngine.nativeCompSink()
        val links = ArrayList<CompLink>()
        for (i in 0 until NativeEngine.nativeCompLinkCount()) {
            val text = NativeEngine.nativeCompLinkAt(i)
            if (text.isEmpty()) continue
            val parts = text.split('|')
            if (parts.size < 4) continue
            val node = parts[0].toIntOrNull() ?: continue
            val src = parts[2].toIntOrNull() ?: continue
            links += CompLink(node, parts[1], src, parts[3])
        }
        compLinks = links
    }

    fun activateComp(on: Boolean) {
        NativeEngine.nativeCompSetActive(on)
        // The viewer must repaint: the comp is cooked on the next frame.
        NativeEngine.nativeSeek(time)
        refreshComp()
        status = if (on) "Comp active — viewport cooks the node graph" else "Timeline view"
    }

    fun addCompNode(op: Int, x: Float, y: Float): Int {
        val id = NativeEngine.nativeCompAddNode(op, x, y)
        refreshComp()
        NativeEngine.nativeSeek(time)
        status = "Added ${CompOps.name(op)}"
        return id
    }

    fun moveCompNode(id: Int, x: Float, y: Float) {
        NativeEngine.nativeCompSetPosition(id, x, y)
        compNodes = compNodes.map { if (it.id == id) it.copy(x = x, y = y) else it }
    }

    fun connectComp(from: Int, to: Int, slot: Int = 0): Boolean {
        val ok = NativeEngine.nativeCompConnect(from, to, slot)
        if (!ok) {
            status = "Cannot connect those nodes (would create a cycle)"
            return false
        }
        NativeEngine.nativeSeek(time)
        refreshComp()
        return true
    }

    fun disconnectComp(from: Int, to: Int, slot: Int) {
        NativeEngine.nativeCompDisconnect(from, to, slot)
        NativeEngine.nativeSeek(time)
        refreshComp()
    }

    fun setCompParam(id: Int, name: String, value: Double) {
        NativeEngine.nativeCompSetParam(id, name, value)
        NativeEngine.nativeSeek(time)
    }

    fun compParam(id: Int, name: String, fallback: Double): Double =
        NativeEngine.nativeCompGetParam(id, name, fallback)

    /** Empty path means the Read node samples the edited timeline instead of a file. */
    fun setCompMedia(id: Int, path: String) {
        NativeEngine.nativeCompSetMedia(id, path)
        NativeEngine.nativeSeek(time)
        status = if (path.isEmpty()) "Read: timeline sequence" else "Read: ${path.substringAfterLast('/')}"
    }

    fun compMedia(id: Int): String = NativeEngine.nativeCompGetMedia(id)

    fun linkFor(node: Int, param: String): CompLink? =
        compLinks.firstOrNull { it.node == node && it.param == param }

    fun linkCompParam(node: Int, param: String, src: Int, srcParam: String): Boolean {
        val ok = NativeEngine.nativeCompLinkParam(node, param, src, srcParam)
        if (!ok) {
            status = "That link would create a cycle"
            return false
        }
        NativeEngine.nativeSeek(time)
        refreshComp()
        status = "#$node.$param follows #$src.$srcParam"
        return true
    }

    fun unlinkCompParam(node: Int, param: String) {
        NativeEngine.nativeCompUnlinkParam(node, param)
        NativeEngine.nativeSeek(time)
        refreshComp()
    }

    fun setCompEnabled(id: Int, enabled: Boolean) {
        NativeEngine.nativeCompSetEnabled(id, enabled)
        NativeEngine.nativeSeek(time)
        refreshComp()
    }

    fun removeCompNode(id: Int) {
        NativeEngine.nativeCompRemoveNode(id)
        NativeEngine.nativeSeek(time)
        refreshComp()
    }

    fun viewCompNode(id: Int) {
        NativeEngine.nativeCompSetSink(id)
        NativeEngine.nativeSeek(time)
        refreshComp()
    }

    fun note(message: String) {
        status = message
        // The same trail a crash report carries: whatever the UI last said it did.
        Diagnostics.note(message)
    }

    fun reload() {
        runCatching { refreshComp() }
        doc = ProjectDoc.parse(NativeEngine.nativeProjectToJson())
        val state = NativeEngine.nativePreviewState()
        if (state.size >= 4) {
            time = state[0]
            playing = state[3] == 1.0
        }
        if (selectedTrackId == -1) selectedTrackId = doc.tracks.firstOrNull()?.id ?: -1
        // The document carries the master grade and the insert chain, so both can
        // have changed under us (open, undo, redo).
        syncGradeFromEngine()
        audioRevision++
    }

    fun seek(seconds: Double) {
        val target = seconds.coerceIn(0.0, duration)
        NativeEngine.nativeSeek(target)
        time = target
    }

    fun step(frames: Int) = seek(time + frames / fps)

    fun togglePlay() {
        if (playing) {
            NativeEngine.nativePause()
            audio.stop()
            playing = false
        } else {
            NativeEngine.nativePlay()
            val state = NativeEngine.nativeAudioState()
            val rate = if (state.size >= 2 && state[0] > 0) state[0].toInt() else 48_000
            val channels = if (state.size >= 2 && state[1] > 0) state[1].toInt() else 2
            audio.start(rate, channels)
            playing = true
        }
    }

    fun select(trackId: Int, clipId: Int) {
        selectedTrackId = trackId
        selectedClipId = clipId
    }

    fun splitAtPlayhead(): Boolean {
        val trackId = selectedTrackId
        val clipId = selectedClipId
        if (trackId < 0 || clipId <= 0) {
            status = "Select a clip first"
            return false
        }
        val ok = NativeEngine.nativeSplitClip(trackId, clipId, time)
        if (ok) {
            markDirty()
            reload()
            status = "Split at ${formatTimecode(time)}"
        } else {
            status = "Nothing to split at the playhead"
        }
        return ok
    }

    fun undo(): Boolean {
        val ok = NativeEngine.nativeUndo()
        if (ok) {
            markDirty()
            reload()
            status = "Undo"
        }
        return ok
    }

    fun redo(): Boolean {
        val ok = NativeEngine.nativeRedo()
        if (ok) {
            markDirty()
            reload()
            status = "Redo"
        }
        return ok
    }

    /** Imports media and drops it on the first track at the playhead. */
    fun importMedia(
        path: String,
        displayName: String? = null,
        appendToEnd: Boolean = false,
    ): Boolean {
        val mediaId = NativeEngine.nativeImportMedia(path)
        if (mediaId <= 0) {
            status = "Could not import that file"
            return false
        }
        reload()
        val media = doc.media.firstOrNull { it.id == mediaId }
        val length = media?.duration?.takeIf { it > 0.05 } ?: 5.0
        val trackId = selectedTrackId.takeIf { it > 0 } ?: doc.tracks.firstOrNull()?.id ?: -1
        val at = if (appendToEnd) doc.timelineEnd else time
        if (trackId > 0) {
            val clipId = NativeEngine.nativeAddClip(trackId, mediaId, at, length)
            if (clipId > 0) {
                selectedTrackId = trackId
                selectedClipId = clipId
            }
        }
        markDirty()
        reload()
        status = "Imported ${displayName ?: media?.name ?: "media"} at " + "%.2f".format(at) + "s"
        return true
    }

    /** Called by the preview loop with the engine's authoritative clock. */
    fun onFrameRendered(nativeTime: Double, isPlaying: Boolean) {
        if (playing != isPlaying) playing = isPlaying
        if (kotlin.math.abs(time - nativeTime) > 0.004) time = nativeTime
    }

    /** Loads a `.cube` look into the engine; the preview loop picks it up next frame. */
    fun applyLook(path: String, name: String): Boolean {
        if (!NativeEngine.nativeLookLoad(path)) {
            status = "Could not read the $name look"
            return false
        }
        NativeEngine.nativeLookSetStrength(lookStrength)
        lookName = name
        status = "Look: $name"
        return true
    }

    fun clearLook() {
        NativeEngine.nativeLookClear()
        lookName = null
        status = "Look cleared"
    }

    fun updateLookStrength(value: Float) {
        lookStrength = value.coerceIn(0f, 1f)
        NativeEngine.nativeLookSetStrength(lookStrength)
    }

    /** Pushes the whole grade to the engine; it is applied to the preview frame. */
    fun applyGrade(
        exposure: Float = gradeExposure,
        contrast: Float = gradeContrast,
        saturation: Float = gradeSaturation,
        temperature: Float = gradeTemperature,
        lift: Float = gradeLift,
        gamma: Float = gradeGamma,
        gain: Float = gradeGain,
    ) {
        gradeExposure = exposure
        gradeContrast = contrast
        gradeSaturation = saturation
        gradeTemperature = temperature
        gradeLift = lift
        gradeGamma = gamma
        gradeGain = gain
        NativeEngine.nativeGradeSet(exposure, contrast, saturation, temperature, lift, gamma, gain)
        gradeActive = true
    }

    private val gradeScratch = FloatArray(7)

    /**
     * Pulls the master grade back from the engine. The document owns it now, so a
     * load or an undo can change it without the sliders being told.
     */
    fun syncGradeFromEngine(): Boolean {
        val active = NativeEngine.nativeGradeGet(gradeScratch)
        gradeActive = active
        if (!active) return false
        gradeExposure = gradeScratch[0]
        gradeContrast = gradeScratch[1]
        gradeSaturation = gradeScratch[2]
        gradeTemperature = gradeScratch[3]
        gradeLift = gradeScratch[4]
        gradeGamma = gradeScratch[5]
        gradeGain = gradeScratch[6]
        return true
    }

    fun resetGrade() {
        gradeExposure = 0f
        gradeContrast = 1f
        gradeSaturation = 1f
        gradeTemperature = 0f
        gradeLift = 0f
        gradeGamma = 1f
        gradeGain = 1f
        NativeEngine.nativeGradeClear()
        gradeActive = false
        status = "Grade reset"
    }

    /** R/G/B/luma histogram of the last rendered frame, `bins` per channel. */
    fun histogram(bins: Int = 64): IntArray {
        val out = IntArray(bins * 4)
        NativeEngine.nativeScopesHistogram(out, bins)
        return out
    }

    /**
     * Adds a keyframe at the playhead for the selected clip.
     * prop: 0 opacity, 1 scale, 2 pos_x, 3 pos_y.
     */
    fun addKeyframe(prop: Int, value: Float): Boolean {
        val (t, c) = selection() ?: run {
            status = "Select a clip first"
            return false
        }
        val clip = doc.clipById(t, c) ?: return false
        val local = (time - clip.start).coerceAtLeast(0.0)
        if (!NativeEngine.nativeKeyframeAdd(t, c, prop, local, value.toDouble())) {
            status = "Could not add the keyframe"
            return false
        }
        markDirty()
        reload()
        status = "Keyframe at ${formatTimecode(time)}"
        return true
    }

    fun clearKeyframes(prop: Int): Boolean {
        val (t, c) = selection() ?: return false
        if (!NativeEngine.nativeKeyframeClear(t, c, prop)) return false
        markDirty()
        reload()
        status = "Animation cleared"
        return true
    }

    /** The clip that follows the selected one on the same track, if any. */
    fun nextClip(): com.motionstudio.app.model.NativeClip? {
        val track = doc.tracks.firstOrNull { it.id == selectedTrackId } ?: return null
        val ordered = track.clips.sortedBy { it.start }
        val index = ordered.indexOfFirst { it.id == selectedClipId }
        if (index < 0) return null
        return ordered.getOrNull(index + 1)
    }

    fun transitionsOnSelectedTrack(): List<com.motionstudio.app.model.NativeTransition> =
        doc.tracks.firstOrNull { it.id == selectedTrackId }?.transitions ?: emptyList()

    fun addTransitionToNext(type: Int, duration: Double = 0.6): Boolean {
        val (t, c) = selection() ?: run {
            status = "Select a clip first"
            return false
        }
        val next = nextClip() ?: run {
            status = "No clip after this one on the track"
            return false
        }
        val id = NativeEngine.nativeTransitionAdd(t, c, next.id, type, duration)
        if (id <= 0) {
            status = "Could not add the transition"
            return false
        }
        markDirty()
        reload()
        status = "Transition added"
        return true
    }

    fun removeTransition(id: Int): Boolean {
        val (t, _) = selection() ?: return false
        if (!NativeEngine.nativeTransitionRemove(t, id)) return false
        markDirty()
        reload()
        status = "Transition removed"
        return true
    }

    // ---- Markers + project settings ----------------------------------------

    fun addMarker(name: String = ""): Boolean {
        val id = NativeEngine.nativeMarkerAdd(time, name)
        if (id <= 0) {
            status = "Could not add the marker"
            return false
        }
        markDirty()
        reload()
        status = "Marker added at ${"%.2f".format(time)}s"
        return true
    }

    fun removeMarker(id: Int): Boolean {
        if (!NativeEngine.nativeMarkerRemove(id)) return false
        markDirty()
        reload()
        status = "Marker removed"
        return true
    }

    fun setProjectSettings(width: Int, height: Int, fps: Double): Boolean {
        if (!NativeEngine.nativeProjectSettings(width, height, fps)) {
            status = "The engine rejected those settings"
            return false
        }
        markDirty()
        reload()
        status = "Project set to ${width}×${height} @ ${fps.toInt()} fps"
        return true
    }

    // ---- Canvas / preview shape --------------------------------------------

    /** Aspect guide drawn by the preview; [PreviewRatio.Project] follows the frame. */
    var previewRatio by mutableStateOf(com.motionstudio.app.model.PreviewRatio.Project)

    /** Where an import lands: the playhead, or the end of the timeline. */
    var appendImportsAtEnd by mutableStateOf(false)

    // ---- Shape masks -------------------------------------------------------

    var clipMask by mutableStateOf(floatArrayOf(0f, 0.1f, 0.1f, 0.8f, 0.8f, 0f))
        private set
    var clipMaskActive by mutableStateOf(false)
        private set
    var clipMaskInverted by mutableStateOf(false)
        private set

    fun refreshClipMask() {
        val (t, c) = selection() ?: run {
            clipMaskActive = false
            return
        }
        val out = FloatArray(6)
        val inv = IntArray(1)
        if (NativeEngine.nativeClipMaskGet(t, c, out, inv)) {
            clipMask = out
            clipMaskInverted = inv[0] != 0
            clipMaskActive = true
        } else {
            clipMaskActive = false
        }
    }

    fun setClipMask(
        shape: Int,
        x: Float,
        y: Float,
        w: Float,
        h: Float,
        feather: Float,
        inverted: Boolean,
    ): Boolean {
        val (t, c) = selection() ?: run {
            status = "Select a clip first"
            return false
        }
        if (!NativeEngine.nativeClipMaskSet(t, c, shape, x, y, w, h, feather, inverted)) return false
        clipMask = floatArrayOf(shape.toFloat(), x, y, w, h, feather)
        clipMaskInverted = inverted
        clipMaskActive = shape != 3
        markDirty()
        return true
    }

    fun clearClipMask(): Boolean {
        val (t, c) = selection() ?: return false
        if (!NativeEngine.nativeClipMaskClear(t, c)) return false
        clipMaskActive = false
        markDirty()
        status = "Mask removed"
        return true
    }

    // ---- Per-clip grade ----------------------------------------------------

    var clipGrade by mutableStateOf(floatArrayOf(0f, 1f, 1f, 0f, 0f, 1f, 1f))
        private set
    var clipGradeActive by mutableStateOf(false)
        private set

    /** Pulls the selected clip's grade into Compose state. */
    fun refreshClipGrade() {
        val (t, c) = selection() ?: run {
            clipGradeActive = false
            return
        }
        val out = FloatArray(7)
        if (NativeEngine.nativeClipGradeGet(t, c, out)) {
            clipGrade = out
            clipGradeActive = true
        } else {
            clipGradeActive = false
        }
    }

    fun applyClipGrade(
        exposure: Float = clipGrade[0],
        contrast: Float = clipGrade[1],
        saturation: Float = clipGrade[2],
        temperature: Float = clipGrade[3],
        lift: Float = clipGrade[4],
        gamma: Float = clipGrade[5],
        gain: Float = clipGrade[6],
    ): Boolean {
        val (t, c) = selection() ?: run {
            status = "Select a clip first"
            return false
        }
        if (!NativeEngine.nativeClipGradeSet(
                t, c, exposure, contrast, saturation, temperature, lift, gamma, gain,
            )
        ) return false
        clipGrade = floatArrayOf(exposure, contrast, saturation, temperature, lift, gamma, gain)
        clipGradeActive = true
        markDirty()
        status = "Clip grade applied"
        return true
    }

    fun clearClipGrade(): Boolean {
        val (t, c) = selection() ?: return false
        if (!NativeEngine.nativeClipGradeClear(t, c)) return false
        clipGrade = floatArrayOf(0f, 1f, 1f, 0f, 0f, 1f, 1f)
        clipGradeActive = false
        markDirty()
        status = "Clip grade cleared"
        return true
    }

    // ---- Waveform ----------------------------------------------------------

    private val peaksCache = HashMap<String, FloatArray>()

    /**
     * Peak envelope for a clip's audio, cached per source range. Empty when the
     * clip has no decodable audio, which is how the timeline decides to skip
     * drawing a waveform.
     */
    fun peaks(trackId: Int, clipId: Int, bins: Int = 96): FloatArray {
        val clip = doc.clipById(trackId, clipId) ?: return FloatArray(0)
        val key = "$trackId:$clipId:${clip.duration}:${clip.mediaId}:$bins"
        peaksCache[key]?.let { return it }
        val out = FloatArray(bins)
        val result = if (NativeEngine.nativeClipPeaks(trackId, clipId, out, bins)) out else FloatArray(0)
        peaksCache[key] = result
        // Keep the cache from growing without bound across a long session.
        if (peaksCache.size > 256) peaksCache.clear()
        return result
    }

    // ---- Text / titles -----------------------------------------------------

    fun selectedIsText(): Boolean {
        val (t, c) = selection() ?: return false
        return doc.clipById(t, c)?.isText == true
    }

    /** Existing style of the selected title, for populating the editor. */
    fun selectedTitleOptions(): TitleOptions {
        val (t, c) = selection() ?: return TitleOptions()
        val clip = doc.clipById(t, c) ?: return TitleOptions()
        return TitleOptions(
            font = clip.textFont.ifEmpty { "anton" },
            size = if (clip.textSize > 0) clip.textSize else 140.0,
        )
    }

    fun selectedTitleText(): String {
        val (t, c) = selection() ?: return ""
        return doc.clipById(t, c)?.text.orEmpty()
    }

    /** Adds a title clip at the playhead on the overlay (or first video) track. */
    fun addTitle(text: String, opts: TitleOptions, duration: Double = 3.0): Boolean {
        if (text.isBlank()) {
            status = "Type the title text first"
            return false
        }
        val track = doc.tracks.firstOrNull { it.type == 1 }
            ?: doc.tracks.firstOrNull { it.type == 0 }
            ?: run {
                status = "No video or overlay track to put the title on"
                return false
            }
        val id = NativeEngine.nativeTextAddClip(
            track.id, time, duration, text, opts.font, opts.size, opts.color,
            opts.align, opts.anchor, opts.outline, opts.outlineColor, opts.shadow,
            opts.lineSpacing, opts.tracking, opts.boxWidth, opts.uppercase,
        )
        if (id <= 0) {
            status = "The text rasteriser could not build that title"
            return false
        }
        selectedTrackId = track.id
        selectedClipId = id
        markDirty()
        reload()
        status = "Title added"
        return true
    }

    /** Re-renders the selected title clip with new text and/or style. */
    fun updateSelectedTitle(text: String, opts: TitleOptions): Boolean {
        val (t, c) = selection() ?: return false
        if (doc.clipById(t, c)?.isText != true) return false
        val ok = NativeEngine.nativeTextUpdateClip(
            t, c, text, opts.font, opts.size, opts.color, opts.align, opts.anchor,
            opts.outline, opts.outlineColor, opts.shadow, opts.lineSpacing,
            opts.tracking, opts.boxWidth, opts.uppercase,
        )
        if (!ok) return false
        markDirty()
        reload()
        return true
    }

    fun keyframeCount(prop: Int): Int {
        val clip = doc.clipById(selectedTrackId, selectedClipId) ?: return 0
        val key = when (prop) {
            0 -> "keys_opacity"
            1 -> "keys_scale"
            2 -> "keys_pos_x"
            else -> "keys_pos_y"
        }
        return clip.keyframes[key] ?: 0
    }

    private fun selection(): Pair<Int, Int>? {
        val t = selectedTrackId
        val c = selectedClipId
        return if (t > 0 && c > 0) t to c else null
    }

    /** Effects currently attached to the selected clip. */
    val selectedEffects: List<com.motionstudio.app.model.NativeEffect>
        get() = selection()?.let { (t, c) -> doc.clipById(t, c)?.effects } ?: emptyList()

    fun removeSelectedClip(): Boolean {
        val (t, c) = selection() ?: run {
            status = "Select a clip first"
            return false
        }
        if (!NativeEngine.nativeClipRemove(t, c)) {
            status = "Could not delete that clip"
            return false
        }
        selectedClipId = -1
        markDirty()
        reload()
        status = "Clip deleted"
        return true
    }

    fun rippleDeleteSelected(): Boolean {
        val (t, c) = selection() ?: run {
            status = "Select a clip first"
            return false
        }
        if (!NativeEngine.nativeClipRippleDelete(t, c)) {
            status = "Could not ripple delete that clip"
            return false
        }
        selectedClipId = -1
        markDirty()
        reload()
        status = "Ripple deleted"
        return true
    }

    fun moveClip(trackId: Int, clipId: Int, start: Double): Boolean {
        val ok = NativeEngine.nativeClipMove(trackId, clipId, start.coerceAtLeast(0.0))
        if (ok) {
            markDirty()
            reload()
            status = "Moved to ${formatTimecode(start)}"
        } else {
            status = "Could not move that clip"
        }
        return ok
    }

    fun trimClip(trackId: Int, clipId: Int, start: Double, duration: Double): Boolean {
        val safeStart = start.coerceAtLeast(0.0)
        val safeDuration = duration.coerceAtLeast(MIN_CLIP_SECONDS)
        val ok = NativeEngine.nativeClipTrim(trackId, clipId, safeStart, safeDuration)
        if (ok) {
            markDirty()
            reload()
            status = "Trimmed to ${formatTimecode(safeDuration)}"
        } else {
            status = "Could not trim that clip"
        }
        return ok
    }

    fun addTrack(video: Boolean = true): Boolean {
        val count = doc.tracks.count { (it.type == 1) != video } + 1
        val name = if (video) "V$count" else "A$count"
        val id = NativeEngine.nativeTrackAdd(name, if (video) 0 else 1)
        if (id <= 0) {
            status = "Could not add a track"
            return false
        }
        markDirty()
        reload()
        selectedTrackId = id
        selectedClipId = -1
        status = "Added $name"
        return true
    }

    fun removeTrack(trackId: Int): Boolean {
        if (!NativeEngine.nativeTrackRemove(trackId)) {
            status = "Could not remove that track"
            return false
        }
        if (selectedTrackId == trackId) {
            selectedTrackId = -1
            selectedClipId = -1
        }
        markDirty()
        reload()
        status = "Track removed"
        return true
    }

    fun toggleTrackMuted(trackId: Int): Boolean {
        val track = doc.tracks.firstOrNull { it.id == trackId } ?: return false
        val ok = NativeEngine.nativeTrackFlags(trackId, !track.muted, track.locked, track.visible, false)
        if (ok) {
            markDirty()
            reload()
            status = if (track.muted) "Unmuted ${track.name}" else "Muted ${track.name}"
        }
        return ok
    }

    fun toggleTrackVisible(trackId: Int): Boolean {
        val track = doc.tracks.firstOrNull { it.id == trackId } ?: return false
        val ok = NativeEngine.nativeTrackFlags(trackId, track.muted, track.locked, !track.visible, false)
        if (ok) {
            markDirty()
            reload()
        }
        return ok
    }

    /**
     * [defaults] must be the values the picker shows. A new effect is stored with
     * no parameters at all, so without this the engine renders its own internal
     * defaults while the UI displays different numbers — and effects whose slider
     * defaults are neutral (opacity 1.0, levels gamma 1.0, crop 0) looked like
     * they did nothing at all.
     */
    fun addEffect(baseId: String, defaults: Map<String, Float> = emptyMap()): Boolean {
        val (t, c) = selection() ?: run {
            status = "Select a clip first"
            return false
        }
        val index = NativeEngine.nativeEffectAdd(t, c, baseId)
        if (index < 0) {
            status = "Could not add that effect"
            return false
        }
        defaults.forEach { (name, value) ->
            NativeEngine.nativeEffectSetParam(t, c, index, name, value)
        }
        markDirty()
        reload()
        status = "Added $baseId"
        return true
    }

    fun removeEffect(index: Int): Boolean {
        val (t, c) = selection() ?: return false
        if (!NativeEngine.nativeEffectRemove(t, c, index)) return false
        markDirty()
        reload()
        status = "Effect removed"
        return true
    }

    fun setEffectParam(index: Int, name: String, value: Float): Boolean {
        val (t, c) = selection() ?: return false
        val ok = NativeEngine.nativeEffectSetParam(t, c, index, name, value)
        if (ok) {
            markDirty()
            reload()
        }
        return ok
    }

    fun setEffectEnabled(index: Int, enabled: Boolean): Boolean {
        val (t, c) = selection() ?: return false
        val ok = NativeEngine.nativeEffectSetEnabled(t, c, index, enabled)
        if (ok) {
            markDirty()
            reload()
        }
        return ok
    }

    fun markDirty() {
        dirty = true
    }

    fun save(): Boolean {
        val json = NativeEngine.nativeProjectToJson()
        if (json.isBlank() || json == "{}") {
            lastError = "Nothing to save yet"
            return false
        }
        saving = true
        val file = currentFile ?: store.save(doc.name, json)
        if (currentFile != null) file.writeText(json)
        currentFile = file
        saving = false
        dirty = false
        lastAutosaveAt = System.currentTimeMillis()
        status = "Saved"
        return true
    }

    /** Autosaves at most every 10 minutes, matching the documented recovery policy. */
    fun maybeAutosave() {
        if (!autosaveEnabled() || !dirty || playing) return
        val now = System.currentTimeMillis()
        if (now - lastAutosaveAt < AUTOSAVE_INTERVAL_MS) return
        val json = NativeEngine.nativeProjectToJson()
        if (json.isBlank() || json == "{}") return
        runCatching { store.autosave(doc.name, json) }
        lastAutosaveAt = now
        status = "Autosaved"
    }

    // ---- Master audio inserts ------------------------------------------------

    /** Bumped on every insert edit so an open sheet re-reads the chain. */
    var audioRevision by mutableIntStateOf(0)
        private set

    fun audioInserts(): List<AudioInsert> {
        val count = NativeEngine.nativeAudioFxCount()
        if (count <= 0) return emptyList()
        return (0 until count).mapNotNull { i ->
            val kind = NativeEngine.nativeAudioFxKind(i)
            if (kind < 0) null else AudioInsert(i, kind, NativeEngine.nativeAudioFxEnabled(i))
        }
    }

    fun addAudioInsert(kind: Int): Boolean {
        val index = NativeEngine.nativeAudioFxAdd(kind)
        if (index < 0) {
            status = "Could not add that insert"
            return false
        }
        audioRevision++
        markDirty()
        status = "Added ${AUDIO_INSERT_NAMES.getOrElse(kind) { "insert" }} to the master chain"
        return true
    }

    fun removeAudioInsert(index: Int): Boolean {
        if (!NativeEngine.nativeAudioFxRemove(index)) {
            status = "Could not remove that insert"
            return false
        }
        audioRevision++
        markDirty()
        status = "Master insert removed"
        return true
    }

    fun clearAudioInserts() {
        NativeEngine.nativeAudioFxClear()
        audioRevision++
        markDirty()
        status = "Master chain cleared"
    }

    fun setAudioInsertEnabled(index: Int, enabled: Boolean): Boolean {
        val ok = NativeEngine.nativeAudioFxSetEnabled(index, enabled)
        if (ok) {
            audioRevision++
            markDirty()
        }
        return ok
    }

    fun audioInsertParam(index: Int, name: String, fallback: Float): Float =
        NativeEngine.nativeAudioFxGetParam(index, name, fallback)

    fun setAudioInsertParam(index: Int, name: String, value: Float): Boolean {
        val ok = NativeEngine.nativeAudioFxSetParam(index, name, value)
        if (ok) markDirty()
        return ok
    }

    fun setTrackGain(trackId: Int, gain: Float): Boolean {
        val ok = NativeEngine.nativeTrackGain(trackId, gain)
        if (ok) {
            markDirty()
            reload()
        }
        return ok
    }

    fun toggleTrackSolo(trackId: Int): Boolean {
        val track = doc.tracks.firstOrNull { it.id == trackId } ?: return false
        val ok = NativeEngine.nativeTrackFlags(trackId, track.muted, track.locked, track.visible, !track.solo)
        if (ok) {
            markDirty()
            reload()
            status = if (track.solo) "Solo off ${track.name}" else "Solo ${track.name}"
        }
        return ok
    }

    fun release() {
        if (dirty && autosaveEnabled()) maybeAutosave()
        audio.stop()
        NativeEngine.nativePause()
        playing = false
    }

    companion object {
        const val AUTOSAVE_INTERVAL_MS = 10 * 60 * 1000L
        const val MIN_CLIP_SECONDS = 0.05

        fun formatTimecode(seconds: Double, fps: Double = 30.0): String {
            val safe = seconds.coerceAtLeast(0.0)
            val total = safe.toLong()
            val mm = (total / 60).toString().padStart(2, '0')
            val ss = (total % 60).toString().padStart(2, '0')
            val ff = ((safe - total) * fps).toInt().coerceIn(0, (fps - 1).toInt())
                .toString().padStart(2, '0')
            return "$mm:$ss:$ff"
        }
    }
}
