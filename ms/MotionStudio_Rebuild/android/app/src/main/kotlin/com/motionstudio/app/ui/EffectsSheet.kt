package com.motionstudio.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts

data class EffectParamDef(
    val name: String,
    val label: String,
    val min: Float,
    val max: Float,
    val default: Float,
)

data class EffectDef(
    val id: String,
    val name: String,
    val note: String,
    val params: List<EffectParamDef>,
    val group: String = "Basic",
)

private fun p(name: String, label: String, min: Float, max: Float, default: Float) =
    EffectParamDef(name, label, min, max, default)

/**
 * Mirrors the CPU registry: cpu_effects.cpp, cpu_effects_extra.cpp and
 * cpu_effects_catalogue.cpp. Every id here is reachable through
 * cpu_effect_apply and covered by ms_effects_catalogue_test.
 */
val MsEffects = listOf(
    // ---- Basic ----
    EffectDef("gaussian_blur", "Gaussian Blur", "Separable CPU blur",
        listOf(p("radius", "Radius", 0f, 64f, 4f)), "Basic"),
    EffectDef("box_blur", "Box Blur", "Running-sum blur",
        listOf(p("radius", "Radius", 0f, 64f, 4f)), "Basic"),
    EffectDef("sharpen", "Sharpen", "Unsharp 3x3",
        listOf(p("amount", "Amount", 0f, 3f, 0.6f)), "Basic"),
    EffectDef("brightness_contrast", "Brightness / Contrast", "Levels",
        listOf(p("brightness", "Brightness", -1f, 1f, 0.06f), p("contrast", "Contrast", 0f, 2f, 1.1f)), "Basic"),
    EffectDef("levels", "Levels", "Black / white / gamma",
        listOf(
            p("black", "Black", 0f, 0.9f, 0f),
            p("white", "White", 0.1f, 1f, 1f),
            p("gamma", "Gamma", 0.2f, 3f, 1.2f),
        ), "Basic"),
    EffectDef("opacity", "Opacity", "Blend to transparent",
        listOf(p("opacity", "Opacity", 0f, 1f, 0.8f)), "Basic"),

    // ---- Colour ----
    EffectDef("grayscale", "Grayscale", "Luma desaturate", emptyList(), "Colour"),
    EffectDef("sepia", "Sepia", "Warm monochrome",
        listOf(p("amount", "Amount", 0f, 1f, 1f)), "Colour"),
    EffectDef("invert", "Invert", "Negative", emptyList(), "Colour"),
    EffectDef("saturation", "Saturation", "1 = unchanged, 0 = grey",
        listOf(p("saturation", "Amount", 0f, 3f, 1.4f)), "Colour"),
    EffectDef("hue_shift", "Hue Shift", "Rotate the colour wheel",
        listOf(p("degrees", "Degrees", -180f, 180f, 30f)), "Colour"),
    EffectDef("white_balance", "White Balance", "Temperature / tint",
        listOf(
            p("temperature", "Temp", -1f, 1f, 0.2f),
            p("tint", "Tint", -1f, 1f, 0f),
        ), "Colour"),
    EffectDef("posterize", "Posterize", "Quantise to N levels",
        listOf(p("levels", "Levels", 2f, 32f, 8f)), "Colour"),
    EffectDef("threshold", "Threshold", "Binarize on luma",
        listOf(p("level", "Level", 0f, 1f, 0.5f)), "Colour"),
    EffectDef("chromakey", "Chroma Key", "Knock out green",
        listOf(p("tolerance", "Tolerance", 0f, 1f, 0.2f)), "Colour"),

    // ---- Stylise ----
    EffectDef("glow", "Glow", "Blur screened back over the image",
        listOf(p("amount", "Amount", 0f, 1f, 0.5f), p("radius", "Radius", 0f, 32f, 8f)), "Stylise"),
    EffectDef("noise", "Film Grain", "Deterministic per-pixel noise",
        listOf(p("amount", "Amount", 0f, 0.6f, 0.15f)), "Stylise"),
    EffectDef("emboss", "Emboss", "Directional relief",
        listOf(p("amount", "Amount", 0f, 2f, 0.5f)), "Stylise"),
    EffectDef("edge_detect", "Edges", "Sobel gradient",
        listOf(p("amount", "Amount", 0f, 3f, 1f)), "Stylise"),
    EffectDef("pixelate", "Pixelate", "Block mosaic",
        listOf(p("size", "Size", 1f, 64f, 8f)), "Stylise"),
    EffectDef("vignette", "Vignette", "Edge falloff",
        listOf(p("strength", "Strength", 0f, 1f, 0.6f), p("radius", "Radius", 0.1f, 1.5f, 1f)), "Stylise"),

    // ---- Framing ----
    EffectDef("mirror", "Mirror", "Flip horizontally or vertically",
        listOf(p("horizontal", "Horizontal", 0f, 1f, 1f)), "Framing"),
    EffectDef("crop", "Crop", "Clear the frame edges",
        listOf(
            p("left", "Left", 0f, 0.9f, 0.08f),
            p("right", "Right", 0f, 0.9f, 0.08f),
            p("top", "Top", 0f, 0.9f, 0.08f),
            p("bottom", "Bottom", 0f, 0.9f, 0.08f),
        ), "Framing"),
)

private fun definition(id: String): EffectDef =
    MsEffects.firstOrNull { it.id == id } ?: EffectDef(id, id.replace('_', ' '), "Plugin effect", emptyList())

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EffectsSheet(engine: EditorEngine, onDismiss: () -> Unit) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    val applied = engine.selectedEffects
    val clipName = engine.doc.clipById(engine.selectedTrackId, engine.selectedClipId)?.name
    // Local slider positions so dragging does not fight the engine round-trip.
    val drafts = remember(applied.size, engine.selectedClipId) { mutableStateMapOf<String, Float>() }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = MsColors.surface,
    ) {
        Column(Modifier.fillMaxWidth().padding(horizontal = 20.dp)) {
            Text("Effects", color = MsColors.textPrimary, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(2.dp))
            Text(
                if (clipName != null) "On “$clipName” · rendered by the C++ effect stack"
                else "Select a clip to add effects",
                color = MsColors.textTertiary,
                fontSize = 12.sp,
            )
            Spacer(Modifier.height(10.dp))

            LazyColumn(Modifier.fillMaxWidth().height(430.dp)) {
                if (clipName != null && applied.isNotEmpty()) {
                    item {
                        Text("APPLIED", color = MsColors.textMuted, fontSize = 9.sp, fontFamily = MsFonts.mono)
                    }
                    items(applied.size) { index ->
                        val effect = applied[index]
                        val def = definition(effect.id)
                        Column(Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Column(Modifier.weight(1f)) {
                                    Text(
                                        def.name,
                                        color = MsColors.textPrimary,
                                        fontSize = 14.sp,
                                        fontWeight = FontWeight.Medium,
                                    )
                                    Text(
                                        "${effect.params.size} params · ${def.note}",
                                        color = MsColors.textMuted,
                                        fontSize = 11.sp,
                                    )
                                }
                                Switch(
                                    checked = effect.enabled,
                                    onCheckedChange = { engine.setEffectEnabled(index, it) },
                                )
                                TextButton(onClick = { engine.removeEffect(index) }) {
                                    Text("Remove", color = MsColors.error, fontSize = 11.sp)
                                }
                            }
                            def.params.forEach { param ->
                                val key = "$index:${param.name}"
                                val current = drafts[key]
                                    ?: (effect.params[param.name]?.toFloat() ?: param.default)
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Text(
                                        "${param.label}  ${"%.2f".format(current)}",
                                        color = MsColors.textSecondary,
                                        fontSize = 11.sp,
                                        fontFamily = MsFonts.mono,
                                        modifier = Modifier.fillMaxWidth(0.34f),
                                    )
                                    Slider(
                                        value = current.coerceIn(param.min, param.max),
                                        onValueChange = {
                                            drafts[key] = it
                                            engine.setEffectParam(index, param.name, it)
                                        },
                                        valueRange = param.min..param.max,
                                        modifier = Modifier.weight(1f),
                                    )
                                }
                            }
                        }
                    }
                    item { Spacer(Modifier.height(10.dp)) }
                }

                item {
                    Text("ADD", color = MsColors.textMuted, fontSize = 9.sp, fontFamily = MsFonts.mono)
                }
                MsEffects.groupBy { it.group }.forEach { (group, defs) ->
                    item {
                        Text(
                            group.uppercase(),
                            color = MsColors.textMuted,
                            fontSize = 9.sp,
                            fontFamily = MsFonts.mono,
                            modifier = Modifier.padding(top = 8.dp),
                        )
                    }
                    items(defs) { def ->
                        Row(
                            Modifier.fillMaxWidth().padding(vertical = 3.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            Column(Modifier.weight(1f)) {
                                Text(def.name, color = MsColors.textPrimary, fontSize = 14.sp)
                                Text(def.note, color = MsColors.textMuted, fontSize = 11.sp)
                            }
                            TextButton(
                                onClick = {
                                    engine.addEffect(def.id, def.params.associate { it.name to it.default })
                                },
                                enabled = clipName != null,
                            ) {
                                Text(
                                    "Add",
                                    color = if (clipName != null) MsColors.accentSoft
                                    else MsColors.textMuted,
                                    fontSize = 12.sp,
                                )
                            }
                        }
                    }
                }
            }
            Spacer(Modifier.height(20.dp))
        }
    }
}
