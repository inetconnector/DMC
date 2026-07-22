package com.inetconnector.dmc

internal enum class ConversationSyncMode {
    INCREMENTAL,
    REBUILD
}

/** Tracks whether the one native KV/DMC state still matches the incoming chat. */
internal class ConversationStateTracker {
    private var activeConversationId: String? = null
    private var completedRequestMessageCount: Int = 0
    private var expectedHistorySignature: String? = null

    fun modeFor(
        conversationId: String?,
        messageCount: Int,
        historySignature: String
    ): ConversationSyncMode {
        val normalizedId = conversationId?.trim()?.takeIf { it.isNotEmpty() }
        val isExpectedNextTurn =
            normalizedId != null &&
                normalizedId == activeConversationId &&
                messageCount == completedRequestMessageCount + 2 &&
                historySignature == expectedHistorySignature
        return if (isExpectedNextTurn) {
            ConversationSyncMode.INCREMENTAL
        } else {
            ConversationSyncMode.REBUILD
        }
    }

    fun markCompleted(
        conversationId: String?,
        messageCount: Int,
        nextHistorySignature: String
    ) {
        activeConversationId = conversationId?.trim()?.takeIf { it.isNotEmpty() }
        completedRequestMessageCount = messageCount
        expectedHistorySignature = nextHistorySignature
    }

    fun invalidate() {
        activeConversationId = null
        completedRequestMessageCount = 0
        expectedHistorySignature = null
    }
}
