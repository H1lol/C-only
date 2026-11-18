package com.motionstudio.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import com.motionstudio.app.data.SettingsStore
import com.motionstudio.app.diag.Diagnostics
import com.motionstudio.app.editor.TextFonts
import com.motionstudio.app.native.NativeEngine
import com.motionstudio.app.ui.MotionStudioRoot

/**
 * Single activity. All editing UI is Jetpack Compose; every render, timeline and
 * audio operation goes through the C++ engine over JNI.
 *
 * Nothing here is allowed to crash the launch: the native library may be
 * absent on an unsupported ABI, and the font unpack can fail on a full disk.
 * Both are recorded and the UI degrades instead.
 */
class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        if (NativeEngine.available) {
            // The signal handler must exist before the engine is used at all:
            // a native crash takes the whole process down without a Java trace.
            runCatching { NativeEngine.nativeInstallCrashHandler(Diagnostics.directory(this)) }
            // Unpack the bundled title fonts so the native rasteriser can open
            // them before the first project is loaded.
            runCatching { TextFonts.ensure(this) }
            runCatching { Diagnostics.decoderBackend = NativeEngine.nativeDecoderBackend() }
        } else {
            Diagnostics.note("native library unavailable: ${NativeEngine.loadError}")
        }
        Diagnostics.note("MainActivity created")

        val settings = SettingsStore(this)
        setContent {
            MotionStudioRoot(settings)
        }
    }
}
