package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ButtonDefaults
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

private data class CallActivityEntry(
    val id: String,
    val title: String,
    val subtitle: String,
    val meta: String,
    val icon: androidx.compose.ui.graphics.vector.ImageVector,
    val tone: UiIconTone,
    val actionLabel: String? = null,
    val onAction: (() -> Unit)? = null
)

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
    val activeEntries = buildList {
        if (activePeerCall != null) {
            add(
                CallActivityEntry(
                    id = "peer:${activePeerCall.callIdHex}",
                    title = activePeerCall.peerUsername,
                    subtitle = if (activePeerCall.video) {
                        tr("call_video", "Video call")
                    } else {
                        tr("call_voice", "Voice call")
                    },
                    meta = tr("calls_live_now", "Live now"),
                    icon = if (activePeerCall.video) MiOwnedIcons.Video else MiOwnedIcons.Call,
                    tone = UiIconTone.Primary,
                    actionLabel = tr("calls_open", "Open"),
                    onAction = { onOpenPeerCall(activePeerCall) }
                )
            )
        }
        if (activeGroupCall != null) {
            add(
                CallActivityEntry(
                    id = "group:${activeGroupCall.callIdHex}",
                    title = activeGroupCall.groupId,
                    subtitle = if (activeGroupCall.video) {
                        tr("call_group_video", "Group video call")
                    } else {
                        tr("call_group_voice", "Group voice call")
                    },
                    meta = tr("calls_live_now", "Live now"),
                    icon = if (activeGroupCall.video) MiOwnedIcons.Video else MiOwnedIcons.Call,
                    tone = UiIconTone.Accent,
                    actionLabel = tr("calls_open", "Open"),
                    onAction = { onOpenGroupCall(activeGroupCall) }
                )
            )
        }
    }
    val attentionEntries = buildList {
        if (pendingCall != null) {
            add(
                CallActivityEntry(
                    id = "pending:${pendingCall.callIdHex}",
                    title = pendingCall.peerUsername,
                    subtitle = if (pendingCall.video) {
                        tr("call_video", "Video call")
                    } else {
                        tr("call_voice", "Voice call")
                    },
                    meta = tr("call_incoming_title", "Incoming call"),
                    icon = if (pendingCall.video) MiOwnedIcons.Video else MiOwnedIcons.Call,
                    tone = UiIconTone.Warning
                )
            )
        }
    }
    val recentEntries = groupRooms.mapIndexed { index, room ->
        CallActivityEntry(
            id = "room:${room.callId}:$index",
            title = room.groupId,
            subtitle = if (room.video) {
                tr("call_group_video", "Group video call")
            } else {
                tr("call_group_voice", "Group voice call")
            },
            meta = tr("calls_recent_room", "Recent room"),
            icon = if (room.video) MiOwnedIcons.Video else MiOwnedIcons.Call,
            tone = UiIconTone.Neutral,
            actionLabel = tr("calls_join", "Join"),
            onAction = { onJoinGroupRoom(room) }
        )
    }
    val isEmpty = activeEntries.isEmpty() && attentionEntries.isEmpty() && recentEntries.isEmpty()

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
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(horizontal = 14.dp, vertical = 6.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            if (!isEmpty) {
                item(key = "calls-summary") {
                    CallSummaryStrip(
                        activeCount = activeEntries.size,
                        attentionCount = attentionEntries.size,
                        recentCount = recentEntries.size
                    )
                }
            }
            if (activeEntries.isNotEmpty()) {
                item(key = "calls-active-title") {
                    SectionHeader(
                        text = "${tr("calls_section_ongoing", "In progress")} · ${activeEntries.size}",
                        modifier = Modifier.padding(top = 2.dp)
                    )
                }
                item(key = "calls-active-group") {
                    CallSection(entries = activeEntries, emphasized = true)
                }
            }
            if (attentionEntries.isNotEmpty()) {
                item(key = "calls-attention-title") {
                    SectionHeader(
                        text = "${tr("calls_section_attention", "Needs attention")} · ${attentionEntries.size}",
                        modifier = Modifier.padding(top = 2.dp)
                    )
                }
                item(key = "calls-attention-group") {
                    CallSection(entries = attentionEntries, emphasized = false)
                }
            }
            if (recentEntries.isNotEmpty()) {
                item(key = "calls-recent-title") {
                    SectionHeader(
                        text = "${tr("calls_recent", "Recent")} · ${recentEntries.size}",
                        modifier = Modifier.padding(top = 2.dp)
                    )
                }
                item(key = "calls-recent-group") {
                    CallSection(entries = recentEntries, emphasized = false)
                }
            }
            if (isEmpty) {
                item(key = "calls-empty") {
                    CallEmptyState(onOpenChats = onOpenChats)
                }
            }
        }
    }
}

@Composable
private fun CallSummaryStrip(
    activeCount: Int,
    attentionCount: Int,
    recentCount: Int
) {
    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            if (activeCount > 0) {
                LabeledChip(
                    label = tr("calls_live_short", "Live") + " · $activeCount",
                    tint = MaterialTheme.colorScheme.primary
                )
            }
            if (attentionCount > 0) {
                LabeledChip(
                    label = tr("calls_attention_short", "Attention") + " · $attentionCount",
                    tint = MaterialTheme.colorScheme.error
                )
            }
        }
        if (recentCount > 0) {
            LabeledChip(
                label = tr("calls_recent_short", "Recent") + " · $recentCount",
                tint = MaterialTheme.colorScheme.secondary
            )
        }
    }
}

@Composable
private fun CallSection(
    entries: List<CallActivityEntry>,
    emphasized: Boolean
) {
    SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.fillMaxWidth()) {
            entries.forEachIndexed { index, entry ->
                CallActivityRow(
                    entry = entry,
                    emphasized = emphasized
                )
                if (index != entries.lastIndex) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(start = 54.dp)
                            .height(1.dp)
                            .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.08f))
                    )
                }
            }
        }
    }
}

@Composable
private fun CallActivityRow(
    entry: CallActivityEntry,
    emphasized: Boolean
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = entry.onAction != null) { entry.onAction?.invoke() }
            .padding(horizontal = 4.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        AvatarBadge(
            initials = entry.title.take(2).uppercase(),
            tint = MaterialTheme.colorScheme.primary,
            size = 40.dp
        )
        Column(
            modifier = Modifier
                .weight(1f)
                .padding(start = 10.dp, end = 8.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = entry.title,
                    modifier = Modifier.weight(1f, fill = false),
                    style = MaterialTheme.typography.titleSmall,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                CallMetaPill(label = entry.meta, tone = entry.tone, emphasized = emphasized)
            }
            Spacer(modifier = Modifier.height(3.dp))
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                UiSemanticIcon(
                    icon = entry.icon,
                    contentDescription = entry.subtitle,
                    tone = entry.tone,
                    size = 18.dp,
                    cornerRadius = 8.dp,
                    iconSize = 10.dp,
                    framed = false
                )
                Text(
                    text = entry.subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }
        if (!entry.actionLabel.isNullOrBlank() && entry.onAction != null) {
            FilledTonalButton(
                onClick = entry.onAction,
                modifier = Modifier.height(34.dp),
                shape = RoundedCornerShape(14.dp),
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
                    style = MaterialTheme.typography.labelMedium
                )
            }
        }
    }
}

@Composable
private fun CallMetaPill(
    label: String,
    tone: UiIconTone,
    emphasized: Boolean
) {
    val tint = when (tone) {
        UiIconTone.Primary -> MaterialTheme.colorScheme.primary
        UiIconTone.Accent -> MaterialTheme.colorScheme.secondary
        UiIconTone.Warning -> MaterialTheme.colorScheme.error
        UiIconTone.Danger -> MaterialTheme.colorScheme.error
        UiIconTone.Neutral -> MaterialTheme.colorScheme.onSurfaceVariant
    }
    Text(
        text = label,
        modifier = Modifier
            .background(
                color = tint.copy(alpha = if (emphasized) 0.12f else 0.08f),
                shape = RoundedCornerShape(999.dp)
            )
            .padding(horizontal = 8.dp, vertical = 3.dp),
        style = MaterialTheme.typography.labelSmall,
        color = tint,
        maxLines = 1
    )
}

@Composable
private fun CallEmptyState(onOpenChats: () -> Unit) {
    SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 4.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            UiSemanticIcon(
                icon = MiOwnedIcons.Call,
                contentDescription = tr("calls_empty_title", "No recent calls"),
                tone = UiIconTone.Neutral,
                size = ChatUiTokens.IconContainerMd,
                iconSize = ChatUiTokens.IconGlyphMd
            )
            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(start = 10.dp, end = 8.dp)
            ) {
                Text(
                    text = tr("calls_empty_title", "No recent calls"),
                    style = MaterialTheme.typography.titleSmall
                )
                Text(
                    text = tr(
                        "calls_empty_subtitle",
                        "Active calls, missed calls, and rooms appear here."
                    ),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            FilledTonalButton(
                onClick = onOpenChats,
                modifier = Modifier.height(36.dp)
            ) {
                Text(tr("nav_chats", "Chats"))
            }
        }
    }
}
