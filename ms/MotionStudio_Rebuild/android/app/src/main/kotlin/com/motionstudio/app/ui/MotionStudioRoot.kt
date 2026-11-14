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
import com.motionstudio.app.data.ProjectStore
import com.motionstudio.app.data.SettingsStore
import com.motionstudio.app.editor.EditorEngine
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
                    val created = EditorEngine(store) { settings.autosaveEnabled }
                    if (created.openFile(file)) {
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
                    val created = EditorEngine(store) { settings.autosaveEnabled }
                    created.openNew(name, width, height, fps, 15.0)
                    engine = created
                },
                onSettings = { screen = Screen.Settings },
            )
        }
    }
}
