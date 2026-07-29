package com.inetconnector.dmc.billing

import android.content.Context
import android.os.SystemClock
import android.provider.Settings
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets
import java.security.MessageDigest
import java.util.Locale

data class TrialStatus(
    val active: Boolean,
    val startedAtMillis: Long,
    val expiresAtMillis: Long,
    val serverTimeMillis: Long
)

class TrialAccessService(
    context: Context,
    private val apiBaseUrl: String
) {
    private val appContext = context.applicationContext
    private val prefs = appContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun hasBeenActivated(): Boolean = prefs.getBoolean(KEY_ACTIVATED, false)

    fun cachedStatus(): TrialStatus? {
        if (!hasBeenActivated()) return null
        val started = prefs.getLong(KEY_STARTED_AT, 0L)
        val expires = prefs.getLong(KEY_EXPIRES_AT, 0L)
        val verifiedServerTime = prefs.getLong(KEY_SERVER_TIME, 0L)
        val verifiedElapsed = prefs.getLong(KEY_ELAPSED_REALTIME, 0L)
        if (started <= 0L || expires <= started || verifiedServerTime <= 0L) return null

        val elapsedNow = SystemClock.elapsedRealtime()
        val elapsedDelta = elapsedNow - verifiedElapsed
        val estimatedNow = if (elapsedDelta >= 0L) {
            maxOf(System.currentTimeMillis(), verifiedServerTime + elapsedDelta)
        } else {
            System.currentTimeMillis()
        }
        if (estimatedNow + CLOCK_ROLLBACK_TOLERANCE_MS < verifiedServerTime) return null
        return TrialStatus(
            active = estimatedNow < expires,
            startedAtMillis = started,
            expiresAtMillis = expires,
            serverTimeMillis = estimatedNow
        )
    }

    fun activateOrRefresh(appVersion: String): TrialStatus {
        val request = JSONObject()
            .put("installationId", installationId())
            .put("appVersion", appVersion.take(64))
            .put("locale", Locale.getDefault().toLanguageTag().take(35))
        val response = postJson("$apiBaseUrl/trial/status", request)
        val status = TrialStatus(
            active = response.getBoolean("active"),
            startedAtMillis = response.getLong("startedAt"),
            expiresAtMillis = response.getLong("expiresAt"),
            serverTimeMillis = response.getLong("serverTime")
        )
        prefs.edit()
            .putBoolean(KEY_ACTIVATED, true)
            .putLong(KEY_STARTED_AT, status.startedAtMillis)
            .putLong(KEY_EXPIRES_AT, status.expiresAtMillis)
            .putLong(KEY_SERVER_TIME, status.serverTimeMillis)
            .putLong(KEY_ELAPSED_REALTIME, SystemClock.elapsedRealtime())
            .apply()
        return status
    }

    fun submitReport(
        reason: String,
        excerpt: String,
        appVersion: String
    ): String {
        val request = JSONObject()
            .put("reason", reason.take(48))
            .put("excerpt", excerpt.trim().take(4_000))
            .put("appVersion", appVersion.take(64))
            .put("locale", Locale.getDefault().toLanguageTag().take(35))
        val response = postJson("$apiBaseUrl/reports", request)
        return response.getString("reportId")
    }

    private fun installationId(): String {
        val androidId = Settings.Secure.getString(
            appContext.contentResolver,
            Settings.Secure.ANDROID_ID
        ).orEmpty()
        val source = "${appContext.packageName}:$androidId"
        return MessageDigest.getInstance("SHA-256")
            .digest(source.toByteArray(StandardCharsets.UTF_8))
            .joinToString("") { "%02x".format(it.toInt() and 0xff) }
    }

    private fun postJson(url: String, body: JSONObject): JSONObject {
        val connection = (URL(url).openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            connectTimeout = 10_000
            readTimeout = 15_000
            doOutput = true
            instanceFollowRedirects = false
            setRequestProperty("Content-Type", "application/json; charset=utf-8")
            setRequestProperty("Accept", "application/json")
            setRequestProperty("User-Agent", "InetMind-Android")
        }
        return try {
            connection.outputStream.use {
                it.write(body.toString().toByteArray(StandardCharsets.UTF_8))
            }
            val stream = if (connection.responseCode in 200..299) {
                connection.inputStream
            } else {
                connection.errorStream
            }
            val responseText = stream?.bufferedReader(StandardCharsets.UTF_8)?.use { it.readText() }
                .orEmpty()
            if (connection.responseCode !in 200..299) {
                val message = runCatching {
                    JSONObject(responseText).optString("error")
                }.getOrNull().orEmpty()
                throw IllegalStateException(
                    message.ifBlank { "Server error (${connection.responseCode})" }
                )
            }
            JSONObject(responseText)
        } finally {
            connection.disconnect()
        }
    }

    companion object {
        private const val PREFS_NAME = "play_access"
        private const val KEY_ACTIVATED = "trial_activated"
        private const val KEY_STARTED_AT = "trial_started_at"
        private const val KEY_EXPIRES_AT = "trial_expires_at"
        private const val KEY_SERVER_TIME = "trial_server_time"
        private const val KEY_ELAPSED_REALTIME = "trial_elapsed_realtime"
        private const val CLOCK_ROLLBACK_TOLERANCE_MS = 5L * 60L * 1000L
    }
}
