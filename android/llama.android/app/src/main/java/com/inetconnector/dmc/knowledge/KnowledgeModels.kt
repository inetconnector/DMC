package com.inetconnector.dmc.knowledge

import org.json.JSONObject
import java.text.Normalizer
import java.util.Locale

enum class KnowledgeModuleKind(val wireName: String) {
    ICD10("icd10"),
    ICD11("icd11"),
    GENERIC("generic");

    companion object {
        fun fromWireName(value: String): KnowledgeModuleKind =
            entries.firstOrNull { it.wireName == value.lowercase(Locale.ROOT) }
                ?: throw IllegalArgumentException("Unsupported knowledge module type: $value")
    }
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
    val enabled: Boolean = true,
    val recordCount: Int = 0,
    val installedAt: Long = System.currentTimeMillis(),
    val checksum: String = ""
) {
    fun toJson(): JSONObject = JSONObject()
        .put("id", id)
        .put("name", name)
        .put("type", kind.wireName)
        .put("version", version)
        .put("language", language)
        .put("jurisdiction", jurisdiction)
        .put("sourceName", sourceName)
        .put("sourceUrl", sourceUrl)
        .put("licenseName", licenseName)
        .put("licenseUrl", licenseUrl)
        .put("attributionText", attributionText)
        .put("termsAcceptedAt", termsAcceptedAt)
        .put("enabled", enabled)
        .put("recordCount", recordCount)
        .put("installedAt", installedAt)
        .put("checksum", checksum)
}

data class KnowledgeRecord(
    val id: String,
    val code: String,
    val title: String,
    val definition: String = "",
    val inclusions: String = "",
    val exclusions: String = "",
    val parents: String = "",
    val children: String = "",
    val uri: String = "",
    val payloadJson: String = ""
) {
    fun searchableText(): String = KnowledgeText.normalize(
        listOf(code, title, definition, inclusions, exclusions, parents)
            .filter { it.isNotBlank() }
            .joinToString(" ")
    )
}

data class KnowledgeHit(
    val module: KnowledgeModule,
    val record: KnowledgeRecord,
    val score: Int
)

data class KnowledgeImportResult(
    val module: KnowledgeModule,
    val replacedExisting: Boolean
)

object KnowledgeText {
    private val whitespace = Regex("\\s+")
    private val punctuation = Regex("[^\\p{L}\\p{N}.]+")
    private val codePattern = Regex("(?i)\\b[A-Z][0-9]{2}(?:\\.[0-9A-Z]{1,4})?\\b")
    private val stopWords = setOf(
        // Conversational and grammatical words must never activate a large reference lookup.
        "hallo", "hello", "hi", "hey", "hola", "bonjour", "salut", "ciao", "ola",
        "bitte", "please", "danke", "thanks", "merci", "gracias", "nur", "only",
        "antworte", "antwort", "answer", "sage", "sag", "tell", "zeige", "zeig", "show",
        "erklaere", "explain",
        "der", "die", "das", "den", "dem", "des", "ein", "eine", "einer", "einem", "einen",
        "und", "oder", "ist", "sind", "war", "waren", "wie", "was", "wer", "wo", "wann",
        "warum", "wieso", "weshalb", "zum", "zur", "von", "vom", "mit", "ohne", "fur",
        "auf", "bei", "als", "ich", "mich", "mir", "mein", "du", "dein", "wir", "ihr",
        "the", "and", "are", "were", "what", "who", "where", "when", "why", "how", "from",
        "with", "without", "for", "into", "that", "this", "you", "your", "they", "their",
        "les", "des", "une", "avec", "sans", "pour", "dans", "que", "qui", "quoi",
        "los", "las", "una", "con", "sin", "para", "que", "como",
        "gli", "una", "con", "senza", "per", "che", "come",
        "een", "met", "zonder", "voor", "wat", "hoe",
        "uma", "com", "sem", "para", "que", "como"
    )

    fun normalize(value: String): String = Normalizer.normalize(value, Normalizer.Form.NFKD)
        .replace(Regex("\\p{M}+"), "")
        .lowercase(Locale.ROOT)
        .replace(punctuation, " ")
        .replace(whitespace, " ")
        .trim()

    fun queryTerms(value: String): List<String> = normalize(value)
        .split(' ')
        .asSequence()
        .map { it.trim('.') }
        .filter { it.length >= 3 }
        .filterNot { it in stopWords }
        .distinct()
        .sortedByDescending { it.length }
        .take(12)
        .toList()

    fun codes(value: String): Set<String> = codePattern.findAll(value)
        .map { it.value.uppercase(Locale.ROOT) }
        .toSet()

    fun tokens(value: String): Set<String> = normalize(value)
        .split(' ')
        .asSequence()
        .map { it.trim('.') }
        .filter { it.isNotBlank() }
        .toSet()
}
