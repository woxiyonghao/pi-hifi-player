package com.pihifi.player

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.view.View
import android.view.WindowInsets
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var glSurfaceView: HifiGLSurfaceView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // 1. 设置沉浸式全屏
        hideSystemUI()

        // 2. 初始化目录结构 (沙盒配置目录与外部音乐目录)
        val configDir = File(filesDir, "hifi_player").apply { mkdirs() }
        extractAssetsIfNeeded(configDir)
        val musicDir = getExternalMusicDir()

        // 3. 原生 C++ 核心引擎路径注入 (与 iOS 沙盒路径设置逻辑一致)
        HifiGLSurfaceView.nativeInit(configDir.absolutePath, musicDir)

        // 4. 动态装配发烧 GLSurfaceView
        glSurfaceView = HifiGLSurfaceView(this)
        setContentView(glSurfaceView)

        // 5. 监听刘海屏、水滴屏与导航栏 Insets 安全区域
        setupWindowInsets()

        // 6. 请求必要运行时权限
        checkAndRequestPermissions()

        // 7. 启动常驻后台前台服务
        AudioPlaybackService.startService(this)
    }

    override fun onResume() {
        super.onResume()
        hideSystemUI()
        glSurfaceView.onResume()
    }

    override fun onPause() {
        super.onPause()
        glSurfaceView.onPause()
    }

    private fun hideSystemUI() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

    private fun setupWindowInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(glSurfaceView) { _, insets ->
            val displayCutout = insets.displayCutout
            val left = (displayCutout?.safeInsetLeft ?: 0).toFloat()
            val top = (displayCutout?.safeInsetTop ?: 0).toFloat()
            val right = (displayCutout?.safeInsetRight ?: 0).toFloat()
            val bottom = (displayCutout?.safeInsetBottom ?: 0).toFloat()

            glSurfaceView.updateSafeArea(left, top, right, bottom)
            insets
        }
    }

    private fun getExternalMusicDir(): String {
        // 1. 优先使用应用专属外部存储目录 (位于 /storage/emulated/0/Android/data/com.pihifi.player/files/Music)
        // 该目录拥有设备全部外部存储容量(上百GB)，且在 Android 10/11/12/13/14+ 下原生支持 POSIX C++ 无权限直接创建/写入文件！
        val appExtMusic = getExternalFilesDir(Environment.DIRECTORY_MUSIC)
        if (appExtMusic != null) {
            if (!appExtMusic.exists()) {
                appExtMusic.mkdirs()
            }
            try {
                val testFile = File(appExtMusic, ".probe_write")
                if (testFile.createNewFile()) {
                    testFile.delete()
                    return appExtMusic.absolutePath
                }
            } catch (_: Exception) {}
        }

        // 2. 检查公共 Music 目录 (/storage/emulated/0/Music) 是否具备写入权限 (例如 Android 9 及以下或已授权 MANAGE_EXTERNAL_STORAGE)
        val pubMusic = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MUSIC)
        if (pubMusic.exists() || pubMusic.mkdirs()) {
            try {
                val testFile = File(pubMusic, ".probe_write")
                if (testFile.createNewFile()) {
                    testFile.delete()
                    return pubMusic.absolutePath
                }
            } catch (_: Exception) {}
        }

        // 3. 保底内部沙盒目录
        val internalMusic = File(filesDir, "Music").apply { mkdirs() }
        return internalMusic.absolutePath
    }

    private fun checkAndRequestPermissions() {
        val permissionsToRequest = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_MEDIA_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {
                permissionsToRequest.add(Manifest.permission.READ_MEDIA_AUDIO)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS)
                != PackageManager.PERMISSION_GRANTED) {
                permissionsToRequest.add(Manifest.permission.POST_NOTIFICATIONS)
            }
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
                permissionsToRequest.add(Manifest.permission.READ_EXTERNAL_STORAGE)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
                permissionsToRequest.add(Manifest.permission.WRITE_EXTERNAL_STORAGE)
            }
        }

        if (permissionsToRequest.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, permissionsToRequest.toTypedArray(), 100)
        }
    }

    private fun extractAssetsIfNeeded(configDir: File) {
        val fontsDir = File(configDir, "fonts").apply { mkdirs() }
        val targetFont = File(fontsDir, "HiraginoSansGB.ttc")
        if (!targetFont.exists() || targetFont.length() < 1000000L) {
            try {
                assets.open("fonts/HiraginoSansGB.ttc").use { input ->
                    targetFont.outputStream().use { output ->
                        input.copyTo(output)
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }
}
