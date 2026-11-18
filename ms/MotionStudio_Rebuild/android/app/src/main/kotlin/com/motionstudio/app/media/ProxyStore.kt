package com.motionstudio.app.media

import android.content.Context
import com.motionstudio.app.model.NativeMedia
import com.motionstudio.app.native.NativeEngine
import java.io.File
import java.security.MessageDigest

/**
 * Where proxy files live and how the engine learns about them.
 *
 * A proxy is a small H.264 stand-in for a large source file. The engine decodes
 * it while you scrub and play, and decodes the original when you export, so 4K
 * phone footage can play at 720p without capping delivery quality.
 *
 * The name is derived from the absolute source path, so regenerating a proxy
 * replaces the old one and two projects pointing at the same file share it. Only
 * the file name is hashed: `std::hash` in the engine is not reproducible from
 * Kotlin, which is why registration passes both paths explicitly instead of
 * letting each side compute a name.
 *
 * The root is installed once from `MsApplication`, so the editor can re-register
 * proxies on every project load without threading a Context through the engine.
 */
object ProxyStore {

    /** Longest edge of a generated proxy: enough for a phone preview. */
    const val MAX_WIDTH = 1280
    const val MAX_HEIGHT = 720

    private var root: File? = null

    fun init(context: Context) {
        root = File(context.filesDir, "proxies")
    }

    fun dir(): File = root ?: File(".")

    fun fileFor(mediaPath: String): File = File(dir(), "${hash(mediaPath)}.mp4")

    fun exists(mediaPath: String): Boolean =
        fileFor(mediaPath).let { it.isFile && it.length() > 0 }

    /**
     * Registers every proxy that already exists for [media] and drops
     * registrations whose file has gone. Called on project load, so proxies
     * survive a restart without being written into the document.
     */
    fun sync(media: List<NativeMedia>): Int {
        if (!NativeEngine.available) return 0
        var registered = 0
        for (m in media) {
            val proxy = fileFor(m.path)
            if (proxy.isFile && proxy.length() > 0) {
                if (NativeEngine.nativeProxyRegister(m.path, proxy.absolutePath)) registered++
            } else {
                NativeEngine.nativeProxyUnregister(m.path)
            }
        }
        return registered
    }

    /** Media in [media] that has no proxy yet and would benefit from one. */
    fun missing(media: List<NativeMedia>): List<NativeMedia> =
        media.filter { m ->
            !exists(m.path) && ProxyGenerator.sourceSize(m.path)?.let { (w, h) ->
                ProxyGenerator.needed(w, h)
            } == true
        }

    fun totalBytes(): Long = dir().listFiles()?.filter { it.isFile }?.sumOf { it.length() } ?: 0L

    fun count(): Int = dir().listFiles()?.count { it.isFile } ?: 0

    fun clear() {
        if (NativeEngine.available) NativeEngine.nativeProxyClear()
        dir().listFiles()?.forEach { it.delete() }
    }

    private fun hash(value: String): String {
        val digest = MessageDigest.getInstance("SHA-1").digest(value.toByteArray())
        return digest.joinToString("") { "%02x".format(it) }.take(24)
    }
}
