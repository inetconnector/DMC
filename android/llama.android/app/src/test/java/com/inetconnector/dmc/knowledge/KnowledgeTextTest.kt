package com.inetconnector.dmc.knowledge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class KnowledgeTextTest {
    @Test
    fun greetingsDoNotProduceRetrievalTerms() {
        assertTrue(KnowledgeText.queryTerms("Hallo").isEmpty())
        assertTrue(KnowledgeText.queryTerms("Hello, bitte!").isEmpty())
    }

    @Test
    fun conversationalWordsAreRemovedFromSpecificQuestion() {
        assertEquals(listOf("banane"), KnowledgeText.queryTerms("Antworte nur mit Banane"))
    }

    @Test
    fun medicalTermsAndCodesRemainSearchable() {
        assertEquals(
            listOf("akrodermatitis", "hallopeau"),
            KnowledgeText.queryTerms("Was ist Akrodermatitis Hallopeau?")
        )
        assertEquals(setOf("L40.2"), KnowledgeText.codes("Was bedeutet l40.2?"))
    }
}
