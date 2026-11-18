package com.motionstudio.app.ui

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View
import com.motionstudio.app.editor.EditorEngine
import com.motionstudio.app.model.PreviewRatio
import com.motionstudio.app.native.NativeEngine

/**
 * Streams frames straight out of the C++ renderer into a Bitmap and blits them.
 *
 * This is the real preview path (`ms_render_frame` + `ms_copy_frame_rgba`), not
 * a placeholder: what the engine composites is what the viewer shows.
 *
 * Rendering runs on its own thread. It used to be a `post`ed Runnable on the UI
 * thread, which meant a slow composite — a 4K clip, a blur, a few effects —
 * blocked input and could trip the ANR watchdog. The worker renders into a
 * bitmap the draw pass also reads, so both sides take the same lock; that is
 * far cheaper than a second full-frame copy per frame and it keeps the preview
 * and the UI independent.
 *
 * [EditorEngine.previewRatio] adds a delivery-aspect guide: the composite is
 * kept whole inside the guide and everything outside it is shaded, so you can
 * see the 9:16 crop of a 16:9 canvas without changing the project.
 */
class PreviewView(context: Context) : View(context) {

    private var engine: EditorEngine? = null

    private val lock = Any()
    private var frame: Bitmap? = null
    private var frameWidth = 0
    private var frameHeight = 0
    private var lastRatio: PreviewRatio = PreviewRatio.Project

    @Volatile
    private var looping = false
    private var worker: Thread? = null

    private val bitmapPaint = Paint(Paint.FILTER_BITMAP_FLAG or Paint.ANTI_ALIAS_FLAG)
    private val backgroundPaint = Paint().apply { color = Color.parseColor("#0B0B0E") }
    private val shadePaint = Paint().apply { color = Color.parseColor("#A6000000") }
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#33FFFFFF")
        style = Paint.Style.STROKE
        strokeWidth = 1f
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#8AFFFFFF")
        textSize = 24f
        letterSpacing = 0.08f
    }
    private val src = Rect()
    private val dst = RectF()

    private var scale = 1f
    private var offsetX = 0f
    private var offsetY = 0f
    private var lastX = 0f
    private var lastY = 0f

    private val scaleDetector = ScaleGestureDetector(
        context,
        object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
            override fun onScale(detector: ScaleGestureDetector): Boolean {
                scale = (scale * detector.scaleFactor).coerceIn(0.5f, 6f)
                invalidate()
                return true
            }
        }
    )

    fun attach(editor: EditorEngine) {
        engine = editor
        if (isAttachedToWindow) startWorker()
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        if (engine != null) startWorker()
    }

    override fun onDetachedFromWindow() {
        stopWorker()
        super.onDetachedFromWindow()
    }

    private fun startWorker() {
        if (worker != null) return
        looping = true
        worker = Thread({
            while (looping) {
                val produced = try {
                    renderOnce()
                } catch (t: Throwable) {
                    // A preview failure must not kill the editor; report once.
                    com.motionstudio.app.diag.Diagnostics.note("preview render failed: $t")
                    false
                }
                if (produced) postInvalidateOnAnimation()
                // Playback wants every frame the renderer can manage; a paused
                // playhead only needs to notice edits.
                val delay = if (engine?.playing == true) 16L else 50L
                try {
                    Thread.sleep(delay)
                } catch (_: InterruptedException) {
                    break
                }
            }
        }, "ms-preview").apply {
            priority = Thread.NORM_PRIORITY + 1
            start()
        }
    }

    private fun stopWorker() {
        looping = false
        worker?.interrupt()
        worker = null
    }

    /** Renders one frame. Runs on the preview thread. Returns true when new pixels landed. */
    private fun renderOnce(): Boolean {
        val editor = engine ?: return false
        val state = NativeEngine.nativePreviewState()
        if (state.size < 4) return false
        val w = state[1].toInt().coerceAtLeast(1)
        val h = state[2].toInt().coerceAtLeast(1)

        if (!NativeEngine.nativeRenderFrame()) return false

        val ready = synchronized(lock) {
            if (frame == null || w != frameWidth || h != frameHeight) {
                frame?.recycle()
                frame = runCatching { Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888) }.getOrNull()
                frameWidth = w
                frameHeight = h
                val expected = NativeEngine.nativeFrameBytes()
                if (frame == null) return@synchronized false
                if (expected > 0 && expected != w.toLong() * h * 4) {
                    // Stride mismatch: skip drawing rather than show corrupt pixels.
                    frame?.eraseColor(Color.BLACK)
                    return@synchronized false
                }
            }
            val bitmap = frame ?: return@synchronized false
            NativeEngine.nativeCopyFrame(bitmap)
        }

        if (ready) {
            // Playhead state belongs to the UI thread.
            post { editor.onFrameRendered(state[0], state[3] == 1.0) }
        }
        val ratio = editor.previewRatio
        if (ratio != lastRatio) {
            lastRatio = ratio
            post { resetView() }
        }
        return ready
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), backgroundPaint)
        val ratio = engine?.previewRatio ?: PreviewRatio.Project
        val pad = 12f
        val availableW = width - pad * 2
        val availableH = height - pad * 2
        if (availableW <= 0f || availableH <= 0f) return

        // The worker may be writing into the bitmap on another thread; hold the
        // same lock for the whole draw so a frame is never half-copied.
        synchronized(lock) {
            val bitmap = frame ?: return

            // 1. Guide = the chosen delivery aspect, contained in the view.
            val guideAspect = ratio.aspect ?: (bitmap.width.toFloat() / bitmap.height.toFloat())
            var guideW = availableW
            var guideH = guideW / guideAspect
            if (guideH > availableH) {
                guideH = availableH
                guideW = guideH * guideAspect
            }
            guideW *= scale
            guideH *= scale
            val guideLeft = (width - guideW) / 2f + offsetX
            val guideTop = (height - guideH) / 2f + offsetY
            val guide = RectF(guideLeft, guideTop, guideLeft + guideW, guideTop + guideH)

            // 2. The composite, contained inside the guide (never cropped by it).
            val frameAspect = bitmap.width.toFloat() / bitmap.height.toFloat()
            var drawW = guide.width()
            var drawH = drawW / frameAspect
            if (drawH > guide.height()) {
                drawH = guide.height()
                drawW = drawH * frameAspect
            }
            val left = guide.centerX() - drawW / 2f
            val top = guide.centerY() - drawH / 2f
            dst.set(left, top, left + drawW, top + drawH)
            src.set(0, 0, bitmap.width, bitmap.height)

            // 3. Everything outside the guide is shaded so the crop reads at a glance.
            if (ratio != PreviewRatio.Project) {
                canvas.drawRect(0f, 0f, width.toFloat(), guide.top, shadePaint)
                canvas.drawRect(0f, guide.bottom, width.toFloat(), height.toFloat(), shadePaint)
                canvas.drawRect(0f, guide.top, guide.left, guide.bottom, shadePaint)
                canvas.drawRect(guide.right, guide.top, width.toFloat(), guide.bottom, shadePaint)
                canvas.drawRect(guide, borderPaint)
                canvas.drawText(ratio.label, guide.left + 10f, guide.top + 30f, labelPaint)
            }

            canvas.drawBitmap(bitmap, src, dst, bitmapPaint)
            canvas.drawRect(dst, borderPaint)
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(event)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastX = event.x
                lastY = event.y
                parent?.requestDisallowInterceptTouchEvent(true)
            }
            MotionEvent.ACTION_MOVE -> {
                if (!scaleDetector.isInProgress && scale > 1.01f) {
                    offsetX += event.x - lastX
                    offsetY += event.y - lastY
                    lastX = event.x
                    lastY = event.y
                    invalidate()
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                parent?.requestDisallowInterceptTouchEvent(false)
            }
        }
        return true
    }

    fun resetView() {
        scale = 1f
        offsetX = 0f
        offsetY = 0f
        invalidate()
    }

    /** Frees the frame bitmap. Called when the editor leaves the screen. */
    fun releaseFrame() {
        stopWorker()
        synchronized(lock) {
            frame?.recycle()
            frame = null
            frameWidth = 0
            frameHeight = 0
        }
    }
}
