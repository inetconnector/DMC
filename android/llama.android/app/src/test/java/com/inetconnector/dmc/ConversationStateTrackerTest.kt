package com.inetconnector.dmc

import org.junit.Assert.assertEquals
import org.junit.Test

class ConversationStateTrackerTest {
    private val tracker = ConversationStateTracker()

    @Test
    fun firstRequestAlwaysRebuilds() {
        assertEquals(ConversationSyncMode.REBUILD, tracker.modeFor("chat-a", 1, "empty"))
    }

    @Test
    fun expectedNextTurnInSameConversationIsIncremental() {
        tracker.markCompleted("chat-a", 1, "expected")

        assertEquals(ConversationSyncMode.INCREMENTAL, tracker.modeFor("chat-a", 3, "expected"))
    }

    @Test
    fun newConversationRebuildsEvenForSameMessageCount() {
        tracker.markCompleted("chat-a", 3, "chat-a-history")

        assertEquals(ConversationSyncMode.REBUILD, tracker.modeFor("chat-b", 1, "empty"))
    }

    @Test
    fun retriedOrEditedTurnRebuilds() {
        tracker.markCompleted("chat-a", 3, "old-history")

        assertEquals(ConversationSyncMode.REBUILD, tracker.modeFor("chat-a", 3, "old-history"))
    }

    @Test
    fun changedHistoryWithExpectedCountRebuilds() {
        tracker.markCompleted("chat-a", 1, "original-history")

        assertEquals(ConversationSyncMode.REBUILD, tracker.modeFor("chat-a", 3, "edited-history"))
    }

    @Test
    fun requestWithoutConversationIdIsStateless() {
        tracker.markCompleted("chat-a", 1, "expected")

        assertEquals(ConversationSyncMode.REBUILD, tracker.modeFor(null, 1, "empty"))
    }
}
