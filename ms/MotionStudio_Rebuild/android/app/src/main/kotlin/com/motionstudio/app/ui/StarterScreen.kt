package com.motionstudio.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.DeleteOutline
import androidx.compose.material.icons.filled.MovieCreation
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
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
import com.motionstudio.app.data.ProjectStore
import com.motionstudio.app.ui.theme.MsColors
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private val resolutions = listOf(
    Triple("1920 × 1080", 1920 to 1080, "Landscape"),
    Triple("1080 × 1920", 1080 to 1920, "Portrait"),
    Triple("1080 × 1080", 1080 to 1080, "Square"),
    Triple("3840 × 2160", 3840 to 2160, "4K UHD"),
)

@Composable
fun StarterScreen(
    store: ProjectStore,
    onOpenProject: (File) -> Unit,
    onCreateProject: (String, Int, Int, Double) -> Unit,
    onSettings: () -> Unit,
) {
    var showNew by remember { mutableStateOf(false) }
    var refreshKey by remember { mutableIntStateOf(0) }
    val projects = remember(refreshKey) { store.projects() }

    Column(
        Modifier
            .fillMaxSize()
            .background(MsColors.background)
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 20.dp, vertical = 16.dp),
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Box(
                Modifier
                    .size(38.dp)
                    .background(MsColors.accentDim, RoundedCornerShape(10.dp)),
                contentAlignment = Alignment.Center,
            ) {
                Text("MS", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            }
            Spacer(Modifier.width(10.dp))
            Column(Modifier.weight(1f)) {
                Text("MOTION STUDIO", color = MsColors.textPrimary, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                Text("Kotlin + C++ · on-device", color = MsColors.textTertiary, fontSize = 11.sp)
            }
            IconButton(onClick = onSettings) {
                Icon(Icons.Filled.Settings, "Settings", tint = MsColors.textSecondary)
            }
        }

        Spacer(Modifier.height(28.dp))
        Text(
            "Make something\nthat moves.",
            color = MsColors.textPrimary,
            fontSize = 34.sp,
            lineHeight = 38.sp,
            fontWeight = FontWeight.Bold,
        )
        Spacer(Modifier.height(10.dp))
        Text(
            "A focused space for motion design, compositing and editing — " +
                "rendered by the native C++ engine.",
            color = MsColors.textSecondary,
            fontSize = 14.sp,
        )

        Spacer(Modifier.height(22.dp))
        Row(
            Modifier
                .fillMaxWidth()
                .background(MsColors.accent, RoundedCornerShape(14.dp))
                .clickable { showNew = true }
                .padding(horizontal = 18.dp, vertical = 18.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(Icons.Filled.Add, "New project", tint = Color.White)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text("New project", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
                Text("Set up a new composition", color = Color.White.copy(alpha = 0.85f), fontSize = 12.sp)
            }
        }

        Spacer(Modifier.height(26.dp))
        Text("Recent projects", color = MsColors.textPrimary, fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
        Spacer(Modifier.height(10.dp))

        if (projects.isEmpty()) {
            Text(
                "Projects you save appear here. Everything stays on this device.",
                color = MsColors.textMuted,
                fontSize = 12.sp,
                modifier = Modifier.padding(vertical = 12.dp),
            )
        } else {
            projects.forEach { file ->
                ProjectRow(
                    file = file,
                    name = store.projectName(file),
                    onOpen = { onOpenProject(file) },
                    onDelete = {
                        store.delete(file)
                        refreshKey++
                    },
                )
            }
        }

        Spacer(Modifier.height(40.dp))
        Text(
            "Motion Studio 2026.1.0 · GPL-3.0",
            color = MsColors.textMuted,
            fontSize = 11.sp,
        )
    }

    if (showNew) {
        NewProjectDialog(
            onDismiss = { showNew = false },
            onCreate = { name, width, height, fps ->
                showNew = false
                onCreateProject(name, width, height, fps)
            },
        )
    }
}

@Composable
private fun ProjectRow(file: File, name: String, onOpen: () -> Unit, onDelete: () -> Unit) {
    val stamp = remember(file.lastModified()) {
        SimpleDateFormat("d MMM · HH:mm", Locale.getDefault()).format(Date(file.lastModified()))
    }
    Row(
        Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp)
            .background(MsColors.surface, RoundedCornerShape(12.dp))
            .clickable { onOpen() }
            .padding(horizontal = 14.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(Icons.Filled.MovieCreation, null, tint = MsColors.accentSoft, modifier = Modifier.size(22.dp))
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(name, color = MsColors.textPrimary, fontSize = 14.sp, fontWeight = FontWeight.Medium, maxLines = 1)
            Text(stamp, color = MsColors.textTertiary, fontSize = 11.sp)
        }
        IconButton(onClick = onDelete) {
            Icon(Icons.Filled.DeleteOutline, "Delete project", tint = MsColors.textMuted)
        }
    }
}

@Composable
private fun NewProjectDialog(
    onDismiss: () -> Unit,
    onCreate: (String, Int, Int, Double) -> Unit,
) {
    var name by remember { mutableStateOf("Untitled Project") }
    var resolution by remember { mutableIntStateOf(0) }
    var fps by remember { mutableDoubleStateOf(30.0) }

    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = MsColors.surface,
        title = { Text("New project", color = MsColors.textPrimary) },
        text = {
            Column {
                OutlinedTextField(
                    value = name,
                    onValueChange = { name = it },
                    label = { Text("Project name") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(14.dp))
                Text("Format", color = MsColors.textTertiary, fontSize = 12.sp)
                Spacer(Modifier.height(6.dp))
                Column {
                    resolutions.forEachIndexed { index, item ->
                        Row(
                            Modifier
                                .fillMaxWidth()
                                .padding(vertical = 2.dp)
                                .background(
                                    if (index == resolution) MsColors.accent.copy(alpha = 0.18f)
                                    else Color.Transparent,
                                    RoundedCornerShape(8.dp),
                                )
                                .clickable { resolution = index }
                                .padding(horizontal = 10.dp, vertical = 9.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                item.first,
                                color = if (index == resolution) MsColors.accentSoft else MsColors.textSecondary,
                                fontSize = 13.sp,
                                modifier = Modifier.weight(1f),
                            )
                            Text(item.third, color = MsColors.textMuted, fontSize = 11.sp)
                        }
                    }
                }
                Spacer(Modifier.height(12.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    listOf(24.0, 30.0, 60.0).forEach { option ->
                        TextButton(onClick = { fps = option }) {
                            Text(
                                "${option.toInt()} fps",
                                color = if (fps == option) MsColors.accentSoft else MsColors.textTertiary,
                                fontSize = 12.sp,
                            )
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val (w, h) = resolutions[resolution].second
                    onCreate(name.trim().ifBlank { "Untitled Project" }, w, h, fps)
                }
            ) { Text("Create project") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}
