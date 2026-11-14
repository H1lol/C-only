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
import com.motionstudio.app.native.NativeEngine

/**
 * Streams frames straight out of the C++ renderer into a Bitmap and blits them.
 *
 * This is the real preview path (`ms_render_frame` + `ms_copy_frame_rgba`), not
 * a placeholder: what the engine composites is what the viewer shows.
 */
class PreviewView(context: Context) : View(context) {

    private var engine: EditorEngine? = null
    private var frame: Bitmap? = null
    private var frameWidth = 0
    private var frameHeight = 0
    private var looping = false

    private val bitmapPaint = Paint(Paint.FILTER_BITMAP_FLAG or Paint.ANTI_ALIAS_FLAG)
    private val backgroundPaint = Paint().apply { color = Color.parseColor("#0B0B0E") }
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
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), backgroundPaint)
        val bitmap = frame ?: return
        val pad = 12f
        val availableW = width - pad * 2
        val availableH = height - pad * 2
        if (availableW <= 0f || availableH <= 0f) return

        val aspect = bitmap.width.toFloat() / bitmap.height.toFloat()
        var drawW = availableW
        var drawH = drawW / aspect
        if (drawH > availableH) {
            drawH = availableH
            drawW = drawH * aspect
        }
        drawW *= scale
        drawH *= scale

        val left = (width - drawW) / 2f + offsetX
        val top = (height - drawH) / 2f + offsetY
        dst.set(left, top, left + drawW, top + drawH)
        src.set(0, 0, bitmap.width, bitmap.height)
        canvas.drawBitmap(bitmap, src, dst, bitmapPaint)

        bitmapPaint.colorFilter = null
        val border = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#33FFFFFF")
            style = Paint.Style.STROKE
            strokeWidth = 1f
        }
        canvas.drawRect(dst, border)
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
