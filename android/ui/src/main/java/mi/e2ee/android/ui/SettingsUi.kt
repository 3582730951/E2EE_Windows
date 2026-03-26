package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.ChatBubble
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Devices
import androidx.compose.material.icons.filled.Language
import androidx.compose.material.icons.filled.Link
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Schedule
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material.icons.filled.VerifiedUser
import androidx.compose.material.icons.filled.Visibility
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
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

@Composable
fun SettingsScreen(
    sdk: SdkBridge,
    themeMode: Int = ThemeMode.ForceDark,
    onThemeModeChange: (Int) -> Unit = {},
    onBack: () -> Unit = {},
    onOpenSecurityCenter: () -> Unit = {},
    onOpenAccount: () -> Unit = {},
    onOpenPrivacy: () -> Unit = {},
    onOpenDiagnostics: () -> Unit = {},
    onOpenChats: () -> Unit = {},
    onOpenContacts: () -> Unit = {}
) {
    val languageController = LocalLanguageController.current
    var notificationsEnabled by remember { mutableStateOf(true) }
    val themeSummary = when (themeMode) {
        ThemeMode.FollowSystem -> tr("settings_theme_system", "System")
        ThemeMode.ForceLight -> tr("settings_theme_light", "Light")
        ThemeMode.ForceDark -> tr("settings_theme_dark", "Dark")
        else -> tr("settings_theme_dark", "Dark")
    }
    val accountSettings = listOf(
        SettingEntry(
            title = tr("settings_security_center", "Security Center"),
            subtitle = tr("settings_security_center_sub", "Root Auth, devices, trusted sessions"),
            icon = { UiSemanticIcon(icon = Icons.Filled.Shield, contentDescription = null, tone = UiIconTone.Primary, framed = false) },
            trailing = { UiChevron() },
            onClick = onOpenSecurityCenter
        ),
        SettingEntry(
            title = tr("settings_account_security", "Account and security"),
            subtitle = tr("settings_account_security_sub", "Password, devices, backup"),
            icon = { UiSemanticIcon(icon = Icons.Filled.VerifiedUser, contentDescription = null, tone = UiIconTone.Primary, framed = false) },
            trailing = { UiChevron() },
            onClick = onOpenAccount
        ),
        SettingEntry(
            title = tr("settings_privacy", "Privacy"),
            subtitle = tr("settings_privacy_sub", "Visibility, read receipts"),
            icon = { UiSemanticIcon(icon = Icons.Filled.Visibility, contentDescription = null, tone = UiIconTone.Primary, framed = false) },
            trailing = { UiChevron() },
            onClick = onOpenPrivacy
        ),
        SettingEntry(
            title = tr("settings_notifications", "Notifications"),
            subtitle = tr("settings_notifications_sub", "Message, call alerts"),
            icon = { UiSemanticIcon(icon = Icons.Filled.Notifications, contentDescription = null, tone = UiIconTone.Accent, framed = false) },
            trailing = {
                Switch(
                    checked = notificationsEnabled,
                    onCheckedChange = { notificationsEnabled = it }
                )
            }
        )
    )

    val appSettings = buildList {
        add(
            SettingEntry(
                title = tr("settings_chat_storage", "Chat and storage"),
                subtitle = tr("settings_chat_storage_sub", "Cache, media, auto-download"),
                icon = { UiSemanticIcon(icon = Icons.Filled.ChatBubble, contentDescription = null, tone = UiIconTone.Primary, framed = false) },
                trailing = { UiChevron() },
                onClick = onOpenChats
            )
        )
        add(
            SettingEntry(
                title = tr("settings_devices", "Devices"),
                subtitle = tr("settings_devices_sub", "Active sessions"),
                icon = { UiSemanticIcon(icon = Icons.Filled.Devices, contentDescription = null, tone = UiIconTone.Accent, framed = false) },
                trailing = { UiChevron() },
                onClick = onOpenSecurityCenter
            )
        )
        add(
            SettingEntry(
                title = tr("settings_appearance", "Appearance"),
                subtitle = tr("settings_appearance_sub", "Theme, font size"),
                icon = { UiSemanticIcon(icon = Icons.Filled.Settings, contentDescription = null, tone = UiIconTone.Neutral, framed = false) },
                trailing = {
                    Text(
                        text = themeSummary,
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            )
        )
        if (BuildConfig.DEBUG) {
            add(
                SettingEntry(
                    title = tr("settings_diagnostics", "Diagnostics"),
                    subtitle = tr("settings_diagnostics_sub", "SDK tools and logs"),
                    icon = { UiSemanticIcon(icon = Icons.Filled.BugReport, contentDescription = null, tone = UiIconTone.Warning, framed = false) },
                    trailing = { UiChevron() },
                    onClick = onOpenDiagnostics
                )
            )
        }
    }
    val connectionEntries = listOf(
        SettingEntry(
            title = tr("settings_heartbeat", "Heartbeat"),
            subtitle = tr("settings_heartbeat_sub", "Send a keep-alive ping"),
            icon = { UiSemanticIcon(icon = Icons.Filled.Schedule, contentDescription = null, tone = UiIconTone.Accent, framed = false) },
            trailing = {
                TextButton(onClick = { sdk.heartbeat() }) {
                    Text(tr("settings_run", "Run"))
                }
            }
        ),
        SettingEntry(
            title = tr("settings_relogin", "Reconnect"),
            subtitle = tr("settings_relogin_sub", "Refresh session with server"),
            icon = { UiSemanticIcon(icon = Icons.Filled.Link, contentDescription = null, tone = UiIconTone.Primary, framed = false) },
            trailing = {
                TextButton(onClick = { sdk.relogin() }) {
                    Text(tr("settings_run", "Run"))
                }
            }
        )
    )

    Scaffold(
        topBar = {
            SettingsTopBar(
                title = tr("settings_title", "Settings"),
                onBack = onBack
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
                .padding(horizontal = 16.dp)
                .testTag("settings-screen"),
            contentPadding = PaddingValues(
                top = ChatUiTokens.ItemSpacing,
                bottom = ChatUiTokens.SectionSpacing
            ),
            verticalArrangement = Arrangement.spacedBy(ChatUiTokens.ItemSpacing)
        ) {
            item {
                SettingsHeader(
                    displayName = sdk.username.ifBlank { tr("settings_user_placeholder", "MI User") },
                    username = sdk.username.ifBlank { "mi_user" },
                    deviceId = sdk.deviceDisplayId,
                    remoteOk = sdk.remoteOk
                )
            }
            item {
                SectionHeader(text = tr("settings_account_section", "Account"))
                Spacer(modifier = Modifier.height(ChatUiTokens.ItemSpacing))
                SettingsSection(entries = accountSettings)
            }
            item {
                SectionHeader(text = tr("settings_preferences_section", "Preferences"))
                Spacer(modifier = Modifier.height(ChatUiTokens.ItemSpacing))
                ThemeModeSection(
                    mode = themeMode,
                    onModeChange = onThemeModeChange
                )
                if (languageController != null && languageController.packs.isNotEmpty()) {
                    Spacer(modifier = Modifier.height(ChatUiTokens.SectionSpacing))
                    LanguageSection(
                        controller = languageController
                    )
                }
                Spacer(modifier = Modifier.height(ChatUiTokens.SectionSpacing))
                SettingsSection(entries = appSettings)
                Spacer(modifier = Modifier.height(ChatUiTokens.SectionSpacing))
                SectionHeader(text = tr("settings_connection", "Connection"))
                Spacer(modifier = Modifier.height(ChatUiTokens.ItemSpacing))
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
    SurfaceSectionCard {
        Row(
            modifier = Modifier
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            UiSemanticIcon(
                icon = Icons.Filled.Person,
                contentDescription = tr("settings_user_placeholder", "MI User"),
                tone = UiIconTone.Primary,
                size = ChatUiTokens.IconContainerLg,
                iconSize = ChatUiTokens.IconGlyphLg,
                framed = false
            )
            Spacer(modifier = Modifier.width(12.dp))
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
            UiStatusCountBadge(
                label = if (remoteOk) tr("settings_remote_ok", "Online") else tr("settings_remote_error", "Offline"),
                tone = if (remoteOk) UiBadgeTone.Accent else UiBadgeTone.Danger
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

    SurfaceSectionCard {
        Column(modifier = Modifier.fillMaxWidth()) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                UiSemanticIcon(
                    icon = Icons.Filled.Settings,
                    contentDescription = tr("settings_theme_mode", "Theme mode"),
                    tone = UiIconTone.Neutral,
                    framed = false
                )
                Spacer(modifier = Modifier.width(12.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = tr("settings_theme_mode", "Theme mode"), style = MaterialTheme.typography.bodyLarge)
                    Text(
                        text = tr("settings_theme_mode_sub", "System / Dark / Light"),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                UiStatusCountBadge(label = activeLabel, tone = UiBadgeTone.Primary)
            }
            Spacer(modifier = Modifier.height(ChatUiTokens.SectionSpacing))
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(ChatUiTokens.ItemSpacing)
            ) {
                options.forEach { option ->
                    val selected = option.mode == mode
                    FilterChip(
                        selected = selected,
                        onClick = { onModeChange(option.mode) },
                        label = { Text(option.label) },
                        leadingIcon = if (selected) {
                            {
                                Icon(
                                    imageVector = Icons.Filled.Check,
                                    contentDescription = null,
                                    modifier = Modifier.size(FilterChipDefaults.IconSize)
                                )
                            }
                        } else {
                            null
                        }
                    )
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
    SurfaceSectionCard {
        Column(modifier = Modifier.fillMaxWidth()) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                UiSemanticIcon(
                    icon = Icons.Filled.Language,
                    contentDescription = tr("settings_language", "Language"),
                    tone = UiIconTone.Primary,
                    framed = false
                )
                Spacer(modifier = Modifier.width(12.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = tr("settings_language", "Language"), style = MaterialTheme.typography.bodyLarge)
                    Text(
                        text = tr("settings_language_sub", "Switch display language"),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                UiStatusCountBadge(label = controller.current.label, tone = UiBadgeTone.Primary)
            }
            Spacer(modifier = Modifier.height(ChatUiTokens.SectionSpacing))
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(ChatUiTokens.ItemSpacing)
            ) {
                controller.packs.forEach { pack ->
                    val selected = pack.code == controller.current.code
                    FilterChip(
                        selected = selected,
                        onClick = { controller.setLanguage(pack.code) },
                        label = { Text(pack.label) },
                        leadingIcon = if (selected) {
                            {
                                Icon(
                                    imageVector = Icons.Filled.Check,
                                    contentDescription = null,
                                    modifier = Modifier.size(FilterChipDefaults.IconSize)
                                )
                            }
                        } else {
                            null
                        }
                    )
                }
            }
        }
    }
}

@Composable
private fun SettingsTopBar(
    title: String,
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    TopAppBar(
        modifier = modifier
            .fillMaxWidth()
            .statusBarsPadding(),
        navigationIcon = {
            IconButton(onClick = onBack) {
                Icon(
                    imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                    contentDescription = tr("settings_back", "Back")
                )
            }
        },
        title = {
            Text(
                text = title,
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onSurface
            )
        },
        colors = TopAppBarDefaults.topAppBarColors(
            containerColor = MaterialTheme.colorScheme.surface.copy(alpha = 0.96f),
            titleContentColor = MaterialTheme.colorScheme.onSurface,
            navigationIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant
        )
    )
}

@Composable
private fun SettingsSection(entries: List<SettingEntry>) {
    SurfaceSectionCard {
        Column(modifier = Modifier.fillMaxWidth()) {
            entries.forEachIndexed { index, entry ->
                SettingsRow(entry)
                if (index < entries.lastIndex) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 6.dp)
                            .height(1.dp)
                            .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.45f))
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
    ListItem(
        modifier = Modifier
            .fillMaxWidth()
            .then(clickableModifier)
            .clip(RoundedCornerShape(12.dp)),
        colors = ListItemDefaults.colors(containerColor = Color.Transparent),
        leadingContent = entry.icon,
        trailingContent = entry.trailing,
        headlineContent = {
            Text(text = entry.title, style = MaterialTheme.typography.bodyLarge)
        },
        supportingContent = {
            Text(
                text = entry.subtitle,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    )
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun SettingsPreview() {
    SettingsApp()
}
