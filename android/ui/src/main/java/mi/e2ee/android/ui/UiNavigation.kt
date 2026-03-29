package mi.e2ee.android.ui

import android.net.Uri
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.listSaver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue

internal sealed interface FlowScreen {
    data object Login : FlowScreen
    data object Register : FlowScreen
    data class QrLoginDisplay(val username: String) : FlowScreen
    data object QrLoginScan : FlowScreen
    data object Conversations : FlowScreen
    data object Calls : FlowScreen
    data class Chat(val conversationId: String) : FlowScreen
    data class GroupChat(val groupId: String) : FlowScreen
    data object Settings : FlowScreen
    data object SecurityCenter : FlowScreen
    data object Account : FlowScreen
    data object Privacy : FlowScreen
    data object Diagnostics : FlowScreen
    data object AddFriend : FlowScreen
    data object FriendRequests : FlowScreen
    data class ContactDetail(val username: String) : FlowScreen
    data class GroupDetail(val groupId: String) : FlowScreen
    data class AddGroupMembers(val groupId: String) : FlowScreen
    data class PeerCall(val callIdHex: String) : FlowScreen
    data class GroupCall(val groupId: String, val callIdHex: String) : FlowScreen
    data object BlockedUsers : FlowScreen
}

@Stable
internal class UiNavigationState(
    initialStack: List<FlowScreen> = listOf(FlowScreen.Login)
) {
    private var stack by mutableStateOf(initialStack.ifEmpty { listOf(FlowScreen.Login) })

    val current: FlowScreen
        get() = stack.last()

    fun stackSnapshot(): List<FlowScreen> = stack

    fun navigate(screen: FlowScreen) {
        stack = stack + screen
    }

    fun resetTo(screen: FlowScreen) {
        stack = listOf(screen)
    }

    fun goBack() {
        if (stack.size > 1) {
            stack = stack.dropLast(1)
        }
    }
}

private fun FlowScreen.toSaveKey(): String = when (this) {
    FlowScreen.Login -> "login"
    FlowScreen.Register -> "register"
    is FlowScreen.QrLoginDisplay -> "qrDisplay:${Uri.encode(username)}"
    FlowScreen.QrLoginScan -> "qrScan"
    FlowScreen.Conversations -> "conversations"
    FlowScreen.Calls -> "calls"
    is FlowScreen.Chat -> "chat:${Uri.encode(conversationId)}"
    is FlowScreen.GroupChat -> "groupChat:${Uri.encode(groupId)}"
    FlowScreen.Settings -> "settings"
    FlowScreen.SecurityCenter -> "securityCenter"
    FlowScreen.Account -> "account"
    FlowScreen.Privacy -> "privacy"
    FlowScreen.Diagnostics -> "diagnostics"
    FlowScreen.AddFriend -> "addFriend"
    FlowScreen.FriendRequests -> "friendRequests"
    is FlowScreen.ContactDetail -> "contact:${Uri.encode(username)}"
    is FlowScreen.GroupDetail -> "groupDetail:${Uri.encode(groupId)}"
    is FlowScreen.AddGroupMembers -> "groupMembers:${Uri.encode(groupId)}"
    is FlowScreen.PeerCall -> "peerCall:${Uri.encode(callIdHex)}"
    is FlowScreen.GroupCall -> "groupCall:${Uri.encode(groupId)}:${Uri.encode(callIdHex)}"
    FlowScreen.BlockedUsers -> "blockedUsers"
}

private fun flowScreenFromSaveKey(value: String): FlowScreen = when {
    value == "login" -> FlowScreen.Login
    value == "register" -> FlowScreen.Register
    value.startsWith("qrDisplay:") -> FlowScreen.QrLoginDisplay(Uri.decode(value.substringAfter("qrDisplay:")))
    value == "qrScan" -> FlowScreen.QrLoginScan
    value == "conversations" -> FlowScreen.Conversations
    value == "calls" -> FlowScreen.Calls
    value.startsWith("chat:") -> FlowScreen.Chat(Uri.decode(value.substringAfter("chat:")))
    value.startsWith("groupChat:") -> FlowScreen.GroupChat(Uri.decode(value.substringAfter("groupChat:")))
    value == "settings" -> FlowScreen.Settings
    value == "securityCenter" -> FlowScreen.SecurityCenter
    value == "account" -> FlowScreen.Account
    value == "privacy" -> FlowScreen.Privacy
    value == "diagnostics" -> FlowScreen.Diagnostics
    value == "addFriend" -> FlowScreen.AddFriend
    value == "friendRequests" -> FlowScreen.FriendRequests
    value.startsWith("contact:") -> FlowScreen.ContactDetail(Uri.decode(value.substringAfter("contact:")))
    value.startsWith("groupDetail:") -> FlowScreen.GroupDetail(Uri.decode(value.substringAfter("groupDetail:")))
    value.startsWith("groupMembers:") -> FlowScreen.AddGroupMembers(Uri.decode(value.substringAfter("groupMembers:")))
    value.startsWith("peerCall:") -> FlowScreen.PeerCall(Uri.decode(value.substringAfter("peerCall:")))
    value.startsWith("groupCall:") -> {
        val payload = value.removePrefix("groupCall:").split(':', limit = 2)
        if (payload.size == 2) {
            FlowScreen.GroupCall(Uri.decode(payload[0]), Uri.decode(payload[1]))
        } else {
            FlowScreen.Conversations
        }
    }
    value == "blockedUsers" -> FlowScreen.BlockedUsers
    else -> FlowScreen.Login
}

private val UiNavigationStateSaver = listSaver<UiNavigationState, String>(
    save = { state -> state.stackSnapshot().map(FlowScreen::toSaveKey) },
    restore = { saved ->
        UiNavigationState(
            saved.map(::flowScreenFromSaveKey).ifEmpty { listOf(FlowScreen.Login) }
        )
    }
)

@Composable
internal fun rememberUiNavigationState(
    initial: FlowScreen = FlowScreen.Login
): UiNavigationState {
    return rememberSaveable(
        initial.toSaveKey(),
        saver = UiNavigationStateSaver
    ) {
        UiNavigationState(listOf(initial))
    }
}
