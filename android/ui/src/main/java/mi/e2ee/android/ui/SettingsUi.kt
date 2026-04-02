package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mi.e2ee.android.BuildConfig

data class SettingEntry(
    val title: String,
    val subtitle: String? = null,
    val icon: @Composable () -> Unit,
    val trailing: @Composable () -> Unit,
    val onClick: (() -> Unit)? = null
)

@Composable
fun SettingsApp() {
    var mode by remember { mutableStateOf(ThemeMode.FollowSystem) }
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
    themeMode: Int = ThemeMode.FollowSystem,
    onThemeModeChange: (Int) -> Unit = {},
    showBackButton: Boolean = false,
    onBack: () -> Unit = {},
    onOpenSecurityCenter: () -> Unit = {},
    onOpenAccount: () -> Unit = {},
    onOpenPrivacy: () -> Unit = {},
    onOpenDiagnostics: () -> Unit = {},
    onOpenChats: () -> Unit = {},
    onOpenCalls: () -> Unit = {},
    onOpenContacts: () -> Unit = {}
) {
    val languageController = LocalLanguageController.current
    var notificationsEnabled by remember { mutableStateOf(true) }
    val themeModeLabel = when (themeMode) {
        ThemeMode.FollowSystem -> tr("settings_theme_system", "System")
        ThemeMode.ForceLight -> tr("settings_theme_light", "Light")
        ThemeMode.ForceDark -> tr("settings_theme_dark", "Dark")
        else -> tr("settings_theme_system", "System")
    }
    val cycleThemeMode = {
        val next = when (themeMode) {
            ThemeMode.FollowSystem -> ThemeMode.ForceDark
            ThemeMode.ForceDark -> ThemeMode.ForceLight
            else -> ThemeMode.FollowSystem
        }
        onThemeModeChange(next)
    }
    val cycleLanguage: () -> Unit = {
        languageController?.let { controller ->
            if (controller.packs.isNotEmpty()) {
                val currentIndex = controller.packs.indexOfFirst { it.code == controller.current.code }
                val nextIndex = if (currentIndex == -1) 0 else (currentIndex + 1) % controller.packs.size
                controller.setLanguage(controller.packs[nextIndex].code)
            }
        }
    }
    val accountSettings = listOf(
        SettingEntry(
            title = tr("settings_security_center", "Security Center"),
            subtitle = tr("settings_security_center_subtitle", "Devices, sessions, and approval identity"),
            icon = { SettingsLeadingIcon(icon = MiOwnedIcons.ShieldCheck, tone = UiIconTone.Primary) },
            trailing = { UiChevron() },
            onClick = onOpenSecurityCenter
        ),
        SettingEntry(
            title = tr("settings_account_security", "Account and security"),
            subtitle = tr("settings_account_security_subtitle", "Password, linked devices, and recovery"),
            icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Lock, tone = UiIconTone.Primary) },
            trailing = { UiChevron() },
            onClick = onOpenAccount
        ),
        SettingEntry(
            title = tr("settings_privacy", "Privacy"),
            subtitle = tr("settings_privacy_subtitle", "Read receipts, blocked users, and visibility"),
            icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Eye, tone = UiIconTone.Primary) },
            trailing = { UiChevron() },
            onClick = onOpenPrivacy
        ),
        SettingEntry(
            title = tr("settings_notifications", "Notifications"),
            subtitle = tr("settings_notifications_subtitle", "Calls, mentions, and message alerts"),
            icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Bell, tone = UiIconTone.Accent) },
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
                subtitle = tr("settings_chat_storage_subtitle", "Media, cache, and auto-download"),
                icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Chat, tone = UiIconTone.Primary) },
                trailing = { UiChevron() },
                onClick = onOpenChats
            )
        )
        add(
            SettingEntry(
                title = tr("settings_devices", "Devices"),
                subtitle = tr("settings_devices_subtitle", "Manage trusted phones and tablets"),
                icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Devices, tone = UiIconTone.Accent) },
                trailing = { UiChevron() },
                onClick = onOpenSecurityCenter
            )
        )
        add(
            SettingEntry(
                title = tr("settings_theme_mode", "Theme mode"),
                subtitle = tr("settings_appearance_subtitle", "Theme and reading comfort"),
                icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Settings, tone = UiIconTone.Neutral) },
                trailing = { SettingsTrailingValue(text = themeModeLabel) },
                onClick = cycleThemeMode
            )
        )
        if (languageController != null && languageController.packs.isNotEmpty()) {
            add(
                SettingEntry(
                    title = tr("settings_language", "Language"),
                    subtitle = tr("settings_language_subtitle", "App language"),
                    icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Link, tone = UiIconTone.Primary) },
                    trailing = { SettingsTrailingValue(text = languageController.current.label) },
                    onClick = cycleLanguage
                )
            )
        }
        if (BuildConfig.DEBUG) {
            add(
                SettingEntry(
                    title = tr("settings_diagnostics", "Diagnostics"),
                    subtitle = tr("settings_diagnostics_subtitle", "Bridge, logs, and smoke tools"),
                    icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Bug, tone = UiIconTone.Warning) },
                    trailing = { UiChevron() },
                    onClick = onOpenDiagnostics
                )
            )
        }
    }
    val connectionEntries = listOf(
        SettingEntry(
            title = tr("settings_heartbeat", "Heartbeat"),
            subtitle = tr("settings_heartbeat_subtitle", "Ping transport and refresh status"),
            icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Clock, tone = UiIconTone.Accent) },
            trailing = {
                TextButton(onClick = { sdk.heartbeat() }) {
                    Text(tr("settings_run", "Run"))
                }
            }
        ),
        SettingEntry(
            title = tr("settings_relogin", "Reconnect"),
            subtitle = tr("settings_relogin_subtitle", "Rebuild the secure session"),
            icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Link, tone = UiIconTone.Primary) },
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
                showBackButton = showBackButton,
                onBack = onBack
            )
        },
        bottomBar = {
            ConversationBottomBar(
                activeTab = ConversationTab.Settings,
                onContacts = onOpenContacts,
                onChats = onOpenChats,
                onCalls = onOpenCalls,
                onSettings = {}
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(horizontal = 14.dp)
                .testTag("settings-screen"),
            contentPadding = PaddingValues(
                top = 8.dp,
                bottom = 12.dp
            ),
            verticalArrangement = Arrangement.spacedBy(8.dp)
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
                Spacer(modifier = Modifier.height(6.dp))
                SettingsSection(entries = accountSettings)
            }
            item {
                SectionHeader(text = tr("settings_preferences_section", "Preferences"))
                Spacer(modifier = Modifier.height(6.dp))
                SettingsSection(entries = appSettings)
                Spacer(modifier = Modifier.height(8.dp))
                SectionHeader(text = tr("settings_connection", "Connection"))
                Spacer(modifier = Modifier.height(6.dp))
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
    GroupedSettingsContainer {
        Row(
            modifier = Modifier
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            UiSemanticIcon(
                icon = MiOwnedIcons.Person,
                contentDescription = tr("settings_user_placeholder", "MI User"),
                tone = UiIconTone.Primary,
                size = ChatUiTokens.IconContainerMd,
                iconSize = ChatUiTokens.IconGlyphMd,
                framed = false
            )
            Spacer(modifier = Modifier.width(10.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(text = displayName, style = MaterialTheme.typography.titleMedium)
                Text(
                    text = username,
                    style = MaterialTheme.typography.bodySmall,
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
private fun SettingsTrailingValue(text: String) {
    Text(
        text = text,
        style = MaterialTheme.typography.labelLarge,
        color = MaterialTheme.colorScheme.onSurfaceVariant
    )
}

@Composable
private fun SettingsLeadingIcon(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    tone: UiIconTone
) {
    UiSemanticIcon(
        icon = icon,
        contentDescription = null,
        tone = tone,
        size = ChatUiTokens.IconContainerSm,
        iconSize = ChatUiTokens.IconGlyphSm
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SettingsTopBar(
    title: String,
    showBackButton: Boolean,
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    TopAppBar(
        modifier = modifier
            .fillMaxWidth()
            .height(56.dp)
            .statusBarsPadding(),
        navigationIcon = {
            if (showBackButton) {
                UiToolbarIconButton(
                    icon = MiOwnedIcons.ArrowBack,
                    contentDescription = tr("settings_back", "Back"),
                    onClick = onBack
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
            containerColor = MaterialTheme.colorScheme.surface.copy(alpha = 0.86f),
            titleContentColor = MaterialTheme.colorScheme.onSurface,
            navigationIconContentColor = MaterialTheme.colorScheme.onSurfaceVariant
        )
    )
}

@Composable
private fun SettingsSection(entries: List<SettingEntry>) {
    GroupedSettingsContainer {
        Column(modifier = Modifier.fillMaxWidth()) {
            entries.forEachIndexed { index, entry ->
                SettingsRow(entry)
                if (index < entries.lastIndex) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(1.dp)
                            .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.12f))
                    )
                }
            }
        }
    }
}

@Composable
private fun GroupedSettingsContainer(content: @Composable () -> Unit) {
    val shape = RoundedCornerShape(ChatUiTokens.CornerLarge)
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(shape)
            .background(MaterialTheme.colorScheme.surface)
            .border(
                width = 1.dp,
                color = MaterialTheme.colorScheme.outline.copy(alpha = ChatUiTokens.SurfaceBorderAlpha),
                shape = shape
            )
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        content()
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
            .clip(RoundedCornerShape(12.dp))
            .padding(horizontal = 4.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        entry.icon()
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(text = entry.title, style = MaterialTheme.typography.bodyLarge)
            if (!entry.subtitle.isNullOrBlank()) {
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = entry.subtitle.orEmpty(),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
        Spacer(modifier = Modifier.width(8.dp))
        entry.trailing()
    }
}

@Preview(showBackground = true, widthDp = 412, heightDp = 915)
@Composable
private fun SettingsPreview() {
    SettingsApp()
}
