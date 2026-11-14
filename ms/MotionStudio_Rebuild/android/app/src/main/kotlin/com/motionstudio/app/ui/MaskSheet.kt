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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts

/** Shape masks: rectangle, ellipse or full frame, with feather and invert. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MaskSheet(engine: EditorEngine, onDismiss: () -> Unit) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)

    var shape by remember { mutableIntStateOf(0) }
    var x by remember { mutableFloatStateOf(0.1f) }
    var y by remember { mutableFloatStateOf(0.1f) }
    var w by remember { mutableFloatStateOf(0.8f) }
    var h by remember { mutableFloatStateOf(0.8f) }
    var feather by remember { mutableFloatStateOf(0f) }
    var inverted by remember { mutableStateOf(false) }

    LaunchedEffect(engine.selectedClipId) {
        engine.refreshClipMask()
        val m = engine.clipMask
        shape = m[0].toInt().coerceIn(0, 2)
        x = m[1]
        y = m[2]
        w = m[3]
        h = m[4]
        feather = m[5]
        inverted = engine.clipMaskInverted
    }

    fun push(nextShape: Int = shape) {
        if (engine.clipMaskActive || nextShape != 3) {
            engine.setClipMask(nextShape, x, y, w, h, feather, inverted)
        }
    }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = MsColors.surface,
    ) {
        Column(
            Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 20.dp)
                .padding(bottom = 24.dp),
        ) {
            Text("Mask", color = MsColors.textPrimary, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
            Text(
                if (engine.selectedClipId < 0) "Select a clip to shape it"
                else if (engine.clipMaskActive) "Mask active on the selected clip"
                else "No mask on the selected clip",
                color = if (engine.clipMaskActive) MsColors.success else MsColors.textTertiary,
                fontSize = 11.sp,
            )

            Spacer(Modifier.height(10.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                listOf("Rect" to 0, "Ellipse" to 1, "Full" to 2).forEach { (label, value) ->
                    val selected = shape == value
                    Box(
                        Modifier
                            .background(
                                if (selected) MsColors.accentSoft.copy(alpha = 0.16f) else MsColors.surfaceHigh,
                                RoundedCornerShape(8.dp),
                            )
                            .border(
                                1.dp,
                                if (selected) MsColors.accentSoft else Color.Transparent,
                                RoundedCornerShape(8.dp),
                            )
                            .clickable {
                                shape = value
                                push(value)
                            }
                            .padding(horizontal = 14.dp, vertical = 8.dp),
                    ) {
                        Text(
                            label,
                            color = if (selected) MsColors.accentSoft else MsColors.textSecondary,
                            fontSize = 12.sp,
                        )
                    }
                }
            }

            Spacer(Modifier.height(8.dp))
            MaskSlider("Centre X", x, 0f, 1f) { x = it; push() }
            MaskSlider("Centre Y", y, 0f, 1f) { y = it; push() }
            MaskSlider("Width", w, 0.02f, 1f) { w = it; push() }
            MaskSlider("Height", h, 0.02f, 1f) { h = it; push() }
            MaskSlider("Feather (px)", feather, 0f, 200f) { feather = it; push() }

            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    "Invert",
                    color = MsColors.textSecondary,
                    fontSize = 12.sp,
                    modifier = Modifier.weight(1f),
                )
                Switch(
                    checked = inverted,
                    onCheckedChange = {
                        inverted = it
                        push()
                    },
                )
            }

            Spacer(Modifier.height(10.dp))
            Text(
                "The mask multiplies the clip's alpha before its transform is composited, " +
                    "so keyframed scale and position still apply to the matted image.",
                color = MsColors.textTertiary,
                fontSize = 11.sp,
            )

            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                TextButton(onClick = { push(); onDismiss() }) {
                    Text("Apply", color = MsColors.accentSoft, fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
                }
                TextButton(
                    enabled = engine.clipMaskActive,
                    onClick = {
                        engine.clearClipMask()
                        onDismiss()
                    },
                ) {
                    Text(
                        "Remove mask",
                        color = if (engine.clipMaskActive) MsColors.textSecondary else MsColors.textMuted,
                        fontSize = 13.sp,
                    )
                }
            }
        }
    }
}

@Composable
private fun MaskSlider(label: String, value: Float, min: Float, max: Float, onChange: (Float) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(
            label,
            color = MsColors.textSecondary,
            fontSize = 11.sp,
            modifier = Modifier.width(120.dp),
        )
        Slider(
            value = value.coerceIn(min, max),
            valueRange = min..max,
            onValueChange = onChange,
            modifier = Modifier.weight(1f),
        )
        Text(
            "%.2f".format(value),
            color = MsColors.textTertiary,
            fontSize = 11.sp,
            fontFamily = MsFonts.mono,
            modifier = Modifier.width(44.dp),
        )
    }
}
