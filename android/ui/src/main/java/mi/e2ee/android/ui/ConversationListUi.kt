package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.gestures.Orientation
import androidx.compose.foundation.gestures.draggable
import androidx.compose.foundation.gestures.rememberDraggableState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.Velocity
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.abs
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay

data class ConversationPreview(
    val id: String,
    val initials: String,
    val name: String,
    val lastMessage: String,
    val time: String,
    val unreadCount: Int,
    val isPinned: Boolean,
    val isMuted: Boolean,
    val isGroup: Boolean,
    val isTyping: Boolean,
    val draft: String? = null,
    val mentionCount: Int = 0
)

data class ConversationAction(
    val id: String,
    val label: String,
    val icon: ImageVector,
    val isDestructive: Boolean = false
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConversationListScreen(
    conversations: List<ConversationPreview>,
    onTogglePin: (ConversationPreview) -> Unit = {},
    onToggleRead: (ConversationPreview) -> Unit = {},
    onToggleMute: (ConversationPreview) -> Unit = {},
    onDeleteConversation: (ConversationPreview) -> Unit = {},
    onOpenConversation: (ConversationPreview) -> Unit = {},
    onOpenSettings: () -> Unit = {},
    onOpenContacts: () -> Unit = {},
    onOpenCalls: () -> Unit = {},
    onOpenNewGroup: () -> Unit = {}
) {
    val strings = LocalStrings.current
    fun t(key: String, fallback: String): String = strings.get(key, fallback)
    val query = remember { mutableStateOf("") }
    val listState = rememberLazyListState()
    val maxSearchHeight = 40.dp
    val density = LocalDensity.current
    val maxSearchPx = with(density) { maxSearchHeight.toPx() }
    var pullOffsetPx by remember { mutableStateOf(0f) }
    val searchHeight by animateDpAsState(
        targetValue = maxSearchHeight,
        animationSpec = tween(140)
    )
    val nestedScrollConnection = remember(listState, maxSearchPx) {
        object : NestedScrollConnection {
            override fun onPreScroll(available: Offset, source: NestedScrollSource): Offset {
                if (available.y < 0 && pullOffsetPx > 0f) {
                    val newOffset = (pullOffsetPx + available.y).coerceIn(0f, maxSearchPx)
                    val consumed = newOffset - pullOffsetPx
                    pullOffsetPx = newOffset
                    return Offset(0f, consumed)
                }
                return Offset.Zero
            }

            override fun onPostScroll(
                consumed: Offset,
                available: Offset,
                source: NestedScrollSource
            ): Offset {
                val atTop = listState.firstVisibleItemIndex == 0 &&
                    listState.firstVisibleItemScrollOffset == 0
                if (available.y > 0 && atTop) {
                    val newOffset = (pullOffsetPx + available.y * 0.5f).coerceIn(0f, maxSearchPx)
                    val consumed = newOffset - pullOffsetPx
                    pullOffsetPx = newOffset
                    return Offset(0f, consumed)
                }
                if (!atTop && pullOffsetPx > 0f) {
                    pullOffsetPx = 0f
                }
                return Offset.Zero
            }

            override suspend fun onPreFling(available: Velocity): Velocity {
                if (pullOffsetPx > 0f) {
                    pullOffsetPx = 0f
                    return Velocity.Zero
                }
                return Velocity.Zero
            }
        }
    }
    var actionTarget by remember { mutableStateOf<ConversationPreview?>(null) }
    var toastMessage by remember { mutableStateOf<String?>(null) }
    var pendingDelete by remember { mutableStateOf<ConversationPreview?>(null) }
    var hiddenIds by remember { mutableStateOf(setOf<String>()) }
    fun commitDelete(item: ConversationPreview) {
        hiddenIds = hiddenIds - item.id
        pendingDelete = null
        onDeleteConversation(item)
    }
    fun requestDelete(item: ConversationPreview) {
        if (hiddenIds.contains(item.id)) {
            return
        }
        pendingDelete = item
        hiddenIds = hiddenIds + item.id
    }
    LaunchedEffect(toastMessage) {
        if (toastMessage != null) {
            delay(1800)
            toastMessage = null
        }
    }
    LaunchedEffect(pendingDelete?.id) {
        val current = pendingDelete ?: return@LaunchedEffect
        delay(3500)
        if (pendingDelete?.id == current.id) {
            commitDelete(current)
        }
    }

    Scaffold(
        topBar = {
            ConversationTopBar(
                title = tr("conversations_title", "Chats"),
                onNewGroup = onOpenNewGroup
            )
        },
        bottomBar = {
            ConversationBottomBar(
                activeTab = ConversationTab.Chats,
                onContacts = onOpenContacts,
                onChats = {},
                onCalls = onOpenCalls,
                onSettings = onOpenSettings
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .testTag("conversation-list-screen")
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 6.dp, vertical = 0.dp)
            ) {
                CompactSearchField(
                    value = query.value,
                    onValueChange = { query.value = it },
                    placeholder = tr("conversations_search", "Search chats"),
                    height = searchHeight,
                    modifier = Modifier
                        .fillMaxWidth()
                        .testTag("conversation-search")
                )
                Spacer(modifier = Modifier.height(1.dp))
                val search = query.value.trim()
                val searched = if (search.isBlank()) {
                    conversations.filterNot { hiddenIds.contains(it.id) }
                } else {
                    conversations.filterNot { hiddenIds.contains(it.id) }.filter {
                        it.name.contains(search, ignoreCase = true) ||
                            it.lastMessage.contains(search, ignoreCase = true) ||
                            (it.draft?.contains(search, ignoreCase = true) == true)
                    }
                }
                val pinned = searched
                    .filter { it.isPinned }
                    .sortedWith(compareByDescending<ConversationPreview> { parseConversationTime(it.time) })
                val others = searched
                    .filterNot { it.isPinned }
                    .sortedWith(
                        compareBy<ConversationPreview> { conversationPriority(it) }
                            .thenByDescending { parseConversationTime(it.time) }
                    )
                val hasResults = pinned.isNotEmpty() || others.isNotEmpty()
                LazyColumn(
                    verticalArrangement = Arrangement.spacedBy(0.dp),
                    modifier = Modifier
                        .fillMaxSize()
                        .nestedScroll(nestedScrollConnection),
                    state = listState
                ) {
                    if (pinned.isNotEmpty()) {
                        items(pinned, key = { it.id }) { item ->
                            SwipeRevealConversation(
                                item = item,
                                onOpenConversation = onOpenConversation,
                                onLongPress = { target -> actionTarget = target },
                                onTogglePin = { target ->
                                    onTogglePin(target)
                                    toastMessage = if (item.isPinned) {
                                        t("conversations_toast_unpinned", "Unpinned")
                                    } else {
                                        t("conversations_toast_pinned", "Pinned")
                                    }
                                },
                                onToggleRead = { target ->
                                    onToggleRead(target)
                                    toastMessage = if (item.unreadCount > 0 || item.mentionCount > 0) {
                                        t("conversations_toast_marked_read", "Marked read")
                                    } else {
                                        t("conversations_toast_marked_unread", "Marked unread")
                                    }
                                },
                                onDelete = { target -> requestDelete(target) }
                            )
                        }
                    }
                    if (others.isNotEmpty()) {
                        items(others, key = { it.id }) { item ->
                            SwipeRevealConversation(
                                item = item,
                                onOpenConversation = onOpenConversation,
                                onLongPress = { target -> actionTarget = target },
                                onTogglePin = { target ->
                                    onTogglePin(target)
                                    toastMessage = if (item.isPinned) {
                                        t("conversations_toast_unpinned", "Unpinned")
                                    } else {
                                        t("conversations_toast_pinned", "Pinned")
                                    }
                                },
                                onToggleRead = { target ->
                                    onToggleRead(target)
                                    toastMessage = if (item.unreadCount > 0 || item.mentionCount > 0) {
                                        t("conversations_toast_marked_read", "Marked read")
                                    } else {
                                        t("conversations_toast_marked_unread", "Marked unread")
                                    }
                                },
                                onDelete = { target -> requestDelete(target) }
                            )
                        }
                    }
                    if (!hasResults) {
                        item(key = "conversation-empty-state") {
                            ConversationListEmptyState(
                                query = search,
                                modifier = Modifier
                                    .fillParentMaxHeight(0.38f)
                                    .fillMaxWidth()
                                    .padding(horizontal = 14.dp, vertical = 6.dp)
                            )
                        }
                    }
                }
            }
            if (actionTarget != null) {
                ConversationActionSheet(
                    item = actionTarget!!,
                    onDismiss = { actionTarget = null },
                    onAction = { action ->
                        val target = actionTarget ?: return@ConversationActionSheet
                        when (action.id) {
                            "pin" -> {
                                onTogglePin(target)
                                toastMessage = if (target.isPinned) {
                                    t("conversations_toast_unpinned", "Unpinned")
                                } else {
                                    t("conversations_toast_pinned", "Pinned")
                                }
                            }
                            "mute" -> {
                                onToggleMute(target)
                                toastMessage = if (target.isMuted) {
                                    t("conversations_toast_unmuted", "Unmuted")
                                } else {
                                    t("conversations_toast_muted", "Muted")
                                }
                            }
                            "read" -> {
                                onToggleRead(target)
                                toastMessage = if (target.unreadCount > 0 || target.mentionCount > 0) {
                                    t("conversations_toast_marked_read", "Marked read")
                                } else {
                                    t("conversations_toast_marked_unread", "Marked unread")
                                }
                            }
                            "delete" -> requestDelete(target)
                        }
                        actionTarget = null
                    }
                )
            }
            if (pendingDelete != null) {
                UndoPill(
                    text = tr("conversations_toast_deleted", "Deleted"),
                    actionLabel = tr("chat_undo", "Undo"),
                    onAction = {
                        val restore = pendingDelete ?: return@UndoPill
                        hiddenIds = hiddenIds - restore.id
                        pendingDelete = null
                        toastMessage = t("conversations_toast_restored", "Restored")
                    },
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = 24.dp)
                )
            } else if (toastMessage != null) {
                ToastPill(
                    text = toastMessage!!,
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = 24.dp)
                )
            }
        }
    }
}

enum class ConversationTab {
    Contacts,
    Chats,
    Calls,
    Settings
}

@Composable
private fun ConversationListEmptyState(
    query: String,
    modifier: Modifier = Modifier
) {
    val title = if (query.isNotBlank()) {
        tr("conversations_search_empty_title", "No matching chats")
    } else {
        tr("conversations_empty_title", "No chats yet")
    }
    val hint = if (query.isNotBlank()) {
        tr("conversations_search_empty_hint", "Try another keyword or clear search.")
    } else {
        tr("conversations_empty_hint", "Start a secure chat from Contacts.")
    }
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.18f)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 18.dp, vertical = 18.dp),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            UiSemanticIcon(
                icon = MiOwnedIcons.Search,
                contentDescription = title,
                tone = UiIconTone.Primary,
                size = ChatUiTokens.IconContainerSm,
                iconSize = ChatUiTokens.IconGlyphSm
            )
            Spacer(modifier = Modifier.height(8.dp))
            Text(
                text = title,
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurface
            )
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text = hint,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ConversationTopBar(
    title: String,
    onNewGroup: () -> Unit,
    modifier: Modifier = Modifier
) {
    TopAppBar(
        modifier = modifier
            .fillMaxWidth()
            .height(56.dp),
        title = {
            Text(
                text = title,
                style = MaterialTheme.typography.titleMedium
            )
        },
        actions = {
            UiToolbarIconButton(
                icon = MiOwnedIcons.Add,
                contentDescription = tr("conversations_quick_new_group", "New group"),
                tone = UiIconTone.Primary,
                onClick = onNewGroup
            )
        },
        colors = TopAppBarDefaults.topAppBarColors(
            containerColor = MaterialTheme.colorScheme.surface.copy(alpha = 0.86f),
            titleContentColor = MaterialTheme.colorScheme.onSurface,
            actionIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant
        )
    )
}

@Composable
fun ConversationBottomBar(
    activeTab: ConversationTab,
    onContacts: () -> Unit,
    onChats: () -> Unit,
    onCalls: () -> Unit,
    onSettings: () -> Unit,
    modifier: Modifier = Modifier
) {
    NavigationBar(
        modifier = modifier
            .fillMaxWidth()
            .height(56.dp)
            .navigationBarsPadding(),
        containerColor = MaterialTheme.colorScheme.surface.copy(alpha = 0.96f),
        tonalElevation = 0.dp
    ) {
        ConversationBottomNavItem(
            selected = activeTab == ConversationTab.Contacts,
            onClick = onContacts,
            icon = MiOwnedIcons.Person,
            label = tr("nav_contacts", "Contacts")
        )
        ConversationBottomNavItem(
            selected = activeTab == ConversationTab.Chats,
            onClick = onChats,
            icon = MiOwnedIcons.Chat,
            label = tr("nav_chats", "Chats")
        )
        ConversationBottomNavItem(
            selected = activeTab == ConversationTab.Calls,
            onClick = onCalls,
            icon = MiOwnedIcons.Call,
            label = tr("nav_calls", "Calls")
        )
        ConversationBottomNavItem(
            selected = activeTab == ConversationTab.Settings,
            onClick = onSettings,
            icon = MiOwnedIcons.Settings,
            label = tr("nav_settings", "Settings")
        )
    }
}

@Composable
private fun RowScope.ConversationBottomNavItem(
    selected: Boolean,
    onClick: () -> Unit,
    icon: ImageVector,
    label: String
) {
    val selectedColor = MaterialTheme.colorScheme.onSurface
    val unselectedColor = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.78f)
    val contentColor = if (selected) selectedColor else unselectedColor
    Column(
        modifier = Modifier
            .weight(1f)
            .clip(RoundedCornerShape(12.dp))
            .clickable(onClick = onClick)
            .background(
                if (selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.05f)
                else Color.Transparent
            )
            .padding(vertical = 3.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Icon(
            imageVector = icon,
            contentDescription = label,
            tint = contentColor,
            modifier = Modifier.size(ChatUiTokens.IconGlyphLg)
        )
        Spacer(modifier = Modifier.height(2.dp))
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = contentColor
        )
    }
}

@Composable
private fun CompactSearchField(
    value: String,
    onValueChange: (String) -> Unit,
    placeholder: String,
    height: Dp,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier.height(height),
        shape = RoundedCornerShape(14.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.58f),
        border = androidx.compose.foundation.BorderStroke(
            width = 1.dp,
            color = MaterialTheme.colorScheme.outline.copy(alpha = 0.12f)
        )
    ) {
        BasicTextField(
            value = value,
            onValueChange = onValueChange,
            singleLine = true,
            textStyle = MaterialTheme.typography.bodyMedium.copy(
                color = MaterialTheme.colorScheme.onSurface,
                lineHeight = 18.sp
            ),
            cursorBrush = SolidColor(MaterialTheme.colorScheme.primary),
            modifier = Modifier.fillMaxWidth(),
            decorationBox = { innerTextField ->
                Row(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(horizontal = 10.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(
                        imageVector = MiOwnedIcons.Search,
                        contentDescription = tr("conversations_search_icon", "Search"),
                        modifier = Modifier.size(14.dp),
                        tint = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Box(
                        modifier = Modifier.fillMaxWidth(),
                        contentAlignment = Alignment.CenterStart
                    ) {
                        if (value.isEmpty()) {
                            Text(
                                text = placeholder,
                                style = MaterialTheme.typography.bodySmall.copy(lineHeight = 18.sp),
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                        innerTextField()
                    }
                }
            }
        )
    }
}

private fun conversationPriority(item: ConversationPreview): Int {
    return when {
        item.isMuted && item.isGroup -> 2
        item.isMuted -> 1
        else -> 0
    }
}

private fun parseConversationTime(label: String): Long {
    val match = TIME_PATTERN.find(label.trim())
    if (match != null) {
        val hours = match.groupValues[1].toIntOrNull() ?: 0
        val minutes = match.groupValues[2].toIntOrNull() ?: 0
        val seconds = match.groupValues.getOrNull(3)?.toIntOrNull() ?: 0
        return (hours * 3600 + minutes * 60 + seconds).toLong()
    }
    val normalized = label.trim().lowercase()
    if (normalized.contains("yesterday") || normalized.contains("昨天")) {
        return -1
    }
    return when {
        normalized.contains("mon") || normalized.contains("周一") -> -2
        normalized.contains("tue") || normalized.contains("周二") -> -3
        normalized.contains("wed") || normalized.contains("周三") -> -4
        normalized.contains("thu") || normalized.contains("周四") -> -5
        normalized.contains("fri") || normalized.contains("周五") -> -6
        normalized.contains("sat") || normalized.contains("周六") -> -7
        normalized.contains("sun") || normalized.contains("周日") -> -8
        else -> Long.MIN_VALUE
    }
}

private val TIME_PATTERN = Regex("""(\d{1,2}):(\d{2})(?::(\d{2}))?""")

@Composable
private fun QuickActionCard(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    modifier: Modifier = Modifier,
    onClick: () -> Unit = {}
) {
    Card(
        modifier = modifier.clickable { onClick() },
        shape = RoundedCornerShape(14.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.2f)
        )
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center
        ) {
            Icon(icon, contentDescription = label, tint = MaterialTheme.colorScheme.primary)
            Spacer(modifier = Modifier.width(8.dp))
            Text(text = label, style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
private fun SwipeRevealConversation(
    item: ConversationPreview,
    onOpenConversation: (ConversationPreview) -> Unit,
    onLongPress: (ConversationPreview) -> Unit,
    onTogglePin: (ConversationPreview) -> Unit,
    onToggleRead: (ConversationPreview) -> Unit,
    onDelete: (ConversationPreview) -> Unit
) {
    val scope = rememberCoroutineScope()
    val haptics = LocalHapticFeedback.current
    val revealWidth = 170.dp
    val revealPx = with(androidx.compose.ui.platform.LocalDensity.current) { revealWidth.toPx() }
    val offset = remember(item.id) { Animatable(0f) }
    val dragState = rememberDraggableState { delta ->
        val newOffset = (offset.value + delta).coerceIn(-revealPx, 0f)
        scope.launch { offset.snapTo(newOffset) }
    }
    val revealProgress = (abs(offset.value) / revealPx).coerceIn(0f, 1f)
    Box(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .fillMaxSize()
                .graphicsLayer { alpha = revealProgress }
                .background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.16f))
                .padding(end = 12.dp),
            horizontalArrangement = Arrangement.End,
            verticalAlignment = Alignment.CenterVertically
        ) {
            SwipeActionButton(
                icon = MiOwnedIcons.Pin,
                label = if (item.isPinned) {
                    tr("conversations_action_unpin", "Unpin")
                } else {
                    tr("conversations_action_pin", "Pin")
                },
                tint = MaterialTheme.colorScheme.primary,
                onClick = {
                    haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                    onTogglePin(item)
                    scope.launch { offset.animateTo(0f, tween(160)) }
                }
            )
            Spacer(modifier = Modifier.width(8.dp))
            SwipeActionButton(
                icon = MiOwnedIcons.CheckDouble,
                label = if (item.unreadCount > 0 || item.mentionCount > 0) {
                    tr("conversations_action_read", "Read")
                } else {
                    tr("conversations_action_unread", "Unread")
                },
                tint = MaterialTheme.colorScheme.secondary,
                onClick = {
                    haptics.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                    onToggleRead(item)
                    scope.launch { offset.animateTo(0f, tween(160)) }
                }
            )
            Spacer(modifier = Modifier.width(8.dp))
            SwipeActionButton(
                icon = MiOwnedIcons.Delete,
                label = tr("conversations_action_delete", "Delete"),
                tint = MaterialTheme.colorScheme.error,
                onClick = {
                    haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                    onDelete(item)
                }
            )
        }
        ConversationRow(
            item = item,
            onClick = { onOpenConversation(item) },
            onLongPress = { onLongPress(item) },
            modifier = Modifier
                .offset { IntOffset(offset.value.roundToInt(), 0) }
                .draggable(
                    state = dragState,
                    orientation = Orientation.Horizontal,
                    onDragStopped = { velocity ->
                        val shouldOpen = abs(offset.value) > revealPx * 0.4f || velocity < -900f
                        val target = if (shouldOpen) -revealPx else 0f
                        offset.animateTo(target, tween(180))
                    }
                )
        )
    }
}

@Composable
private fun SwipeActionButton(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    tint: Color,
    onClick: () -> Unit
) {
    Column(
        modifier = Modifier
            .clip(RoundedCornerShape(14.dp))
            .clickable { onClick() }
            .padding(horizontal = 4.dp, vertical = 4.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(
            modifier = Modifier
                .size(ChatUiTokens.IconContainerMd)
                .background(tint.copy(alpha = 0.1f), CircleShape),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = icon,
                contentDescription = label,
                tint = tint,
                modifier = Modifier.size(ChatUiTokens.IconGlyphSm)
            )
        }
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = tint
        )
    }
}

@Composable
private fun ConversationStatusGlyph(
    icon: ImageVector,
    contentDescription: String,
    tone: UiIconTone,
    modifier: Modifier = Modifier
) {
    UiStatusIconBadge(
        icon = icon,
        contentDescription = contentDescription,
        modifier = modifier,
        tone = tone,
        framed = true
    )
}

@Composable
@OptIn(ExperimentalFoundationApi::class)
private fun ConversationRow(
    item: ConversationPreview,
    onClick: () -> Unit,
    onLongPress: () -> Unit,
    modifier: Modifier = Modifier
) {
    val haptics = LocalHapticFeedback.current
    Card(
        modifier = modifier.combinedClickable(
            onClick = onClick,
            onLongClick = {
                haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                onLongPress()
            }
        ),
        shape = RoundedCornerShape(0.dp),
        colors = CardDefaults.cardColors(containerColor = Color.Transparent)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(72.dp)
                    .padding(horizontal = 8.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Box {
                    AvatarBadge(
                        initials = item.initials,
                        tint = MaterialTheme.colorScheme.primary,
                        size = 48.dp
                    )
                    if (item.isGroup) {
                        UiSemanticIcon(
                            icon = MiOwnedIcons.Group,
                            contentDescription = tr("conversations_group", "Group"),
                            modifier = Modifier
                                .align(Alignment.BottomEnd)
                                .size(16.dp),
                            tone = UiIconTone.Primary,
                            size = 16.dp,
                            cornerRadius = 8.dp,
                            iconSize = 10.dp,
                            framed = false
                        )
                    }
                }
                Spacer(modifier = Modifier.width(10.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            text = item.name,
                            style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.SemiBold),
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                    Spacer(modifier = Modifier.height(1.dp))
                    val previewText = when {
                        item.draft != null ->
                            tr("conversations_draft_prefix", "Draft: %s").format(item.draft)
                        item.isTyping -> tr("conversations_typing", "Typing...")
                        else -> item.lastMessage
                    }
                    val previewColor = when {
                        item.draft != null -> MaterialTheme.colorScheme.primary
                        item.isTyping -> MaterialTheme.colorScheme.primary
                        else -> MaterialTheme.colorScheme.onSurfaceVariant
                    }
                    Text(
                        text = previewText,
                        style = MaterialTheme.typography.bodyMedium,
                        color = previewColor,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
                Spacer(modifier = Modifier.width(8.dp))
                Column(horizontalAlignment = Alignment.End) {
                    Text(
                        text = item.time,
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Spacer(modifier = Modifier.height(3.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        val hasBadges = item.isMuted || item.mentionCount > 0 || item.unreadCount > 0
                        if (item.isMuted) {
                            ConversationStatusGlyph(
                                icon = MiOwnedIcons.BellOff,
                                contentDescription = tr("conversations_muted", "Muted"),
                                tone = UiIconTone.Neutral
                            )
                            Spacer(modifier = Modifier.width(4.dp))
                        }
                        if (item.mentionCount > 0) {
                            UiStatusCountBadge(
                                label = "@${item.mentionCount}",
                                tone = UiBadgeTone.Warning
                            )
                            Spacer(modifier = Modifier.width(4.dp))
                        }
                        if (item.unreadCount > 0) {
                            UiStatusCountBadge(
                                label = if (item.unreadCount > 9) "9+" else item.unreadCount.toString(),
                                tone = UiBadgeTone.Primary
                            )
                        }
                        if (item.isPinned) {
                            if (hasBadges) {
                                Spacer(modifier = Modifier.width(4.dp))
                            }
                            ConversationStatusGlyph(
                                icon = MiOwnedIcons.Pin,
                                contentDescription = tr("conversations_pinned", "Pinned"),
                                tone = UiIconTone.Neutral
                            )
                        }
                    }
                }
            }
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(1.dp)
                    .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.06f))
            )
        }
    }
}

@Composable
private fun ConversationActionSheet(
    item: ConversationPreview,
    onDismiss: () -> Unit,
    onAction: (ConversationAction) -> Unit
) {
    val actions = listOf(
        ConversationAction(
            id = "pin",
            label = if (item.isPinned) {
                tr("conversations_action_unpin", "Unpin")
            } else {
                tr("conversations_action_pin", "Pin")
            },
            icon = MiOwnedIcons.Pin
        ),
        ConversationAction(
            id = "mute",
            label = if (item.isMuted) {
                tr("conversations_action_unmute", "Unmute")
            } else {
                tr("conversations_action_mute", "Mute")
            },
            icon = if (item.isMuted) MiOwnedIcons.Bell else MiOwnedIcons.BellOff
        ),
        ConversationAction(
            id = "read",
            label = if (item.unreadCount > 0 || item.mentionCount > 0) {
                tr("conversations_action_mark_read", "Mark read")
            } else {
                tr("conversations_action_mark_unread", "Mark unread")
            },
            icon = MiOwnedIcons.CheckDouble
        ),
        ConversationAction(
            id = "delete",
            label = tr("conversations_action_delete", "Delete"),
            icon = MiOwnedIcons.Delete,
            isDestructive = true
        )
    )

    Box(modifier = Modifier.fillMaxSize()) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.32f))
                .clickable(onClick = onDismiss)
        )
        Column(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .clip(RoundedCornerShape(topStart = 24.dp, topEnd = 24.dp))
                .background(MaterialTheme.colorScheme.surface)
                .padding(horizontal = 16.dp, vertical = 14.dp)
        ) {
            Box(
                modifier = Modifier
                    .align(Alignment.CenterHorizontally)
                    .width(36.dp)
                    .height(4.dp)
                    .background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(4.dp))
            )
            Spacer(modifier = Modifier.height(12.dp))
            ConversationSheetPreview(item)
            Spacer(modifier = Modifier.height(12.dp))
            actions.forEach { action ->
                ConversationActionRow(
                    action = action,
                    onClick = { onAction(action) }
                )
            }
        }
    }
}

@Composable
private fun ConversationSheetPreview(item: ConversationPreview) {
    val previewText = when {
        item.draft != null ->
            tr("conversations_draft_prefix", "Draft: %s").format(item.draft)
        item.isTyping -> tr("conversations_typing", "Typing...")
        else -> item.lastMessage
    }
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(MaterialTheme.colorScheme.surfaceVariant)
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        Text(
            text = item.name,
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurface
        )
        Spacer(modifier = Modifier.height(6.dp))
        Text(
            text = previewText,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
private fun ConversationActionRow(
    action: ConversationAction,
    onClick: () -> Unit
) {
    val tint = if (action.isDestructive) MaterialTheme.colorScheme.error
    else MaterialTheme.colorScheme.onSurface
    ListItem(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .clickable { onClick() },
        colors = ListItemDefaults.colors(containerColor = Color.Transparent),
        leadingContent = {
            Icon(
                imageVector = action.icon,
                contentDescription = action.label,
                tint = tint
            )
        },
        headlineContent = {
            Text(
                text = action.label,
                style = MaterialTheme.typography.bodyLarge,
                color = tint
            )
        }
    )
}

@Composable
private fun ToastPill(text: String, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        shadowElevation = 2.dp
    ) {
        Text(
            text = text,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp),
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurface
        )
    }
}

@Composable
private fun UndoPill(
    text: String,
    actionLabel: String,
    onAction: () -> Unit,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        shadowElevation = 2.dp
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = text,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurface
            )
            Spacer(modifier = Modifier.width(12.dp))
            Text(
                text = actionLabel,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.clickable { onAction() }
            )
        }
    }
}

internal fun sampleConversations(): List<ConversationPreview> {
    return listOf(
        ConversationPreview(
            id = "c1",
            initials = "AS",
            name = "Aster Stone",
            lastMessage = "Uploading now. The top risk is retry storms.",
            time = "09:12",
            unreadCount = 2,
            isPinned = true,
            isMuted = false,
            isGroup = false,
            isTyping = true,
            mentionCount = 1
        ),
        ConversationPreview(
            id = "c2",
            initials = "DO",
            name = "Design Ops",
            lastMessage = "Meeting link updated in files channel.",
            time = "08:55",
            unreadCount = 0,
            isPinned = true,
            isMuted = true,
            isGroup = true,
            isTyping = false
        ),
        ConversationPreview(
            id = "c3",
            initials = "LN",
            name = "Lena Novak",
            lastMessage = "Thanks. I will test the flow tonight.",
            time = "Yesterday",
            unreadCount = 0,
            isPinned = false,
            isMuted = false,
            isGroup = false,
            isTyping = false,
            draft = "Need to share the report"
        ),
        ConversationPreview(
            id = "c4",
            initials = "QT",
            name = "Project Aurora",
            lastMessage = "Rin: Updated the privacy copy.",
            time = "Yesterday",
            unreadCount = 5,
            isPinned = false,
            isMuted = false,
            isGroup = true,
            isTyping = false,
            mentionCount = 2
        ),
        ConversationPreview(
            id = "c5",
            initials = "HR",
            name = "Helio Risk",
            lastMessage = "Draft: Backoff plan is ready.",
            time = "Mon",
            unreadCount = 0,
            isPinned = false,
            isMuted = true,
            isGroup = false,
            isTyping = false
        ),
        ConversationPreview(
            id = "c6",
            initials = "OS",
            name = "Ops Sync",
            lastMessage = "Queue cap increased to 512.",
            time = "08:12",
            unreadCount = 5,
            isPinned = false,
            isMuted = false,
            isGroup = true,
            isTyping = false,
            mentionCount = 2
        ),
        ConversationPreview(
            id = "c7",
            initials = "PT",
            name = "Platform",
            lastMessage = "Smoke gate passed on API33 with the latest fix.",
            time = "07:42",
            unreadCount = 0,
            isPinned = false,
            isMuted = false,
            isGroup = true,
            isTyping = false
        )
    )
}

@Preview(showBackground = true, widthDp = 412, heightDp = 915)
@Composable
private fun ConversationListPreview() {
    ChatTheme { ConversationListScreen(conversations = sampleConversations()) }
}
