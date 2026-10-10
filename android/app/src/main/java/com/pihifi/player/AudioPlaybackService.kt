package com.pihifi.player

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import androidx.core.app.NotificationCompat

class AudioPlaybackService : Service() {

    companion object {
        const val CHANNEL_ID = "pihifi_playback_channel"
        const val NOTIFICATION_ID = 1001

        const val ACTION_START = "com.pihifi.player.ACTION_START"
        const val ACTION_PLAY_PAUSE = "com.pihifi.player.ACTION_PLAY_PAUSE"
        const val ACTION_NEXT = "com.pihifi.player.ACTION_NEXT"
        const val ACTION_PREV = "com.pihifi.player.ACTION_PREV"

        @JvmStatic
        external fun nativeTogglePlayPause()

        @JvmStatic
        external fun nativeNextTrack()

        @JvmStatic
        external fun nativePrevTrack()

        @JvmStatic
        external fun nativeIsPlaying(): Boolean

        @JvmStatic
        external fun nativeGetCurrentTrackTitle(): String

        @JvmStatic
        external fun nativeGetCurrentTrackArtist(): String

        @JvmStatic
        external fun nativeGetCurrentPositionMs(): Long

        @JvmStatic
        external fun nativeGetDurationMs(): Long

        fun startService(context: Context) {
            val intent = Intent(context, AudioPlaybackService::class.java).apply {
                action = ACTION_START
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }
    }

    private val handler = Handler(Looper.getMainLooper())
    private val syncRunnable = object : Runnable {
        override fun run() {
            updateNotification()
            handler.postDelayed(this, 1000)
        }
    }

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        startForeground(NOTIFICATION_ID, buildNotification())
        handler.post(syncRunnable)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_PLAY_PAUSE -> {
                nativeTogglePlayPause()
                updateNotification()
            }
            ACTION_NEXT -> {
                nativeNextTrack()
                updateNotification()
            }
            ACTION_PREV -> {
                nativePrevTrack()
                updateNotification()
            }
        }
        return START_STICKY
    }

    override fun onDestroy() {
        handler.removeCallbacks(syncRunnable)
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "PiHiEndMusic 播放服务",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "保持发烧音频后台常驻与系统锁屏播控"
                setShowBadge(false)
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager?.createNotificationChannel(channel)
        }
    }

    private fun buildNotification(): Notification {
        val isPlaying = nativeIsPlaying()
        val title = nativeGetCurrentTrackTitle()
        val artist = nativeGetCurrentTrackArtist()

        val contentIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val prevIntent = PendingIntent.getService(
            this, 1,
            Intent(this, AudioPlaybackService::class.java).apply { action = ACTION_PREV },
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val playPauseIntent = PendingIntent.getService(
            this, 2,
            Intent(this, AudioPlaybackService::class.java).apply { action = ACTION_PLAY_PAUSE },
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val nextIntent = PendingIntent.getService(
            this, 3,
            Intent(this, AudioPlaybackService::class.java).apply { action = ACTION_NEXT },
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val playPauseIcon = if (isPlaying) {
            android.R.drawable.ic_media_pause
        } else {
            android.R.drawable.ic_media_play
        }

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(title)
            .setContentText(artist)
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setContentIntent(contentIntent)
            .setOngoing(isPlaying)
            .setOnlyAlertOnce(true)
            .addAction(android.R.drawable.ic_media_previous, "上一首", prevIntent)
            .addAction(playPauseIcon, if (isPlaying) "暂停" else "播放", playPauseIntent)
            .addAction(android.R.drawable.ic_media_next, "下一首", nextIntent)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .build()
    }

    private fun updateNotification() {
        val manager = getSystemService(NotificationManager::class.java)
        manager?.notify(NOTIFICATION_ID, buildNotification())
    }
}
