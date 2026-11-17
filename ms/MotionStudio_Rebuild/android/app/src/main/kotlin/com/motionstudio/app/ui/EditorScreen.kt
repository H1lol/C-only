package com.motionstudio.app.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Redo
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.SkipNext
import androidx.compose.material.icons.filled.SkipPrevious
import androidx.compose.material.icons.filled.Undo
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.native.NativeEngine
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Editor workspaces. Each tab swaps the panel in the lower half; none of them is
 * decorative — TIMELINE is the timeline, NODES is the clip's render chain,
 * KEYS reads keyframe counts from the engine, GRADE drives the master grade,
 * CLIP inspects the selection.
 */
private enum class Workspace(val label: String) {
    Timeline("TIMELINE"),
    Nodes("NODES"),
    Keys("KEYS"),
    Grade("GRADE"),
    Clip("CLIP"),
}

@Composable
fun EditorScreen(
    engine: EditorEngine,
    onBack: () -> Unit,
    onExportFrame: () -> Unit,
) {
    val context = LocalContext.current
    val snackbar = remember { SnackbarHostState() }
    var workspace by remember { mutableStateOf(Workspace.Timeline) }
    var showMore by remember { mutableStateOf(false) }
    var showLooks by remember { mutableStateOf(false) }
    var showEffects by remember { mutableStateOf(false) }
    var showColor by remember { mutableStateOf(false) }
    var showMotion by remember { mutableStateOf(false) }
    var showText by remember { mutableStateOf(false) }
    var showMask by remember { mutableStateOf(false) }
    var showAudio by remember { mutableStateOf(false) }
    var showExport by remember { mutableStateOf(false) }

    val importScope = rememberCoroutineScope()
    val picker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        // Copying a video is disk-bound; doing it on the main thread froze the
        // editor for as long as the file took to copy.
        importScope.launch {
            engine.note("Importing…")
            val imported = withContext(Dispatchers.IO) {
                runCatching {
                    val raw = uri.lastPathSegment?.substringAfterLast('/') ?: "media"
                    val name = raw.ifBlank { "media" }
                    // Media lives in filesDir: the cache can be purged by the OS
                    // and the project would then point at a missing path.
                    val target = File(context.filesDir, "imports").apply { mkdirs() }
                    // Never overwrite an earlier import that happens to share a name.
                    val file = File(target, uniqueName(target, name))
                    val input = context.contentResolver.openInputStream(uri)
                        ?: return@runCatching false
                    input.use { source ->
                        file.outputStream().use { output -> source.copyTo(output) }
                    }
                    if (file.length() <= 0L) {
                        file.delete()
                        return@runCatching false
                    }
                    engine.importMedia(
                        file.absolutePath,
                        name,
                        appendToEnd = engine.appendImportsAtEnd,
                    )
                }.getOrDefault(false)
            }
            if (!imported) engine.note("Could not import that file")
        }
    }

    LaunchedEffect(engine.status) {
        snackbar.showSnackbar(engine.status)
    }
    LaunchedEffect(engine.lastError) {
        engine.lastError?.let { snackbar.showSnackbar(it) }
    }

    Scaffold(
        snackbarHost = { SnackbarHost(snackbar) },
        containerColor = MsColors.background,
    ) { padding ->
        BoxWithConstraints(Modifier.fillMaxSize().padding(padding)) {
            val landscape = maxWidth > maxHeight
            Column(Modifier.fillMaxSize()) {
                CommandBar(engine = engine, onBack = onBack, onSave = { engine.save() })
                if (landscape) {
                    Row(Modifier.weight(1f).fillMaxWidth()) {
                        Column(Modifier.weight(0.60f).fillMaxHeight()) {
                            Viewport(engine, modifier = Modifier.weight(1f))
                            TransportBar(engine)
                        }
                        Column(
                            Modifier.weight(0.40f).fillMaxHeight()
                                .border(1.dp, MsColors.borderSoft)
                        ) {
                            WorkspaceTabs(selected = workspace, onSelect = { workspace = it })
                            WorkspacePanel(
                                engine = engine,
                                workspace = workspace,
                                modifier = Modifier.weight(1f),
                                onOpenEffects = { showEffects = true },
                                onOpenText = { showText = true },
                                onOpenMask = { showMask = true },
                                onOpenMotion = { showMotion = true },
                            )
                            ActionRail(
                                onImport = { picker.launch(arrayOf("video/*", "audio/*", "image/*")) },
                                onSplit = { engine.splitAtPlayhead() },
                                onEffects = { showEffects = true },
                                onLook = { showLooks = true },
                                onColor = { showColor = true },
                                onMotion = { showMotion = true },
                                onText = { showText = true },
                                onMask = { showMask = true },
                                onAudio = { showAudio = true },
                                onExport = { showExport = true },
                                onMore = { showMore = true },
                            )
                        }
                    }
                } else {
                    Viewport(engine, modifier = Modifier.weight(1f))
                    TransportBar(engine)
                    WorkspaceTabs(selected = workspace, onSelect = { workspace = it })
                    WorkspacePanel(
                        engine = engine,
                        workspace = workspace,
                        modifier = Modifier.weight(1.05f),
                        onOpenEffects = { showEffects = true },
                        onOpenText = { showText = true },
                        onOpenMask = { showMask = true },
                        onOpenMotion = { showMotion = true },
                    )
                    ActionRail(
                        onImport = { picker.launch(arrayOf("video/*", "audio/*", "image/*")) },
                        onSplit = { engine.splitAtPlayhead() },
                        onEffects = { showEffects = true },
                        onLook = { showLooks = true },
                        onColor = { showColor = true },
                        onMotion = { showMotion = true },
                        onText = { showText = true },
                        onMask = { showMask = true },
                        onAudio = { showAudio = true },
                        onExport = { onExportFrame() },
                        onMore = { showMore = true },
                    )
                }
            }

            if (showMore) {
                MoreSheet(
                    engine = engine,
                    onDismiss = { showMore = false },
                    onExportFrame = onExportFrame,
                    onOpenExport = {
                        showMore = false
                        showExport = true
                    },
                )
            }
            if (showExport) {
                ExportSheet(engine = engine, onDismiss = { showExport = false })
            }
            if (showLooks) LooksSheet(engine = engine, onDismiss = { showLooks = false })
            if (showEffects) EffectsSheet(engine = engine, onDismiss = { showEffects = false })
            if (showColor) ColorSheet(engine = engine, onDismiss = { showColor = false })
            if (showMotion) MotionSheet(engine = engine, onDismiss = { showMotion = false })
            if (showText) TextSheet(engine = engine, onDismiss = { showText = false })
            if (showMask) MaskSheet(engine = engine, onDismiss = { showMask = false })
            if (showAudio) AudioSheet(engine = engine, onDismiss = { showAudio = false })
        }
    }
}

@Composable
private fun WorkspacePanel(
    engine: EditorEngine,
    workspace: Workspace,
    modifier: Modifier,
    onOpenEffects: () -> Unit,
    onOpenText: () -> Unit,
    onOpenMask: () -> Unit,
    onOpenMotion: () -> Unit,
) {
    when (workspace) {
        Workspace.Timeline -> TimelinePane(
            engine = engine,
            modifier = modifier.fillMaxWidth().background(MsColors.surface),
        )
        Workspace.Nodes -> NodeGraphPane(engine, modifier.fillMaxWidth())
        Workspace.Keys -> KeyframePane(engine, modifier.fillMaxWidth())
        Workspace.Grade -> GradePane(engine, modifier.fillMaxWidth())
        Workspace.Clip -> ClipPane(
            engine = engine,
            onOpenEffects = onOpenEffects,
            onOpenText = onOpenText,
            onOpenMask = onOpenMask,
            onOpenMotion = onOpenMotion,
            modifier = modifier.fillMaxWidth(),
        )
    }
}

// --------------------------------------------------------------- chrome ----

@Composable
private fun CommandBar(engine: EditorEngine, onBack: () -> Unit, onSave: () -> Unit) {
    Column {
        Row(
            Modifier.fillMaxWidth().height(48.dp).background(MsColors.surface)
                .padding(horizontal = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            IconButton(onClick = onBack, modifier = Modifier.size(40.dp)) {
                Icon(
                    Icons.AutoMirrored.Filled.ArrowBack,
                    "Back to projects",
                    tint = MsColors.textSecondary,
                )
            }
            Column(Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        engine.doc.name,
                        color = MsColors.textPrimary,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 1,
                    )
                    Spacer(Modifier.width(8.dp))
                    Box(
                        Modifier.size(5.dp).background(
                            if (engine.dirty) MsColors.warning else MsColors.success,
                            CircleShape,
                        )
                    )
                }
                Text(
                    "${engine.doc.width}×${engine.doc.height}  ·  ${engine.fps.toInt()} FPS  ·  " +
                        "${EditorEngine.formatTimecode(engine.duration, engine.fps)}",
                    color = MsColors.textTertiary,
                    fontSize = 9.sp,
                    fontFamily = MsFonts.mono,
                    maxLines = 1,
                )
            }
            IconButton(onClick = { engine.undo() }, modifier = Modifier.size(38.dp)) {
                Icon(Icons.Filled.Undo, "Undo", tint = MsColors.textSecondary)
            }
            IconButton(onClick = { engine.redo() }, modifier = Modifier.size(38.dp)) {
                Icon(Icons.Filled.Redo, "Redo", tint = MsColors.textSecondary)
            }
            IconButton(onClick = onSave, modifier = Modifier.size(38.dp)) {
                Icon(Icons.Filled.Save, "Save", tint = MsColors.accentSoft)
            }
        }
        Box(Modifier.fillMaxWidth().height(1.dp).background(MsColors.border))
    }
}

@Composable
private fun WorkspaceTabs(selected: Workspace, onSelect: (Workspace) -> Unit) {
    Row(
        Modifier.fillMaxWidth().height(32.dp).background(MsColors.surface)
            .horizontalScroll(rememberScrollState()),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Workspace.entries.forEach { tab ->
            val active = tab == selected
            Column(
                Modifier
                    .clickable { onSelect(tab) }
                    .padding(horizontal = 14.dp, vertical = 6.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Text(
                    tab.label,
                    color = if (active) MsColors.textPrimary else MsColors.textMuted,
                    fontSize = 10.sp,
                    fontFamily = MsFonts.mono,
                    fontWeight = if (active) FontWeight.Bold else FontWeight.Normal,
                )
                Spacer(Modifier.height(4.dp))
                Box(
                    Modifier
                        .width(if (active) 18.dp else 0.dp)
                        .height(2.dp)
                        .background(if (active) MsColors.accent else Color.Transparent)
                )
            }
        }
    }
    Box(Modifier.fillMaxWidth().height(1.dp).background(MsColors.border))
}

@Composable
private fun Viewport(engine: EditorEngine, modifier: Modifier = Modifier) {
    Column(modifier.fillMaxWidth().background(MsColors.viewerBg)) {
        Box(Modifier.fillMaxWidth().weight(1f)) {
            AndroidView(
                factory = { context -> PreviewView(context).also { it.attach(engine) } },
                modifier = Modifier.fillMaxSize(),
            )
            // Safe-area guides: an inset rule plus centre cross, the standard
            // framing overlay in a compositor viewport.
            Box(
                Modifier
                    .fillMaxSize()
                    .padding(horizontal = 26.dp, vertical = 18.dp)
                    .border(1.dp, MsColors.guide.copy(alpha = 0.55f))
            )
            Box(
                Modifier
                    .align(Alignment.TopStart)
                    .padding(8.dp)
                    .background(MsColors.background.copy(alpha = 0.72f))
                    .padding(horizontal = 6.dp, vertical = 2.dp)
            ) {
                Text(
                    "${NativeEngine.nativeDecoderBackend().uppercase()}  ·  " +
                        "${engine.doc.width}×${engine.doc.height}  ·  ${engine.fps.toInt()}P",
                    color = MsColors.textTertiary,
                    fontSize = 9.sp,
                    fontFamily = MsFonts.mono,
                )
            }
            Box(
                Modifier
                    .align(Alignment.TopEnd)
                    .padding(8.dp)
                    .background(MsColors.background.copy(alpha = 0.72f))
                    .padding(horizontal = 6.dp, vertical = 2.dp)
            ) {
                Text(
                    if (engine.playing) "PLAYING" else "PAUSED",
                    color = if (engine.playing) MsColors.success else MsColors.textMuted,
                    fontSize = 9.sp,
                    fontFamily = MsFonts.mono,
                )
            }
            Box(
                Modifier
                    .align(Alignment.BottomStart)
                    .padding(8.dp)
                    .background(MsColors.background.copy(alpha = 0.72f))
                    .padding(horizontal = 6.dp, vertical = 2.dp)
            ) {
                Text(
                    "TC " + EditorEngine.formatTimecode(engine.time, engine.fps),
                    color = MsColors.accentSoft,
                    fontSize = 10.sp,
                    fontFamily = MsFonts.mono,
                )
            }
            engine.lookName?.let { look ->
                Box(
                    Modifier
                        .align(Alignment.BottomEnd)
                        .padding(8.dp)
                        .background(MsColors.background.copy(alpha = 0.72f))
                        .padding(horizontal = 6.dp, vertical = 2.dp)
                ) {
                    Text(
                        "LUT ${look.uppercase()}",
                        color = MsColors.info,
                        fontSize = 9.sp,
                        fontFamily = MsFonts.mono,
                    )
                }
            }
        }
    }
}

@Composable
private fun TransportBar(engine: EditorEngine) {
    Column {
        Box(Modifier.fillMaxWidth().height(1.dp).background(MsColors.border))
        Row(
            Modifier.fillMaxWidth().height(46.dp).background(MsColors.surface)
                .padding(horizontal = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            IconButton(onClick = { engine.step(-1) }, modifier = Modifier.size(38.dp)) {
                Icon(Icons.Filled.SkipPrevious, "Previous frame", tint = MsColors.textSecondary)
            }
            Box(
                Modifier
                    .size(34.dp)
                    .background(MsColors.accent, CircleShape)
                    .clickable { engine.togglePlay() },
                contentAlignment = Alignment.Center,
            ) {
                Icon(
                    if (engine.playing) Icons.Filled.Pause else Icons.Filled.PlayArrow,
                    if (engine.playing) "Pause" else "Play",
                    tint = Color.Black,
                    modifier = Modifier.size(20.dp),
                )
            }
            IconButton(onClick = { engine.step(1) }, modifier = Modifier.size(38.dp)) {
                Icon(Icons.Filled.SkipNext, "Next frame", tint = MsColors.textSecondary)
            }
            Spacer(Modifier.width(10.dp))
            Text(
                EditorEngine.formatTimecode(engine.time, engine.fps),
                color = MsColors.accentSoft,
                fontSize = 12.sp,
                fontFamily = MsFonts.mono,
                fontWeight = FontWeight.Bold,
            )
            Text(
                " / ${EditorEngine.formatTimecode(engine.duration, engine.fps)}",
                color = MsColors.textMuted,
                fontSize = 11.sp,
                fontFamily = MsFonts.mono,
            )
            Spacer(Modifier.weight(1f))
            Text(
                "F${(engine.time * engine.fps).toInt()}",
                color = MsColors.textMuted,
                fontSize = 9.sp,
                fontFamily = MsFonts.mono,
            )
        }
    }
}

@Composable
private fun ActionRail(
    onImport: () -> Unit,
    onSplit: () -> Unit,
    onEffects: () -> Unit,
    onLook: () -> Unit,
    onColor: () -> Unit,
    onMotion: () -> Unit,
    onText: () -> Unit,
    onMask: () -> Unit,
    onAudio: () -> Unit,
    onExport: () -> Unit,
    onMore: () -> Unit,
) {
    Column {
        Box(Modifier.fillMaxWidth().height(1.dp).background(MsColors.border))
        Row(
            Modifier
                .fillMaxWidth()
                .height(46.dp)
                .background(MsColors.surfaceHigh)
                .horizontalScroll(rememberScrollState())
                .padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            RailCommand("IMPORT", onImport)
            RailCommand("SPLIT", onSplit)
            RailCommand("FX", onEffects)
            RailCommand("LOOK", onLook)
            RailCommand("GRADE", onColor)
            RailCommand("MOTION", onMotion)
            RailCommand("TITLE", onText)
            RailCommand("MASK", onMask)
            RailCommand("MIX", onAudio)
            RailCommand("EXPORT", onExport)
            RailCommand("MORE", onMore)
        }
    }
}

@Composable
private fun RailCommand(label: String, onClick: () -> Unit) {
    Box(
        Modifier
            .border(1.dp, MsColors.border, RoundedCornerShape(2.dp))
            .clickable(onClick = onClick)
            .padding(horizontal = 9.dp, vertical = 5.dp)
    ) {
        Text(
            label,
            color = MsColors.textSecondary,
            fontSize = 9.sp,
            fontFamily = MsFonts.mono,
            letterSpacing = 0.6.sp,
        )
    }
}

/** Picks a filename that does not collide with an earlier import. */
private fun uniqueName(dir: File, name: String): String {
    if (!File(dir, name).exists()) return name
    val dot = name.lastIndexOf('.')
    val base = if (dot > 0) name.substring(0, dot) else name
    val ext = if (dot > 0) name.substring(dot) else ""
    var i = 2
    while (File(dir, "$base-$i$ext").exists()) i++
    return "$base-$i$ext"
}
