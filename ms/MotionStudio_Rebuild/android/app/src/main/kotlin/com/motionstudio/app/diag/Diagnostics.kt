package com.motionstudio.app.diag

import android.content.Context
import android.os.Build
import com.motionstudio.app.BuildConfig
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Crash capture and a breadcrumb trail, readable from Settings without adb.
 *
 * A phone tester cannot attach a debugger, so the app records two files in
 * `filesDir/diagnostics`:
 *
 *  - `last-crash.txt` — the uncaught exception, device facts, ABI and whether
 *    the native library loaded at all;
 *  - `session.log` — a bounded trail of what the editor was doing.
 *
 * The native signal handler writes `native-crash.txt` beside them; a segfault
 * inside the engine kills the process before any Java handler can run, so that
 * file is often the only evidence of what happened.
 */
object Diagnostics {

    private const val MAX_LOG_BYTES = 256 * 1024
    private const val LOG_FILE = "session.log"
    private const val CRASH_FILE = "last-crash.txt"
    private const val NATIVE_CRASH_FILE = "native-crash.txt"

    private val lock = Any()
    private var root: File? = null
    private var installed = false
    private val stamp = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)

    /** True once `libmotionstudio_jni.so` is loaded. Set by [com.motionstudio.app.native.NativeEngine]. */
    @Volatile
    var nativeLoaded: Boolean = false

    /** Why the library failed to load, when it did. */
    @Volatile
    var nativeError: String? = null

    /** Decoder backend reported by the engine, for the diagnostics panel. */
    @Volatile
    var decoderBackend: String = "unknown"

    /** Installs the process-wide uncaught-exception handler. Safe to call twice. */
    fun install(context: Context) {
        if (installed) return
        installed = true
        root = File(context.filesDir, "diagnostics").apply { mkdirs() }
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, error ->
            runCatching { writeCrash(thread.name, error) }
            // Hand back to the platform handler so the system still reports it.
            if (previous != null) {
                previous.uncaughtException(thread, error)
            } else {
                android.os.Process.killProcess(android.os.Process.myPid())
            }
        }
        note("app start · ${Build.MANUFACTURER} ${Build.MODEL} · Android ${Build.VERSION.RELEASE} " +
            "(SDK ${Build.VERSION.SDK_INT}) · ${Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown abi"}")
    }

    /** Directory the native crash handler should write into. */
    fun directory(context: Context): String =
        File(context.filesDir, "diagnostics").apply { mkdirs() }.absolutePath

    /** Appends one line to the breadcrumb trail. Never throws. */
    fun note(message: String) {
        val dir = root ?: return
        runCatching {
            synchronized(lock) {
                val file = File(dir, LOG_FILE)
                if (file.length() > MAX_LOG_BYTES) file.delete()
                file.appendText("${stamp.format(Date())}  $message\n")
            }
        }
    }

    fun lastCrash(): String = read(CRASH_FILE)

    fun nativeCrash(): String = read(NATIVE_CRASH_FILE)

    fun sessionLog(): String = read(LOG_FILE)

    /** Everything the user could paste into a bug report. */
    fun report(context: Context): String = buildString {
        appendLine("Motion Studio diagnostics")
        appendLine("app: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
        appendLine("device: ${Build.MANUFACTURER} ${Build.MODEL} · ${Build.DEVICE}")
        appendLine("android: ${Build.VERSION.RELEASE} (SDK ${Build.VERSION.SDK_INT})")
        appendLine("abis: ${Build.SUPPORTED_ABIS.joinToString(", ")}")
        appendLine("native library: ${if (nativeLoaded) "loaded" else "FAILED — ${nativeError ?: "unknown"}"}")
        appendLine("decoder backend: $decoderBackend")
        appendLine("heap limit: ${Runtime.getRuntime().maxMemory() / (1024 * 1024)} MB")
        appendLine()
        appendLine("=== last java crash ===")
        appendLine(lastCrash().ifBlank { "(none recorded)" })
        appendLine("=== last native crash ===")
        appendLine(nativeCrash().ifBlank { "(none recorded)" })
        appendLine("=== session log (tail) ===")
        val log = sessionLog()
        appendLine(if (log.length > 12_000) log.takeLast(12_000) else log.ifBlank { "(empty)" })
    }

    /** Clears the recorded crashes, keeping the breadcrumb trail. */
    fun clearCrashes() {
        val dir = root ?: return
        runCatching { File(dir, CRASH_FILE).delete() }
        runCatching { File(dir, NATIVE_CRASH_FILE).delete() }
    }

    private fun read(name: String): String {
        val dir = root ?: return ""
        return runCatching {
            val file = File(dir, name)
            if (file.exists() && file.length() > 0) file.readText() else ""
        }.getOrDefault("")
    }

    private fun writeCrash(threadName: String, error: Throwable) {
        val dir = root
        val text = buildString {
            appendLine("--- Motion Studio crash ---")
            appendLine("time: ${stamp.format(Date())}")
            appendLine("app: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
            appendLine("device: ${Build.MANUFACTURER} ${Build.MODEL} · Android ${Build.VERSION.RELEASE} (SDK ${Build.VERSION.SDK_INT})")
            appendLine("abis: ${Build.SUPPORTED_ABIS.joinToString(", ")}")
            appendLine("thread: $threadName")
            appendLine("exception: ${error.javaClass.name}: ${error.message}")
            appendLine(error.stackTraceToString())
            val cause = error.cause
            if (cause != null && cause !== error) {
                appendLine("caused by: ${cause.javaClass.name}: ${cause.message}")
                appendLine(cause.stackTraceToString())
            }
            appendLine("=== session log (tail) ===")
            val log = if (dir != null) runCatching { File(dir, LOG_FILE).readText() }.getOrDefault("") else ""
            appendLine(if (log.length > 8_000) log.takeLast(8_000) else log)
        }
        if (dir != null) {
            synchronized(lock) {
                File(dir, CRASH_FILE).writeText(text)
            }
        }
        // Logcat is still worth writing: it costs nothing and helps anyone with adb.
        android.util.Log.e("MotionStudio", text)
    }
}
