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
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.DarkMode
import androidx.compose.material.icons.filled.Devices
import androidx.compose.material.icons.filled.Language
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.PrivacyTip
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Storage
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mi.e2ee.android.BuildConfig

data class SettingEntry(
    val title: String,
    val subtitle: String,
    val icon: @Composable () -> Unit,
    val trailing: @Composable () -> Unit,
    val onClick: (() -> Unit)? = null
)

@Composable
fun SettingsApp() {
    var mode by remember { mutableStateOf(ThemeMode.ForceDark) }
    val context = LocalContext.current
    val sdk = remember(context) { SdkBridge(context) }
    LaunchedEffect(Unit) { sdk.init() }
    ChatTheme(mode = mode) {
        SettingsScreen(
            sdk = sdk,
            themeMode = mode,
            onThemeModeChange = { mode = it }
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    sdk: SdkBridge,
    themeMode: Int = ThemeMode.ForceDark,
    onThemeModeChange: (Int) -> Unit = {},
    onBack: () -> Unit = {},
    onOpenAccount: () -> Unit = {},
    onOpenPrivacy: () -> Unit = {},
    onOpenDiagnostics: () -> Unit = {},
    onOpenChats: () -> Unit = {},
    onOpenContacts: () -> Unit = {}
) {
    val languageController = LocalLanguageController.current
    val accountSettings = listOf(
        SettingEntry(
            title = tr("settings_account_security", "Account and security"),
            subtitle = tr("settings_account_security_sub", "Password, devices, backup"),
            icon = { Icon(Icons.Filled.Security, contentDescription = "Security") },
            trailing = { Icon(Icons.Filled.ChevronRight, contentDescription = "Open") },
            onClick = onOpenAccount
        ),
        SettingEntry(
            title = tr("settings_privacy", "Privacy"),
            subtitle = tr("settings_privacy_sub", "Visibility, read receipts"),
            icon = { Icon(Icons.Filled.PrivacyTip, contentDescription = "Privacy") },
            trailing = { Icon(Icons.Filled.ChevronRight, contentDescription = "Open") },
            onClick = onOpenPrivacy
        ),
        SettingEntry(
            title = tr("settings_notifications", "Notifications"),
            subtitle = tr("settings_notifications_sub", "Message, call alerts"),
            icon = { Icon(Icons.Filled.Notifications, contentDescription = "Notifications") },
            trailing = { Switch(checked = true, onCheckedChange = {}) }
        )
    )

    val appSettings = buildList {
        add(
            SettingEntry(
                title = tr("settings_chat_storage", "Chat and storage"),
                subtitle = tr("settings_chat_storage_sub", "Cache, media, auto-download"),
                icon = { Icon(Icons.Filled.Storage, contentDescription = "Storage") },
                trailing = { Icon(Icons.Filled.ChevronRight, contentDescription = "Open") }
            )
        )
        add(
            SettingEntry(
                title = tr("settings_devices", "Devices"),
                subtitle = tr("settings_devices_sub", "Active sessions"),
                icon = { Icon(Icons.Filled.Devices, contentDescription = "Devices") },
                trailing = { Icon(Icons.Filled.ChevronRight, contentDescription = "Open") }
            )
        )
        add(
            SettingEntry(
                title = tr("settings_appearance", "Appearance"),
                subtitle = tr("settings_appearance_sub", "Theme, font size"),
                icon = { Icon(Icons.Filled.Tune, contentDescription = "Appearance") },
                trailing = { Icon(Icons.Filled.ChevronRight, contentDescription = "Open") }
            )
        )
        if (BuildConfig.DEBUG) {
            add(
                SettingEntry(
                    title = tr("settings_diagnostics", "Diagnostics"),
                    subtitle = tr("settings_diagnostics_sub", "SDK tools and logs"),
                    icon = { Icon(Icons.Filled.BugReport, contentDescription = "Diagnostics") },
                    trailing = { Icon(Icons.Filled.ChevronRight, contentDescription = "Open") },
                    onClick = onOpenDiagnostics
                )
            )
        }
    }
    val connectionEntries = listOf(
        SettingEntry(
            title = tr("settings_heartbeat", "Heartbeat"),
            subtitle = tr("settings_heartbeat_sub", "Send a keep-alive ping"),
            icon = { Icon(Icons.Filled.Notifications, contentDescription = "Heartbeat") },
            trailing = {
                TextButton(onClick = { sdk.heartbeat() }) {
                    Text(tr("settings_run", "Run"))
                }
            }
        ),
        SettingEntry(
            title = tr("settings_relogin", "Reconnect"),
            subtitle = tr("settings_relogin_sub", "Refresh session with server"),
            icon = { Icon(Icons.Filled.Security, contentDescription = "Reconnect") },
            trailing = {
                TextButton(onClick = { sdk.relogin() }) {
                    Text(tr("settings_run", "Run"))
                }
            }
        )
    )

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("settings_title", "Settings")) },
                modifier = Modifier.shadow(4.dp),
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                }
            )
        },
        bottomBar = {
            ConversationBottomBar(
                activeTab = ConversationTab.Settings,
                onContacts = onOpenContacts,
                onChats = onOpenChats,
                onSettings = {}
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(horizontal = 16.dp, vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            item {
                SettingsHeader(
                    displayName = sdk.username.ifBlank { tr("settings_user_placeholder", "MI User") },
                    username = sdk.username.ifBlank { "mi_user" },
                    deviceId = sdk.deviceId,
                    remoteOk = sdk.remoteOk
                )
            }
            item {
                SectionHeader(text = tr("settings_account_section", "Account"))
                Spacer(modifier = Modifier.height(8.dp))
                SettingsSection(entries = accountSettings)
            }
            item {
                SectionHeader(text = tr("settings_preferences_section", "Preferences"))
                Spacer(modifier = Modifier.height(8.dp))
                ThemeModeSection(
                    mode = themeMode,
                    onModeChange = onThemeModeChange
                )
                if (languageController != null && languageController.packs.isNotEmpty()) {
                    Spacer(modifier = Modifier.height(12.dp))
                    LanguageSection(
                        controller = languageController
                    )
                }
                Spacer(modifier = Modifier.height(12.dp))
                SettingsSection(entries = appSettings)
                Spacer(modifier = Modifier.height(12.dp))
                SectionHeader(text = tr("settings_connection", "Connection"))
                Spacer(modifier = Modifier.height(8.dp))
                SettingsSection(entries = connectionEntries)
            }
        }
    }
}

@Composable
private fun SettingsHeader(
    displayName: String,
    username: String,
    deviceId: String,
    remoteOk: Boolean
) {
    Card(
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            AvatarBadge(initials = displayName.take(2).uppercase(), tint = MaterialTheme.colorScheme.primary, size = 54.dp)
            Spacer(modifier = Modifier.width(16.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(text = displayName, style = MaterialTheme.typography.titleLarge)
                Text(
                    text = username,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                if (deviceId.isNotBlank()) {
                    Text(
                        text = tr("settings_device_id", "Device: %s").format(deviceId),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
            LabeledChip(
                label = if (remoteOk) tr("settings_remote_ok", "Online") else tr("settings_remote_error", "Offline"),
                tint = if (remoteOk) MaterialTheme.colorScheme.secondary else MaterialTheme.colorScheme.error
            )
        }
    }
}

@Composable
private fun ThemeModeSection(
    mode: Int,
    onModeChange: (Int) -> Unit
) {
    val options = listOf(
        ThemeModeOption(ThemeMode.FollowSystem, tr("settings_theme_system", "System")),
        ThemeModeOption(ThemeMode.ForceDark, tr("settings_theme_dark", "Dark")),
        ThemeModeOption(ThemeMode.ForceLight, tr("settings_theme_light", "Light"))
    )
    val activeLabel = when (mode) {
        ThemeMode.FollowSystem -> tr("settings_theme_system", "System")
        ThemeMode.ForceLight -> tr("settings_theme_light", "Light")
        ThemeMode.ForceDark -> tr("settings_theme_dark", "Dark")
        else -> tr("settings_theme_dark", "Dark")
    }

    Card(
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier
                        .size(34.dp)
                        .background(
                            MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                            RoundedCornerShape(10.dp)
                        ),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(Icons.Filled.DarkMode, contentDescription = "Theme mode")
                }
                Spacer(modifier = Modifier.width(12.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = tr("settings_theme_mode", "Theme mode"), style = MaterialTheme.typography.bodyLarge)
                    Text(
                        text = tr("settings_theme_mode_sub", "System / Dark / Light"),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                LabeledChip(label = activeLabel, tint = MaterialTheme.colorScheme.primary)
            }
            Spacer(modifier = Modifier.height(12.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                options.forEach { option ->
                    val selected = option.mode == mode
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .height(36.dp)
                            .background(
                                if (selected) MaterialTheme.colorScheme.primary
                                else MaterialTheme.colorScheme.surfaceVariant,
                                RoundedCornerShape(12.dp)
                            )
                            .clickable { onModeChange(option.mode) },
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = option.label,
                            style = MaterialTheme.typography.bodyMedium,
                            color = if (selected) Color.White else MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
            }
        }
    }
}

private data class ThemeModeOption(
    val mode: Int,
    val label: String
)

@Composable
private fun LanguageSection(controller: LanguageController) {
    Card(
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier
                        .size(34.dp)
                        .background(
                            MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                            RoundedCornerShape(10.dp)
                        ),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(Icons.Filled.Language, contentDescription = "Language")
                }
                Spacer(modifier = Modifier.width(12.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = tr("settings_language", "Language"), style = MaterialTheme.typography.bodyLarge)
                    Text(
                        text = tr("settings_language_sub", "Switch display language"),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                LabeledChip(label = controller.current.label, tint = MaterialTheme.colorScheme.primary)
            }
            Spacer(modifier = Modifier.height(12.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                controller.packs.forEach { pack ->
                    val selected = pack.code == controller.current.code
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .height(36.dp)
                            .background(
                                if (selected) MaterialTheme.colorScheme.primary
                                else MaterialTheme.colorScheme.surfaceVariant,
                                RoundedCornerShape(12.dp)
                            )
                            .clickable { controller.setLanguage(pack.code) },
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = pack.label,
                            style = MaterialTheme.typography.bodyMedium,
                            color = if (selected) Color.White else MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun SettingsSection(entries: List<SettingEntry>) {
    Card(
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            entries.forEachIndexed { index, entry ->
                SettingsRow(entry)
                if (index < entries.lastIndex) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(1.dp)
                            .background(MaterialTheme.colorScheme.outline)
                            .padding(horizontal = 16.dp)
                    )
                }
            }
        }
    }
}

@Composable
private fun SettingsRow(entry: SettingEntry) {
    val clickableModifier = if (entry.onClick != null) {
        Modifier.clickable { entry.onClick.invoke() }
    } else {
        Modifier
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .then(clickableModifier)
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(34.dp)
                .background(
                    MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                    RoundedCornerShape(10.dp)
                ),
            contentAlignment = Alignment.Center
        ) {
            entry.icon()
        }
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(text = entry.title, style = MaterialTheme.typography.bodyLarge)
            Text(
                text = entry.subtitle,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        entry.trailing()
    }
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun SettingsPreview() {
    SettingsApp()
}
