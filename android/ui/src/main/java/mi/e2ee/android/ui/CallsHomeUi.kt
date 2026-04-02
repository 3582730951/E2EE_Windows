@file:OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)

package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
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
            actionLabel = tr("calls_open", "Open"),
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
            actionLabel = tr("calls_open", "Open"),
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
                tr("call_group_video", "Group video call")
            } else {
                tr("call_group_voice", "Group voice call")
            },
            trailing = tr("calls_recent_room", "Recent room"),
            initials = room.groupId.take(2).uppercase(),
            tone = UiIconTone.Neutral,
            onOpen = { onJoinGroupRoom(room) }
        )
        }
    val isEmpty = ongoingEntry == null && recentEntries.isEmpty()

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        text = tr("calls_title", "Calls"),
                        style = MaterialTheme.typography.titleLarge
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
                        .padding(horizontal = 16.dp),
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
            .height(56.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        AvatarBadge(
            initials = entry.initials,
            tint = when (entry.tone) {
                UiIconTone.Primary -> MaterialTheme.colorScheme.primary
                UiIconTone.Accent -> MaterialTheme.colorScheme.secondary
                UiIconTone.Warning -> MaterialTheme.colorScheme.error
                UiIconTone.Danger -> MaterialTheme.colorScheme.error
                UiIconTone.Neutral -> MaterialTheme.colorScheme.primary
            },
            size = 40.dp
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
        FilledTonalButton(
            onClick = { entry.onAction?.invoke() },
            modifier = Modifier.height(32.dp),
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
            .padding(start = 52.dp)
            .height(1.dp)
            .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.08f))
    }
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
        AvatarBadge(
            initials = entry.initials,
            tint = when (entry.tone) {
                UiIconTone.Primary -> MaterialTheme.colorScheme.primary
                UiIconTone.Accent -> MaterialTheme.colorScheme.secondary
                UiIconTone.Warning -> MaterialTheme.colorScheme.error
                UiIconTone.Danger -> MaterialTheme.colorScheme.error
                UiIconTone.Neutral -> MaterialTheme.colorScheme.primary
            },
            size = 40.dp
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
        Text(
            text = entry.trailing,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 1
        )
    }
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 52.dp)
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
        verticalArrangement = Arrangement.Center
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            UiSemanticIcon(
                icon = MiOwnedIcons.Call,
                contentDescription = tr("calls_empty_title", "No recent calls"),
                tone = UiIconTone.Neutral,
                size = 40.dp,
                iconSize = ChatUiTokens.IconGlyphMd
            )
            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(start = 12.dp, end = 12.dp)
            ) {
                Text(
                    text = tr("calls_empty_title", "No recent calls"),
                    style = MaterialTheme.typography.bodyLarge
                )
                Text(
                    text = tr(
                        "calls_empty_subtitle",
                        "Finished calls and joined rooms appear here."
                    ),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis
                )
            }
            FilledTonalButton(
                onClick = onOpenChats,
                modifier = Modifier.height(32.dp),
                shape = RoundedCornerShape(12.dp)
            ) {
                Text(
                    text = tr("nav_chats", "Chats"),
                    style = MaterialTheme.typography.labelSmall
                )
            }
        }
    }
}
