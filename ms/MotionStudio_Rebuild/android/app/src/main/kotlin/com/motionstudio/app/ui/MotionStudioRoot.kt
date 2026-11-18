package com.motionstudio.app.ui

import android.widget.Toast
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import android.os.Build
import com.motionstudio.app.data.ProjectStore
import com.motionstudio.app.data.SettingsStore
import com.motionstudio.app.diag.Diagnostics
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.native.NativeEngine
import com.motionstudio.app.ui.theme.MotionStudioTheme

private enum class Screen { Starter, Settings }

@Composable
fun MotionStudioRoot(settings: SettingsStore) {
    val context = LocalContext.current
    val store = remember { ProjectStore(context) }
    var engine by remember { mutableStateOf<EditorEngine?>(null) }
    var screen by remember { mutableStateOf(Screen.Starter) }

    LifecycleEventEffect(Lifecycle.Event.ON_PAUSE) {
        engine?.maybeAutosave()
    }

    MotionStudioTheme(accent = settings.accent) {
        // Without the engine there is no editor to show. Replacing the UI here
        // keeps the first native call from throwing out of a button handler.
        if (!NativeEngine.available) {
            EngineUnavailableScreen(NativeEngine.loadError, Build.SUPPORTED_ABIS.toList())
            return@MotionStudioTheme
        }

        val active = engine
        when {
            active != null -> EditorScreen(
                engine = active,
                onBack = {
                    active.release()
                    engine = null
                },
                onExportFrame = {
                    val file = exportCurrentFrame(context, active)
                    Toast.makeText(
                        context,
                        file?.let { "Saved ${it.name}" } ?: "Could not render a frame",
                        Toast.LENGTH_SHORT,
                    ).show()
                },
            )

            screen == Screen.Settings -> SettingsScreen(
                settings = settings,
                onBack = { screen = Screen.Starter },
            )

            else -> StarterScreen(
                store = store,
                onOpenProject = { file ->
                    val created = runCatching { EditorEngine(store) { settings.autosaveEnabled } }
                        .getOrNull()
                    if (created == null) {
                        Diagnostics.note("EditorEngine construction threw")
                        Toast.makeText(context, "The engine could not start", Toast.LENGTH_LONG).show()
                    } else if (created.openFile(file)) {
                        engine = created
                    } else {
                        created.release()
                        Toast.makeText(
                            context,
                            created.lastError ?: "Could not open that project",
                            Toast.LENGTH_SHORT,
                        ).show()
                    }
                },
                onCreateProject = { name, width, height, fps ->
                    val created = runCatching { EditorEngine(store) { settings.autosaveEnabled } }
                        .getOrNull()
                    if (created == null) {
                        Diagnostics.note("EditorEngine construction threw")
                        Toast.makeText(context, "The engine could not start", Toast.LENGTH_LONG).show()
                    } else {
                        created.openNew(name, width, height, fps, 15.0)
                        engine = created
                    }
                },
                onSettings = { screen = Screen.Settings },
            )
        }
    }
}
