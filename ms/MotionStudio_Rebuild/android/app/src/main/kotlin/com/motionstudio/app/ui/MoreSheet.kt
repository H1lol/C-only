package com.motionstudio.app.ui

import android.content.Context
import android.graphics.Bitmap
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.export.VideoExporter
import com.motionstudio.app.native.NativeEngine
import com.motionstudio.app.ui.theme.MsColors
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MoreSheet(
    engine: EditorEngine,
    onDismiss: () -> Unit,
    onExportFrame: () -> Unit,
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    var gainDb by remember { mutableFloatStateOf(0f) }
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var exporting by remember { mutableStateOf(false) }
    var exportProgress by remember { mutableFloatStateOf(0f) }
    var exportMessage by remember { mutableStateOf<String?>(null) }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = MsColors.surface,
    ) {
        Column(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 4.dp)) {
            Text(
                "Session",
                color = MsColors.textPrimary,
                fontSize = 16.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                "${engine.doc.width} × ${engine.doc.height} · ${engine.fps.toInt()} fps · " +
                    "${engine.doc.media.size} media · ${engine.doc.tracks.sumOf { it.clips.size }} clips",
                color = MsColors.textTertiary,
                fontSize = 12.sp,
            )
            Spacer(Modifier.height(12.dp))

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextButton(onClick = { engine.splitAtPlayhead() }) { Text("Split clip") }
                TextButton(onClick = { engine.undo() }) { Text("Undo") }
                TextButton(onClick = { engine.redo() }) { Text("Redo") }
            }

            Spacer(Modifier.height(10.dp))
            Text("Markers", color = MsColors.textSecondary, fontSize = 13.sp)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextButton(onClick = { engine.addMarker() }) {
                    Text("Add at playhead (${engine.time.toInt()}s)", fontSize = 12.sp)
                }
            }
            engine.doc.markers.forEach { m ->
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        "%.2f s".format(m.time) + if (m.name.isBlank()) "" else "  ${m.name}",
                        color = MsColors.textSecondary,
                        fontSize = 11.sp,
                        modifier = Modifier.weight(1f),
                    )
                    TextButton(onClick = { engine.removeMarker(m.id) }) {
                        Text("Remove", color = MsColors.textTertiary, fontSize = 11.sp)
                    }
                }
            }

            Spacer(Modifier.height(10.dp))
            Text("Project size", color = MsColors.textSecondary, fontSize = 13.sp)
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                listOf(
                    "1080×1920" to (1080 to 1920),
                    "1920×1080" to (1920 to 1080),
                    "1080×1080" to (1080 to 1080),
                    "2160×3840" to (2160 to 3840),
                ).forEach { (label, dims) ->
                    val active = engine.doc.width == dims.first && engine.doc.height == dims.second
                    TextButton(
                        onClick = { engine.setProjectSettings(dims.first, dims.second, engine.fps) },
                    ) {
                        Text(
                            label,
                            color = if (active) MsColors.accentSoft else MsColors.textSecondary,
                            fontSize = 11.sp,
                        )
                    }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                listOf(24.0, 30.0, 60.0).forEach { f ->
                    val active = kotlin.math.abs(engine.fps - f) < 0.5
                    TextButton(
                        onClick = { engine.setProjectSettings(engine.doc.width, engine.doc.height, f) },
                    ) {
                        Text(
                            "${f.toInt()} fps",
                            color = if (active) MsColors.accentSoft else MsColors.textSecondary,
                            fontSize = 11.sp,
                        )
                    }
                }
            }

            Spacer(Modifier.height(8.dp))
            Text(
                "Master gain  ${gainDb.toInt()} dB",
                color = MsColors.textSecondary,
                fontSize = 13.sp,
            )
            Slider(
                value = gainDb,
                onValueChange = {
                    gainDb = it
                    NativeEngine.nativeSetMasterGain(it)
                },
                valueRange = -24f..12f,
            )

            Spacer(Modifier.height(4.dp))
            Text(
                "Renderer: ${NativeEngine.nativePerfFrameMs().toInt()} ms/frame · " +
                    "${NativeEngine.nativePerfFps().toInt()} fps · native C++ CPU compositor",
                color = MsColors.textMuted,
                fontSize = 11.sp,
            )

            Spacer(Modifier.height(12.dp))
            TextButton(
                enabled = !exporting,
                onClick = {
                    scope.launch {
                        exporting = true
                        exportProgress = 0f
                        exportMessage = null
                        val dir = File(context.filesDir, "exports").apply { mkdirs() }
                        val out = File(dir, "export_${System.currentTimeMillis()}.mp4")
                        val result = withContext(Dispatchers.Default) {
                            VideoExporter(engine).export(out) { exportProgress = it }
                        }
                        exporting = false
                        exportMessage = if (result.error == null) {
                            "Exported ${out.name} · ${result.frames} frames · H.264 (no audio)"
                        } else {
                            "Export failed: ${result.error}"
                        }
                    }
                },
            ) {
                Text(
                    if (exporting) "Encoding… ${(exportProgress * 100).toInt()}%" else "Export MP4",
                    color = MsColors.accentSoft,
                    fontWeight = FontWeight.Medium,
                )
            }
            Text(
                "Encoded on-device with MediaCodec (H.264, video only). Audio muxing is " +
                    "not wired yet.",
                color = MsColors.textMuted,
                fontSize = 11.sp,
            )
            exportMessage?.let { msg ->
                Spacer(Modifier.height(6.dp))
                Text(msg, color = MsColors.textSecondary, fontSize = 11.sp)
            }
            Spacer(Modifier.height(10.dp))
            TextButton(onClick = onExportFrame) {
                Text(
                    "Render current frame to PNG",
                    color = MsColors.textSecondary,
                    fontWeight = FontWeight.Medium,
                )
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}

/** Renders the current engine frame and writes it as a PNG. */
fun exportCurrentFrame(context: Context, engine: EditorEngine): File? {
    val state = NativeEngine.nativePreviewState()
    if (state.size < 4) return null
    val width = state[1].toInt()
    val height = state[2].toInt()
    if (width <= 0 || height <= 0) return null
    if (!NativeEngine.nativeRenderFrame()) return null

    val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
    if (!NativeEngine.nativeCopyFrame(bitmap)) return null

    val dir = File(context.filesDir, "exports").apply { mkdirs() }
    val target = File(dir, "frame_${System.currentTimeMillis()}.png")
    FileOutputStream(target).use { out ->
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, out)
    }
    bitmap.recycle()
    return target
}
