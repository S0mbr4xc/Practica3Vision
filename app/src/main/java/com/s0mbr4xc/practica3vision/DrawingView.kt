package com.s0mbr4xc.practica3vision

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View

class DrawingView(context: Context, attrs: AttributeSet) : View(context, attrs) {
    private val paint = Paint().apply {
        color = Color.WHITE
        strokeWidth = 8f
        style = Paint.Style.STROKE
        isAntiAlias = true
    }

    private val path = Path()
    private val bitmapPaint = Paint(Paint.DITHER_FLAG)
    private var bitmap: Bitmap? = null
    private var canvasBmp: Canvas? = null

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        canvasBmp = Canvas(bitmap!!)
        canvasBmp?.drawColor(Color.BLACK)
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawBitmap(bitmap!!, 0f, 0f, bitmapPaint)
        canvas.drawPath(path, paint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val x = event.x
        val y = event.y

        when (event.action) {
            MotionEvent.ACTION_DOWN -> path.moveTo(x, y)
            MotionEvent.ACTION_MOVE -> path.lineTo(x, y)
            MotionEvent.ACTION_UP -> canvasBmp?.drawPath(path, paint)
        }

        invalidate()
        return true
    }

    fun getDrawingBitmap(): Bitmap = bitmap!!
    fun clearCanvas() {
        path.reset()
        canvasBmp?.drawColor(Color.BLACK)
        invalidate()
    }
}
