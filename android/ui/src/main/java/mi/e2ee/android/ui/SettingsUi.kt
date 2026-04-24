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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

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
    onOpenChats: () -> Unit = {},
    onOpenCalls: () -> Unit = {},
    onOpenContacts: () -> Unit = {}
) {
    val colors = phaseOneColors()
    val languageController = LocalLanguageController.current
    var notificationsEnabled by remember { mutableStateOf(true) }
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
    val accountEntry = SettingEntry(
        title = sdk.deviceDisplayId.ifBlank { tr("app_name", "MI E2EE") },
        subtitle = tr("settings_account_section", "Account"),
        icon = {
            IdentityAvatar(
                label = sdk.deviceDisplayId.ifBlank { tr("app_name", "MI E2EE") },
                seed = sdk.deviceDisplayId.ifBlank { "mi-e2ee" },
                kind = IdentityAvatarKind.Person,
                size = 36.dp
            )
        },
        trailing = { UiChevron() },
        onClick = onOpenAccount
    )

    val coreSettings = listOf(
        SettingEntry(
            title = tr("settings_security_center", "Security Center"),
            subtitle = null,
            icon = { SettingsLeadingIcon(icon = MiOwnedIcons.ShieldCheck, tone = UiIconTone.Primary) },
            trailing = { UiChevron() },
            onClick = onOpenSecurityCenter
        ),
        SettingEntry(
            title = tr("settings_account_security", "Account and security"),
            subtitle = null,
            icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Lock, tone = UiIconTone.Primary) },
            trailing = { UiChevron() },
            onClick = onOpenAccount
        ),
        SettingEntry(
            title = tr("settings_privacy", "Privacy"),
            subtitle = null,
            icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Eye, tone = UiIconTone.Primary) },
            trailing = { UiChevron() },
            onClick = onOpenPrivacy
        )
    )

    val preferenceSettings = buildList {
        add(
            SettingEntry(
                title = tr("settings_notifications", "Notifications"),
                subtitle = null,
                icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Bell, tone = UiIconTone.Accent) },
                trailing = { UiChevron() },
                onClick = { notificationsEnabled = !notificationsEnabled }
            )
        )
        add(
            SettingEntry(
                title = tr("settings_chat_storage", "Chat and storage"),
                subtitle = null,
                icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Chat, tone = UiIconTone.Primary) },
                trailing = { UiChevron() },
                onClick = onOpenChats
            )
        )
        add(
            SettingEntry(
                title = tr("settings_devices", "Devices"),
                subtitle = null,
                icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Devices, tone = UiIconTone.Accent) },
                trailing = { UiChevron() },
                onClick = onOpenSecurityCenter
            )
        )
        add(
            SettingEntry(
                title = tr("settings_theme_mode", "Theme mode"),
                subtitle = null,
                icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Settings, tone = UiIconTone.Neutral) },
                trailing = { UiChevron() },
                onClick = cycleThemeMode
            )
        )
        if (languageController != null && languageController.packs.isNotEmpty()) {
            add(
                SettingEntry(
                    title = tr("settings_language", "Language"),
                    subtitle = null,
                    icon = { SettingsLeadingIcon(icon = MiOwnedIcons.Link, tone = UiIconTone.Primary) },
                    trailing = { UiChevron() },
                    onClick = cycleLanguage
                )
            )
        }
    }

    Scaffold(
        topBar = {
            SettingsTopBar(
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
        containerColor = colors.background
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .background(colors.background)
                .padding(horizontal = 12.dp)
                .testTag("settings-screen"),
            contentPadding = PaddingValues(
                top = 10.dp,
                bottom = 96.dp
            ),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            item(key = "settings-account") {
                InsetGroupedCard {
                    SettingsRow(accountEntry, showDivider = false)
                }
            }
            item(key = "settings-core-group") {
                InsetGroupedCard {
                    // items(coreSettings)
                    coreSettings.forEachIndexed { index, entry ->
                        SettingsRow(entry = entry, showDivider = index != coreSettings.lastIndex)
                    }
                }
            }
            item(key = "settings-preferences-group") {
                InsetGroupedCard {
                    // items(preferenceSettings)
                    preferenceSettings.forEachIndexed { index, entry ->
                        SettingsRow(entry = entry, showDivider = index != preferenceSettings.lastIndex)
                    }
                }
            }
        }
    }
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
        size = 34.dp,
        iconSize = ChatUiTokens.IconGlyphSm
    )
}

@Composable
private fun SettingsTopBar(
    showBackButton: Boolean,
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    val colors = phaseOneColors()
    // CenterAlignedTopAppBar(
    GlassTopAppBar(
        modifier = modifier,
        leadingContent = if (showBackButton) {
            {
                UiToolbarIconButton(
                    icon = MiOwnedIcons.ArrowBack,
                    contentDescription = tr("settings_back", "Back"),
                    onClick = onBack
                )
            }
        } else {
            null
        },
        titleContent = {
            Text(
                text = tr("settings_title", "Settings"),
                modifier = Modifier.padding(
                    top = PhaseOneTokens.LargeTitleTopPadding,
                    bottom = PhaseOneTokens.LargeTitleBottomPadding
                ),
                style = phaseOneLargeTitleTextStyle(),
                color = colors.onSurface
            )
            Text(
                text = tr("settings_subtitle", "Privacy, devices, and chat preferences"),
                style = MaterialTheme.typography.bodySmall,
                color = colors.onSurfaceMuted
            )
        }
    )
}

@Composable
private fun UiChevron() {
    Icon(
        imageVector = MiOwnedIcons.ChevronRight,
        contentDescription = null,
        tint = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.size(16.dp)
    )
}

@Composable
private fun SettingsRow(
    entry: SettingEntry,
    showDivider: Boolean
) {
    val colors = phaseOneColors()
    val clickableModifier = if (entry.onClick != null) {
        Modifier.clickable { entry.onClick.invoke() }
    } else {
        Modifier
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .then(clickableModifier)
            .clip(RoundedCornerShape(14.dp))
            .padding(horizontal = 14.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        entry.icon()
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = entry.title,
                style = MaterialTheme.typography.bodyLarge.copy(fontWeight = FontWeight.SemiBold),
                color = colors.onSurface
            )
            if (!entry.subtitle.isNullOrBlank()) {
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = entry.subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = colors.onSurfaceMuted
                )
            }
        }
        Spacer(modifier = Modifier.width(8.dp))
        entry.trailing()
    }

    if (showDivider) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(start = 60.dp)
                .height(1.dp)
                .background(colors.outline.copy(alpha = 0.28f))
        )
    }
}

@Preview(showBackground = true, widthDp = 412, heightDp = 915)
@Composable
private fun SettingsPreview() {
    SettingsApp()
}
