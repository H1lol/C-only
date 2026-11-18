package com.motionstudio.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.motionstudio.app.ui.theme.MsColors
import com.motionstudio.app.ui.theme.MsFonts

/**
 * Shown when `libmotionstudio_jni.so` could not be loaded. Without the engine
 * there is nothing to edit, so this replaces the whole UI rather than letting
 * the first `external fun` call throw an `UnsatisfiedLinkError` out of a
 * gesture handler.
 */
@Composable
fun EngineUnavailableScreen(reason: String?, abis: List<String>) {
    Column(
        Modifier
            .fillMaxSize()
            .background(MsColors.background)
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
    ) {
        Text(
            "ENGINE NOT LOADED",
            color = MsColors.danger,
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            letterSpacing = 1.5.sp,
            fontFamily = MsFonts.mono,
        )
        Spacer(Modifier.height(12.dp))
        Text(
            "Motion Studio could not open its native engine, so the editor cannot start. " +
                "The project files on this device are untouched.",
            color = MsColors.textPrimary,
            fontSize = 15.sp,
        )
        Spacer(Modifier.height(16.dp))
        Text(
            reason ?: "Unknown loader error",
            color = MsColors.textSecondary,
            fontSize = 12.sp,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(20.dp))
        Text(
            "THIS DEVICE",
            color = MsColors.textMuted,
            fontSize = 10.sp,
            fontWeight = FontWeight.Bold,
            letterSpacing = 1.sp,
            fontFamily = MsFonts.mono,
        )
        Spacer(Modifier.height(6.dp))
        Text(
            "ABIs: ${abis.joinToString(", ").ifBlank { "unknown" }}\n" +
                "The build ships arm64-v8a and x86_64 only. A 32-bit (armeabi-v7a) " +
                "device or an x86 emulator without 64-bit support cannot run it.",
            color = MsColors.textSecondary,
            fontSize = 12.sp,
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(20.dp))
        Text(
            "Send this screen (or Settings → Diagnostics) to the developer. " +
                "The full report is also written to the app's diagnostics folder.",
            color = MsColors.textMuted,
            fontSize = 11.sp,
        )
    }
}
