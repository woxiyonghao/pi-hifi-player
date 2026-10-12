package com.pihifi.player

import android.content.Context
import android.opengl.GLSurfaceView
import android.text.InputType
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputMethodManager
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class HifiGLSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : GLSurfaceView(context, attrs), GLSurfaceView.Renderer {

    companion object {
        const val KEY_BACKSPACE = 1
        const val KEY_ENTER = 2

        init {
            System.loadLibrary("PiHifiCore")
        }

        @JvmStatic
        external fun nativeInit(configDir: String, musicDir: String)

        @JvmStatic
        external fun nativeAddInputCharacters(text: String)

        @JvmStatic
        external fun nativeSendKeyEvent(keyCode: Int)
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

        // 允许获取焦点以支持软键盘与输入法
        isFocusable = true
        isFocusableInTouchMode = true
    }

    fun updateSafeArea(left: Float, top: Float, right: Float, bottom: Float) {
        queueEvent {
            nativeSetSafeArea(left, top, right, bottom)
        }
    }

    fun showKeyboard() {
        post {
            requestFocus()
            val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
            if (imm != null) {
                if (!imm.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT)) {
                    imm.showSoftInput(this, InputMethodManager.SHOW_FORCED)
                }
            }
        }
    }

    fun hideKeyboard() {
        post {
            val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
            imm?.hideSoftInputFromWindow(windowToken, 0)
            clearFocus()
        }
    }

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_NORMAL
        outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE or EditorInfo.IME_FLAG_NO_FULLSCREEN or EditorInfo.IME_FLAG_NO_EXTRACT_UI
        return object : BaseInputConnection(this, true) {
            override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
                text?.let {
                    if (it.isNotEmpty()) {
                        queueEvent {
                            nativeAddInputCharacters(it.toString())
                        }
                    }
                }
                return true
            }

            override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
                queueEvent {
                    for (i in 0 until beforeLength) {
                        nativeSendKeyEvent(KEY_BACKSPACE)
                    }
                }
                return true
            }

            override fun sendKeyEvent(event: KeyEvent): Boolean {
                if (event.action == KeyEvent.ACTION_DOWN) {
                    when (event.keyCode) {
                        KeyEvent.KEYCODE_DEL -> {
                            queueEvent { nativeSendKeyEvent(KEY_BACKSPACE) }
                            return true
                        }
                        KeyEvent.KEYCODE_ENTER -> {
                            queueEvent { nativeSendKeyEvent(KEY_ENTER) }
                            return true
                        }
                    }
                }
                return super.sendKeyEvent(event)
            }

            override fun performEditorAction(actionCode: Int): Boolean {
                if (actionCode == EditorInfo.IME_ACTION_DONE || actionCode == EditorInfo.IME_ACTION_GO) {
                    queueEvent { nativeSendKeyEvent(KEY_ENTER) }
                    hideKeyboard()
                    return true
                }
                return super.performEditorAction(actionCode)
            }
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
