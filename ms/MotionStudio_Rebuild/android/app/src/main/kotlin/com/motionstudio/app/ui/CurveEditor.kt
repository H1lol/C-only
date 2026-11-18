package com.motionstudio.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.editor.KEY_INTERP_NAMES
import com.motionstudio.app.editor.KeyPoint
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

private val CHANNELS = listOf(
    Triple(0, "Opacity", "keys_opacity"),
    Triple(1, "Scale", "keys_scale"),
    Triple(2, "Pos X", "keys_pos_x"),
    Triple(3, "Pos Y", "keys_pos_y"),
)

/**
 * Keyframe curve editor.
 *
 * The curve is drawn from samples the engine returns, not from a second easing
 * implementation in Kotlin, so the shape on screen is the shape that renders.
 * Tapping near a key selects it; the selected key can be retimed, retargeted and
 * given its own interpolation, which is what makes the difference between
 * "there is a keyframe somewhere" and animating.
 */
@Composable
fun KeyframePane(engine: EditorEngine, modifier: Modifier = Modifier) {
    val clip = engine.doc.clipById(engine.selectedTrackId, engine.selectedClipId)
    var channel by remember { mutableIntStateOf(0) }
    var selected by remember { mutableIntStateOf(-1) }
    var revision by remember { mutableIntStateOf(0) }

    val prop = CHANNELS[channel].first
    val span = clip?.duration?.coerceAtLeast(0.1) ?: 1.0
    val keys: List<KeyPoint> = remember(channel, revision, clip?.id) {
        if (clip == null) emptyList() else engine.keyframes(prop)
    }
    val samples: FloatArray = remember(channel, revision, clip?.id) {
        if (clip == null) FloatArray(2) else engine.sampleCurve(prop, 0.0, span, 128)
    }
    val local = clip?.let { (engine.time - it.start).coerceIn(0.0, span) } ?: 0.0

    Column(modifier.background(MsColors.background)) {
        PanelHeader(
            "KEYFRAMES",
            clip?.let { "local %.2fs".format(local) } ?: "no clip",
        )

        if (clip == null) {
            Text(
                "Select a clip on the timeline to animate it.",
                color = MsColors.textTertiary,
                fontSize = 12.sp,
                modifier = Modifier.padding(12.dp),
            )
            return@Column
        }

        // Channel selector with per-channel key counts.
        Row(
            Modifier.fillMaxWidth().horizontalScroll(rememberScrollState())
                .padding(horizontal = 10.dp, vertical = 4.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            CHANNELS.forEachIndexed { index, (_, label, jsonKey) ->
                val count = clip.keyframes[jsonKey] ?: 0
                val active = index == channel
                Text(
                    if (count > 0) "$label $count" else label,
                    color = if (active) MsColors.background else MsColors.textSecondary,
                    fontSize = 10.sp,
                    fontFamily = MsFonts.mono,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier
                        .background(
                            if (active) MsColors.accent else MsColors.surfaceHigh,
                            RoundedCornerShape(4.dp),
                        )
                        .clickable {
                            channel = index
                            selected = -1
                        }
                        .padding(horizontal = 9.dp, vertical = 6.dp),
                )
            }
        }

        // The curve itself.
        CurveCanvas(
            samples = samples,
            keys = keys,
            span = span,
            playhead = local,
            selected = selected,
            onSelect = { selected = it },
        )

        Column(
            Modifier.fillMaxWidth().weight(1f, fill = false)
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 12.dp, vertical = 6.dp),
        ) {
            if (keys.isEmpty()) {
                Text(
                    "No keys on ${CHANNELS[channel].second}. \"Key at playhead\" captures " +
                        "the current value; with two keys the channel animates.",
                    color = MsColors.textMuted,
                    fontSize = 10.sp,
                )
            } else {
                val key = keys.getOrNull(selected)
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        if (key == null) "TAP A KEY ON THE CURVE"
                        else "KEY ${key.index} · %.2fs · %.3f".format(key.time, key.value),
                        color = MsColors.accentSoft,
                        fontSize = 10.sp,
                        fontFamily = MsFonts.mono,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier.weight(1f),
                    )
                    TextButton(onClick = { selected = -1 }) {
                        Text("DESELECT", color = MsColors.textMuted, fontSize = 9.sp, fontFamily = MsFonts.mono)
                    }
                }

                if (key != null) {
                    // Interpolation for the segment that starts at this key.
                    Row(
                        Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(5.dp),
                    ) {
                        KEY_INTERP_NAMES.forEachIndexed { index, name ->
                            val active = key.interp == index
                            Text(
                                name,
                                color = if (active) MsColors.background else MsColors.textSecondary,
                                fontSize = 9.sp,
                                fontFamily = MsFonts.mono,
                                fontWeight = FontWeight.Bold,
                                modifier = Modifier
                                    .background(
                                        if (active) MsColors.accent else MsColors.surfaceHigh,
                                        RoundedCornerShape(4.dp),
                                    )
                                    .clickable {
                                        engine.setKeyframeInterp(prop, key.index, index)
                                        revision++
                                    }
                                    .padding(horizontal = 8.dp, vertical = 5.dp),
                            )
                        }
                    }
                    Spacer(Modifier.height(4.dp))
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Stepper("TIME", "%.2f".format(key.time), onMinus = {
                            engine.setKeyframe(prop, key.index, max(0.0, key.time - 0.1), key.value, key.interp)
                            revision++
                        }, onPlus = {
                            engine.setKeyframe(prop, key.index, min(span, key.time + 0.1), key.value, key.interp)
                            revision++
                        }, Modifier.weight(1f))
                        Stepper("VALUE", "%.3f".format(key.value), onMinus = {
                            engine.setKeyframe(prop, key.index, key.time, key.value - valueStep(prop, key.value), key.interp)
                            revision++
                        }, onPlus = {
                            engine.setKeyframe(prop, key.index, key.time, key.value + valueStep(prop, key.value), key.interp)
                            revision++
                        }, Modifier.weight(1f))
                    }
                    Spacer(Modifier.height(4.dp))
                }
            }

            Spacer(Modifier.height(6.dp))
            Row(
                Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    "Adds a key at the playhead with the clip's current value.",
                    color = MsColors.textMuted,
                    fontSize = 9.sp,
                    modifier = Modifier.weight(1f),
                )
                TextButton(onClick = {
                    val value = when (prop) {
                        0 -> clip.opacity.toFloat()
                        else -> if (prop == 1) 1f else 0f
                    }
                    engine.addKeyframe(prop, value)
                    revision++
                }) {
                    Text("+ KEY", color = MsColors.accentSoft, fontSize = 10.sp, fontFamily = MsFonts.mono)
                }
                TextButton(onClick = {
                    engine.clearKeyframes(prop)
                    selected = -1
                    revision++
                }) {
                    Text("CLEAR", color = MsColors.error, fontSize = 10.sp, fontFamily = MsFonts.mono)
                }
            }
        }
    }
}

private fun valueStep(prop: Int, value: Double): Double = when (prop) {
    0 -> 0.05          // opacity 0..1
    1 -> 0.05          // scale
    else -> 5.0        // pixels
}

@Composable
private fun CurveCanvas(
    samples: FloatArray,
    keys: List<KeyPoint>,
    span: Double,
    playhead: Double,
    selected: Int,
    onSelect: (Int) -> Unit,
) {
    // Size comes from layout, never from inside the draw pass: writing state
    // while drawing schedules another frame and loops.
    var canvasSize by remember { mutableStateOf(IntSize.Zero) }

    // A stable value range across the samples and keys, with a little air.
    var lo = Float.MAX_VALUE
    var hi = -Float.MAX_VALUE
    samples.forEach { v ->
        if (v < lo) lo = v
        if (v > hi) hi = v
    }
    keys.forEach { k ->
        val v = k.value.toFloat()
        if (v < lo) lo = v
        if (v > hi) hi = v
    }
    if (lo > hi) {
        lo = 0f
        hi = 1f
    }
    val pad = max(0.08f * (hi - lo), 0.05f)
    lo -= pad
    hi += pad
    val range = max(hi - lo, 1e-4f)

    fun xAt(t: Double): Float =
        ((t / span).coerceIn(0.0, 1.0) * canvasSize.width).toFloat()

    fun yAt(v: Float): Float =
        canvasSize.height - ((v - lo) / range) * canvasSize.height

    Canvas(
        Modifier
            .fillMaxWidth()
            .height(150.dp)
            .padding(horizontal = 12.dp)
            .border(1.dp, MsColors.border, RoundedCornerShape(3.dp))
            .onSizeChanged { canvasSize = it }
            .pointerInput(keys, span, canvasSize) {
                detectTapGestures { offset ->
                    if (keys.isEmpty()) return@detectTapGestures
                    // Nearest key by horizontal distance, within a finger's width.
                    var bestIndex = -1
                    var bestDistance = Float.MAX_VALUE
                    keys.forEach { k ->
                        val d = abs(xAt(k.time) - offset.x)
                        if (d < bestDistance) {
                            bestDistance = d
                            bestIndex = k.index
                        }
                    }
                    if (bestIndex >= 0 && bestDistance <= 48f) onSelect(bestIndex)
                }
            },
    ) {
        // Quarter grid.
        listOf(0.25f, 0.5f, 0.75f).forEach { f ->
            drawLine(
                MsColors.border,
                Offset(0f, size.height * f),
                Offset(size.width, size.height * f),
                strokeWidth = 1f,
            )
            drawLine(
                MsColors.border,
                Offset(size.width * f, 0f),
                Offset(size.width * f, size.height),
                strokeWidth = 1f,
            )
        }

        // The engine's own samples.
        if (samples.size >= 2) {
            val path = Path()
            samples.forEachIndexed { index, value ->
                val x = size.width * index / (samples.size - 1).toFloat()
                val y = yAt(value)
                if (index == 0) path.moveTo(x, y) else path.lineTo(x, y)
            }
            drawPath(path, MsColors.accent, style = Stroke(width = 2f))
        }

        // Keys.
        keys.forEach { k ->
            val isSelected = k.index == selected
            val center = Offset(xAt(k.time), yAt(k.value.toFloat()))
            drawCircle(
                color = if (isSelected) Color.White else MsColors.accentSoft,
                radius = if (isSelected) 6f else 4.5f,
                center = center,
            )
            drawCircle(
                color = MsColors.accent,
                radius = if (isSelected) 6f else 4.5f,
                center = center,
                style = Stroke(width = 1.5f),
            )
        }

        // Playhead.
        val px = size.width * (playhead / span).coerceIn(0.0, 1.0).toFloat()
        drawLine(MsColors.accent, Offset(px, 0f), Offset(px, size.height), strokeWidth = 1.5f)
    }
}

@Composable
private fun Stepper(
    label: String,
    value: String,
    onMinus: () -> Unit,
    onPlus: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier
            .background(MsColors.surfaceHigh, RoundedCornerShape(3.dp))
            .padding(horizontal = 6.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, color = MsColors.textMuted, fontSize = 9.sp, fontFamily = MsFonts.mono)
        Spacer(Modifier.weight(1f))
        Text(
            "−",
            color = MsColors.accent,
            fontSize = 14.sp,
            fontFamily = MsFonts.mono,
            modifier = Modifier.clickable(onClick = onMinus).padding(horizontal = 7.dp),
        )
        Text(value, color = MsColors.textPrimary, fontSize = 11.sp, fontFamily = MsFonts.mono)
        Text(
            "+",
            color = MsColors.accent,
            fontSize = 14.sp,
            fontFamily = MsFonts.mono,
            modifier = Modifier.clickable(onClick = onPlus).padding(horizontal = 7.dp),
        )
    }
}
