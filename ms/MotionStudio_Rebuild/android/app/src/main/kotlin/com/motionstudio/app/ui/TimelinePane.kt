package com.motionstudio.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.model.NativeClip
import com.motionstudio.app.model.NativeTrack
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts
import kotlin.math.abs
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.roundToInt

private val HeaderWidth = 104.dp
private val RulerHeight = 26.dp
private val LaneHeight = 48.dp
private val ClipInset = 7.dp
private val GripWidth = 14.dp

/**
 * Timeline: ruler, track headers and lanes over one shared time axis.
 *
 * Two things here matter more than the decoration. First, scrubbing is layered
 * *under* the clips — Compose only hit-tests nodes that declare pointer input,
 * so an overlay drawn on top of the lanes swallows every clip gesture (the
 * previous version had exactly that bug and clips could not be touched at all).
 * Second, every drag is committed through the C++ engine on release, so undo and
 * the rendered frame stay authoritative and the preview cannot drift from the
 * picture the drag was drawn from.
 */
@Composable
fun TimelinePane(engine: EditorEngine, modifier: Modifier = Modifier) {
    val doc = engine.doc
    // An empty project has no duration yet; keep a usable axis instead of dividing by zero.
    val span = max(doc.timelineEnd, 4.0)
    var zoom by remember { mutableFloatStateOf(1f) }
    var snapping by remember { mutableStateOf(true) }
    var snapTime by remember { mutableStateOf<Double?>(null) }
    val hScroll = rememberScrollState()
    val vScroll = rememberScrollState()
    val density = LocalDensity.current

    BoxWithConstraints(modifier.background(MsColors.surface)) {
        val laneViewport: Dp = (maxWidth - HeaderWidth).coerceAtLeast(1.dp)
        val contentWidth: Dp = (laneViewport * zoom).coerceAtLeast(160.dp)
        val secondsPerDp = span / contentWidth.value.toDouble()

        fun timeAt(xPx: Float): Double =
            (xPx / with(density) { contentWidth.toPx() }).toDouble() * span

        fun xOf(seconds: Double): Dp = (seconds / span).toFloat().let { it * contentWidth.value }.dp

        Column(Modifier.fillMaxSize()) {
            // ---- head-up strip -------------------------------------------------
            Row(
                Modifier.fillMaxWidth().height(30.dp).background(MsColors.surfaceHigh)
                    .padding(horizontal = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                Text(
                    tc(engine.time, engine.fps),
                    color = MsColors.accent,
                    fontSize = 12.sp,
                    fontFamily = MsFonts.mono,
                    fontWeight = FontWeight.Bold,
                )
                Text(
                    "/ ${tc(span, engine.fps)}",
                    color = MsColors.textMuted,
                    fontSize = 10.sp,
                    fontFamily = MsFonts.mono,
                )
                Text("F${(engine.time * engine.fps).roundToInt()}",
                    color = MsColors.textMuted, fontSize = 9.sp, fontFamily = MsFonts.mono)
                Spacer(Modifier.weight(1f))
                Chip(
                    label = if (snapping) "SNAP" else "FREE",
                    active = snapping,
                    onClick = { snapping = !snapping },
                )
                Chip(label = "−", active = false) { zoom = (zoom / 1.5f).coerceAtLeast(0.4f) }
                Text(
                    "${(zoom * 100).roundToInt()}%",
                    color = MsColors.textTertiary,
                    fontSize = 9.sp,
                    fontFamily = MsFonts.mono,
                )
                Chip(label = "+", active = false) { zoom = (zoom * 1.5f).coerceAtMost(24f) }
                Chip(label = "FIT", active = false) { zoom = 1f }
            }

            // ---- ruler ---------------------------------------------------------
            Row(Modifier.fillMaxWidth()) {
                Box(Modifier.width(HeaderWidth).height(RulerHeight).background(MsColors.surface)) {
                    Text(
                        "TRACKS",
                        color = MsColors.textMuted,
                        fontSize = 8.sp,
                        fontFamily = MsFonts.mono,
                        modifier = Modifier.align(Alignment.Center),
                    )
                }
                Box(
                    Modifier.weight(1f).height(RulerHeight)
                        .horizontalScroll(hScroll)
                        .background(MsColors.surface)
                ) {
                    Ruler(
                        span = span,
                        contentWidth = contentWidth,
                        time = engine.time,
                        fps = engine.fps,
                        markers = doc.markers.map { it.time },
                        onSeek = { x -> engine.seek(timeAt(x)) },
                    )
                }
            }

            // ---- tracks --------------------------------------------------------
            Row(Modifier.fillMaxWidth().weight(1f)) {
                Column(
                    Modifier.width(HeaderWidth).fillMaxHeight().verticalScroll(vScroll)
                        .background(MsColors.background)
                ) {
                    doc.tracks.forEach { track ->
                        TrackHeader(
                            track = track,
                            selected = track.id == engine.selectedTrackId,
                            onMute = { engine.toggleTrackMuted(track.id) },
                            onVisible = { engine.toggleTrackVisible(track.id) },
                            onRemove = { engine.removeTrack(track.id) },
                        )
                    }
                    Row(
                        Modifier.fillMaxWidth().height(LaneHeight).padding(horizontal = 6.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        Chip(label = "+V", active = false) { engine.addTrack(video = true) }
                        Chip(label = "+A", active = false) { engine.addTrack(video = false) }
                    }
                }

                Box(
                    Modifier.weight(1f).fillMaxHeight()
                        .horizontalScroll(hScroll)
                        .verticalScroll(vScroll)
                ) {
                    Box(
                        Modifier
                            .width(contentWidth)
                            .height(LaneHeight * doc.tracks.size + LaneHeight)
                    ) {
                        // Scrubbing sits *below* the clips: taps on a clip reach the
                        // clip, taps on empty lane space reach this layer.
                        Box(
                            Modifier.matchParentSize().pointerInput(span, contentWidth) {
                                detectTapGestures { offset -> engine.seek(timeAt(offset.x)) }
                            }
                        )
                        Box(
                            Modifier.matchParentSize().pointerInput(span, contentWidth) {
                                detectDragGestures { change, _ -> engine.seek(timeAt(change.position.x)) }
                            }
                        )

                        doc.tracks.forEachIndexed { index, track ->
                            Lane(
                                engine = engine,
                                track = track,
                                top = LaneHeight * index,
                                span = span,
                                contentWidth = contentWidth,
                                gridStep = gridStep(secondsPerDp),
                                secondsPerDp = secondsPerDp,
                                snapping = snapping,
                                onSnapChange = { snapTime = it },
                            )
                        }

                        if (doc.tracks.isEmpty() || doc.tracks.all { it.clips.isEmpty() }) {
                            Text(
                                "Import media, then drag it onto a lane",
                                color = MsColors.textMuted,
                                fontSize = 11.sp,
                                modifier = Modifier.padding(start = 14.dp, top = 16.dp),
                            )
                        }

                        snapTime?.let { snapped ->
                            Box(
                                Modifier.offset(x = xOf(snapped))
                                    .width(1.dp)
                                    .fillMaxHeight()
                                    .background(MsColors.warning)
                            )
                        }

                        // Playhead last so it sits above clips and the snap guide.
                        Box(
                            Modifier
                                .offset(x = xOf(engine.time) - 0.5.dp)
                                .width(1.dp)
                                .fillMaxHeight()
                                .background(MsColors.accent)
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun Ruler(
    span: Double,
    contentWidth: Dp,
    time: Double,
    fps: Double,
    markers: List<Double>,
    onSeek: (Float) -> Unit,
) {
    val step = tickStep(span, contentWidth)
    Box(
        Modifier.width(contentWidth).height(RulerHeight)
            .pointerInput(span, contentWidth) {
                detectTapGestures { offset -> onSeek(offset.x) }
            }
            .pointerInput(span, contentWidth) {
                detectDragGestures { change, _ -> onSeek(change.position.x) }
            },
    ) {
        var t = 0.0
        while (t <= span + 1e-6) {
            val x = xDp(t, span, contentWidth)
            Box(
                Modifier.offset(x = x).width(1.dp).height(if (isMajor(t, step)) 10.dp else 6.dp)
                    .background(MsColors.border)
                    .align(Alignment.BottomStart)
            )
            if (isMajor(t, step)) {
                Text(
                    tickLabel(t, fps, step),
                    color = MsColors.textMuted,
                    fontSize = 8.sp,
                    fontFamily = MsFonts.mono,
                    modifier = Modifier.offset(x = x + 3.dp, y = 2.dp),
                )
            }
            t += step
        }

        markers.forEach { marker ->
            Box(
                Modifier.offset(x = xDp(marker, span, contentWidth) - 3.dp, y = 13.dp)
                    .size(6.dp)
                    .background(MsColors.info, CircleShape)
            )
        }

        // Playhead grip: a triangle the user can actually grab.
        val playX = xDp(time, span, contentWidth)
        Box(
            Modifier.offset(x = playX - 5.dp).width(10.dp).height(9.dp)
                .background(MsColors.accent, RoundedCornerShape(bottomStart = 3.dp, bottomEnd = 3.dp))
        )
        Box(
            Modifier.offset(x = playX - 0.5.dp).width(1.dp).fillMaxHeight()
                .background(MsColors.accent)
        )
    }
}

@Composable
private fun TrackHeader(
    track: NativeTrack,
    selected: Boolean,
    onMute: () -> Unit,
    onVisible: () -> Unit,
    onRemove: () -> Unit,
) {
    val audio = track.type == 1
    Row(
        Modifier
            .fillMaxWidth()
            .height(LaneHeight)
            .background(if (selected) MsColors.surfaceHighest else Color.Transparent)
            .border(0.5.dp, MsColors.borderSoft),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            Modifier.width(3.dp).fillMaxHeight().padding(vertical = 12.dp)
                .clip(RoundedCornerShape(2.dp))
                .background(if (track.muted) MsColors.textMuted else trackColor(track, audio))
        )
        Column(Modifier.weight(1f).padding(start = 7.dp)) {
            Text(
                track.name,
                color = if (track.muted) MsColors.textMuted else MsColors.textPrimary,
                fontSize = 11.sp,
                maxLines = 1,
            )
            Text(
                when {
                    track.muted -> "MUTED"
                    audio -> "${track.clips.size} AUD"
                    else -> "${track.clips.size} VID"
                },
                color = if (track.muted) MsColors.warning else MsColors.textMuted,
                fontSize = 8.sp,
                fontFamily = MsFonts.mono,
            )
        }
        Column(Modifier.padding(end = 4.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Row(horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                Box(
                    Modifier.size(15.dp)
                        .background(
                            if (track.muted) MsColors.warning.copy(alpha = 0.25f) else MsColors.surfaceHighest,
                            RoundedCornerShape(2.dp),
                        )
                        .clickable { onMute() },
                    contentAlignment = Alignment.Center,
                ) { Text("M", color = MsColors.textSecondary, fontSize = 8.sp, fontFamily = MsFonts.mono) }
                Box(
                    Modifier.size(15.dp)
                        .background(
                            if (!track.visible) MsColors.error.copy(alpha = 0.2f) else MsColors.surfaceHighest,
                            RoundedCornerShape(2.dp),
                        )
                        .clickable { onVisible() },
                    contentAlignment = Alignment.Center,
                ) { Text("V", color = MsColors.textSecondary, fontSize = 8.sp, fontFamily = MsFonts.mono) }
                Box(
                    Modifier.size(15.dp).background(MsColors.surfaceHighest, RoundedCornerShape(2.dp))
                        .clickable { onRemove() },
                    contentAlignment = Alignment.Center,
                ) { Text("×", color = MsColors.textMuted, fontSize = 9.sp) }
            }
        }
    }
}

@Composable
private fun Lane(
    engine: EditorEngine,
    track: NativeTrack,
    top: Dp,
    span: Double,
    contentWidth: Dp,
    gridStep: Double,
    secondsPerDp: Double,
    snapping: Boolean,
    onSnapChange: (Double?) -> Unit,
) {
    Box(
        Modifier
            .offset(y = top)
            .width(contentWidth)
            .height(LaneHeight)
            .background(
                when {
                    !track.visible -> MsColors.viewerBg
                    engine.selectedTrackId == track.id -> MsColors.surface.copy(alpha = 0.55f)
                    else -> Color.Transparent
                }
            )
    ) {
        // Grid lines: time orientation across the lane, at the ruler's spacing.
        Canvas(Modifier.fillMaxSize()) {
            var t = 0.0
            while (t <= span + 1e-6) {
                val x = (t / span).toFloat() * size.width
                drawLine(MsColors.guide.copy(alpha = 0.35f), Offset(x, 0f), Offset(x, size.height), 1f)
                t += gridStep
            }
        }

        // Transitions sit on the cut between two clips.
        track.transitions.forEach { transition ->
            val outgoing = track.clips.firstOrNull { it.id == transition.outgoing } ?: return@forEach
            val x = ((outgoing.end / span).toFloat() * contentWidth.value).dp
            val w = ((transition.duration / span).toFloat() * contentWidth.value).dp
            Box(
                Modifier.offset(x = x - w / 2, y = 2.dp).width(w.coerceAtLeast(4.dp)).height(4.dp)
                    .background(MsColors.accentDim, RoundedCornerShape(2.dp))
            )
        }

        track.clips.forEach { clip ->
            ClipBox(
                engine = engine,
                track = track,
                clip = clip,
                span = span,
                contentWidth = contentWidth,
                secondsPerDp = secondsPerDp,
                selected = clip.id == engine.selectedClipId,
                snapping = snapping,
                onSnapChange = onSnapChange,
            )
        }
    }
}

@Composable
private fun ClipBox(
    engine: EditorEngine,
    track: NativeTrack,
    clip: NativeClip,
    span: Double,
    contentWidth: Dp,
    secondsPerDp: Double,
    selected: Boolean,
    snapping: Boolean,
    onSnapChange: (Double?) -> Unit,
) {
    val audio = track.type == 1
    var start by remember(clip.id, clip.start) { mutableFloatStateOf(clip.start.toFloat()) }
    var length by remember(clip.id, clip.duration) { mutableFloatStateOf(clip.duration.toFloat()) }
    var dragging by remember(clip.id) { mutableStateOf(false) }

    val leftDp = (start / span).toFloat() * contentWidth.value
    val widthDp = ((length / span).toFloat() * contentWidth.value).coerceAtLeast(2f)
    val wideEnoughForGrips = widthDp > GripWidth.value * 2.4f

    Box(
        Modifier
            .offset(x = leftDp.dp, y = ClipInset)
            .width(widthDp.dp)
            .height(LaneHeight - ClipInset * 2)
            .clip(RoundedCornerShape(4.dp))
            .background(trackColor(track, audio).copy(alpha = if (track.visible) 0.42f else 0.16f))
            .border(
                width = if (selected || dragging) 1.5.dp else 0.5.dp,
                color = when {
                    dragging -> MsColors.accent
                    selected -> MsColors.accentSoft
                    else -> trackColor(track, audio).copy(alpha = 0.8f)
                },
                shape = RoundedCornerShape(4.dp),
            )
            .pointerInput(clip.id) {
                detectTapGestures(onTap = { engine.select(track.id, clip.id) })
            },
    ) {
        Row(Modifier.fillMaxSize(), verticalAlignment = Alignment.CenterVertically) {
            if (wideEnoughForGrips) {
                TrimGrip(
                    selected = selected,
                    onStart = { dragging = true; engine.select(track.id, clip.id) },
                    onEnd = {
                        dragging = false
                        engine.trimClip(track.id, clip.id, start.toDouble(), length.toDouble())
                    },
                    onDrag = { dx ->
                        val end = start + length
                        val newStart = (start + (dx * secondsPerDp).toFloat()).coerceIn(0f, end - 0.1f)
                        start = newStart
                        length = (end - newStart).coerceAtLeast(0.1f)
                    },
                )
            }

            // The body owns moving; the grips own trimming. Separate nodes mean a
            // single gesture can never do both.
            Box(
                Modifier
                    .weight(1f)
                    .fillMaxHeight()
                    .pointerInput(clip.id, span, snapping) {
                        detectDragGestures(
                            onDragStart = { dragging = true; engine.select(track.id, clip.id) },
                            onDragEnd = {
                                dragging = false
                                onSnapChange(null)
                                engine.moveClip(
                                    track.id, clip.id,
                                    snapTo(snapTargets(engine, clip), start.toDouble(), snapping, onSnapChange),
                                )
                            },
                            onDragCancel = { dragging = false; onSnapChange(null) },
                        ) { change, drag ->
                            change.consume()
                            val maxStart = max(0f, span.toFloat() - length)
                            start = (start + (drag.x * secondsPerDp).toFloat()).coerceIn(0f, maxStart)
                            if (snapping) {
                                start = snapTo(snapTargets(engine, clip), start.toDouble(), true, onSnapChange)
                                    .toFloat().coerceIn(0f, maxStart)
                            }
                        }
                    },
            ) {
                if (audio) {
                    val peaks = remember(track.id, clip.id, clip.mediaId, clip.duration) {
                        engine.peaks(track.id, clip.id, 96)
                    }
                    if (peaks.isNotEmpty()) {
                        Canvas(Modifier.matchParentSize().padding(horizontal = 1.dp, vertical = 3.dp)) {
                            val mid = size.height / 2f
                            val bar = size.width / peaks.size
                            peaks.forEachIndexed { index, value ->
                                val amp = value.coerceIn(0f, 1f) * mid * 0.92f
                                if (amp > 0.2f) {
                                    drawRect(
                                        color = MsColors.audioTrack.copy(alpha = 0.55f),
                                        topLeft = Offset(index * bar, mid - amp),
                                        size = Size((bar * 0.72f).coerceAtLeast(0.6f), amp * 2f),
                                    )
                                }
                            }
                        }
                    }
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        clip.name.ifBlank { if (clip.isText) "TITLE" else "Clip" },
                        color = MsColors.textPrimary,
                        fontSize = 11.sp,
                        maxLines = 1,
                        modifier = Modifier.padding(start = 5.dp, end = 3.dp),
                    )
                    if (clip.effects.isNotEmpty()) {
                        Text(
                            "FX ${clip.effects.count { it.enabled }}",
                            color = MsColors.accentSoft,
                            fontSize = 8.sp,
                            fontFamily = MsFonts.mono,
                        )
                    }
                    if (clip.mute) {
                        Text(" M", color = MsColors.warning, fontSize = 8.sp, fontFamily = MsFonts.mono)
                    }
                }
            }

            if (wideEnoughForGrips) {
                TrimGrip(
                    selected = selected,
                    onStart = { dragging = true; engine.select(track.id, clip.id) },
                    onEnd = {
                        dragging = false
                        engine.trimClip(track.id, clip.id, start.toDouble(), length.toDouble())
                    },
                    onDrag = { dx ->
                        length = (length + (dx * secondsPerDp).toFloat())
                            .coerceIn(0.1f, span.toFloat() - start)
                    },
                )
            }
        }
    }
}

@Composable
private fun TrimGrip(
    selected: Boolean,
    onStart: () -> Unit,
    onEnd: () -> Unit,
    onDrag: (Float) -> Unit,
) {
    Box(
        Modifier
            .width(GripWidth)
            .fillMaxHeight()
            .background(if (selected) MsColors.accentSoft.copy(alpha = 0.3f) else Color.Transparent)
            .pointerInput(selected) {
                detectDragGestures(
                    onDragStart = { onStart() },
                    onDragEnd = { onEnd() },
                    onDragCancel = { onEnd() },
                ) { change, drag ->
                    change.consume()
                    onDrag(drag.x)
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        Box(
            Modifier.width(1.5.dp).height(12.dp)
                .background(if (selected) MsColors.accentSoft else MsColors.guideStrong)
        )
    }
}

@Composable
private fun Chip(label: String, active: Boolean, onClick: () -> Unit) {
    Box(
        Modifier
            .border(
                1.dp,
                if (active) MsColors.accent else MsColors.border,
                RoundedCornerShape(2.dp),
            )
            .clickable { onClick() }
            .padding(horizontal = 7.dp, vertical = 3.dp),
    ) {
        Text(
            label,
            color = if (active) MsColors.accent else MsColors.textSecondary,
            fontSize = 9.sp,
            fontFamily = MsFonts.mono,
            fontWeight = if (active) FontWeight.Bold else FontWeight.Normal,
        )
    }
}

/** Snap candidates: the playhead, markers, and every other clip's edges. */
private fun snapTargets(engine: EditorEngine, clip: NativeClip): List<Double> =
    buildList {
        add(engine.time)
        engine.doc.markers.forEach { add(it.time) }
        engine.doc.tracks.forEach { track ->
            track.clips.forEach { other ->
                if (other.id != clip.id) {
                    add(other.start)
                    add(other.end)
                }
            }
        }
    }

private fun snapTo(
    targets: List<Double>,
    candidate: Double,
    enabled: Boolean,
    onSnap: (Double?) -> Unit,
): Double {
    if (!enabled) {
        onSnap(null)
        return candidate
    }
    val nearest = targets.minByOrNull { abs(it - candidate) } ?: return candidate
    return if (abs(nearest - candidate) <= 0.08) {
        onSnap(nearest)
        nearest.coerceAtLeast(0.0)
    } else {
        onSnap(null)
        candidate
    }
}

private fun trackColor(track: NativeTrack, audio: Boolean): Color = when {
    track.muted -> MsColors.textMuted
    audio -> MsColors.audioTrack
    track.name.contains("overlay", ignoreCase = true) -> MsColors.overlayTrack
    else -> MsColors.videoTrack
}

private fun xDp(seconds: Double, span: Double, width: Dp): Dp =
    ((seconds / span).toFloat() * width.value).dp

/** Smallest nice step whose tick spacing clears ~54dp at the current zoom. */
private fun tickStep(span: Double, width: Dp): Double {
    val perDp = span / width.value.coerceAtLeast(1f).toDouble()
    val minimum = perDp * 54.0
    val candidates = listOf(
        1.0 / 30.0, 1.0 / 15.0, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0,
    )
    return candidates.firstOrNull { it >= minimum } ?: 240.0
}

/** Grid lines use a quarter of the label step so lanes read as a ruler, not a wall. */
private fun gridStep(secondsPerDp: Double): Double =
    max(secondsPerDp * 54.0, 1.0 / 30.0)

private fun isMajor(t: Double, step: Double): Boolean =
    step > 0 && floor(t / step + 1e-6) >= 0 && abs(t / step - (t / step).roundToInt()) < 1e-6

private fun tickLabel(t: Double, fps: Double, step: Double): String =
    if (step < 0.2) "${(t * fps).roundToInt()}f" else tc(t, fps)

private fun tc(seconds: Double, fps: Double): String {
    val clamped = seconds.coerceAtLeast(0.0)
    val totalFrames = (clamped * fps).roundToInt()
    val fr = (totalFrames % fps.roundToInt().coerceAtLeast(1))
    val totalSeconds = totalFrames / fps.roundToInt().coerceAtLeast(1)
    val m = totalSeconds / 60
    val s = totalSeconds % 60
    return "%02d:%02d:%02d".format(m, s, fr)
}

