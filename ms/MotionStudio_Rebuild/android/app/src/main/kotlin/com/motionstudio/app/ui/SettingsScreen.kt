package com.motionstudio.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.data.SettingsStore
import com.motionstudio.app.ui.theme.MsColors

private val accents = listOf(
    Color(0xFF9476F5),
    Color(0xFF58B997),
    Color(0xFFE8B45F),
    Color(0xFFE87979),
    Color(0xFF6FA8F5),
)

@Composable
fun SettingsScreen(settings: SettingsStore, onBack: () -> Unit) {
    Column(
        Modifier
            .fillMaxSize()
            .background(MsColors.background)
            .verticalScroll(rememberScrollState()),
    ) {
        Row(
            Modifier.fillMaxWidth().height(60.dp).padding(horizontal = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            IconButton(onClick = onBack) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, "Back", tint = MsColors.textSecondary)
            }
            Text("Settings", color = MsColors.textPrimary, fontSize = 18.sp, fontWeight = FontWeight.SemiBold)
        }

        Section("Accent") {
            Row(Modifier.padding(horizontal = 16.dp)) {
                accents.forEach { color ->
                    Box(
                        Modifier
                            .padding(end = 10.dp)
                            .size(34.dp)
                            .background(color, CircleShape)
                            .then(
                                if (settings.accent == color) {
                                    Modifier.padding(0.dp)
                                } else Modifier
                            )
                            .clickable { settings.updateAccent(color) },
                        contentAlignment = Alignment.Center,
                    ) {
                        if (settings.accent == color) {
                            Box(Modifier.size(12.dp).background(Color.White, CircleShape))
                        }
                    }
                }
            }
        }

        Section("Projects") {
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(Modifier.weight(1f)) {
                    Text("Autosave every 10 minutes", color = MsColors.textPrimary, fontSize = 14.sp)
                    Text(
                        "Snapshots are written only while the project has unsaved changes.",
                        color = MsColors.textTertiary,
                        fontSize = 11.sp,
                    )
                }
                Switch(
                    checked = settings.autosaveEnabled,
                    onCheckedChange = { settings.setAutosave(it) },
                )
            }
        }

        Section("Device") {
            Text(
                "Android 10 (API 29) or newer · 4 GB RAM minimum",
                color = MsColors.textSecondary,
                fontSize = 12.sp,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
            )
            Text(
                "Composition, effects and audio mixing run in the native C++ engine; " +
                    "the UI is Jetpack Compose.",
                color = MsColors.textMuted,
                fontSize = 11.sp,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
            )
        }

        Spacer(Modifier.height(24.dp))
        Text(
            "Motion Studio 2026.1.0 · GPL-3.0-or-later",
            color = MsColors.textMuted,
            fontSize = 11.sp,
            modifier = Modifier.padding(horizontal = 16.dp),
        )
        Spacer(Modifier.height(32.dp))
    }
}

@Composable
private fun Section(title: String, content: @Composable () -> Unit) {
    Text(
        title.uppercase(),
        color = MsColors.textMuted,
        fontSize = 10.sp,
        fontWeight = FontWeight.Bold,
        letterSpacing = 1.sp,
        modifier = Modifier.padding(start = 16.dp, top = 18.dp, bottom = 8.dp),
    )
    content()
}
