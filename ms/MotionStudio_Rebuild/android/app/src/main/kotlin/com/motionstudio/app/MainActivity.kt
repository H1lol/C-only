package com.motionstudio.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import com.motionstudio.app.data.SettingsStore
import com.motionstudio.app.editor.TextFonts
import com.motionstudio.app.ui.MotionStudioRoot

/**
 * Single activity. All editing UI is Jetpack Compose; every render, timeline and
 * audio operation goes through the C++ engine over JNI.
 */
class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        // Unpack the bundled title fonts so the native rasteriser can open them
        // before the first project is loaded.
        TextFonts.ensure(this)
        val settings = SettingsStore(this)
        setContent {
            MotionStudioRoot(settings)
        }
    }
}
