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

    companion object {
        @JvmStatic
        var appContext: android.content.Context? = null

        @JvmStatic
        fun queryMediaStoreAudioPaths(): Array<String> {
            val paths = LinkedHashSet<String>()
            val ctx = appContext ?: return emptyArray()
            val supportedExts = setOf(
                "flac", "wav", "dsf", "dff", "cue", "mp3", "m4a", "alac", "aac", "aiff", "aif", "caf"
            )

            // 1. 查询系统 MediaStore 音频数据表
            try {
                val uri = android.provider.MediaStore.Audio.Media.EXTERNAL_CONTENT_URI
                val projection = arrayOf(android.provider.MediaStore.Audio.Media.DATA)
                ctx.contentResolver.query(uri, projection, null, null, null)?.use { cursor ->
                    val dataCol = cursor.getColumnIndex(android.provider.MediaStore.Audio.Media.DATA)
                    if (dataCol >= 0) {
                        while (cursor.moveToNext()) {
                            val p = cursor.getString(dataCol)
                            if (!p.isNullOrEmpty()) {
                                paths.add(p)
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }

            // 2. 查询 MediaStore.Files 全局文件数据表 (全面捕获未被系统媒体扫描器标记为 Audio 的 FLAC, WAV, DSF, DFF, CUE 等)
            try {
                val filesUri = android.provider.MediaStore.Files.getContentUri("external")
                val projection = arrayOf(android.provider.MediaStore.Files.FileColumns.DATA)
                val selSb = StringBuilder()
                val selArgs = ArrayList<String>()
                for (ext in supportedExts) {
                    if (selSb.isNotEmpty()) selSb.append(" OR ")
                    selSb.append("${android.provider.MediaStore.Files.FileColumns.DATA} LIKE ?")
                    selArgs.add("%.$ext")
                }
                ctx.contentResolver.query(filesUri, projection, selSb.toString(), selArgs.toTypedArray(), null)?.use { cursor ->
                    val dataCol = cursor.getColumnIndex(android.provider.MediaStore.Files.FileColumns.DATA)
                    if (dataCol >= 0) {
                        while (cursor.moveToNext()) {
                            val p = cursor.getString(dataCol)
                            if (!p.isNullOrEmpty()) {
                                paths.add(p)
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }

            // 3. Kotlin 框架级物理深度递归检索 (直接突破 MediaStore 尚未建立索引或格式未识别的限制)
            fun scanDir(dir: File, depth: Int) {
                if (depth > 12 || !dir.exists() || !dir.canRead()) return
                val files = dir.listFiles() ?: return
                for (f in files) {
                    val name = f.name
                    if (name.startsWith(".") || name.equals("Android", ignoreCase = true) ||
                        name.equals("DCIM", ignoreCase = true) || name.equals("Pictures", ignoreCase = true) ||
                        name.equals("Movies", ignoreCase = true) || name.equals("cache", ignoreCase = true)) {
                        continue
                    }
                    if (f.isDirectory) {
                        scanDir(f, depth + 1)
                    } else if (f.isFile) {
                        val dotIdx = name.lastIndexOf('.')
                        if (dotIdx > 0 && dotIdx < name.length - 1) {
                            val ext = name.substring(dotIdx + 1).lowercase()
                            if (supportedExts.contains(ext)) {
                                paths.add(f.absolutePath)
                            }
                        }
                    }
                }
            }

            val scannedRoots = HashSet<String>()
            val candidateDirs = listOfNotNull(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MUSIC),
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                Environment.getExternalStorageDirectory(),
                File("/storage/emulated/0/Music"),
                File("/storage/emulated/0/Download"),
                File("/storage/emulated/0/HiFi"),
                File("/storage/emulated/0/Audio"),
                File("/storage/emulated/0/QQMusic"),
                File("/storage/emulated/0/CloudMusic"),
                File("/storage/emulated/0/KuGou"),
                File("/storage/emulated/0/BaiduNetdisk"),
                ctx.getExternalFilesDir(Environment.DIRECTORY_MUSIC)
            )

            for (d in candidateDirs) {
                if (d.exists() && scannedRoots.add(d.absolutePath.lowercase())) {
                    scanDir(d, 0)
                }
            }

            // 4. 外置 SD/TF 卡与 USB OTG 存储扫描
            try {
                val storageRoot = File("/storage")
                if (storageRoot.exists() && storageRoot.canRead()) {
                    storageRoot.listFiles()?.forEach { dev ->
                        if (dev.isDirectory && dev.name != "emulated" && dev.name != "self" && !dev.name.startsWith(".")) {
                            if (scannedRoots.add(dev.absolutePath.lowercase())) {
                                scanDir(dev, 0)
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }

            android.util.Log.i("PiHifiScanner", "queryMediaStoreAudioPaths 总计汇集音频项: ${paths.size}")
            return paths.toTypedArray()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        appContext = applicationContext

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
        glSurfaceView.queueEvent {
            glSurfaceView.nativeOnResume()
        }
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
        // 1. 优先使用系统公共音乐目录 (/storage/emulated/0/Music)
        val pubMusic = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MUSIC)
        if (pubMusic != null && (pubMusic.exists() || pubMusic.mkdirs())) {
            return pubMusic.absolutePath
        }

        // 2. 其次使用外部存储根目录 (/storage/emulated/0)
        val extStorage = Environment.getExternalStorageDirectory()
        if (extStorage != null && extStorage.exists()) {
            return extStorage.absolutePath
        }

        // 3. 再次使用应用专属外部存储目录 (位于 /storage/emulated/0/Android/data/com.pihifi.player/files/Music)
        val appExtMusic = getExternalFilesDir(Environment.DIRECTORY_MUSIC)
        if (appExtMusic != null) {
            if (!appExtMusic.exists()) {
                appExtMusic.mkdirs()
            }
            return appExtMusic.absolutePath
        }

        // 4. 保底内部沙盒目录
        val internalMusic = File(filesDir, "Music").apply { mkdirs() }
        return internalMusic.absolutePath
    }

    private fun checkAndRequestPermissions() {
        val permissionsToRequest = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                try {
                    android.widget.Toast.makeText(
                        this,
                        "请在设置中开启「授予所有文件管理权限」，以便检索与播放本地无损 FLAC / WAV / DSD 发烧音乐",
                        android.widget.Toast.LENGTH_LONG
                    ).show()
                    val intent = android.content.Intent(android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
                        data = android.net.Uri.parse("package:$packageName")
                    }
                    startActivity(intent)
                } catch (e: Exception) {
                    try {
                        val intent = android.content.Intent(android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
                        startActivity(intent)
                    } catch (e2: Exception) {
                        e2.printStackTrace()
                    }
                }
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_MEDIA_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {
                permissionsToRequest.add(Manifest.permission.READ_MEDIA_AUDIO)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_MEDIA_IMAGES)
                != PackageManager.PERMISSION_GRANTED) {
                permissionsToRequest.add(Manifest.permission.READ_MEDIA_IMAGES)
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
