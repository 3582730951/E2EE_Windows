package mi.e2ee.android.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CallsHomeScreen(
    pendingCall: IncomingCall?,
    activePeerCall: PeerCallState?,
    activeGroupCall: GroupCallState?,
    groupRooms: List<GroupCallRoomUi>,
    onOpenPeerCall: (PeerCallState) -> Unit = {},
    onOpenGroupCall: (GroupCallState) -> Unit = {},
    onJoinGroupRoom: (GroupCallRoomUi) -> Unit = {},
    onOpenChats: () -> Unit = {},
    onOpenContacts: () -> Unit = {},
    onOpenSettings: () -> Unit = {}
) {
    val hasActive = activePeerCall != null || activeGroupCall != null
    val hasPending = pendingCall != null
    val hasRooms = groupRooms.isNotEmpty()
    val isEmpty = !hasActive && !hasPending && !hasRooms

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("calls_title", "Calls")) },
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
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            if (activePeerCall != null) {
                item(key = "peer-active") {
                    ActiveCallCard(
                        title = activePeerCall.peerUsername,
                        subtitle = if (activePeerCall.video) {
                            tr("call_video", "Video call")
                        } else {
                            tr("call_voice", "Voice call")
                        },
                        icon = if (activePeerCall.video) MiOwnedIcons.Video else MiOwnedIcons.Call,
                        onOpen = { onOpenPeerCall(activePeerCall) }
                    )
                }
            }
            if (activeGroupCall != null) {
                item(key = "group-active") {
                    ActiveCallCard(
                        title = activeGroupCall.groupId,
                        subtitle = if (activeGroupCall.video) {
                            tr("call_group_video", "Group video call")
                        } else {
                            tr("call_group_voice", "Group voice call")
                        },
                        icon = if (activeGroupCall.video) MiOwnedIcons.Video else MiOwnedIcons.Call,
                        onOpen = { onOpenGroupCall(activeGroupCall) }
                    )
                }
            }
            if (pendingCall != null) {
                item(key = "pending-call") {
                    StatusCallCard(
                        title = tr("call_incoming_title", "Incoming call"),
                        subtitle = tr("call_incoming_body", "%s is calling you").format(pendingCall.peerUsername),
                        icon = if (pendingCall.video) MiOwnedIcons.Video else MiOwnedIcons.Call
                    )
                }
            }
            if (groupRooms.isNotEmpty()) {
                item(key = "group-rooms-header") {
                    SectionHeader(
                        text = tr("calls_rooms", "Group rooms"),
                        modifier = Modifier.padding(top = 8.dp)
                    )
                }
                items(groupRooms, key = { room -> "${room.groupId}:${room.callId}" }) { room ->
                    GroupRoomRow(
                        room = room,
                        onJoin = { onJoinGroupRoom(room) }
                    )
                }
            }
            if (isEmpty) {
                item(key = "calls-empty") {
                    StatusCallCard(
                        title = tr("calls_empty_title", "No active calls"),
                        subtitle = tr("calls_empty_subtitle", "Call status and group rooms appear here."),
                        icon = MiOwnedIcons.Call
                    )
                }
            }
        }
    }
}

@Composable
private fun ActiveCallCard(
    title: String,
    subtitle: String,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    onOpen: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onOpen() },
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            UiSemanticIcon(
                icon = icon,
                contentDescription = subtitle,
                tone = UiIconTone.Primary,
                size = ChatUiTokens.IconContainerMd,
                iconSize = ChatUiTokens.IconGlyphMd
            )
            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(start = 10.dp, end = 8.dp)
            ) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            FilledTonalButton(onClick = onOpen) {
                Text(tr("calls_open", "Open"))
            }
        }
    }
}

@Composable
private fun StatusCallCard(
    title: String,
    subtitle: String,
    icon: androidx.compose.ui.graphics.vector.ImageVector
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.26f))
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            UiSemanticIcon(
                icon = icon,
                contentDescription = title,
                tone = UiIconTone.Neutral,
                size = ChatUiTokens.IconContainerMd,
                iconSize = ChatUiTokens.IconGlyphMd
            )
            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(start = 10.dp)
            ) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleSmall
                )
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
    }
}

@Composable
private fun GroupRoomRow(
    room: GroupCallRoomUi,
    onJoin: () -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            UiSemanticIcon(
                icon = if (room.video) MiOwnedIcons.Video else MiOwnedIcons.Call,
                contentDescription = tr("calls_join", "Join"),
                tone = UiIconTone.Accent,
                size = ChatUiTokens.IconContainerSm,
                iconSize = ChatUiTokens.IconGlyphSm
            )
            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(start = 10.dp, end = 8.dp)
            ) {
                Text(
                    text = room.groupId,
                    style = MaterialTheme.typography.bodyLarge,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    text = if (room.video) tr("call_group_video", "Group video call") else tr("call_group_voice", "Group voice call"),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            FilledTonalButton(onClick = onJoin) {
                Text(tr("calls_join", "Join"))
            }
        }
    }
}
