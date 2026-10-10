package com.pihifi.player

import android.content.Context
import android.opengl.GLSurfaceView
import android.util.AttributeSet
import android.view.MotionEvent
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class HifiGLSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : GLSurfaceView(context, attrs), GLSurfaceView.Renderer {

    companion object {
        init {
            System.loadLibrary("PiHifiCore")
        }

        @JvmStatic
        external fun nativeInit(configDir: String, musicDir: String)
    }

    external fun nativeSurfaceCreated()
    external fun nativeSurfaceChanged(width: Int, height: Int, density: Float)
    external fun nativeSetSafeArea(left: Float, top: Float, right: Float, bottom: Float)
    external fun nativeDrawFrame()
    external fun nativeOnTouchEvent(action: Int, x: Float, y: Float, eventTimeMs: Long)

    init {
        // 请求 OpenGL ES 3.0 上下文环境
        setEGLContextClientVersion(3)
        setEGLConfigChooser(8, 8, 8, 8, 16, 0)
        setRenderer(this)
        renderMode = RENDERMODE_CONTINUOUSLY
        preserveEGLContextOnPause = true
    }

    fun updateSafeArea(left: Float, top: Float, right: Float, bottom: Float) {
        queueEvent {
            nativeSetSafeArea(left, top, right, bottom)
        }
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        nativeSurfaceCreated()
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        val shortEdge = minOf(width, height).toFloat()
        // 标准 Android FHD+ 横屏规范高度 (412 dp，完美释放垂直空间，消除拥挤感)
        val targetLogicalHeight = 412.0f
        val computedDensity = shortEdge / targetLogicalHeight
        val rawDensity = resources.displayMetrics.density
        val effectiveDensity = if (computedDensity in 1.2f..5.0f) computedDensity else rawDensity

        nativeSurfaceChanged(width, height, effectiveDensity)
    }

    override fun onDrawFrame(gl: GL10?) {
        nativeDrawFrame()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val action = event.actionMasked
        val x = event.x
        val y = event.y
        val timeMs = event.eventTime

        queueEvent {
            nativeOnTouchEvent(action, x, y, timeMs)
        }
        return true
    }
}
