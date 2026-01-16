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
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.Link
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Person
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
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mi.e2ee.android.sdk.GroupMemberRole

private fun memberInitials(username: String): String {
    val trimmed = username.trim()
    if (trimmed.isEmpty()) return "?"
    val parts = trimmed.split(" ")
    val first = parts.firstOrNull()?.take(1) ?: "?"
    val second = parts.getOrNull(1)?.take(1) ?: trimmed.drop(1).take(1)
    return (first + second).uppercase().take(2)
}

private fun friendDisplayName(friend: FriendUi): String {
    return friend.remark.ifBlank { friend.username }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GroupDetailScreen(
    groupId: String,
    groupName: String,
    members: List<GroupMemberUi>,
    selfUsername: String,
    canInvite: Boolean,
    canManage: Boolean,
    onBack: () -> Unit = {},
    onAddMembers: () -> Unit = {},
    onSetRole: (String, Int) -> Unit = { _, _ -> },
    onKick: (String) -> Unit = {},
    onLeaveGroup: () -> Unit = {}
) {
    val roleOrder = listOf(GroupMemberRole.OWNER, GroupMemberRole.ADMIN, GroupMemberRole.MEMBER)
    val grouped = members.groupBy { it.role }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("group_info_title", "Group info")) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(onClick = {}) {
                        Icon(Icons.Filled.MoreVert, contentDescription = "More")
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
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            item {
                GroupHeaderCard(name = groupName, groupId = groupId)
            }
            item {
                SectionHeader(text = tr("group_members_section", "Members"))
                Spacer(modifier = Modifier.height(8.dp))
                Card(
                    shape = RoundedCornerShape(20.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
                ) {
                    Column(modifier = Modifier.fillMaxWidth().padding(12.dp)) {
                        FilledTonalButton(
                            onClick = onAddMembers,
                            modifier = Modifier.fillMaxWidth(),
                            enabled = canInvite,
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                                contentColor = MaterialTheme.colorScheme.primary
                            )
                        ) {
                            Icon(Icons.Filled.Add, contentDescription = "Add")
                            Spacer(modifier = Modifier.width(8.dp))
                            Text(tr("group_add_members", "Add members"))
                        }
                        if (!canInvite) {
                            Spacer(modifier = Modifier.height(8.dp))
                            PermissionHint(text = tr("group_permission_hint", "Only admins can invite"))
                        }
                        Spacer(modifier = Modifier.height(12.dp))
                        roleOrder.forEachIndexed { roleIndex, role ->
                            val roleMembers = grouped[role].orEmpty()
                            if (roleMembers.isNotEmpty()) {
                                RoleHeader(text = roleLabel(role))
                                roleMembers.forEachIndexed { index, member ->
                                    MemberRow(
                                        member = member,
                                        showRole = false,
                                        canManage = canManage,
                                        isSelf = member.username == selfUsername,
                                        onSetRole = onSetRole,
                                        onKick = onKick
                                    )
                                    if (index < roleMembers.lastIndex) {
                                        DividerLine()
                                    }
                                }
                                if (roleIndex < roleOrder.lastIndex && roleOrder.drop(roleIndex + 1).any {
                                        grouped[it].orEmpty().isNotEmpty()
                                    }
                                ) {
                                    Spacer(modifier = Modifier.height(8.dp))
                                }
                            }
                        }
                    }
                }
            }
            item {
                SectionHeader(text = tr("group_tools_section", "Group tools"))
                Spacer(modifier = Modifier.height(8.dp))
                Card(
                    shape = RoundedCornerShape(20.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
                ) {
                    Column(modifier = Modifier.fillMaxWidth()) {
                        SimpleRow(
                            icon = Icons.Filled.Link,
                            title = tr("group_invite_link", "Invite link"),
                            subtitle = tr("group_invite_link_sub", "Active, expires in 7 days")
                        )
                        DividerLine()
                        SimpleRow(
                            icon = Icons.Filled.Notifications,
                            title = tr("group_mute_notifications", "Mute notifications"),
                            subtitle = tr("group_mute_notifications_sub", "Off"),
                            trailing = { Switch(checked = false, onCheckedChange = {}) }
                        )
                        DividerLine()
                        SimpleRow(
                            icon = Icons.Filled.Lock,
                            title = tr("group_leave", "Leave group"),
                            subtitle = tr("group_leave_sub", "Remove yourself from this group"),
                            trailing = {
                                Text(
                                    text = tr("group_leave_action", "Leave"),
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = MaterialTheme.colorScheme.error,
                                    modifier = Modifier.clickable { onLeaveGroup() }
                                )
                            }
                        )
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AddGroupMembersScreen(
    groupId: String,
    friends: List<FriendUi>,
    canInvite: Boolean,
    onBack: () -> Unit = {},
    onAddMembers: (List<FriendUi>) -> Unit = {}
) {
    val query = remember { mutableStateOf("") }
    val selected = remember {
        mutableStateMapOf<String, Boolean>()
    }
    val selectedFriends = friends.filter { selected[it.username] == true }
    val selectedCount = selectedFriends.size
    val search = query.value.trim()
    val filtered = if (search.isBlank()) {
        friends
    } else {
        friends.filter { friend ->
            friend.username.contains(search, ignoreCase = true) ||
                friend.remark.contains(search, ignoreCase = true)
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("group_add_members_title", "Add members")) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                }
            )
        },
        bottomBar = {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(MaterialTheme.colorScheme.surface)
                    .padding(16.dp)
            ) {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = tr("group_selected_count", "Selected %d").format(selectedCount),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                        Spacer(modifier = Modifier.weight(1f))
                        if (!canInvite) {
                            Text(
                                text = tr("group_permission_hint", "Only admins can invite"),
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.error
                            )
                        }
                    }
                    PrimaryButton(
                        label = tr("group_add_members_action", "Add members"),
                        enabled = canInvite && selectedCount > 0,
                        onClick = { onAddMembers(selectedFriends) }
                    )
                }
            }
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
                placeholder = { Text(tr("group_search_contacts", "Search contacts")) },
                leadingIcon = { Icon(Icons.Filled.Search, contentDescription = "Search") },
                shape = RoundedCornerShape(16.dp)
            )
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    filtered.forEachIndexed { index, friend ->
                        val isSelected = selected[friend.username] == true
                        SelectableRow(
                            friend = friend,
                            selected = isSelected,
                            enabled = canInvite,
                            onToggle = {
                                selected[friend.username] = !(selected[friend.username] == true)
                            }
                        )
                        if (index < filtered.lastIndex) {
                            DividerLine()
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun GroupHeaderCard(name: String, groupId: String) {
    Card(
        shape = RoundedCornerShape(24.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            AvatarBadge(initials = memberInitials(name), tint = MaterialTheme.colorScheme.primary, size = 64.dp)
            Spacer(modifier = Modifier.height(12.dp))
            Text(text = name, style = MaterialTheme.typography.titleLarge)
            Text(
                text = tr("group_id_label", "ID: %s").format(groupId),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
private fun MemberRow(
    member: GroupMemberUi,
    showRole: Boolean = true,
    canManage: Boolean,
    isSelf: Boolean,
    onSetRole: (String, Int) -> Unit,
    onKick: (String) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        AvatarBadge(initials = memberInitials(member.username), tint = MaterialTheme.colorScheme.primary)
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(text = member.username, style = MaterialTheme.typography.bodyLarge)
            if (showRole) {
                Text(
                    text = roleLabel(member.role),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
        if (canManage && !isSelf && member.role != GroupMemberRole.OWNER) {
            val promote = if (member.role == GroupMemberRole.ADMIN) {
                tr("group_role_demote", "Make member")
            } else {
                tr("group_role_promote", "Make admin")
            }
            val targetRole = if (member.role == GroupMemberRole.ADMIN) {
                GroupMemberRole.MEMBER
            } else {
                GroupMemberRole.ADMIN
            }
            TextButton(onClick = { onSetRole(member.username, targetRole) }) {
                Text(promote)
            }
            TextButton(onClick = { onKick(member.username) }) {
                Text(tr("group_remove_member", "Remove"), color = MaterialTheme.colorScheme.error)
            }
        } else {
            Icon(Icons.Filled.Person, contentDescription = "Profile", tint = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun SelectableRow(
    friend: FriendUi,
    selected: Boolean,
    enabled: Boolean,
    onToggle: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(if (!enabled) MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.35f) else Color.Transparent)
            .clickable(enabled = enabled) { onToggle() }
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        AvatarBadge(initials = memberInitials(friendDisplayName(friend)), tint = MaterialTheme.colorScheme.primary)
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(text = friendDisplayName(friend), style = MaterialTheme.typography.bodyLarge)
            Text(
                text = friend.username,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        Box(
            modifier = Modifier
                .size(22.dp)
                .background(
                    if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                    RoundedCornerShape(6.dp)
                )
        )
    }
}

@Composable
private fun RoleHeader(text: String) {
    Text(
        text = text.uppercase(),
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.padding(horizontal = 4.dp, vertical = 6.dp)
    )
}

@Composable
private fun roleLabel(role: Int): String {
    return when (role) {
        GroupMemberRole.OWNER -> tr("group_role_owner", "Owner")
        GroupMemberRole.ADMIN -> tr("group_role_admin", "Admin")
        GroupMemberRole.MEMBER -> tr("group_role_member", "Member")
        else -> tr("group_role_member", "Member")
    }
}

@Composable
private fun PermissionHint(text: String) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(
            imageVector = Icons.Filled.Lock,
            contentDescription = text,
            tint = MaterialTheme.colorScheme.error,
            modifier = Modifier.size(14.dp)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = text,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.error
        )
    }
}

@Composable
private fun SimpleRow(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    title: String,
    subtitle: String,
    trailing: @Composable () -> Unit = { Icon(Icons.Filled.ChevronRight, contentDescription = "Open") }
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(36.dp)
                .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.12f), RoundedCornerShape(10.dp)),
            contentAlignment = Alignment.Center
        ) {
            Icon(icon, contentDescription = title)
        }
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(text = title, style = MaterialTheme.typography.bodyLarge)
            Text(
                text = subtitle,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        trailing()
    }
}

@Composable
private fun DividerLine() {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(1.dp)
            .background(MaterialTheme.colorScheme.outline)
            .padding(horizontal = 16.dp)
    )
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun GroupDetailPreview() {
    val members = listOf(
        GroupMemberUi("aster", GroupMemberRole.OWNER),
        GroupMemberUi("mina", GroupMemberRole.ADMIN),
        GroupMemberUi("rin", GroupMemberRole.MEMBER)
    )
    ChatTheme {
        GroupDetailScreen(
            groupId = "c4",
            groupName = "Design Ops",
            members = members,
            selfUsername = "mina",
            canInvite = false,
            canManage = false
        )
    }
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun AddMembersPreview() {
    val friends = listOf(
        FriendUi("jenna", "Jenna Kim", ""),
        FriendUi("ari", "Ari Reed", ""),
        FriendUi("bao", "Bao Chen", "")
    )
    ChatTheme {
        AddGroupMembersScreen(
            groupId = "c4",
            friends = friends,
            canInvite = false
        )
    }
}
