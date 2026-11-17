package com.motionstudio.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.editor.AUDIO_INSERT_NAMES
import com.motionstudio.app.editor.AudioInsert
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.model.NativeTrack
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts
import kotlin.math.log10

/** One knob an insert exposes, with the range the mixer offers. */
data class AudioKnob(
    val name: String,
    val label: String,
    val min: Float,
    val max: Float,
    val default: Float,
)

private fun k(name: String, label: String, min: Float, max: Float, default: Float) =
    AudioKnob(name, label, min, max, default)

/**
 * Mirrors ms::AudioFxParams, in the same order as ms::AudioFxKind. Every name
 * here is accepted by ms_audio_fx_set_param; anything else is rejected by the
 * bridge, so this table cannot drift into silently dead sliders.
 */
fun audioKnobs(kind: Int): List<AudioKnob> = when (kind) {
    0 -> listOf(k("gain_db", "Gain", -60f, 12f, 0f))
    1 -> listOf(
        k("low_gain_db", "Low", -18f, 18f, 0f),
        k("low_freq_hz", "Low Hz", 30f, 320f, 100f),
        k("mid_gain_db", "Mid", -18f, 18f, 0f),
        k("mid_freq_hz", "Mid Hz", 300f, 6000f, 1000f),
        k("mid_q", "Mid Q", 0.2f, 8f, 1f),
        k("high_gain_db", "High", -18f, 18f, 0f),
        k("high_freq_hz", "High Hz", 2000f, 16000f, 8000f),
        k("wet", "Mix", 0f, 1f, 1f),
    )
    2 -> listOf(
        k("threshold_db", "Threshold", -48f, 0f, -18f),
        k("ratio", "Ratio", 1f, 20f, 4f),
        k("attack_ms", "Attack", 0.1f, 100f, 10f),
        k("release_ms", "Release", 10f, 1000f, 100f),
        k("makeup_db", "Makeup", 0f, 24f, 0f),
        k("wet", "Mix", 0f, 1f, 1f),
    )
    3 -> listOf(
        k("delay_ms", "Delay", 10f, 1500f, 280f),
        k("feedback", "Feedback", 0f, 0.9f, 0.25f),
        k("wet", "Mix", 0f, 1f, 0.4f),
    )
    4 -> listOf(
        k("room_size", "Room", 0f, 1f, 0.72f),
        k("damping", "Damping", 0f, 0.95f, 0.35f),
        k("wet", "Mix", 0f, 1f, 0.4f),
    )
    5 -> listOf(
        k("ceiling_db", "Ceiling", -12f, 0f, -1f),
        k("wet", "Mix", 0f, 1f, 1f),
    )
    else -> emptyList()
}

private fun formatKnob(knob: AudioKnob, value: Float): String = when {
    knob.name.endsWith("_db") -> "%.1f dB".format(value)
    knob.name.endsWith("_hz") -> "%.0f Hz".format(value)
    knob.name.endsWith("_ms") -> "%.0f ms".format(value)
    else -> "%.2f".format(value)
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AudioSheet(engine: EditorEngine, onDismiss: () -> Unit) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    val revision = engine.audioRevision
    val inserts = remember(revision) { engine.audioInserts() }
    // Slider positions live here while dragging so the engine round-trip does
    // not pull the handle back under the finger.
    val drafts = remember(revision) { mutableStateMapOf<String, Float>() }
    val tracks = engine.doc.tracks

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = MsColors.surface,
    ) {
        Column(Modifier.fillMaxWidth().padding(horizontal = 20.dp)) {
            Text("Audio", color = MsColors.textPrimary, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(2.dp))
            Text(
                "Master insert chain and track mixer · mixed in C++",
                color = MsColors.textTertiary,
                fontSize = 12.sp,
            )
            Spacer(Modifier.height(10.dp))

            LazyColumn(Modifier.fillMaxWidth().height(430.dp)) {
                item { SectionLabel("MASTER CHAIN") }
                if (inserts.isEmpty()) {
                    item {
                        Text(
                            "No inserts — the mix passes through untouched.",
                            color = MsColors.textMuted,
                            fontSize = 11.sp,
                            modifier = Modifier.padding(vertical = 6.dp),
                        )
                    }
                }
                items(inserts) { insert ->
                    InsertCard(engine, insert, drafts)
                }
                if (inserts.isNotEmpty()) {
                    item {
                        TextButton(onClick = { engine.clearAudioInserts() }) {
                            Text("Clear chain", color = MsColors.error, fontSize = 11.sp)
                        }
                    }
                }

                item { SectionLabel("ADD INSERT") }
                items(AUDIO_INSERT_NAMES.chunked(3)) { row ->
                    Row(
                        Modifier.fillMaxWidth().padding(vertical = 2.dp),
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        row.forEach { name ->
                            val kind = AUDIO_INSERT_NAMES.indexOf(name)
                            Box(Modifier.weight(1f)) {
                                TextButton(
                                    onClick = { engine.addAudioInsert(kind) },
                                    modifier = Modifier.fillMaxWidth(),
                                ) {
                                    Text(name, color = MsColors.accentSoft, fontSize = 11.sp, maxLines = 1)
                                }
                            }
                        }
                        // Keep the last row aligned with the grid above it.
                        repeat(3 - row.size) { Spacer(Modifier.weight(1f)) }
                    }
                }

                item { SectionLabel("TRACK MIXER") }
                if (tracks.isEmpty()) {
                    item {
                        Text(
                            "No tracks yet.",
                            color = MsColors.textMuted,
                            fontSize = 11.sp,
                            modifier = Modifier.padding(vertical = 6.dp),
                        )
                    }
                }
                items(tracks) { track -> TrackStrip(engine, track) }
            }
            Spacer(Modifier.height(20.dp))
        }
    }
}

@Composable
private fun SectionLabel(text: String) {
    Text(
        text,
        color = MsColors.textMuted,
        fontSize = 9.sp,
        fontFamily = MsFonts.mono,
        modifier = Modifier.padding(top = 12.dp, bottom = 4.dp),
    )
}

@Composable
private fun InsertCard(
    engine: EditorEngine,
    insert: AudioInsert,
    drafts: MutableMap<String, Float>,
) {
    val name = AUDIO_INSERT_NAMES.getOrElse(insert.kind) { "Insert" }
    Column(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(name, color = MsColors.textPrimary, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                Text(
                    "INSERT ${insert.index + 1}" + if (insert.enabled) "" else " · BYPASSED",
                    color = if (insert.enabled) MsColors.textMuted else MsColors.warning,
                    fontSize = 9.sp,
                    fontFamily = MsFonts.mono,
                )
            }
            Switch(
                checked = insert.enabled,
                onCheckedChange = { engine.setAudioInsertEnabled(insert.index, it) },
            )
            TextButton(onClick = { engine.removeAudioInsert(insert.index) }) {
                Text("Remove", color = MsColors.error, fontSize = 11.sp)
            }
        }
        audioKnobs(insert.kind).forEach { knob ->
            val key = "${insert.index}:${knob.name}"
            val current = drafts[key]
                ?: engine.audioInsertParam(insert.index, knob.name, knob.default)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    knob.label,
                    color = MsColors.textSecondary,
                    fontSize = 11.sp,
                    fontFamily = MsFonts.mono,
                    modifier = Modifier.fillMaxWidth(0.30f),
                )
                Slider(
                    value = current.coerceIn(knob.min, knob.max),
                    onValueChange = {
                        drafts[key] = it
                        engine.setAudioInsertParam(insert.index, knob.name, it)
                    },
                    valueRange = knob.min..knob.max,
                    enabled = insert.enabled,
                    modifier = Modifier.weight(1f),
                )
                Text(
                    formatKnob(knob, current),
                    color = MsColors.textTertiary,
                    fontSize = 9.sp,
                    fontFamily = MsFonts.mono,
                    modifier = Modifier.padding(start = 6.dp),
                )
            }
        }
    }
}

@Composable
private fun TrackStrip(engine: EditorEngine, track: NativeTrack) {
    val gain = track.gain.toFloat().coerceIn(0f, 2f)
    Column(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Column(Modifier.weight(1f)) {
                Text(track.name, color = MsColors.textPrimary, fontSize = 13.sp)
                Text(
                    (if (track.type == 1) "AUDIO" else "VIDEO") + " · ${track.clips.size} CLIPS",
                    color = MsColors.textMuted,
                    fontSize = 9.sp,
                    fontFamily = MsFonts.mono,
                )
            }
            MiniChip("MUTE", track.muted) { engine.toggleTrackMuted(track.id) }
            MiniChip("SOLO", track.solo) { engine.toggleTrackSolo(track.id) }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                "FADER",
                color = MsColors.textSecondary,
                fontSize = 10.sp,
                fontFamily = MsFonts.mono,
                modifier = Modifier.fillMaxWidth(0.30f),
            )
            Slider(
                value = gain,
                onValueChange = { engine.setTrackGain(track.id, it) },
                valueRange = 0f..2f,
                modifier = Modifier.weight(1f),
            )
            Text(
                if (gain <= 0.0001f) "-inf" else "%.1f dB".format(20.0 * log10(gain.toDouble())),
                color = MsColors.textTertiary,
                fontSize = 9.sp,
                fontFamily = MsFonts.mono,
                modifier = Modifier.padding(start = 6.dp),
            )
        }
    }
}

@Composable
private fun MiniChip(label: String, active: Boolean, onClick: () -> Unit) {
    Box(
        Modifier
            .border(1.dp, if (active) MsColors.accent else MsColors.border, RoundedCornerShape(2.dp))
            .background(if (active) MsColors.accentDim else Color.Transparent)
            .clickable(onClick = onClick)
            .padding(horizontal = 8.dp, vertical = 4.dp)
    ) {
        Text(
            label,
            color = if (active) MsColors.accentSoft else MsColors.textSecondary,
            fontSize = 9.sp,
            fontFamily = MsFonts.mono,
        )
    }
}
