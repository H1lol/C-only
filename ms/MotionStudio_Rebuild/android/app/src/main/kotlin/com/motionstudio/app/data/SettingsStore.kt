package com.motionstudio.app.data

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb

class SettingsStore(context: Context) {

    private val prefs = context.getSharedPreferences("motion_studio_settings", Context.MODE_PRIVATE)

    var accent: Color by mutableStateOf(Color(prefs.getInt(KEY_ACCENT, DEFAULT_ACCENT)))
        private set

    var autosaveEnabled: Boolean by mutableStateOf(prefs.getBoolean(KEY_AUTOSAVE, true))
        private set

    fun updateAccent(color: Color) {
        accent = color
        prefs.edit().putInt(KEY_ACCENT, color.toArgb()).apply()
    }

    fun setAutosave(enabled: Boolean) {
        autosaveEnabled = enabled
        prefs.edit().putBoolean(KEY_AUTOSAVE, enabled).apply()
    }

    companion object {
        const val DEFAULT_ACCENT = 0xFF9476F5.toInt()
        private const val KEY_ACCENT = "accent"
        private const val KEY_AUTOSAVE = "autosave"
    }
}
