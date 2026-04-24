@file:OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)

package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CenterAlignedTopAppBar
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

private data class OngoingCallEntry(
    val id: String,
    val title: String,
    val state: String,
    val initials: String,
    val tone: UiIconTone,
    val actionLabel: String,
    val onAction: (() -> Unit)? = null
)

private data class RecentCallEntry(
    val id: String,
    val title: String,
    val detail: String,
    val trailing: String,
    val initials: String,
    val tone: UiIconTone,
    val onOpen: (() -> Unit)? = null
)

@Composable
fun CallsHomeScreen(
    pendingCall: IncomingCall?,
    activePeerCall: PeerCallState?,
    activeGroupCall: GroupCallState?,
    groupRooms: List<GroupCallRoomUi>,
    onOpenPeerCall: (PeerCallState) -> Unit = {},
    onOpenGroupCall: (GroupCallState) -> Unit = {},
    onAcceptPendingCall: () -> Unit = {},
    onJoinGroupRoom: (GroupCallRoomUi) -> Unit = {},
    onOpenChats: () -> Unit = {},
    onOpenContacts: () -> Unit = {},
    onOpenSettings: () -> Unit = {}
) {
    val ongoingEntry = when {
        pendingCall != null -> OngoingCallEntry(
            id = "pending:${pendingCall.callIdHex}",
            title = pendingCall.peerUsername,
            state = if (pendingCall.video) {
                tr("call_incoming_video", "Incoming video call")
            } else {
                tr("call_incoming_voice", "Incoming voice call")
            },
            initials = pendingCall.peerUsername.take(2).uppercase(),
            tone = UiIconTone.Warning,
            actionLabel = tr("calls_answer", "Answer"),
            onAction = onAcceptPendingCall
        )
        activePeerCall != null -> OngoingCallEntry(
            id = "peer:${activePeerCall.callIdHex}",
            title = activePeerCall.peerUsername,
            state = if (activePeerCall.video) {
                tr("call_video_live", "Video call in progress")
            } else {
                tr("call_voice_live", "Voice call in progress")
            },
            initials = activePeerCall.peerUsername.take(2).uppercase(),
            tone = UiIconTone.Primary,
            actionLabel = tr("calls_resume", "Return"),
            onAction = { onOpenPeerCall(activePeerCall) }
        )
        activeGroupCall != null -> OngoingCallEntry(
            id = "group:${activeGroupCall.callIdHex}",
            title = activeGroupCall.groupId,
            state = if (activeGroupCall.video) {
                tr("call_group_video_live", "Group video call in progress")
            } else {
                tr("call_group_voice_live", "Group voice call in progress")
            },
            initials = activeGroupCall.groupId.take(2).uppercase(),
            tone = UiIconTone.Accent,
            actionLabel = tr("calls_join", "Join"),
            onAction = { onOpenGroupCall(activeGroupCall) }
        )
        else -> null
    }
    val recentEntries = groupRooms
        .filterNot { room -> activeGroupCall != null && room.callId == activeGroupCall.callIdHex }
        .mapIndexed { index, room ->
        RecentCallEntry(
            id = "room:${room.callId}:$index",
            title = room.groupId,
            detail = if (room.video) {
                tr("call_group_video", "Video")
            } else {
                tr("call_group_voice", "Voice")
            },
            trailing = when (index) {
                0 -> "09:42"
                1 -> "09:18"
                2 -> "Yesterday"
                3 -> "Mon"
                else -> "Sun"
            },
            initials = room.groupId.take(2).uppercase(),
            tone = UiIconTone.Neutral,
            onOpen = { onJoinGroupRoom(room) }
        )
        }
    val isEmpty = ongoingEntry == null && recentEntries.isEmpty()

    Scaffold(
        topBar = {
            CenterAlignedTopAppBar(
                title = {
                    Text(
                        text = tr("calls_title", "Calls"),
                        style = MaterialTheme.typography.titleMedium
                    )
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surface.copy(alpha = 0.86f),
                    titleContentColor = MaterialTheme.colorScheme.onSurface
                )
            )
        },
        bottomBar = {
            ConversationBottomBar(
                activeTab = ConversationTab.Calls,
                onContacts = onOpenContacts,
                onChats = onOpenChats,
                onCalls = {},
                onSettings = onOpenSettings
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        BoxWithConstraints(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
        ) {
            if (isEmpty) {
                CallEmptyState(
                    onOpenChats = onOpenChats,
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(max = maxHeight * 0.35f)
                        .padding(horizontal = 16.dp, vertical = 8.dp)
                )
            } else {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(horizontal = 16.dp)
                        .testTag("calls-screen"),
                    verticalArrangement = Arrangement.spacedBy(0.dp)
                ) {
                    if (ongoingEntry != null) {
                        item(key = ongoingEntry.id) {
                            OngoingCallStrip(entry = ongoingEntry)
                        }
                    }
                    items(
                        items = recentEntries,
                        key = { it.id }
                    ) { entry ->
                        RecentCallRow(entry = entry)
                    }
                }
            }
        }
    }
}

@Composable
private fun OngoingCallStrip(entry: OngoingCallEntry) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(60.dp)
            .testTag("calls-ongoing-entry"),
        verticalAlignment = Alignment.CenterVertically
    ) {
        IdentityAvatar(
            label = entry.title,
            seed = entry.id,
            kind = IdentityAvatarKind.Person,
            size = 44.dp,
            presenceState = when (entry.tone) {
                UiIconTone.Primary -> PresenceState.Online
                UiIconTone.Accent -> PresenceState.Secure
                UiIconTone.Warning -> PresenceState.Busy
                UiIconTone.Danger -> PresenceState.Busy
                UiIconTone.Neutral -> null
            }
        )
        Column(
            modifier = Modifier
                .weight(1f)
                .padding(start = 12.dp, end = 8.dp),
            verticalArrangement = Arrangement.Center
        ) {
            Text(
                text = entry.title,
                style = MaterialTheme.typography.bodyLarge,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = entry.state,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
        MediaHintChip(
            kind = if (entry.state.contains("video", ignoreCase = true)) {
                MediaHintKind.Photo
            } else {
                MediaHintKind.Voice
            },
            label = if (entry.state.contains("video", ignoreCase = true)) "Video" else "Voice"
        )
        Spacer(modifier = Modifier.width(8.dp))
        FilledTonalButton(
            onClick = { entry.onAction?.invoke() },
            modifier = Modifier.height(30.dp),
            shape = RoundedCornerShape(12.dp),
            colors = ButtonDefaults.filledTonalButtonColors(
                containerColor = when (entry.tone) {
                    UiIconTone.Primary -> MaterialTheme.colorScheme.primary.copy(alpha = 0.12f)
                    UiIconTone.Accent -> MaterialTheme.colorScheme.secondary.copy(alpha = 0.14f)
                    UiIconTone.Warning -> MaterialTheme.colorScheme.error.copy(alpha = 0.12f)
                    UiIconTone.Danger -> MaterialTheme.colorScheme.error.copy(alpha = 0.18f)
                    UiIconTone.Neutral -> MaterialTheme.colorScheme.surfaceVariant
                },
                contentColor = when (entry.tone) {
                    UiIconTone.Primary -> MaterialTheme.colorScheme.primary
                    UiIconTone.Accent -> MaterialTheme.colorScheme.secondary
                    UiIconTone.Warning -> MaterialTheme.colorScheme.error
                    UiIconTone.Danger -> MaterialTheme.colorScheme.error
                    UiIconTone.Neutral -> MaterialTheme.colorScheme.onSurface
                }
            )
        ) {
            Text(
                text = entry.actionLabel,
                style = MaterialTheme.typography.labelSmall,
                maxLines = 1
            )
        }
    }
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 56.dp)
            .height(1.dp)
            .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.08f))
    )
}

@Composable
private fun RecentCallRow(entry: RecentCallEntry) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(72.dp)
            .clickable(enabled = entry.onOpen != null) { entry.onOpen?.invoke() },
        verticalAlignment = Alignment.CenterVertically
    ) {
        IdentityAvatar(
            label = entry.title,
            seed = entry.id,
            kind = IdentityAvatarKind.Group,
            size = 44.dp,
            badgeIcon = MiOwnedIcons.Call
        )
        Column(
            modifier = Modifier
                .weight(1f)
                .padding(start = 12.dp, end = 8.dp),
            verticalArrangement = Arrangement.Center
        ) {
            Text(
                text = entry.title,
                style = MaterialTheme.typography.bodyLarge,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = entry.detail,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
        Column(horizontalAlignment = Alignment.End) {
            if (entry.trailing.isNotBlank()) {
                Text(
                    text = entry.trailing,
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1
                )
                Spacer(modifier = Modifier.height(8.dp))
            }
            UiToolbarIconButton(
                icon = if (entry.detail.contains("Video", ignoreCase = true)) {
                    MiOwnedIcons.Video
                } else {
                    MiOwnedIcons.Call
                },
                contentDescription = entry.detail,
                onClick = { entry.onOpen?.invoke() },
                tone = if (entry.detail.contains("Video", ignoreCase = true)) {
                    UiIconTone.Primary
                } else {
                    UiIconTone.Warning
                }
            )
        }
    }
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 56.dp)
            .height(1.dp)
            .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.08f))
    )
}

@Composable
private fun CallEmptyState(
    onOpenChats: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier,
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        EmptyStateIllustration(
            icon = MiOwnedIcons.Call,
            contentDescription = tr("calls_empty_title", "No recent calls"),
            modifier = Modifier.size(ChatUiTokens.IllustrationFrame),
            tone = UiIconTone.Neutral,
            chipLabel = tr("calls_empty_chip", "Ready")
        )
        Spacer(modifier = Modifier.height(14.dp))
        Text(
            text = tr("calls_empty_title", "No recent calls"),
            style = MaterialTheme.typography.bodyLarge
        )
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            text = tr(
                "calls_empty_subtitle",
                "Calls appear here."
            ),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
        Spacer(modifier = Modifier.height(14.dp))
        FilledTonalButton(
            onClick = onOpenChats,
            modifier = Modifier.height(34.dp),
            shape = RoundedCornerShape(12.dp)
        ) {
            Text(
                text = tr("nav_chats", "Chats"),
                style = MaterialTheme.typography.labelSmall
            )
        }
    }
}
