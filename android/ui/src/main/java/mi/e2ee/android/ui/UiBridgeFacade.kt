package mi.e2ee.android.ui

internal interface UiBridgeFacade {
    val sdk: SdkBridge
    val loggedIn: Boolean

    fun login(username: String, password: String, rootCode: String): Boolean
    fun register(username: String, password: String): Boolean
    fun createGroupAndRoute(): FlowScreen.AddGroupMembers?
    fun openConversationRoute(conversation: ConversationPreview): FlowScreen

    fun togglePinned(conversationId: String)
    fun markConversationRead(conversationId: String)
    fun toggleConversationMute(conversationId: String)
    fun deleteConversation(conversation: ConversationPreview)

    fun acceptIncomingCall(): PeerCallState?
    fun declineIncomingCall()
    fun trustPendingServer(pin: String)
    fun trustPendingPeer(pin: String)
}

internal class SdkUiBridgeFacade(
    override val sdk: SdkBridge
) : UiBridgeFacade {
    override val loggedIn: Boolean
        get() = sdk.loggedIn

    override fun login(username: String, password: String, rootCode: String): Boolean {
        return sdk.login(username, password, rootCode)
    }

    override fun register(username: String, password: String): Boolean {
        return sdk.register(username, password)
    }

    override fun createGroupAndRoute(): FlowScreen.AddGroupMembers? {
        val groupId = sdk.createGroup() ?: return null
        return FlowScreen.AddGroupMembers(groupId)
    }

    override fun openConversationRoute(conversation: ConversationPreview): FlowScreen {
        sdk.setActiveConversation(conversation.id, conversation.isGroup)
        sdk.loadHistory(conversation.id, conversation.isGroup)
        return if (conversation.isGroup) {
            sdk.refreshGroupMembers(conversation.id)
            FlowScreen.GroupChat(conversation.id)
        } else {
            FlowScreen.Chat(conversation.id)
        }
    }

    override fun togglePinned(conversationId: String) {
        sdk.conversations.firstOrNull { it.id == conversationId }?.let { sdk.togglePin(it.id) }
    }

    override fun markConversationRead(conversationId: String) {
        sdk.conversations.firstOrNull { it.id == conversationId }?.let { sdk.markRead(it.id) }
    }

    override fun toggleConversationMute(conversationId: String) {
        sdk.conversations.firstOrNull { it.id == conversationId }?.let { sdk.toggleMute(it.id) }
    }

    override fun deleteConversation(conversation: ConversationPreview) {
        sdk.deleteChatHistory(
            conversation.id,
            conversation.isGroup,
            deleteAttachments = true,
            secureWipe = false
        )
    }

    override fun acceptIncomingCall(): PeerCallState? {
        return sdk.acceptIncomingCall()
    }

    override fun declineIncomingCall() {
        sdk.declineIncomingCall()
    }

    override fun trustPendingServer(pin: String) {
        sdk.trustPendingServer(pin)
    }

    override fun trustPendingPeer(pin: String) {
        sdk.trustPendingPeer(pin)
    }
}
