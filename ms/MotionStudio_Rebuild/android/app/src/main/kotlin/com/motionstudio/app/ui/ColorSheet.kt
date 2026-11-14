package com.motionstudio.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts
import kotlinx.coroutines.delay

/**
 * Master grade plus a live RGB/luma histogram read from the engine's last
 * rendered frame (`Scopes::histogram` in C++).
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ColorSheet(engine: EditorEngine, onDismiss: () -> Unit) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    var hist by remember { mutableStateOf(IntArray(256)) }
    // Master grade (whole sequence) or the selected clip's own grade.
    var clipMode by remember { mutableStateOf(false) }

    LaunchedEffect(engine) {
        while (true) {
            hist = engine.histogram(64)
            delay(350L)
        }
    }
    LaunchedEffect(engine.selectedClipId) { engine.refreshClipGrade() }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = MsColors.surface,
    ) {
        Column(
            Modifier
                .fillMaxWidth()
                .padding(horizontal = 20.dp)
                .verticalScroll(rememberScrollState()),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text("Color", color = MsColors.textPrimary, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
                    Text(
                        when {
                            clipMode && engine.clipGradeActive -> "Clip grade active"
                            clipMode -> "This clip has no grade"
                            engine.gradeActive -> "Grade active"
                            else -> "No grade applied"
                        },
                        color = if ((clipMode && engine.clipGradeActive) || (!clipMode && engine.gradeActive))
                            MsColors.success else MsColors.textTertiary,
                        fontSize = 11.sp,
                    )
                }
                TextButton(onClick = {
                    if (clipMode) engine.clearClipGrade() else engine.resetGrade()
                }) {
                    Text("Reset", color = MsColors.textTertiary, fontSize = 12.sp)
                }
            }

            Spacer(Modifier.height(6.dp))
            Row {
                TextButton(
                    onClick = { clipMode = false },
                    modifier = Modifier
                        .clip(RoundedCornerShape(8.dp))
                        .background(if (!clipMode) MsColors.accentSoft.copy(alpha = 0.16f) else Color.Transparent),
                ) {
                    Text(
                        "Sequence",
                        color = if (!clipMode) MsColors.accentSoft else MsColors.textSecondary,
                        fontSize = 12.sp,
                    )
                }
                TextButton(
                    onClick = {
                        clipMode = true
                        engine.refreshClipGrade()
                    },
                    modifier = Modifier
                        .clip(RoundedCornerShape(8.dp))
                        .background(if (clipMode) MsColors.accentSoft.copy(alpha = 0.16f) else Color.Transparent),
                ) {
                    Text(
                        "Selected clip",
                        color = if (clipMode) MsColors.accentSoft else MsColors.textSecondary,
                        fontSize = 12.sp,
                    )
                }
            }

            Spacer(Modifier.height(6.dp))
            Histogram(hist)
            Spacer(Modifier.height(10.dp))

            if (clipMode) {
                val g = engine.clipGrade
                GradeSlider("Exposure", g[0], -2f, 2f) { engine.applyClipGrade(exposure = it) }
                GradeSlider("Contrast", g[1], 0f, 2f) { engine.applyClipGrade(contrast = it) }
                GradeSlider("Saturation", g[2], 0f, 2f) { engine.applyClipGrade(saturation = it) }
                GradeSlider("Temperature", g[3], -1f, 1f) { engine.applyClipGrade(temperature = it) }
                GradeSlider("Lift", g[4], -0.5f, 0.5f) { engine.applyClipGrade(lift = it) }
                GradeSlider("Gamma", g[5], 0.4f, 2.5f) { engine.applyClipGrade(gamma = it) }
                GradeSlider("Gain", g[6], 0.2f, 2f) { engine.applyClipGrade(gain = it) }
                Spacer(Modifier.height(4.dp))
                Text(
                    "Per-clip grades apply before compositing, so one shot can be matched " +
                        "without changing the rest of the sequence.",
                    color = MsColors.textTertiary,
                    fontSize = 11.sp,
                )
            } else {
                GradeSlider("Exposure", engine.gradeExposure, -2f, 2f) { engine.applyGrade(exposure = it) }
                GradeSlider("Contrast", engine.gradeContrast, 0f, 2f) { engine.applyGrade(contrast = it) }
                GradeSlider("Saturation", engine.gradeSaturation, 0f, 2f) { engine.applyGrade(saturation = it) }
                GradeSlider("Temperature", engine.gradeTemperature, -1f, 1f) { engine.applyGrade(temperature = it) }
                GradeSlider("Lift", engine.gradeLift, -0.5f, 0.5f) { engine.applyGrade(lift = it) }
                GradeSlider("Gamma", engine.gradeGamma, 0.4f, 2.5f) { engine.applyGrade(gamma = it) }
                GradeSlider("Gain", engine.gradeGain, 0.2f, 2f) { engine.applyGrade(gain = it) }
            }
            Spacer(Modifier.height(20.dp))
        }
    }
}

@Composable
private fun Histogram(hist: IntArray) {
    val bins = hist.size / 4
    Canvas(
        Modifier
            .fillMaxWidth()
            .height(96.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(MsColors.viewerBg),
    ) {
        if (bins <= 0) return@Canvas
        var peak = 1
        for (i in 0 until bins * 4) if (hist[i] > peak) peak = hist[i]
        val w = size.width / bins
        val channels = listOf(
            Color(0xFFFF6B6B) to 0,
            Color(0xFF5BD6A8) to 1,
            Color(0xFF6B9BFF) to 2,
        )
        for ((color, channel) in channels) {
            for (i in 0 until bins) {
                val v = hist[channel * bins + i].toFloat() / peak
                val h = v * size.height
                drawLine(
                    color = color.copy(alpha = 0.45f),
                    start = Offset(i * w, size.height),
                    end = Offset(i * w, size.height - h),
                    strokeWidth = w.coerceAtLeast(1f),
                )
            }
        }
        for (i in 0 until bins) {
            val v = hist[3 * bins + i].toFloat() / peak
            drawLine(
                color = Color.White.copy(alpha = 0.55f),
                start = Offset(i * w, size.height),
                end = Offset(i * w, size.height - v * size.height),
                strokeWidth = 1f,
            )
        }
    }
}

@Composable
private fun GradeSlider(
    label: String,
    value: Float,
    min: Float,
    max: Float,
    onChange: (Float) -> Unit,
) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(
            label,
            color = MsColors.textSecondary,
            fontSize = 12.sp,
            modifier = Modifier.fillMaxWidth(0.30f),
        )
        Text(
            "%.2f".format(value),
            color = MsColors.textPrimary,
            fontSize = 11.sp,
            fontFamily = MsFonts.mono,
            modifier = Modifier.fillMaxWidth(0.34f),
        )
        Slider(
            value = value.coerceIn(min, max),
            onValueChange = onChange,
            valueRange = min..max,
            modifier = Modifier.fillMaxWidth(),
        )
    }
}
