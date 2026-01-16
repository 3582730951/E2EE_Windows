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
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Block
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Group
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.PersonAdd
import androidx.compose.material.icons.filled.QrCode
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp

private fun friendDisplayName(friend: FriendUi): String {
    return friend.remark.ifBlank { friend.username }
}

private fun friendInitials(friend: FriendUi): String {
    val name = friendDisplayName(friend).trim()
    if (name.isEmpty()) return "?"
    val parts = name.split(" ")
    val first = parts.firstOrNull()?.take(1) ?: "?"
    val second = parts.getOrNull(1)?.take(1) ?: name.drop(1).take(1)
    return (first + second).uppercase().take(2)
}

@Composable
fun ContactsApp() {
    ChatTheme {
        AddFriendScreen()
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AddFriendScreen(
    friends: List<FriendUi> = emptyList(),
    requests: List<FriendRequestUi> = emptyList(),
    onBack: () -> Unit = {},
    onOpenRequests: () -> Unit = {},
    onContactSelected: (FriendUi) -> Unit = {},
    onOpenChats: () -> Unit = {},
    onOpenSettings: () -> Unit = {},
    onSendRequest: (String, String) -> Unit = { _, _ -> },
    onAddFriend: (String, String) -> Unit = { _, _ -> },
    onJoinGroup: (String) -> Unit = {}
) {
    val query = remember { mutableStateOf("") }
    val usernameInput = remember { mutableStateOf("") }
    val remarkInput = remember { mutableStateOf("") }
    val groupIdInput = remember { mutableStateOf("") }
    val search = query.value.trim()
    val filtered = if (search.isBlank()) {
        friends
    } else {
        friends.filter { friend ->
            friend.username.contains(search, ignoreCase = true) ||
                friendDisplayName(friend).contains(search, ignoreCase = true) ||
                friend.status.contains(search, ignoreCase = true)
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("contacts_add_friend_title", "Add friend")) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(onClick = onOpenRequests) {
                        Icon(Icons.Filled.Notifications, contentDescription = tr("contacts_requests_title", "Requests"))
                    }
                    IconButton(onClick = {}) {
                        Icon(Icons.Filled.QrCode, contentDescription = tr("contacts_scan", "Scan"))
                    }
                }
            )
        },
        bottomBar = {
            ConversationBottomBar(
                activeTab = ConversationTab.Contacts,
                onContacts = {},
                onChats = onOpenChats,
                onSettings = onOpenSettings
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            OutlinedTextField(
                value = query.value,
                onValueChange = { query.value = it },
                modifier = Modifier.fillMaxWidth(),
                placeholder = { Text(tr("contacts_search_placeholder", "Search by username or phone")) },
                leadingIcon = {
                    Icon(Icons.Filled.Search, contentDescription = tr("contacts_search", "Search"))
                },
                shape = RoundedCornerShape(16.dp)
            )
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
                    Text(text = tr("contacts_add_friend_title", "Add friend"), style = MaterialTheme.typography.titleMedium)
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedTextField(
                        value = usernameInput.value,
                        onValueChange = { usernameInput.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("contacts_username", "Username")) },
                        leadingIcon = { Icon(Icons.Filled.PersonAdd, contentDescription = "Username") },
                        shape = RoundedCornerShape(16.dp),
                        singleLine = true
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedTextField(
                        value = remarkInput.value,
                        onValueChange = { remarkInput.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("contacts_remark", "Remark (optional)")) },
                        leadingIcon = { Icon(Icons.Filled.QrCode, contentDescription = "Remark") },
                        shape = RoundedCornerShape(16.dp),
                        singleLine = true
                    )
                    Spacer(modifier = Modifier.height(10.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilledTonalButton(
                            onClick = {
                                onSendRequest(usernameInput.value.trim(), remarkInput.value.trim())
                                usernameInput.value = ""
                                remarkInput.value = ""
                            },
                            modifier = Modifier.weight(1f),
                            enabled = usernameInput.value.isNotBlank(),
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                                contentColor = MaterialTheme.colorScheme.primary
                            )
                        ) {
                            Text(tr("contacts_send_request", "Send request"))
                        }
                        FilledTonalButton(
                            onClick = {
                                onAddFriend(usernameInput.value.trim(), remarkInput.value.trim())
                                usernameInput.value = ""
                                remarkInput.value = ""
                            },
                            modifier = Modifier.weight(1f),
                            enabled = usernameInput.value.isNotBlank(),
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.secondary.copy(alpha = 0.12f),
                                contentColor = MaterialTheme.colorScheme.secondary
                            )
                        ) {
                            Text(tr("contacts_add_action", "Add"))
                        }
                    }
                }
            }
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
                    Text(text = tr("contacts_join_group", "Join group"), style = MaterialTheme.typography.titleMedium)
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedTextField(
                        value = groupIdInput.value,
                        onValueChange = { groupIdInput.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("contacts_group_id", "Group id")) },
                        leadingIcon = { Icon(Icons.Filled.Group, contentDescription = "Group") },
                        shape = RoundedCornerShape(16.dp),
                        singleLine = true
                    )
                    Spacer(modifier = Modifier.height(10.dp))
                    FilledTonalButton(
                        onClick = {
                            onJoinGroup(groupIdInput.value.trim())
                            groupIdInput.value = ""
                        },
                        enabled = groupIdInput.value.isNotBlank(),
                        colors = ButtonDefaults.filledTonalButtonColors(
                            containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                            contentColor = MaterialTheme.colorScheme.primary
                        ),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(tr("contacts_join_group_action", "Join"))
                    }
                }
            }
            if (requests.isNotEmpty()) {
                Card(
                    shape = RoundedCornerShape(20.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
                    modifier = Modifier.clickable { onOpenRequests() }
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 16.dp, vertical = 14.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(Icons.Filled.Notifications, contentDescription = tr("contacts_requests_title", "Requests"))
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = tr("contacts_requests_count", "Requests (%d)").format(requests.size),
                            style = MaterialTheme.typography.bodyLarge
                        )
                    }
                }
            }
            SectionHeader(text = tr("contacts_friends", "Friends"))
            LazyColumn(
                verticalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxSize()
            ) {
                items(filtered) { friend ->
                    FriendRow(friend, onClick = { onContactSelected(friend) })
                }
            }
        }
    }
}

@Composable
private fun FriendRow(friend: FriendUi, onClick: () -> Unit) {
    Card(
        shape = RoundedCornerShape(18.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        modifier = Modifier.clickable { onClick() }
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            AvatarBadge(initials = friendInitials(friend), tint = MaterialTheme.colorScheme.primary)
            Spacer(modifier = Modifier.width(12.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(text = friendDisplayName(friend), style = MaterialTheme.typography.bodyLarge)
                Text(
                    text = friend.status,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            FilledTonalButton(
                onClick = onClick,
                colors = ButtonDefaults.filledTonalButtonColors(
                    containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.15f),
                    contentColor = MaterialTheme.colorScheme.primary
                )
            ) {
                Icon(Icons.Filled.ChevronRight, contentDescription = tr("contacts_open", "Open"))
                Spacer(modifier = Modifier.width(4.dp))
                Text(tr("contacts_open", "Open"))
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FriendRequestsScreen(
    requests: List<FriendRequestUi> = emptyList(),
    onBack: () -> Unit = {},
    onAccept: (FriendRequestUi) -> Unit = {},
    onDecline: (FriendRequestUi) -> Unit = {}
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("contacts_requests_title", "Friend requests")) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                }
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            items(requests) { contact ->
                RequestRow(contact, onAccept = { onAccept(contact) }, onDecline = { onDecline(contact) })
            }
        }
    }
}

@Composable
private fun RequestRow(request: FriendRequestUi, onAccept: () -> Unit, onDecline: () -> Unit) {
    Card(
        shape = RoundedCornerShape(18.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                AvatarBadge(
                    initials = friendInitials(FriendUi(request.username, request.remark, "")),
                    tint = MaterialTheme.colorScheme.primary
                )
                Spacer(modifier = Modifier.width(12.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = request.username, style = MaterialTheme.typography.bodyLarge)
                    Text(
                        text = request.remark.ifBlank { tr("contacts_request_note", "Friend request") },
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
            Spacer(modifier = Modifier.height(12.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                PrimaryButton(
                    label = tr("contacts_request_accept", "Accept"),
                    modifier = Modifier.weight(1f),
                    fillMaxWidth = false,
                    onClick = onAccept
                )
                SecondaryButton(
                    label = tr("contacts_request_decline", "Decline"),
                    modifier = Modifier.weight(1f),
                    fillMaxWidth = false,
                    onClick = onDecline
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ContactDetailScreen(
    friend: FriendUi,
    isBlocked: Boolean,
    onBack: () -> Unit = {},
    onMessage: () -> Unit = {},
    onDelete: () -> Unit = {},
    onToggleBlock: (Boolean) -> Unit = {},
    onUpdateRemark: (String) -> Unit = {}
) {
    val remark = remember(friend.username) { mutableStateOf(friend.remark) }
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("contacts_detail_title", "Contact")) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(onClick = {}) {
                        Icon(Icons.Filled.ChevronRight, contentDescription = tr("contacts_more", "More"))
                    }
                }
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth().padding(16.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    AvatarBadge(initials = friendInitials(friend), tint = MaterialTheme.colorScheme.primary, size = 64.dp)
                    Spacer(modifier = Modifier.height(12.dp))
                    Text(text = friendDisplayName(friend), style = MaterialTheme.typography.titleLarge)
                    Text(
                        text = friend.username,
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Spacer(modifier = Modifier.height(12.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilledTonalButton(onClick = onMessage) { Text(tr("contacts_message", "Message")) }
                        FilledTonalButton(onClick = {}) { Text(tr("contacts_call", "Call")) }
                    }
                }
            }
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
                    Text(text = tr("contacts_remark", "Remark"), style = MaterialTheme.typography.bodyLarge)
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedTextField(
                        value = remark.value,
                        onValueChange = { remark.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("contacts_remark_hint", "Add a remark")) },
                        shape = RoundedCornerShape(16.dp),
                        singleLine = true
                    )
                    Spacer(modifier = Modifier.height(10.dp))
                    PrimaryButton(
                        label = tr("contacts_save_remark", "Save remark"),
                        onClick = { onUpdateRemark(remark.value.trim()) },
                        fillMaxWidth = true
                    )
                }
            }

            SectionHeader(text = tr("contacts_safety", "Safety"))
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    ActionRow(
                        icon = Icons.Filled.Delete,
                        label = tr("contacts_delete", "Delete contact"),
                        tint = MaterialTheme.colorScheme.error,
                        onClick = onDelete
                    )
                    ActionRow(
                        icon = Icons.Filled.Block,
                        label = if (isBlocked) tr("contacts_unblock", "Unblock user") else tr("contacts_block", "Block user"),
                        tint = MaterialTheme.colorScheme.error,
                        onClick = { onToggleBlock(!isBlocked) }
                    )
                }
            }
        }
    }
}

@Composable
private fun ActionRow(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    tint: Color,
    onClick: () -> Unit = {}
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(34.dp)
                .background(tint.copy(alpha = 0.12f), RoundedCornerShape(10.dp)),
            contentAlignment = Alignment.Center
        ) {
            Icon(icon, contentDescription = label, tint = tint)
        }
        Spacer(modifier = Modifier.width(12.dp))
        Text(text = label, style = MaterialTheme.typography.bodyLarge, color = tint)
    }
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun AddFriendPreview() {
    ContactsApp()
}
