package com.inetconnector.dmc.knowledge

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.security.MessageDigest
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

@RunWith(AndroidJUnit4::class)
class KnowledgeModuleIntegrationTest {
    private lateinit var context: Context
    private lateinit var store: KnowledgeModuleStore
    private lateinit var importer: KnowledgePackageImporter

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        context.deleteDatabase("offline-knowledge.db")
        store = KnowledgeModuleStore(context)
        importer = KnowledgePackageImporter(context, store)
    }

    @After
    fun tearDown() {
        store.close()
        context.deleteDatabase("offline-knowledge.db")
    }

    @Test
    fun importRetrieveToggleAndDeleteAreIsolated() {
        val packageFile = packageFile(
            listOf(
                record("entity-1", "1A00", "Cholera", "https://id.who.int/icd/entity/1"),
                record("entity-2", "1B10", "Tuberculosis", "https://id.who.int/icd/entity/2")
            )
        )

        val result = importer.importFile(
            packageFile,
            packageFile.name,
            sha256(packageFile),
            System.currentTimeMillis()
        )
        assertEquals(2, result.module.recordCount)
        assertEquals("1A00", store.retrieve("1A00").single().record.code)
        assertEquals("1B10", store.retrieve("tuberculosis").single().record.code)

        store.setEnabled(result.module.id, false)
        assertTrue(store.retrieve("cholera").isEmpty())
        store.setEnabled(result.module.id, true)
        assertFalse(store.retrieve("cholera").isEmpty())

        assertTrue(store.deleteModule(result.module.id))
        assertTrue(store.listModules().isEmpty())
        packageFile.delete()
    }

    @Test
    fun failedReplacementRollsBackExistingModule() {
        val valid = packageFile(
            listOf(record("entity-1", "1A00", "Cholera", "https://id.who.int/icd/entity/1"))
        )
        importer.importFile(valid, valid.name, sha256(valid), System.currentTimeMillis())

        val invalid = packageFile(
            listOf(record("entity-2", "1B10", "", "https://id.who.int/icd/entity/2")),
            version = "2026-02"
        )
        runCatching {
            importer.importFile(invalid, invalid.name, sha256(invalid), System.currentTimeMillis())
        }.onSuccess { throw AssertionError("Invalid replacement was accepted") }

        assertEquals("Cholera", store.retrieve("1A00").single().record.title)
        valid.delete()
        invalid.delete()
    }

    @Test
    fun importsNestedIcd10ClamlXmlAndIgnoresBundledXsd() {
        val archive = File.createTempFile("icd10gm2026syst-claml-", ".zip", context.cacheDir)
        ZipOutputStream(archive.outputStream().buffered()).use { zip ->
            zip.putNextEntry(ZipEntry("Klassifikationsdateien/ClaML2.0.0.xsd"))
            zip.write("<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"/>".toByteArray())
            zip.closeEntry()
            zip.putNextEntry(
                ZipEntry("Klassifikationsdateien/icd10gm2026syst_claml_20250912.xml")
            )
            zip.write(
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <ClaML>
                  <Class code="A00" kind="category">
                    <Rubric kind="preferred"><Label>Cholera</Label></Rubric>
                  </Class>
                </ClaML>
                """.trimIndent().toByteArray()
            )
            zip.closeEntry()
        }

        val result = importer.importFile(
            archive,
            archive.name,
            sha256(archive),
            System.currentTimeMillis()
        )

        assertEquals(KnowledgeModuleKind.ICD10, result.module.kind)
        assertEquals("2026", result.module.version)
        assertEquals("A00", store.retrieve("Cholera").single().record.code)
        archive.delete()
    }

    @Test
    fun casualGreetingDoesNotPrefixMatchHallopeau() {
        val packageFile = packageFile(
            listOf(
                record(
                    "entity-l40-2",
                    "L40.2",
                    "Acrodermatitis continua suppurativa Hallopeau",
                    "https://id.who.int/icd/entity/l40-2"
                )
            )
        )
        importer.importFile(
            packageFile,
            packageFile.name,
            sha256(packageFile),
            System.currentTimeMillis()
        )

        assertTrue(store.retrieve("Hallo").isEmpty())
        assertTrue(store.buildEvidenceContext("Hallo").isEmpty())
        assertEquals("L40.2", store.retrieve("Hallopeau").single().record.code)
        assertEquals("L40.2", store.retrieve("L40.2").single().record.code)
        packageFile.delete()
    }

    private fun packageFile(records: List<JSONObject>, version: String = "2026-01"): File {
        val file = File.createTempFile("knowledge-test-", ".dmcknowledge", context.cacheDir)
        val manifest = JSONObject()
            .put("formatVersion", 1)
            .put("id", "who.icd11.test.en")
            .put("name", "ICD-11 test")
            .put("type", "icd11")
            .put("version", version)
            .put("language", "en")
            .put("jurisdiction", "international")
            .put("sourceName", "World Health Organization (WHO)")
            .put("sourceUrl", "https://icd.who.int/browse11")
            .put("licenseName", "CC BY-ND 3.0 IGO")
            .put("licenseUrl", "https://icd.who.int/en/docs/icd11-license.pdf")
            .put(
                "attributionText",
                "International Classification of Diseases, Eleventh Revision (ICD-11), " +
                    "World Health Organization (WHO) 2019 https://icd.who.int/browse11."
            )
            .put("officialContentUnmodified", true)
            .put("recordCount", records.size)
        ZipOutputStream(file.outputStream().buffered()).use { zip ->
            zip.putNextEntry(ZipEntry("manifest.json"))
            zip.write(manifest.toString().toByteArray())
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("records.jsonl"))
            zip.write(records.joinToString("\n").toByteArray())
            zip.closeEntry()
        }
        return file
    }

    private fun record(id: String, code: String, title: String, uri: String): JSONObject =
        JSONObject().put("id", id).put("code", code).put("title", title).put("uri", uri)

    private fun sha256(file: File): String = MessageDigest.getInstance("SHA-256")
        .digest(file.readBytes())
        .joinToString("") { "%02x".format(it) }
}
