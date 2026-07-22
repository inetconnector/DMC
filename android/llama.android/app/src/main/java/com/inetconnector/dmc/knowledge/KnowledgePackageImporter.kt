package com.inetconnector.dmc.knowledge

import android.content.Context
import android.net.Uri
import org.json.JSONArray
import org.json.JSONObject
import org.xmlpull.v1.XmlPullParser
import org.xmlpull.v1.XmlPullParserFactory
import java.io.BufferedInputStream
import java.io.File
import java.io.FileInputStream
import java.io.FilterInputStream
import java.io.InputStream
import java.net.URI
import java.security.DigestInputStream
import java.security.MessageDigest
import java.util.Locale
import java.util.zip.ZipFile

class KnowledgePackageImporter(
    private val context: Context,
    private val store: KnowledgeModuleStore
) {
    companion object {
        private const val MAX_IMPORT_BYTES = 1024L * 1024L * 1024L
        private const val MAX_MANIFEST_BYTES = 1024L * 1024L
        private const val MAX_ZIP_ENTRIES = 64
        private const val MANIFEST_NAME = "manifest.json"
        private const val RECORDS_NAME = "records.jsonl"
        private const val BFARM_SOURCE =
            "https://www.bfarm.de/DE/Kodiersysteme/Klassifikationen/ICD/ICD-10-GM/_node.html"
        private const val BFARM_LICENSE =
            "https://www.bfarm.de/SharedDocs/Downloads/DE/Kodiersysteme/downloadbedingungen-2025.pdf?__blob=publicationFile"
        private const val BFARM_ATTRIBUTION =
            "Herausgegeben vom Bundesinstitut für Arzneimittel und Medizinprodukte (BfArM) im Auftrag des Bundesministeriums für Gesundheit (BMG). Die Erstellung bzw. der Druck erfolgt unter Verwendung der maschinenlesbaren Fassung des Bundesinstituts für Arzneimittel und Medizinprodukte (BfArM)."
    }

    fun import(uri: Uri, termsAcceptedAt: Long): KnowledgeImportResult {
        require(termsAcceptedAt > 0) { "License terms must be accepted before import" }
        val displayName = queryDisplayName(uri).ifBlank { "knowledge-module" }
        val temporary = File.createTempFile("knowledge-import-", ".tmp", context.cacheDir)
        return try {
            val checksum = copyAndHash(uri, temporary)
            importFile(temporary, displayName, checksum, termsAcceptedAt)
        } finally {
            temporary.delete()
        }
    }

    internal fun importFile(
        file: File,
        displayName: String,
        checksum: String,
        termsAcceptedAt: Long
    ): KnowledgeImportResult = when {
        isZip(file) -> importZip(file, displayName, checksum, termsAcceptedAt)
        looksLikeXml(file, displayName) -> importClaml(
            FileInputStream(file), displayName, checksum, termsAcceptedAt
        )
        else -> throw IllegalArgumentException(
            "Unsupported file. Select an official ClaML XML/ZIP or a validated .dmcknowledge package."
        )
    }

    private fun importZip(
        file: File,
        displayName: String,
        checksum: String,
        termsAcceptedAt: Long
    ): KnowledgeImportResult = ZipFile(file).use { zip ->
        val entries = zip.entries().toList()
        require(entries.size <= MAX_ZIP_ENTRIES) { "Knowledge package contains too many files" }
        entries.forEach { entry ->
            require(!entry.name.contains("..") && !entry.name.startsWith('/') && !entry.name.startsWith('\\')) {
                "Unsafe path in knowledge package"
            }
            require(entry.size in -1..MAX_IMPORT_BYTES) { "Knowledge package entry is too large" }
        }

        val manifestEntry = entries.firstOrNull { it.name.substringAfterLast('/') == MANIFEST_NAME }
        if (manifestEntry != null) {
            val recordsEntry = entries.firstOrNull { it.name.substringAfterLast('/') == RECORDS_NAME }
                ?: throw IllegalArgumentException("Knowledge package is missing $RECORDS_NAME")
            val manifest = zip.openLimited(manifestEntry, MAX_MANIFEST_BYTES)
                .bufferedReader()
                .use { JSONObject(it.readText()) }
            val module = parseManifest(manifest, checksum, termsAcceptedAt)
            zip.openLimited(recordsEntry, MAX_IMPORT_BYTES).bufferedReader().use { reader ->
                val records = reader.lineSequence()
                    .filter { it.isNotBlank() }
                    .mapIndexed { index, line -> parseRecord(JSONObject(line), index + 1, module.kind) }
                val declaredCount = manifest.optInt("recordCount", 0)
                store.replaceModule(module, records, declaredCount.takeIf { it > 0 })
            }
        } else {
            val clamlEntry = entries.asSequence()
                .filter { !it.isDirectory && it.name.lowercase(Locale.ROOT).endsWith(".xml") }
                .firstOrNull { entry -> zip.getInputStream(entry).buffered().use(::hasClamlRoot) }
                ?: throw IllegalArgumentException(
                    "Archive is neither a knowledge package nor an official ClaML archive"
                )
            zip.openLimited(clamlEntry, MAX_IMPORT_BYTES).use { stream ->
                importClaml(stream, clamlEntry.name.ifBlank { displayName }, checksum, termsAcceptedAt)
            }
        }
    }

    private fun hasClamlRoot(input: InputStream): Boolean {
        val prefix = ByteArray(16 * 1024)
        val count = input.read(prefix).coerceAtLeast(0)
        val text = String(prefix, 0, count, Charsets.UTF_8)
        return Regex("<(?:[A-Za-z_][\\w.-]*:)?ClaML(?:\\s|>)").containsMatchIn(text)
    }

    private fun importClaml(
        input: InputStream,
        displayName: String,
        checksum: String,
        termsAcceptedAt: Long
    ): KnowledgeImportResult {
        val version = Regex("20[0-9]{2}").find(displayName)?.value ?: "unknown"
        val module = KnowledgeModule(
            id = "icd10-gm-$version-de",
            name = "ICD-10-GM $version",
            kind = KnowledgeModuleKind.ICD10,
            version = version,
            language = "de",
            jurisdiction = "DE",
            sourceName = "Bundesinstitut für Arzneimittel und Medizinprodukte (BfArM)",
            sourceUrl = BFARM_SOURCE,
            licenseName = "BfArM Downloadbedingungen für ICD-10-GM",
            licenseUrl = BFARM_LICENSE,
            attributionText = BFARM_ATTRIBUTION,
            termsAcceptedAt = termsAcceptedAt,
            checksum = checksum
        )
        return input.buffered().use { store.replaceModule(module, parseClamlRecords(it)) }
    }

    private fun parseManifest(
        json: JSONObject,
        checksum: String,
        termsAcceptedAt: Long
    ): KnowledgeModule {
        require(json.optInt("formatVersion", -1) == 1) { "Unsupported knowledge package format" }
        val kind = KnowledgeModuleKind.fromWireName(json.required("type"))
        if (kind != KnowledgeModuleKind.GENERIC) {
            require(json.optBoolean("officialContentUnmodified", false)) {
                "Official classification packages must declare unmodified content"
            }
        }
        val module = KnowledgeModule(
            id = json.required("id").lowercase(Locale.ROOT),
            name = json.required("name"),
            kind = kind,
            version = json.required("version"),
            language = json.required("language"),
            jurisdiction = json.optString("jurisdiction", "international").ifBlank { "international" },
            sourceName = json.required("sourceName"),
            sourceUrl = json.required("sourceUrl"),
            licenseName = json.required("licenseName"),
            licenseUrl = json.required("licenseUrl"),
            attributionText = json.required("attributionText"),
            termsAcceptedAt = termsAcceptedAt,
            checksum = checksum
        )
        validateOfficialMetadata(module)
        return module
    }

    private fun validateOfficialMetadata(module: KnowledgeModule) {
        when (module.kind) {
            KnowledgeModuleKind.ICD10 -> {
                require(officialHost(module.sourceUrl, "bfarm.de")) { "ICD-10-GM source must be BfArM" }
                require(officialHost(module.licenseUrl, "bfarm.de")) { "ICD-10-GM license must be BfArM" }
                require(module.attributionText.contains("BfArM")) { "BfArM attribution is required" }
            }
            KnowledgeModuleKind.ICD11 -> {
                require(officialHost(module.sourceUrl, "who.int")) { "ICD-11 source must be WHO" }
                require(officialHost(module.licenseUrl, "who.int")) { "ICD-11 license must be WHO" }
                require(module.licenseName.contains("CC BY-ND 3.0 IGO", ignoreCase = true)) {
                    "ICD-11 must retain the CC BY-ND 3.0 IGO license"
                }
                require(module.attributionText.contains("World Health Organization", ignoreCase = true)) {
                    "WHO attribution is required"
                }
            }
            KnowledgeModuleKind.GENERIC -> Unit
        }
    }

    private fun parseRecord(json: JSONObject, lineNumber: Int, kind: KnowledgeModuleKind): KnowledgeRecord {
        val code = json.optString("code").trim()
        val id = json.optString("id").trim().ifBlank { code }
        val uri = json.optString("uri").trim()
        require(id.isNotBlank()) { "Record on line $lineNumber has neither id nor code" }
        if (kind == KnowledgeModuleKind.ICD11) {
            require(code.isNotBlank()) { "ICD-11 record on line $lineNumber is missing its code" }
            require(officialHost(uri, "id.who.int")) {
                "ICD-11 record on line $lineNumber is missing its official WHO URI"
            }
        }
        return KnowledgeRecord(
            id = id,
            code = code,
            title = json.required("title"),
            definition = json.valueAsText("definition"),
            inclusions = json.valueAsText("inclusions"),
            exclusions = json.valueAsText("exclusions"),
            parents = json.valueAsText("parents"),
            children = json.valueAsText("children"),
            uri = uri,
            payloadJson = json.optJSONObject("metadata")?.toString().orEmpty()
        )
    }

    private fun parseClamlRecords(input: InputStream): Sequence<KnowledgeRecord> = sequence {
        val parser = XmlPullParserFactory.newInstance().newPullParser().apply { setInput(input, "UTF-8") }
        var currentCode = ""
        var currentKind = ""
        var parentCode = ""
        var rubricKind = ""
        var labelDepth = -1
        var labelText: StringBuilder? = null
        val preferred = mutableListOf<String>()
        val definitions = mutableListOf<String>()
        val inclusions = mutableListOf<String>()
        val exclusions = mutableListOf<String>()

        fun resetClass() {
            currentCode = ""; currentKind = ""; parentCode = ""; rubricKind = ""
            labelDepth = -1; labelText = null
            preferred.clear(); definitions.clear(); inclusions.clear(); exclusions.clear()
        }

        var event = parser.eventType
        while (event != XmlPullParser.END_DOCUMENT) {
            when (event) {
                XmlPullParser.START_TAG -> when (parser.name.substringAfter(':')) {
                    "Class" -> {
                        resetClass()
                        currentCode = parser.getAttributeValue(null, "code").orEmpty().trim()
                        currentKind = parser.getAttributeValue(null, "kind").orEmpty().trim()
                    }
                    "SuperClass" -> parentCode = parser.getAttributeValue(null, "code").orEmpty().trim()
                    "Rubric" -> rubricKind = parser.getAttributeValue(null, "kind").orEmpty().trim()
                    "Label" -> { labelDepth = parser.depth; labelText = StringBuilder() }
                }
                XmlPullParser.TEXT, XmlPullParser.CDSECT -> if (labelDepth >= 0) labelText?.append(parser.text)
                XmlPullParser.END_TAG -> when (parser.name.substringAfter(':')) {
                    "Label" -> {
                        val text = labelText.toString().replace(Regex("\\s+"), " ").trim()
                        if (text.isNotBlank()) when (rubricKind.lowercase(Locale.ROOT)) {
                            "preferred" -> preferred += text
                            "inclusion" -> inclusions += text
                            "exclusion" -> exclusions += text
                            "definition", "note", "coding-hint", "introduction" -> definitions += text
                        }
                        labelDepth = -1; labelText = null
                    }
                    "Rubric" -> rubricKind = ""
                    "Class" -> if (currentCode.isNotBlank()) {
                        yield(
                            KnowledgeRecord(
                                id = currentCode,
                                code = currentCode,
                                title = preferred.firstOrNull().orEmpty().ifBlank {
                                    "$currentKind $currentCode".trim()
                                },
                                definition = definitions.joinToString(" | "),
                                inclusions = inclusions.joinToString(" | "),
                                exclusions = exclusions.joinToString(" | "),
                                parents = parentCode
                            )
                        )
                    }
                }
            }
            event = parser.next()
        }
    }

    private fun copyAndHash(uri: Uri, destination: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        val input = context.contentResolver.openInputStream(uri)
            ?: throw IllegalArgumentException("Selected file cannot be opened")
        var total = 0L
        DigestInputStream(BufferedInputStream(input), digest).use { source ->
            destination.outputStream().buffered().use { target ->
                val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                while (true) {
                    val read = source.read(buffer)
                    if (read < 0) break
                    total += read
                    require(total <= MAX_IMPORT_BYTES) { "Knowledge package exceeds 1 GiB" }
                    target.write(buffer, 0, read)
                }
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }

    private fun isZip(file: File): Boolean = FileInputStream(file).use { input ->
        val signature = ByteArray(4)
        input.read(signature) == 4 && signature.contentEquals(byteArrayOf(0x50, 0x4b, 0x03, 0x04))
    }

    private fun looksLikeXml(file: File, displayName: String): Boolean {
        if (displayName.lowercase(Locale.ROOT).endsWith(".xml")) return true
        return FileInputStream(file).buffered().use { input ->
            val prefix = ByteArray(256)
            val count = input.read(prefix).coerceAtLeast(0)
            String(prefix, 0, count, Charsets.UTF_8).trimStart().startsWith("<")
        }
    }

    private fun queryDisplayName(uri: Uri): String {
        context.contentResolver.query(
            uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null
        )?.use { cursor -> if (cursor.moveToFirst()) return cursor.getString(0).orEmpty() }
        return uri.lastPathSegment.orEmpty()
    }

    private fun officialHost(url: String, expected: String): Boolean = runCatching {
        val host = URI(url).host.orEmpty().lowercase(Locale.ROOT)
        host == expected || host.endsWith(".$expected")
    }.getOrDefault(false)

    private fun ZipFile.openLimited(
        entry: java.util.zip.ZipEntry,
        maxBytes: Long
    ): InputStream = SizeLimitedInputStream(getInputStream(entry), maxBytes)

    private fun JSONObject.required(key: String): String = optString(key).trim().also {
        require(it.isNotBlank()) { "Knowledge package manifest is missing $key" }
    }

    private fun JSONObject.valueAsText(key: String): String = when (val value = opt(key)) {
        is JSONArray -> buildList {
            for (index in 0 until value.length()) {
                value.optString(index).trim().takeIf { it.isNotBlank() }?.let(::add)
            }
        }.joinToString(" | ")
        is String -> value.trim()
        null, JSONObject.NULL -> ""
        else -> value.toString().trim()
    }

    private class SizeLimitedInputStream(
        input: InputStream,
        private val maxBytes: Long
    ) : FilterInputStream(input) {
        private var bytesRead = 0L

        override fun read(): Int = super.read().also { value ->
            if (value >= 0) account(1)
        }

        override fun read(buffer: ByteArray, offset: Int, length: Int): Int =
            super.read(buffer, offset, length).also { count ->
                if (count > 0) account(count.toLong())
            }

        private fun account(count: Long) {
            bytesRead += count
            require(bytesRead <= maxBytes) { "Expanded knowledge package entry exceeds the size limit" }
        }
    }
}
