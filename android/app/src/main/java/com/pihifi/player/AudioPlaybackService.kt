package com.pihifi.player

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat

class AudioPlaybackService : Service() {

    companion object {
        private const val TAG = "AudioPlaybackService"
        const val CHANNEL_ID = "pihifi_playback_channel"
        const val NOTIFICATION_ID = 1001

        const val ACTION_START = "com.pihifi.player.ACTION_START"
        const val ACTION_PLAY_PAUSE = "com.pihifi.player.ACTION_PLAY_PAUSE"
        const val ACTION_NEXT = "com.pihifi.player.ACTION_NEXT"
        const val ACTION_PREV = "com.pihifi.player.ACTION_PREV"

        @JvmStatic
        external fun nativePlay()

        @JvmStatic
        external fun nativePause()

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
    private var audioManager: AudioManager? = null
    private var audioFocusRequest: AudioFocusRequest? = null
    private var hasAudioFocus = false
    private var wasPlayingBeforeFocusLoss = false
    private var lastObservedPlaying = false

    private val noisyReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == AudioManager.ACTION_AUDIO_BECOMING_NOISY) {
                Log.i(TAG, "耳机/外置DAC断开连接，暂停音频播放")
                if (nativeIsPlaying()) {
                    wasPlayingBeforeFocusLoss = false
                    nativePause()
                    abandonAudioFocus()
                    updateNotification()
                }
            }
        }
    }

    private val audioFocusChangeListener = AudioManager.OnAudioFocusChangeListener { focusChange ->
        when (focusChange) {
            AudioManager.AUDIOFOCUS_LOSS -> {
                Log.i(TAG, "永久失去音频焦点 (其他应用开始播放)")
                wasPlayingBeforeFocusLoss = false
                hasAudioFocus = false
                if (nativeIsPlaying()) {
                    nativePause()
                    updateNotification()
                }
            }
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT,
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> {
                Log.i(TAG, "临时失去音频焦点 (来电/通话接入)，暂停播放并等待挂断恢复")
                if (nativeIsPlaying()) {
                    wasPlayingBeforeFocusLoss = true
                    nativePause()
                    updateNotification()
                }
            }
            AudioManager.AUDIOFOCUS_GAIN -> {
                Log.i(TAG, "重新获取音频焦点 (来电已挂断/焦点归还)，wasPlayingBeforeFocusLoss=$wasPlayingBeforeFocusLoss")
                hasAudioFocus = true
                if (wasPlayingBeforeFocusLoss) {
                    wasPlayingBeforeFocusLoss = false
                    nativePlay()
                    updateNotification()
                }
            }
        }
    }

    private fun requestAudioFocus(): Boolean {
        if (hasAudioFocus) return true
        val am = audioManager ?: return false
        val res = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (audioFocusRequest == null) {
                val attrs = AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build()
                audioFocusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(attrs)
                    .setAcceptsDelayedFocusGain(true)
                    .setOnAudioFocusChangeListener(audioFocusChangeListener, handler)
                    .build()
            }
            am.requestAudioFocus(audioFocusRequest!!)
        } else {
            @Suppress("DEPRECATION")
            am.requestAudioFocus(
                audioFocusChangeListener,
                AudioManager.STREAM_MUSIC,
                AudioManager.AUDIOFOCUS_GAIN
            )
        }
        hasAudioFocus = (res == AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
        Log.i(TAG, "申请系统音频焦点结果: granted=$hasAudioFocus")
        return hasAudioFocus
    }

    private fun abandonAudioFocus() {
        val am = audioManager ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            audioFocusRequest?.let { am.abandonAudioFocusRequest(it) }
        } else {
            @Suppress("DEPRECATION")
            am.abandonAudioFocus(audioFocusChangeListener)
        }
        hasAudioFocus = false
        Log.i(TAG, "释放系统音频焦点")
    }

    private val syncRunnable = object : Runnable {
        override fun run() {
            val isPlaying = nativeIsPlaying()
            if (isPlaying && !lastObservedPlaying) {
                if (!hasAudioFocus) {
                    requestAudioFocus()
                }
            } else if (!isPlaying && lastObservedPlaying && !wasPlayingBeforeFocusLoss) {
                abandonAudioFocus()
            }
            lastObservedPlaying = isPlaying

            updateNotification()
            handler.postDelayed(this, 1000)
        }
    }

    override fun onCreate() {
        super.onCreate()
        audioManager = getSystemService(Context.AUDIO_SERVICE) as? AudioManager
        try {
            ContextCompat.registerReceiver(
                this,
                noisyReceiver,
                IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY),
                ContextCompat.RECEIVER_NOT_EXPORTED
            )
        } catch (_: Exception) {
            registerReceiver(noisyReceiver, IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY))
        }

        createNotificationChannel()
        startForeground(NOTIFICATION_ID, buildNotification())
        handler.post(syncRunnable)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_PLAY_PAUSE -> {
                if (nativeIsPlaying()) {
                    wasPlayingBeforeFocusLoss = false
                    nativePause()
                    abandonAudioFocus()
                } else {
                    requestAudioFocus()
                    nativePlay()
                }
                updateNotification()
            }
            ACTION_NEXT -> {
                requestAudioFocus()
                nativeNextTrack()
                updateNotification()
            }
            ACTION_PREV -> {
                requestAudioFocus()
                nativePrevTrack()
                updateNotification()
            }
        }
        return START_STICKY
    }

    override fun onDestroy() {
        handler.removeCallbacks(syncRunnable)
        try {
            unregisterReceiver(noisyReceiver)
        } catch (_: Exception) {}
        abandonAudioFocus()
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
