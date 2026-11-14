package com.motionstudio.app.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
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
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.CallSplit
import androidx.compose.material.icons.filled.Loop
import androidx.compose.material.icons.filled.MoreHoriz
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Redo
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.SkipNext
import androidx.compose.material.icons.filled.SkipPrevious
import androidx.compose.material.icons.filled.Animation
import androidx.compose.material.icons.filled.ColorLens
import androidx.compose.material.icons.filled.Create
import androidx.compose.material.icons.filled.CropSquare
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.filled.Undo
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
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
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts
import androidx.compose.runtime.rememberCoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

private val workspaces = listOf(
    "Edit", "Animation", "Compositing", "Nodes", "3D", "Color", "Audio", "Export",
)

@Composable
fun EditorScreen(
    engine: EditorEngine,
    onBack: () -> Unit,
    onExportFrame: () -> Unit,
) {
    val context = LocalContext.current
    val snackbar = remember { SnackbarHostState() }
    var workspace by remember { mutableStateOf(0) }
    var showMore by remember { mutableStateOf(false) }
    var showLooks by remember { mutableStateOf(false) }
    var showEffects by remember { mutableStateOf(false) }
    var showColor by remember { mutableStateOf(false) }
    var showMotion by remember { mutableStateOf(false) }
    var showText by remember { mutableStateOf(false) }
    var showMask by remember { mutableStateOf(false) }
    var showMenu by remember { mutableStateOf(false) }

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
                    engine.importMedia(file.absolutePath, name)
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
                TopBar(
                    engine = engine,
                    showMenu = showMenu,
                    onShowMenu = { showMenu = it },
                    onBack = onBack,
                    onSave = { engine.save() },
                )
                if (!landscape) {
                    WorkspaceStrip(selected = workspace, onSelect = { workspace = it })
                }
                if (landscape) {
                    Row(Modifier.weight(1f).fillMaxWidth()) {
                        Column(Modifier.weight(0.62f).fillMaxHeight()) {
                            ViewerBlock(engine)
                        }
                        Column(Modifier.weight(0.38f).fillMaxHeight()) {
                            TransportBar(engine)
                            TimelinePane(
                                engine = engine,
                                modifier = Modifier.weight(1f).fillMaxWidth()
                                    .background(MsColors.surface),
                            )
                            ToolRail(
                                onImport = { picker.launch(arrayOf("video/*", "audio/*", "image/*")) },
                                onSplit = { engine.splitAtPlayhead() },
                                onLook = { showLooks = true },
                                onEffects = { showEffects = true },
                                onColor = { showColor = true },
                                onMotion = { showMotion = true },
                                onText = { showText = true },
                                onMask = { showMask = true },
                                onMore = { showMore = true },
                                onSeek = { engine.seek(engine.time) },
                            )
                        }
                    }
                } else {
                    Column(Modifier.weight(1f).fillMaxWidth()) {
                        ViewerBlock(engine, modifier = Modifier.weight(1f))
                        TransportBar(engine)
                        TimelinePane(
                            engine = engine,
                            modifier = Modifier.weight(0.85f).fillMaxWidth()
                                .background(MsColors.surface),
                        )
                        ToolRail(
                            onImport = { picker.launch(arrayOf("video/*", "audio/*", "image/*")) },
                            onSplit = { engine.splitAtPlayhead() },
                            onLook = { showLooks = true },
                            onEffects = { showEffects = true },
                            onColor = { showColor = true },
                            onMotion = { showMotion = true },
                            onText = { showText = true },
                            onMask = { showMask = true },
                            onMore = { showMore = true },
                            onSeek = { engine.seek(engine.time) },
                        )
                    }
                }
            }

            if (showMore) {
                MoreSheet(
                    engine = engine,
                    onDismiss = { showMore = false },
                    onExportFrame = onExportFrame,
                )
            }
            if (showLooks) {
                LooksSheet(engine = engine, onDismiss = { showLooks = false })
            }
            if (showEffects) {
                EffectsSheet(engine = engine, onDismiss = { showEffects = false })
            }
            if (showColor) {
                ColorSheet(engine = engine, onDismiss = { showColor = false })
            }
            if (showMotion) {
                MotionSheet(engine = engine, onDismiss = { showMotion = false })
            }
            if (showText) {
                TextSheet(engine = engine, onDismiss = { showText = false })
            }
            if (showMask) {
                MaskSheet(engine = engine, onDismiss = { showMask = false })
            }
        }
    }
}

@Composable
private fun TopBar(
    engine: EditorEngine,
    showMenu: Boolean,
    onShowMenu: (Boolean) -> Unit,
    onBack: () -> Unit,
    onSave: () -> Unit,
) {
    Surface(color = MsColors.background, shadowElevation = 0.dp) {
        Row(
            Modifier.fillMaxWidth().height(60.dp).padding(horizontal = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            IconButton(onClick = onBack) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, "Back to projects", tint = MsColors.textSecondary)
            }
            Column(Modifier.weight(1f)) {
                Text(
                    engine.doc.name,
                    color = MsColors.textPrimary,
                    fontSize = 15.sp,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 1,
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        Modifier.size(6.dp).background(
                            if (engine.dirty) MsColors.warning else MsColors.success,
                            CircleShape,
                        )
                    )
                    Spacer(Modifier.width(5.dp))
                    Text(
                        if (engine.dirty) "Unsaved changes" else engine.status,
                        color = MsColors.textTertiary,
                        fontSize = 11.sp,
                        maxLines = 1,
                    )
                }
            }
            IconButton(onClick = { engine.undo() }, enabled = true) {
                Icon(Icons.Filled.Undo, "Undo", tint = MsColors.textSecondary)
            }
            IconButton(onClick = { engine.redo() }) {
                Icon(Icons.Filled.Redo, "Redo", tint = MsColors.textSecondary)
            }
            IconButton(onClick = onSave) {
                Icon(Icons.Filled.Save, "Save project", tint = MsColors.accentSoft)
            }
            Box {
                IconButton(onClick = { onShowMenu(true) }) {
                    Icon(Icons.Filled.MoreHoriz, "More", tint = MsColors.textSecondary)
                }
                DropdownMenu(expanded = showMenu, onDismissRequest = { onShowMenu(false) }) {
                    DropdownMenuItem(
                        text = { Text("Split at playhead") },
                        onClick = { onShowMenu(false); engine.splitAtPlayhead() },
                    )
                    DropdownMenuItem(
                        text = { Text("Save project") },
                        onClick = { onShowMenu(false); engine.save() },
                    )
                    DropdownMenuItem(
                        text = { Text("${engine.doc.width} × ${engine.doc.height} · ${engine.fps.toInt()} fps") },
                        onClick = { onShowMenu(false) },
                        enabled = false,
                    )
                }
            }
        }
    }
}

@Composable
private fun WorkspaceStrip(selected: Int, onSelect: (Int) -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .height(44.dp)
            .background(MsColors.surface)
            .horizontalScroll(rememberScrollState())
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        workspaces.forEachIndexed { index, label ->
            val active = index == selected
            TextButton(
                onClick = { onSelect(index) },
                shape = RoundedCornerShape(8.dp),
            ) {
                Text(
                    label,
                    color = if (active) MsColors.accentSoft else MsColors.textTertiary,
                    fontSize = 12.sp,
                    fontWeight = if (active) FontWeight.SemiBold else FontWeight.Normal,
                )
            }
        }
    }
}

@Composable
private fun ViewerBlock(engine: EditorEngine, modifier: Modifier = Modifier) {
    Column(modifier.fillMaxWidth().background(MsColors.viewerBg)) {
        Row(
            Modifier.fillMaxWidth().height(34.dp).background(MsColors.surface).padding(horizontal = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                "EDIT  /  ${engine.doc.width}×${engine.doc.height}  /  ${engine.fps.toInt()} fps",
                color = MsColors.textTertiary,
                fontSize = 11.sp,
                fontWeight = FontWeight.Medium,
            )
            Spacer(Modifier.weight(1f))
            engine.lookName?.let { look ->
                Text(
                    "LOOK · ${look.uppercase()}",
                    color = MsColors.accentSoft,
                    fontSize = 10.sp,
                    fontFamily = MsFonts.mono,
                    fontWeight = FontWeight.Medium,
                )
                Spacer(Modifier.width(10.dp))
            }
            Text(
                "${engine.doc.tracks.sumOf { it.clips.size }} clips",
                color = MsColors.textMuted,
                fontSize = 11.sp,
            )
        }
        AndroidView(
            factory = { context -> PreviewView(context).also { it.attach(engine) } },
            modifier = Modifier.fillMaxWidth().weight(1f),
        )
    }
}

@Composable
private fun TransportBar(engine: EditorEngine) {
    Row(
        Modifier
            .fillMaxWidth()
            .height(52.dp)
            .background(MsColors.surface)
            .padding(horizontal = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        IconButton(onClick = { engine.step(-1) }) {
            Icon(Icons.Filled.SkipPrevious, "Previous frame", tint = MsColors.textSecondary)
        }
        IconButton(
            onClick = { engine.togglePlay() },
            modifier = Modifier.size(44.dp).background(MsColors.accent, CircleShape),
        ) {
            Icon(
                if (engine.playing) Icons.Filled.Pause else Icons.Filled.PlayArrow,
                if (engine.playing) "Pause" else "Play",
                tint = Color.White,
            )
        }
        IconButton(onClick = { engine.step(1) }) {
            Icon(Icons.Filled.SkipNext, "Next frame", tint = MsColors.textSecondary)
        }
        Spacer(Modifier.width(6.dp))
        Text(
            EditorEngine.formatTimecode(engine.time, engine.fps),
            color = MsColors.accentSoft,
            fontSize = 13.sp,
            fontFamily = MsFonts.mono,
            fontWeight = FontWeight.Bold,
        )
        Text(
            " / ${EditorEngine.formatTimecode(engine.duration, engine.fps)}",
            color = MsColors.textMuted,
            fontSize = 12.sp,
            fontFamily = MsFonts.mono,
        )
        Spacer(Modifier.weight(1f))
        Icon(Icons.Filled.Loop, "Loop", tint = MsColors.textMuted, modifier = Modifier.size(18.dp))
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

@Composable
private fun ToolRail(
    onImport: () -> Unit,
    onSplit: () -> Unit,
    onLook: () -> Unit,
    onEffects: () -> Unit,
    onColor: () -> Unit,
    onMotion: () -> Unit,
    onText: () -> Unit,
    onMask: () -> Unit,
    onMore: () -> Unit,
    onSeek: () -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .height(64.dp)
            .background(MsColors.surfaceHigh),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Row(
            Modifier.weight(1f).horizontalScroll(rememberScrollState()).padding(horizontal = 6.dp),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            RailButton("Import", Icons.Filled.Add, onImport)
            RailButton("Split", Icons.Filled.CallSplit, onSplit)
            RailButton("Look", Icons.Filled.Palette, onLook)
            RailButton("Effects", Icons.Filled.Tune, onEffects)
            RailButton("Color", Icons.Filled.ColorLens, onColor)
            RailButton("Motion", Icons.Filled.Animation, onMotion)
            RailButton("Title", Icons.Filled.Create, onText)
            RailButton("Mask", Icons.Filled.CropSquare, onMask)
            RailButton("Playhead", Icons.Filled.SkipNext, onSeek)
        }
        IconButton(onClick = onMore) {
            Icon(Icons.Filled.MoreHoriz, "More tools", tint = MsColors.accentSoft)
        }
    }
}

@Composable
private fun RailButton(label: String, icon: androidx.compose.ui.graphics.vector.ImageVector, onClick: () -> Unit) {
    TextButton(onClick = onClick) {
        Icon(icon, label, tint = MsColors.textSecondary, modifier = Modifier.size(18.dp))
        Spacer(Modifier.width(6.dp))
        Text(label, color = MsColors.textSecondary, fontSize = 12.sp)
    }
}
