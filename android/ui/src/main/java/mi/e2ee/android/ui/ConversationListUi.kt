package mi.e2ee.android.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.tween
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Group
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.NotificationsOff
import androidx.compose.material.icons.filled.PushPin
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.DoneAll
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.Velocity
import androidx.compose.ui.unit.dp
import kotlin.math.abs
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay
import mi.e2ee.android.R

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
    val showSearch = pullOffsetPx > 2f
    val searchHeight by animateDpAsState(
        targetValue = if (showSearch) maxSearchHeight else 0.dp,
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
                onSettings = onOpenSettings
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 16.dp, vertical = 8.dp)
            ) {
                val showSearchNow = searchHeight > 0.dp
                CompactSearchField(
                    value = query.value,
                    onValueChange = { query.value = it },
                    placeholder = tr("conversations_search", "Search chats"),
                    height = searchHeight,
                    modifier = Modifier.fillMaxWidth()
                )
                if (showSearchNow) {
                    Spacer(modifier = Modifier.height(12.dp))
                }
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
    Settings
}

@Composable
private fun ConversationTopBar(
    title: String,
    onNewGroup: () -> Unit,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier
            .fillMaxWidth()
            .shadow(6.dp, RoundedCornerShape(bottomStart = 20.dp, bottomEnd = 20.dp)),
        color = MaterialTheme.colorScheme.surface.copy(alpha = 0.96f),
        tonalElevation = 2.dp
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(48.dp)
                .padding(horizontal = 16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onSurface
            )
            Spacer(modifier = Modifier.weight(1f))
            CompactIconButton(
                painter = painterResource(R.drawable.ic_3d_compose),
                contentDescription = tr("conversations_quick_new_group", "New group"),
                onClick = onNewGroup
            )
        }
    }
}

@Composable
fun ConversationBottomBar(
    activeTab: ConversationTab,
    onContacts: () -> Unit,
    onChats: () -> Unit,
    onSettings: () -> Unit,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier.fillMaxWidth(),
        color = MaterialTheme.colorScheme.surface.copy(alpha = 0.98f),
        tonalElevation = 3.dp,
        shadowElevation = 8.dp
    ) {
        val contactsPainter = painterResource(R.drawable.ic_3d_contacts)
        val chatsPainter = painterResource(R.drawable.ic_3d_chats)
        val settingsPainter = painterResource(R.drawable.ic_3d_settings)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(58.dp)
                .padding(horizontal = 12.dp),
            horizontalArrangement = Arrangement.SpaceEvenly,
            verticalAlignment = Alignment.CenterVertically
        ) {
            BottomNavItem(
                label = tr("nav_contacts", "Contacts"),
                painter = contactsPainter,
                selected = activeTab == ConversationTab.Contacts,
                onClick = onContacts
            )
            BottomNavItem(
                label = tr("nav_chats", "Chats"),
                painter = chatsPainter,
                selected = activeTab == ConversationTab.Chats,
                onClick = onChats
            )
            BottomNavItem(
                label = tr("nav_settings", "Settings"),
                painter = settingsPainter,
                selected = activeTab == ConversationTab.Settings,
                onClick = onSettings
            )
        }
    }
}

@Composable
private fun BottomNavItem(
    label: String,
    painter: Painter,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val tint = if (selected) MaterialTheme.colorScheme.primary
    else MaterialTheme.colorScheme.onSurfaceVariant
    val badgeColor = if (selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.18f)
    else MaterialTheme.colorScheme.surfaceVariant
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(16.dp))
            .clickable { onClick() }
            .padding(horizontal = 10.dp, vertical = 6.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        ThreeDIconBadge(
            painter = painter,
            contentDescription = label,
            background = badgeColor,
            size = 28.dp,
            iconSize = 18.dp,
            shadow = if (selected) 8.dp else 5.dp
        )
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = tint
        )
    }
}

@Composable
private fun ThreeDIconBadge(
    painter: Painter,
    contentDescription: String,
    background: Color,
    size: Dp,
    iconSize: Dp,
    shadow: Dp,
    overlaySlash: Boolean = false,
    modifier: Modifier = Modifier
) {
    val density = LocalDensity.current
    val sizePx = with(density) { size.toPx() }
    val highlight = background.copy(alpha = 0.95f)
    val shade = background.copy(alpha = 0.55f)
    Box(
        modifier = modifier
            .size(size)
            .shadow(shadow, CircleShape, clip = false)
            .clip(CircleShape)
            .background(
                brush = Brush.radialGradient(
                    colors = listOf(Color.White.copy(alpha = 0.35f), highlight, shade),
                    center = Offset(sizePx * 0.3f, sizePx * 0.25f),
                    radius = sizePx
                ),
                shape = CircleShape
            )
            .border(1.dp, Color.White.copy(alpha = 0.18f), CircleShape),
        contentAlignment = Alignment.Center
    ) {
        Box(
            modifier = Modifier
                .align(Alignment.TopStart)
                .offset(x = 2.dp, y = 2.dp)
                .size(size * 0.32f)
                .background(Color.White.copy(alpha = 0.18f), CircleShape)
        )
        Image(
            painter = painter,
            contentDescription = contentDescription,
            modifier = Modifier.size(iconSize),
            contentScale = ContentScale.Fit
        )
        if (overlaySlash) {
            Canvas(modifier = Modifier.size(iconSize)) {
                val stroke = 2.dp.toPx()
                drawLine(
                    color = MaterialTheme.colorScheme.error.copy(alpha = 0.9f),
                    start = Offset(0f, size.height),
                    end = Offset(size.width, 0f),
                    strokeWidth = stroke,
                    cap = StrokeCap.Round
                )
            }
        }
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
        color = MaterialTheme.colorScheme.surfaceVariant
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(height)
                .padding(horizontal = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                imageVector = Icons.Filled.Search,
                contentDescription = tr("conversations_search_icon", "Search"),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.size(16.dp)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Box(modifier = Modifier.weight(1f)) {
                if (value.isBlank()) {
                    Text(
                        text = placeholder,
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                BasicTextField(
                    value = value,
                    onValueChange = onValueChange,
                    singleLine = true,
                    textStyle = MaterialTheme.typography.bodyMedium.copy(
                        color = MaterialTheme.colorScheme.onSurface
                    ),
                    cursorBrush = SolidColor(MaterialTheme.colorScheme.primary),
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }
    }
}

@Composable
private fun CompactIconButton(
    painter: Painter,
    contentDescription: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .clip(CircleShape)
            .clickable { onClick() },
        contentAlignment = Alignment.Center
    ) {
        ThreeDIconBadge(
            painter = painter,
            contentDescription = contentDescription,
            background = MaterialTheme.colorScheme.surfaceVariant,
            size = 30.dp,
            iconSize = 18.dp,
            shadow = 6.dp
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
        shape = RoundedCornerShape(18.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
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
    Box(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .fillMaxSize()
                .background(MaterialTheme.colorScheme.surfaceVariant)
                .padding(end = 12.dp),
            horizontalArrangement = Arrangement.End,
            verticalAlignment = Alignment.CenterVertically
        ) {
            SwipeActionButton(
                icon = Icons.Filled.PushPin,
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
                icon = Icons.Filled.DoneAll,
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
                icon = Icons.Filled.Delete,
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
                .size(42.dp)
                .background(tint.copy(alpha = 0.15f), CircleShape),
            contentAlignment = Alignment.Center
        ) {
            Icon(imageVector = icon, contentDescription = label, tint = tint)
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
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 14.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Box {
                    AvatarBadge(initials = item.initials, tint = MaterialTheme.colorScheme.primary)
                    if (item.isGroup) {
                        Box(
                            modifier = Modifier
                                .align(Alignment.BottomEnd)
                                .size(16.dp)
                                .background(MaterialTheme.colorScheme.surface, RoundedCornerShape(8.dp)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Filled.Group,
                                contentDescription = tr("conversations_group", "Group"),
                                tint = MaterialTheme.colorScheme.primary,
                                modifier = Modifier.size(12.dp)
                            )
                        }
                    }
                }
                Spacer(modifier = Modifier.width(12.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            text = item.name,
                            style = MaterialTheme.typography.bodyLarge.copy(fontWeight = FontWeight.SemiBold)
                        )
                    }
                    Spacer(modifier = Modifier.height(4.dp))
                    val previewText = when {
                        item.draft != null ->
                            tr("conversations_draft_prefix", "Draft: %s").format(item.draft)
                        item.isTyping -> tr("conversations_typing", "Typing...")
                        else -> item.lastMessage
                    }
                    val previewColor = when {
                        item.draft != null -> MaterialTheme.colorScheme.error
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
                    Spacer(modifier = Modifier.height(6.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        val mutePainter = painterResource(R.drawable.ic_3d_mute)
                        val pinPainter = painterResource(R.drawable.ic_3d_pin)
                        val hasBadges = item.isMuted || item.mentionCount > 0 || item.unreadCount > 0
                        if (item.isMuted) {
                            ThreeDIconBadge(
                                painter = mutePainter,
                                contentDescription = tr("conversations_muted", "Muted"),
                                background = MaterialTheme.colorScheme.surfaceVariant,
                                size = 18.dp,
                                iconSize = 12.dp,
                                shadow = 4.dp,
                                overlaySlash = true
                            )
                            Spacer(modifier = Modifier.width(6.dp))
                        }
                        if (item.mentionCount > 0) {
                            Box(
                                modifier = Modifier
                                    .background(
                                        MaterialTheme.colorScheme.tertiary,
                                        RoundedCornerShape(10.dp)
                                    )
                                    .padding(horizontal = 6.dp, vertical = 2.dp)
                            ) {
                                Text(
                                    text = "@${item.mentionCount}",
                                    style = MaterialTheme.typography.labelSmall,
                                    color = Color.White
                                )
                            }
                            Spacer(modifier = Modifier.width(6.dp))
                        }
                        if (item.unreadCount > 0) {
                            Box(
                                modifier = Modifier
                                    .background(MaterialTheme.colorScheme.primary, RoundedCornerShape(10.dp))
                                    .padding(horizontal = 6.dp, vertical = 2.dp)
                            ) {
                                Text(
                                    text = if (item.unreadCount > 9) "9+" else item.unreadCount.toString(),
                                    style = MaterialTheme.typography.labelSmall,
                                    color = Color.White
                                )
                            }
                        }
                        if (item.isPinned) {
                            if (hasBadges) {
                                Spacer(modifier = Modifier.width(6.dp))
                            }
                            ThreeDIconBadge(
                                painter = pinPainter,
                                contentDescription = tr("conversations_pinned", "Pinned"),
                                background = MaterialTheme.colorScheme.surfaceVariant,
                                size = 18.dp,
                                iconSize = 12.dp,
                                shadow = 4.dp
                            )
                        }
                    }
                }
            }
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(1.dp)
                    .background(MaterialTheme.colorScheme.surfaceVariant)
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
            icon = Icons.Filled.PushPin
        ),
        ConversationAction(
            id = "mute",
            label = if (item.isMuted) {
                tr("conversations_action_unmute", "Unmute")
            } else {
                tr("conversations_action_mute", "Mute")
            },
            icon = if (item.isMuted) Icons.Filled.Notifications else Icons.Filled.NotificationsOff
        ),
        ConversationAction(
            id = "read",
            label = if (item.unreadCount > 0 || item.mentionCount > 0) {
                tr("conversations_action_mark_read", "Mark read")
            } else {
                tr("conversations_action_mark_unread", "Mark unread")
            },
            icon = Icons.Filled.DoneAll
        ),
        ConversationAction(
            id = "delete",
            label = tr("conversations_action_delete", "Delete"),
            icon = Icons.Filled.Delete,
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
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .clickable { onClick() }
            .padding(horizontal = 8.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = action.icon,
            contentDescription = action.label,
            tint = tint
        )
        Spacer(modifier = Modifier.width(12.dp))
        Text(
            text = action.label,
            style = MaterialTheme.typography.bodyLarge,
            color = tint
        )
    }
}

@Composable
private fun ToastPill(text: String, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        shadowElevation = 6.dp
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
        shadowElevation = 6.dp
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
        )
    )
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun ConversationListPreview() {
    ChatTheme { ConversationListScreen(conversations = sampleConversations()) }
}
