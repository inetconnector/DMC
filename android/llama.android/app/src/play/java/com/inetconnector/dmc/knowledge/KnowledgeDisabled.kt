package com.inetconnector.dmc.knowledge

import android.content.Context
import android.net.Uri
import org.json.JSONObject
import java.io.File

enum class KnowledgeModuleKind(val wireName: String) {
    GENERIC("generic")
}

data class KnowledgeModule(
    val id: String,
    val name: String,
    val kind: KnowledgeModuleKind,
    val version: String,
    val language: String,
    val jurisdiction: String,
    val sourceName: String,
    val sourceUrl: String,
    val licenseName: String,
    val licenseUrl: String,
    val attributionText: String,
    val termsAcceptedAt: Long,
    val enabled: Boolean = false,
    val recordCount: Int = 0,
    val installedAt: Long = 0L,
    val checksum: String = ""
) {
    fun toJson(): JSONObject = JSONObject()
}

data class KnowledgeImportResult(
    val module: KnowledgeModule,
    val replacedExisting: Boolean
)

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

class KnowledgeModuleStore(context: Context) {
    fun listModules(): List<KnowledgeModule> = emptyList()
    fun setEnabled(moduleId: String, enabled: Boolean) = Unit
    fun deleteModule(moduleId: String): Boolean = false
    fun buildEvidenceContext(query: String): String = ""
}

class KnowledgePackageImporter(
    context: Context,
    store: KnowledgeModuleStore
) {
    fun import(uri: Uri, termsAcceptedAt: Long): KnowledgeImportResult =
        disabled()

    internal fun importFile(
        packageFile: File,
        displayName: String,
        expectedChecksum: String,
        termsAcceptedAt: Long
    ): KnowledgeImportResult = disabled()

    private fun disabled(): Nothing =
        throw UnsupportedOperationException("Offline reference modules are not included in this build.")
}

class KnowledgeSourceCatalog(context: Context) {
    fun load(): List<KnowledgeSource> = emptyList()
}
