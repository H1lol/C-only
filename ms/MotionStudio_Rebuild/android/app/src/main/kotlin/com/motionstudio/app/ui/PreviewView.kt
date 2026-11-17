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
 * [EditorEngine.previewRatio] adds a delivery-aspect guide: the composite is
 * kept whole inside the guide and everything outside it is shaded, so you can
 * see the 9:16 crop of a 16:9 canvas without changing the project.
 */
class PreviewView(context: Context) : View(context) {

    private var engine: EditorEngine? = null
    private var frame: Bitmap? = null
    private var frameWidth = 0
    private var frameHeight = 0
    private var looping = false
    private var lastRatio: PreviewRatio = PreviewRatio.Project

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

    private val frameLoop = object : Runnable {
        override fun run() {
            if (!looping) return
            renderOnce()
            postInvalidateOnAnimation()
            postDelayed(this, 16L)
        }
    }

    fun attach(editor: EditorEngine) {
        engine = editor
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        looping = true
        post(frameLoop)
    }

    override fun onDetachedFromWindow() {
        looping = false
        removeCallbacks(frameLoop)
        super.onDetachedFromWindow()
    }

    private fun renderOnce() {
        val editor = engine ?: return
        val state = NativeEngine.nativePreviewState()
        if (state.size < 4) return
        val w = state[1].toInt().coerceAtLeast(1)
        val h = state[2].toInt().coerceAtLeast(1)

        if (frame == null || w != frameWidth || h != frameHeight) {
            frame?.recycle()
            frame = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
            frameWidth = w
            frameHeight = h
            val expected = NativeEngine.nativeFrameBytes()
            if (expected > 0 && expected != w.toLong() * h * 4) {
                // Stride mismatch: skip drawing rather than show corrupt pixels.
                frame?.eraseColor(Color.BLACK)
            }
        }

        if (!NativeEngine.nativeRenderFrame()) return
        val bitmap = frame ?: return
        if (NativeEngine.nativeCopyFrame(bitmap)) {
            editor.onFrameRendered(state[0], state[3] == 1.0)
        }
        if (editor.previewRatio != lastRatio) {
            lastRatio = editor.previewRatio
            resetView()
        }
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), backgroundPaint)
        val bitmap = frame ?: return
        val ratio = engine?.previewRatio ?: PreviewRatio.Project
        val pad = 12f
        val availableW = width - pad * 2
        val availableH = height - pad * 2
        if (availableW <= 0f || availableH <= 0f) return

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
}
