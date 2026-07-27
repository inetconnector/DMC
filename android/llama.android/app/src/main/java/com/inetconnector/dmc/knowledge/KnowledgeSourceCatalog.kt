package com.inetconnector.dmc.knowledge

import android.content.Context
import org.json.JSONObject

data class KnowledgeSource(
    val id: String,
    val name: String,
    val publisher: String,
    val category: String,
    val description: String,
    val officialUrl: String,
    val licenseUrl: String,
    val access: String,
    val importMode: String,
    val updateCadence: String,
    val defaultSelected: Boolean,
    val android: Boolean
)

class KnowledgeSourceCatalog(private val context: Context) {
    fun load(): List<KnowledgeSource> {
        val root = context.assets.open(ASSET_NAME).bufferedReader(Charsets.UTF_8).use {
            JSONObject(it.readText())
        }
        require(root.getInt("formatVersion") == FORMAT_VERSION) {
            "Unsupported knowledge source catalog"
        }
        val sources = root.getJSONArray("sources")
        return buildList {
            for (index in 0 until sources.length()) {
                val source = sources.getJSONObject(index)
                val parsed = KnowledgeSource(
                    id = source.required("id"),
                    name = source.required("name"),
                    publisher = source.required("publisher"),
                    category = source.required("category"),
                    description = source.required("description"),
                    officialUrl = source.requiredHttps("officialUrl"),
                    licenseUrl = source.requiredHttps("licenseUrl"),
                    access = source.required("access"),
                    importMode = source.required("importMode"),
                    updateCadence = source.required("updateCadence"),
                    defaultSelected = source.getBoolean("defaultSelected"),
                    android = source.getBoolean("android")
                )
                require(parsed.id.matches(Regex("[a-z0-9][a-z0-9._-]{2,95}"))) {
                    "Invalid source id: ${parsed.id}"
                }
                if (parsed.android) add(parsed)
            }
        }.also { loaded ->
            require(loaded.isNotEmpty()) { "Knowledge source catalog is empty" }
            require(loaded.map { it.id }.distinct().size == loaded.size) {
                "Knowledge source catalog contains duplicate ids"
            }
        }
    }

    private fun JSONObject.required(key: String): String =
        getString(key).trim().also { require(it.isNotEmpty()) { "Catalog requires $key" } }

    private fun JSONObject.requiredHttps(key: String): String =
        required(key).also { require(it.startsWith("https://")) { "$key must use HTTPS" } }

    companion object {
        private const val ASSET_NAME = "source-catalog.json"
        private const val FORMAT_VERSION = 1
    }
}
