import 'dart:ui';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/chat_providers.dart';
import '../../application/session_controller.dart';
import '../../bootstrap/app_providers.dart';
import '../../bootstrap/device_capability_providers.dart';
import '../../domain/entities/models.dart';
import '../theme/app_icons.dart';
import '../theme/app_theme.dart';
import '../theme/app_tokens.dart';
import '../theme/visual_tier.dart';
import '../widgets/shell_chrome.dart';

class SettingsSecurityScreen extends ConsumerWidget {
  const SettingsSecurityScreen({super.key});

  static const double _sectionSpacing = 12;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final theme = Theme.of(context);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final devices = ref
        .watch(devicesProvider)
        .maybeWhen(
          data: (value) => value,
          orElse: () => const <DeviceTrustInfo>[],
        );
    final session = ref.watch(sessionControllerProvider);
    final preference = ref.watch(visualPreferenceProvider);
    final resolvedVisualTier = ref.watch(resolvedVisualTierProvider);
    final pendingDevices = devices
        .where((device) => device.trustLabel != '已验证')
        .toList(growable: false);
    final reviewTarget = pendingDevices.isEmpty ? null : pendingDevices.first;
    final trustedDeviceCount = devices.length - pendingDevices.length;

    if (isCupertinoStyle) {
      return _CupertinoSettingsBody(
        devices: devices,
        session: session,
        preference: preference,
        resolvedVisualTier: resolvedVisualTier,
        pendingDevices: pendingDevices,
        reviewTarget: reviewTarget,
        trustedDeviceCount: trustedDeviceCount,
        onPreferenceChanged: (value) {
          ref.read(visualPreferenceProvider.notifier).setPreference(value);
        },
      );
    }

    return _MaterialSettingsBody(
      devices: devices,
      session: session,
      preference: preference,
      resolvedVisualTier: resolvedVisualTier,
      pendingDevices: pendingDevices,
      reviewTarget: reviewTarget,
      trustedDeviceCount: trustedDeviceCount,
      onPreferenceChanged: (value) {
        ref.read(visualPreferenceProvider.notifier).setPreference(value);
      },
    );
  }
}

class _CupertinoSettingsBody extends StatefulWidget {
  const _CupertinoSettingsBody({
    required this.devices,
    required this.session,
    required this.preference,
    required this.resolvedVisualTier,
    required this.pendingDevices,
    required this.reviewTarget,
    required this.trustedDeviceCount,
    required this.onPreferenceChanged,
  });

  final List<DeviceTrustInfo> devices;
  final SessionState session;
  final VisualPreference preference;
  final VisualTier resolvedVisualTier;
  final List<DeviceTrustInfo> pendingDevices;
  final DeviceTrustInfo? reviewTarget;
  final int trustedDeviceCount;
  final ValueChanged<VisualPreference> onPreferenceChanged;

  @override
  State<_CupertinoSettingsBody> createState() => _CupertinoSettingsBodyState();
}

class _CupertinoSettingsBodyState extends State<_CupertinoSettingsBody> {
  late final ScrollController _scrollController;
  double _collapseProgress = 0;

  @override
  void initState() {
    super.initState();
    _scrollController = ScrollController()..addListener(_handleScroll);
  }

  void _handleScroll() {
    final nextProgress =
        (_scrollController.hasClients ? _scrollController.offset / 34 : 0.0)
            .clamp(0.0, 1.0);
    if ((nextProgress - _collapseProgress).abs() < 0.02) {
      return;
    }
    setState(() {
      _collapseProgress = nextProgress;
    });
  }

  @override
  void dispose() {
    _scrollController
      ..removeListener(_handleScroll)
      ..dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Stack(
      children: <Widget>[
        ListView(
          controller: _scrollController,
          padding: const EdgeInsets.fromLTRB(0, 50, 0, AppTokens.spacingXl),
          children: <Widget>[
            Padding(
              padding: const EdgeInsets.fromLTRB(18, 0, 18, 10),
              child: Text(
                '设置',
                key: const Key('settings-security-large-title-cupertino'),
                style: Theme.of(context).textTheme.headlineMedium?.copyWith(
                  fontSize: 30.5,
                  fontWeight: FontWeight.w600,
                  letterSpacing: -0.72,
                  height: 1.0,
                ),
              ),
            ),
            _SettingsSectionHeader(
              title: '账户',
              key: const Key('settings-security-section-account'),
            ),
            const SizedBox(height: 7),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 14),
              child: _SettingsGroup(
                key: const Key('settings-security-group-account'),
                children: <Widget>[
                  _SettingsAccountRow(
                    displayName: widget.session.profile?.displayName ?? '当前账号',
                    username: widget.session.profile?.username ?? '已登录',
                    subtitle: widget.pendingDevices.isEmpty
                        ? '消息与设备同步正常'
                        : '${widget.reviewTarget?.name ?? '某台设备'} 需要确认',
                    trustedDeviceCount: widget.trustedDeviceCount,
                    pendingDeviceCount: widget.pendingDevices.length,
                  ),
                ],
              ),
            ),
            const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
            _SettingsSectionHeader(
              key: const Key('settings-security-section-messages'),
              title: '通知',
            ),
            const SizedBox(height: 7),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 14),
              child: _SettingsGroup(
                key: const Key('settings-security-group-messages'),
                children: const <Widget>[
                  _SettingsRow(
                    icon: AppSemanticIcon.notifications,
                    title: '通知与提醒',
                    subtitle: '新消息、@提及与服务通知',
                    trailingLabel: '已开启',
                    tint: AppPalette.mistBlue,
                  ),
                  _SettingsRow(
                    icon: AppSemanticIcon.sessions,
                    title: '消息预览',
                    subtitle: '仅显示联系人与摘要',
                    trailingLabel: '摘要',
                    tint: AppPalette.success,
                  ),
                ],
              ),
            ),
            const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
            _SettingsSectionHeader(
              key: const Key('settings-security-section-privacy'),
              title: '聊天',
            ),
            const SizedBox(height: 7),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 14),
              child: _SettingsGroup(
                key: const Key('settings-security-group-privacy'),
                children: const <Widget>[
                  _SettingsRow(
                    icon: AppSemanticIcon.sessions,
                    title: '聊天记录',
                    subtitle: '消息、文件和草稿同步',
                    trailingLabel: '同步',
                    tint: AppPalette.mistBlue,
                  ),
                  _SettingsRow(
                    icon: AppSemanticIcon.file,
                    title: '文件与媒体',
                    subtitle: '图片、文件和下载入口',
                    trailingLabel: '按需',
                    tint: AppPalette.success,
                  ),
                ],
              ),
            ),
            const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
            _SettingsSectionHeader(title: '外观'),
            const SizedBox(height: 7),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 14),
              child: _SettingsGroup(
                key: const Key('settings-security-group-appearance'),
                children: <Widget>[
                  _VisualModePanel(
                    preference: widget.preference,
                    resolvedVisualTier: widget.resolvedVisualTier,
                    onChanged: widget.onPreferenceChanged,
                    cupertinoStyle: true,
                  ),
                ],
              ),
            ),
            const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
            _SettingsSectionHeader(title: '隐私与安全'),
            const SizedBox(height: 7),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 14),
              child: _SettingsGroup(
                key: const Key('settings-security-group-device-security'),
                children: <Widget>[
                  const _SettingsRow(
                    icon: AppSemanticIcon.lock,
                    title: '会话锁定',
                    subtitle: '离开应用 15 分钟后重新确认',
                    trailingLabel: '15 分钟',
                    tint: AppPalette.warning,
                  ),
                  const _SettingsRow(
                    icon: AppSemanticIcon.privacy,
                    title: '隐私保护',
                    subtitle: '敏感页面按需增强',
                    trailingLabel: '按需',
                    tint: AppPalette.mistBlue,
                  ),
                  const _SettingsRow(
                    icon: AppSemanticIcon.key,
                    title: '加密与设备',
                    subtitle: '查看当前聊天和登录状态',
                    trailingLabel: '已开启',
                    tint: AppPalette.success,
                  ),
                  _SettingsRow(
                    icon: AppSemanticIcon.alert,
                    title: '需要确认的设备',
                    subtitle: widget.reviewTarget == null
                        ? '当前无需要确认的设备'
                        : '${widget.reviewTarget!.name} 需要确认',
                    trailingLabel: widget.reviewTarget == null ? '0' : '1',
                    tint: AppPalette.warning,
                  ),
                ],
              ),
            ),
            const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
            _SettingsSectionHeader(title: '设备'),
            const SizedBox(height: 7),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 14),
              child: _SettingsGroup(
                key: const Key('settings-security-group-devices'),
                children: <Widget>[
                  if (widget.devices.isEmpty)
                    const _EmptyStateRow(
                      icon: AppSemanticIcon.device,
                      title: '暂无设备',
                      subtitle: '等待同步后会在这里显示已登录设备。',
                    )
                  else
                    for (var index = 0; index < widget.devices.length; index++)
                      _DeviceRow(
                        device: widget.devices[index],
                        isLast: index == widget.devices.length - 1,
                      ),
                ],
              ),
            ),
            const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
            _SettingsSectionHeader(title: '存储与恢复'),
            const SizedBox(height: 7),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 14),
              child: _SettingsGroup(
                key: const Key('settings-security-group-storage'),
                children: const <Widget>[
                  _SettingsRow(
                    icon: AppSemanticIcon.file,
                    title: '本机缓存',
                    subtitle: '聊天缓存与离线文件',
                    trailingLabel: '管理',
                    tint: AppPalette.mistBlue,
                  ),
                  _SettingsRow(
                    icon: AppSemanticIcon.sync,
                    title: '自动恢复队列',
                    subtitle: '联网后继续失败任务',
                    trailingLabel: '自动',
                    tint: AppPalette.success,
                  ),
                ],
              ),
            ),
          ],
        ),
        _CupertinoSettingsHeader(progress: _collapseProgress),
      ],
    );
  }
}

class _MaterialSettingsBody extends StatelessWidget {
  const _MaterialSettingsBody({
    required this.devices,
    required this.session,
    required this.preference,
    required this.resolvedVisualTier,
    required this.pendingDevices,
    required this.reviewTarget,
    required this.trustedDeviceCount,
    required this.onPreferenceChanged,
  });

  final List<DeviceTrustInfo> devices;
  final SessionState session;
  final VisualPreference preference;
  final VisualTier resolvedVisualTier;
  final List<DeviceTrustInfo> pendingDevices;
  final DeviceTrustInfo? reviewTarget;
  final int trustedDeviceCount;
  final ValueChanged<VisualPreference> onPreferenceChanged;

  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.fromLTRB(
        AppTokens.spacingLg,
        AppTokens.spacingLg,
        AppTokens.spacingLg,
        AppTokens.spacingXl,
      ),
      children: <Widget>[
        const ShellPageHeader(title: '设置', subtitle: null, eyebrow: null),
        const SizedBox(height: AppTokens.spacingLg),
        _SettingsProfileCard(
          displayName: session.profile?.displayName ?? '当前账号',
          username: session.profile?.username ?? '已登录',
          subtitle: pendingDevices.isEmpty
              ? '消息与设备同步正常'
              : '${reviewTarget?.name ?? '某台设备'} 需要确认',
          trustedDeviceCount: trustedDeviceCount,
          pendingDeviceCount: pendingDevices.length,
        ),
        const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
        const _SettingsSectionHeader(title: '通知'),
        const SizedBox(height: AppTokens.spacingSm),
        const _SettingsGroup(
          children: <Widget>[
            _SettingsRow(
              icon: AppSemanticIcon.notifications,
              title: '通知与提醒',
              subtitle: '新消息、@提及与服务通知',
              trailingLabel: '已开启',
              tint: AppPalette.mistBlue,
            ),
          ],
        ),
        const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
        const _SettingsSectionHeader(title: '聊天'),
        const SizedBox(height: AppTokens.spacingSm),
        const _SettingsGroup(
          children: <Widget>[
            _SettingsRow(
              icon: AppSemanticIcon.sessions,
              title: '聊天记录',
              subtitle: '消息、文件和草稿同步',
              trailingLabel: '同步',
              tint: AppPalette.mistBlue,
            ),
          ],
        ),
        const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
        const _SettingsSectionHeader(title: '外观'),
        const SizedBox(height: AppTokens.spacingSm),
        _SettingsGroup(
          children: <Widget>[
            _VisualModePanel(
              preference: preference,
              resolvedVisualTier: resolvedVisualTier,
              onChanged: onPreferenceChanged,
              cupertinoStyle: false,
            ),
          ],
        ),
        const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
        const _SettingsSectionHeader(title: '隐私与安全'),
        const SizedBox(height: AppTokens.spacingSm),
        _SettingsGroup(
          children: <Widget>[
            const _SettingsRow(
              icon: AppSemanticIcon.privacy,
              title: '隐私保护',
              subtitle: '敏感页面按需增强',
              trailingLabel: '按需',
              tint: AppPalette.mistBlue,
            ),
            const _SettingsRow(
              icon: AppSemanticIcon.lock,
              title: '会话锁定',
              subtitle: '离开应用 15 分钟后重新确认',
              trailingLabel: '15 分钟',
              tint: AppPalette.warning,
            ),
            const _SettingsRow(
              icon: AppSemanticIcon.key,
              title: '加密与设备',
              subtitle: '查看当前聊天和登录状态',
              trailingLabel: '已开启',
              tint: AppPalette.success,
            ),
          ],
        ),
        const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
        const _SettingsSectionHeader(title: '设备'),
        const SizedBox(height: AppTokens.spacingSm),
        _SettingsGroup(
          children: <Widget>[
            _SettingsRow(
              icon: AppSemanticIcon.device,
              title: '登录中的设备',
              subtitle: '${devices.length} 台在线，$trustedDeviceCount 台已验证',
              trailingLabel: '$trustedDeviceCount 已验证',
              tint: AppPalette.mistBlue,
            ),
            _SettingsRow(
              icon: AppSemanticIcon.alert,
              title: '需要确认的设备',
              subtitle: reviewTarget == null
                  ? '当前无需要确认的设备'
                  : '${reviewTarget!.name} 需要确认',
              trailingLabel: reviewTarget == null ? '0' : '1',
              tint: AppPalette.warning,
            ),
          ],
        ),
        const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
        const _SettingsSectionHeader(title: '设备列表'),
        const SizedBox(height: AppTokens.spacingSm),
        _SettingsGroup(
          children: <Widget>[
            if (devices.isEmpty)
              const _EmptyStateRow(
                icon: AppSemanticIcon.device,
                title: '暂无设备',
                subtitle: '等待同步后会在这里显示已登录设备。',
              )
            else
              for (var index = 0; index < devices.length; index++)
                _DeviceRow(
                  device: devices[index],
                  isLast: index == devices.length - 1,
                ),
          ],
        ),
        const SizedBox(height: SettingsSecurityScreen._sectionSpacing),
        const _SettingsSectionHeader(title: '存储与恢复'),
        const SizedBox(height: AppTokens.spacingSm),
        const _SettingsGroup(
          children: <Widget>[
            _SettingsRow(
              icon: AppSemanticIcon.file,
              title: '本机缓存',
              subtitle: '聊天缓存与离线文件',
              trailingLabel: '管理',
              tint: AppPalette.mistBlue,
            ),
            _SettingsRow(
              icon: AppSemanticIcon.sync,
              title: '自动恢复队列',
              subtitle: '联网后继续失败任务',
              trailingLabel: '自动',
              tint: AppPalette.success,
            ),
          ],
        ),
      ],
    );
  }
}

class _CupertinoSettingsHeader extends StatelessWidget {
  const _CupertinoSettingsHeader({required this.progress});

  final double progress;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final palette = theme.extension<AppShellColors>()!;
    final easedProgress = Curves.easeOut.transform(progress);
    final headerSurface = Color.alphaBlend(
      Colors.white.withValues(
        alpha: (material.isGlass ? 0.012 : 0.08) * easedProgress,
      ),
      material.topBarColor.withValues(
        alpha: (material.isGlass ? 0.36 : 0.60) * easedProgress,
      ),
    );
    final headerBorder = Color.alphaBlend(
      Colors.white.withValues(
        alpha: (material.isGlass ? 0.03 : 0.03) * easedProgress,
      ),
      palette.divider.withValues(alpha: 0.05 * easedProgress),
    );

    return SafeArea(
      bottom: false,
      child: Align(
        alignment: Alignment.topCenter,
        child: Padding(
          padding: const EdgeInsets.only(top: 2),
          child: ConstrainedBox(
            constraints: const BoxConstraints.tightFor(width: 96, height: 28),
            child: Opacity(
              opacity: easedProgress,
              child: LiquidGlassPanel(
                key: const Key('settings-security-header-shell-cupertino'),
                enabled: material.isGlass,
                role: ShellChromeRole.topBar,
                radius: 13,
                blurSigma: material.blurSigma * 0.42,
                shadowOpacity: 0.004 * easedProgress,
                backgroundColor: headerSurface,
                borderColor: headerBorder,
                padding: const EdgeInsets.symmetric(horizontal: 0.5),
                child: Center(
                  child: Opacity(
                    opacity: easedProgress,
                    child: Text(
                      '设置',
                      key: const Key('settings-security-nav-title-cupertino'),
                      style: theme.textTheme.titleMedium?.copyWith(
                        fontSize: 15.8,
                        fontWeight: FontWeight.w600,
                        letterSpacing: -0.24,
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _SettingsProfileCard extends StatelessWidget {
  const _SettingsProfileCard({
    required this.displayName,
    required this.username,
    required this.subtitle,
    required this.trustedDeviceCount,
    required this.pendingDeviceCount,
  });

  final String displayName;
  final String username;
  final String subtitle;
  final int trustedDeviceCount;
  final int pendingDeviceCount;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final palette = theme.extension<AppShellColors>()!;

    return Container(
      padding: const EdgeInsets.symmetric(
        horizontal: AppTokens.spacingLg,
        vertical: AppTokens.spacingSm + 2,
      ),
      decoration: BoxDecoration(
        color: material.surfaceColor.withValues(alpha: 0.96),
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: palette.divider.withValues(alpha: 0.18)),
      ),
      child: Row(
        children: <Widget>[
          CircleAvatar(
            radius: 26,
            backgroundColor: AppPalette.mistBlue.withValues(alpha: 0.16),
            child: Text(
              displayName.characters.first,
              style: theme.textTheme.titleLarge?.copyWith(
                fontWeight: FontWeight.w700,
              ),
            ),
          ),
          const SizedBox(width: AppTokens.spacingMd),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  '当前账号',
                  style: theme.textTheme.labelMedium?.copyWith(
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: 0.76,
                    ),
                    fontWeight: FontWeight.w700,
                  ),
                ),
                const SizedBox(height: 6),
                Text(
                  displayName,
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 2),
                Text(username, style: theme.textTheme.labelMedium),
                const SizedBox(height: 4),
                Text(subtitle, style: theme.textTheme.bodySmall),
              ],
            ),
          ),
          const SizedBox(width: AppTokens.spacingSm),
          _SettingsStatusBadge(
            label: pendingDeviceCount == 0
                ? '$trustedDeviceCount 已验证'
                : '$pendingDeviceCount 需确认',
            tint: pendingDeviceCount == 0
                ? AppPalette.success
                : AppPalette.warning,
          ),
        ],
      ),
    );
  }
}

class _SettingsAccountRow extends StatelessWidget {
  const _SettingsAccountRow({
    required this.displayName,
    required this.username,
    required this.subtitle,
    required this.trustedDeviceCount,
    required this.pendingDeviceCount,
  });

  final String displayName;
  final String username;
  final String subtitle;
  final int trustedDeviceCount;
  final int pendingDeviceCount;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final tint = pendingDeviceCount == 0
        ? AppPalette.success
        : AppPalette.warning;
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;

    return Padding(
      padding: EdgeInsets.fromLTRB(
        isCupertinoStyle ? 13 : 14,
        isCupertinoStyle ? 9 : 11,
        isCupertinoStyle ? 13 : 14,
        isCupertinoStyle ? 9 : 11,
      ),
      child: Row(
        children: <Widget>[
          CircleAvatar(
            radius: isCupertinoStyle ? 18 : 21,
            backgroundColor: AppPalette.mistBlue.withValues(alpha: 0.16),
            child: Text(
              displayName.characters.first,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w700,
              ),
            ),
          ),
          const SizedBox(width: 11),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  '当前账号',
                  style: theme.textTheme.labelSmall?.copyWith(
                    fontSize: 11.2,
                    fontWeight: FontWeight.w700,
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: 0.66,
                    ),
                  ),
                ),
                const SizedBox(height: 1),
                Text(
                  displayName,
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontSize: isCupertinoStyle ? 15.5 : 15.2,
                    fontWeight: FontWeight.w500,
                    letterSpacing: -0.24,
                  ),
                ),
                const SizedBox(height: 1),
                Text(
                  '$username · $subtitle',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(
                    fontSize: isCupertinoStyle ? 11.6 : 12.0,
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: 0.74,
                    ),
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 8),
          if (isCupertinoStyle)
            Row(
              mainAxisSize: MainAxisSize.min,
              children: <Widget>[
                _SettingsStatusBadge(
                  label: pendingDeviceCount == 0
                      ? '$trustedDeviceCount 已验证'
                      : '$pendingDeviceCount 需确认',
                  tint: tint,
                  compact: true,
                ),
                const SizedBox(width: 5),
                AppIcon(
                  AppSemanticIcon.chevronRight,
                  size: 13,
                  color: theme.textTheme.bodySmall?.color?.withValues(
                    alpha: 0.5,
                  ),
                ),
              ],
            )
          else
            _SettingsStatusBadge(
              label: pendingDeviceCount == 0
                  ? '$trustedDeviceCount 已验证'
                  : '$pendingDeviceCount 需确认',
              tint: tint,
              compact: true,
            ),
        ],
      ),
    );
  }
}

class _SettingsStatusBadge extends StatelessWidget {
  const _SettingsStatusBadge({
    required this.label,
    required this.tint,
    this.compact = false,
  });

  final String label;
  final Color tint;
  final bool compact;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final neutralText = theme.textTheme.bodySmall?.color?.withValues(
      alpha: compact ? 0.78 : 0.82,
    );
    final keepSemanticTint =
        tint == AppPalette.warning || tint == AppPalette.danger;
    return Container(
      padding: EdgeInsets.symmetric(
        horizontal: compact ? 6.5 : 10,
        vertical: compact ? 3.5 : 7,
      ),
      decoration: BoxDecoration(
        color: keepSemanticTint
            ? tint.withValues(alpha: 0.1)
            : Theme.of(context)
                  .extension<AppShellColors>()!
                  .surfaceVariant
                  .withValues(alpha: compact ? 0.46 : 0.58),
        borderRadius: BorderRadius.circular(999),
      ),
      child: Text(
        label,
        style: theme.textTheme.labelMedium?.copyWith(
          color: keepSemanticTint ? tint : neutralText,
          fontWeight: FontWeight.w600,
          fontSize: compact ? 10.1 : null,
        ),
      ),
    );
  }
}

class _SettingsSectionHeader extends StatelessWidget {
  const _SettingsSectionHeader({super.key, required this.title});

  final String title;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    return Padding(
      padding: EdgeInsets.symmetric(horizontal: isCupertinoStyle ? 17 : 4),
      child: Text(
        title,
        style:
            (isCupertinoStyle
                    ? theme.textTheme.labelMedium
                    : theme.textTheme.labelLarge)
                ?.copyWith(
                  color: theme
                      .extension<AppShellColors>()!
                      .textSecondary
                      .withValues(alpha: isCupertinoStyle ? 0.78 : 1),
                  fontWeight: FontWeight.w600,
                  fontSize: isCupertinoStyle ? 11.0 : null,
                  letterSpacing: isCupertinoStyle ? 0.18 : 0.22,
                ),
      ),
    );
  }
}

class _SettingsGroup extends StatelessWidget {
  const _SettingsGroup({super.key, required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final palette = theme.extension<AppShellColors>()!;
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final radius = BorderRadius.circular(isCupertinoStyle ? 12 : 20);
    final decoration = BoxDecoration(
      color: Color.alphaBlend(
        Colors.white.withValues(
          alpha: isCupertinoStyle ? (material.isGlass ? 0.007 : 0.01) : 0,
        ),
        material.surfaceColor.withValues(
          alpha: isCupertinoStyle ? (material.isGlass ? 0.78 : 0.974) : 0.90,
        ),
      ),
      gradient: isCupertinoStyle
          ? LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment.bottomCenter,
              colors: <Color>[
                Colors.white.withValues(
                  alpha: material.isGlass ? 0.009 : 0.008,
                ),
                Colors.transparent,
              ],
            )
          : null,
      borderRadius: radius,
      border: Border.all(
        color: palette.divider.withValues(
          alpha: isCupertinoStyle ? (material.isGlass ? 0.018 : 0.018) : 0.14,
        ),
      ),
      boxShadow: isCupertinoStyle && material.isGlass
          ? <BoxShadow>[
              BoxShadow(
                color: Colors.black.withValues(alpha: 0.004),
                blurRadius: 10,
                offset: const Offset(0, 3),
              ),
            ]
          : const <BoxShadow>[],
    );

    final content = DecoratedBox(
      decoration: decoration,
      child: ClipRRect(
        borderRadius: radius,
        child: Column(
          children: <Widget>[
            for (var index = 0; index < children.length; index++) ...<Widget>[
              children[index],
              if (index != children.length - 1)
                Divider(
                  height: 1,
                  indent: isCupertinoStyle ? 48 : 62,
                  endIndent: isCupertinoStyle ? 12 : 16,
                  color: palette.divider.withValues(
                    alpha: isCupertinoStyle
                        ? (material.isGlass ? 0.07 : 0.06)
                        : 0.22,
                  ),
                ),
            ],
          ],
        ),
      ),
    );

    if (isCupertinoStyle && material.isGlass) {
      return ClipRRect(
        borderRadius: radius,
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 8, sigmaY: 8),
          child: content,
        ),
      );
    }

    return content;
  }
}

class _SettingsRow extends StatelessWidget {
  const _SettingsRow({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.trailingLabel,
    required this.tint,
  });

  final AppSemanticIcon icon;
  final String title;
  final String subtitle;
  final String trailingLabel;
  final Color tint;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final iconTint = tint == AppPalette.warning || tint == AppPalette.danger
        ? tint
        : theme.colorScheme.primary.withValues(alpha: 0.82);
    final chevronColor = theme.textTheme.bodySmall?.color?.withValues(
      alpha: 0.54,
    );

    return Padding(
      padding: EdgeInsets.symmetric(
        horizontal: isCupertinoStyle ? 13 : AppTokens.spacingLg,
        vertical: isCupertinoStyle ? 6.5 : 14,
      ),
      child: Row(
        children: <Widget>[
          Container(
            width: isCupertinoStyle ? 22 : 30,
            height: isCupertinoStyle ? 22 : 30,
            decoration: BoxDecoration(
              color: iconTint.withValues(alpha: isCupertinoStyle ? 0.11 : 0.12),
              borderRadius: BorderRadius.circular(isCupertinoStyle ? 6.0 : 10),
            ),
            alignment: Alignment.center,
            child: AppIcon(
              icon,
              size: isCupertinoStyle ? 12.5 : 16,
              color: iconTint,
            ),
          ),
          const SizedBox(width: 9),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  title,
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: isCupertinoStyle
                        ? FontWeight.w500
                        : FontWeight.w600,
                    fontSize: isCupertinoStyle ? 15.2 : null,
                    letterSpacing: isCupertinoStyle ? -0.24 : null,
                  ),
                ),
                const SizedBox(height: 1),
                Text(
                  subtitle,
                  style: theme.textTheme.bodySmall?.copyWith(
                    fontSize: isCupertinoStyle ? 11.4 : null,
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: isCupertinoStyle ? 0.72 : 1,
                    ),
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: AppTokens.spacingSm),
          Text(
            trailingLabel,
            style: theme.textTheme.labelMedium?.copyWith(
              color: theme.textTheme.bodySmall?.color?.withValues(
                alpha: isCupertinoStyle ? 0.72 : 0.76,
              ),
              fontWeight: FontWeight.w500,
              fontSize: isCupertinoStyle ? 10.8 : 11.0,
            ),
          ),
          const SizedBox(width: 4.5),
          AppIcon(
            AppSemanticIcon.chevronRight,
            size: isCupertinoStyle ? 12.5 : 18,
            color: chevronColor,
          ),
        ],
      ),
    );
  }
}

class _VisualModePanel extends StatelessWidget {
  const _VisualModePanel({
    required this.preference,
    required this.resolvedVisualTier,
    required this.onChanged,
    required this.cupertinoStyle,
  });

  final VisualPreference preference;
  final VisualTier resolvedVisualTier;
  final ValueChanged<VisualPreference> onChanged;
  final bool cupertinoStyle;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    Widget segmentedControl;
    if (cupertinoStyle) {
      segmentedControl = CupertinoSlidingSegmentedControl<VisualPreference>(
        key: const ValueKey('visual-mode-segmented-control'),
        groupValue: preference,
        children: const <VisualPreference, Widget>{
          VisualPreference.auto: _VisualModeChoice(label: '自动', compact: true),
          VisualPreference.forceStandard: _VisualModeChoice(
            label: '标准',
            compact: true,
          ),
          VisualPreference.forceEnhanced: _VisualModeChoice(
            label: '增强',
            compact: true,
          ),
        },
        onValueChanged: (value) {
          if (value != null) {
            onChanged(value);
          }
        },
      );
    } else {
      segmentedControl = _MaterialVisualModeControl(
        key: const ValueKey('visual-mode-segmented-control'),
        value: preference,
        onChanged: onChanged,
      );
    }

    return Padding(
      padding: EdgeInsets.fromLTRB(
        cupertinoStyle ? 11.5 : AppTokens.spacingLg,
        cupertinoStyle ? 6.5 : AppTokens.spacingMd,
        cupertinoStyle ? 11.5 : AppTokens.spacingLg,
        cupertinoStyle ? 6.5 : AppTokens.spacingLg,
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Row(
            children: <Widget>[
              Expanded(
                child: Text(
                  '视觉模式',
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: cupertinoStyle
                        ? FontWeight.w500
                        : FontWeight.w600,
                    fontSize: cupertinoStyle ? 15.2 : null,
                    letterSpacing: cupertinoStyle ? -0.22 : null,
                  ),
                ),
              ),
              if (cupertinoStyle)
                Text(
                  _visualModeShortLabel(preference),
                  style: theme.textTheme.labelMedium?.copyWith(
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: 0.72,
                    ),
                    fontWeight: FontWeight.w500,
                  ),
                ),
            ],
          ),
          const SizedBox(height: 1.5),
          Text(
            _visualModeSubtitle(
              preference: preference,
              resolvedVisualTier: resolvedVisualTier,
            ),
            style: theme.textTheme.bodySmall?.copyWith(
              fontSize: cupertinoStyle ? 11.5 : null,
            ),
          ),
          const SizedBox(height: 6),
          segmentedControl,
          const SizedBox(height: 4),
          Text(
            '增强模式仅在支持的设备上启用。',
            style: theme.textTheme.bodySmall?.copyWith(
              fontSize: cupertinoStyle ? 10.9 : null,
            ),
          ),
        ],
      ),
    );
  }
}

class _VisualModeChoice extends StatelessWidget {
  const _VisualModeChoice({required this.label, required this.compact});

  final String label;
  final bool compact;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.symmetric(
        horizontal: compact ? 8 : 10,
        vertical: compact ? 8 : 9,
      ),
      child: Text(
        label,
        style: Theme.of(
          context,
        ).textTheme.labelMedium?.copyWith(fontWeight: FontWeight.w700),
      ),
    );
  }
}

class _MaterialVisualModeControl extends StatelessWidget {
  const _MaterialVisualModeControl({
    super.key,
    required this.value,
    required this.onChanged,
  });

  final VisualPreference value;
  final ValueChanged<VisualPreference> onChanged;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Container(
      decoration: BoxDecoration(
        color: palette.surfaceVariant.withValues(alpha: 0.34),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: palette.divider.withValues(alpha: 0.12)),
      ),
      child: Row(
        children: <Widget>[
          for (final option in VisualPreference.values)
            Expanded(
              child: Padding(
                padding: const EdgeInsets.all(2),
                child: _MaterialVisualModeControlChip(
                  label: switch (option) {
                    VisualPreference.auto => '自动',
                    VisualPreference.forceStandard => '标准模式',
                    VisualPreference.forceEnhanced => '增强模式',
                  },
                  selected: option == value,
                  onTap: () => onChanged(option),
                ),
              ),
            ),
        ],
      ),
    );
  }
}

class _MaterialVisualModeControlChip extends StatelessWidget {
  const _MaterialVisualModeControlChip({
    required this.label,
    required this.selected,
    required this.onTap,
  });

  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return InkWell(
      borderRadius: BorderRadius.circular(10),
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 180),
        curve: Curves.easeOutCubic,
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 8),
        decoration: BoxDecoration(
          color: selected
              ? theme.colorScheme.primary.withValues(alpha: 0.12)
              : Colors.transparent,
          borderRadius: BorderRadius.circular(10),
          border: Border.all(
            color: selected
                ? theme.colorScheme.primary.withValues(alpha: 0.16)
                : Colors.transparent,
          ),
        ),
        child: Text(
          label,
          textAlign: TextAlign.center,
          style: theme.textTheme.labelMedium?.copyWith(
            fontWeight: selected ? FontWeight.w700 : FontWeight.w600,
            color: selected
                ? theme.colorScheme.onSurface
                : palette.textSecondary.withValues(alpha: 0.86),
          ),
        ),
      ),
    );
  }
}

class _DeviceRow extends StatelessWidget {
  const _DeviceRow({required this.device, required this.isLast});

  final DeviceTrustInfo device;
  final bool isLast;

  @override
  Widget build(BuildContext context) {
    final tint = device.trustLabel == '已验证'
        ? AppPalette.success
        : AppPalette.warning;
    final theme = Theme.of(context);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final iconTint = device.trustLabel == '已验证'
        ? theme.colorScheme.primary.withValues(alpha: 0.82)
        : tint;

    return Padding(
      padding: EdgeInsets.symmetric(
        horizontal: isCupertinoStyle ? 13 : AppTokens.spacingLg,
        vertical: isCupertinoStyle ? 7.5 : 14,
      ),
      child: Row(
        children: <Widget>[
          Container(
            width: isCupertinoStyle ? 23 : 30,
            height: isCupertinoStyle ? 23 : 30,
            decoration: BoxDecoration(
              color: iconTint.withValues(alpha: 0.12),
              borderRadius: BorderRadius.circular(isCupertinoStyle ? 6.0 : 10),
            ),
            alignment: Alignment.center,
            child: AppIcon(
              AppSemanticIcon.device,
              size: isCupertinoStyle ? 13.0 : 16,
              color: iconTint,
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  device.name,
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: isCupertinoStyle
                        ? FontWeight.w500
                        : FontWeight.w600,
                    fontSize: isCupertinoStyle ? 15.5 : null,
                    letterSpacing: isCupertinoStyle ? -0.22 : null,
                  ),
                ),
                const SizedBox(height: 1),
                Text(
                  '${device.fingerprint} · ${device.lastSeenLabel}',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(
                    fontSize: isCupertinoStyle ? 11.6 : null,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: AppTokens.spacingSm),
          Text(
            device.trustLabel,
            style: theme.textTheme.labelMedium?.copyWith(
              color: theme.textTheme.bodySmall?.color?.withValues(
                alpha: isCupertinoStyle ? 0.74 : 0.78,
              ),
              fontWeight: FontWeight.w500,
              fontSize: isCupertinoStyle ? 10.9 : 11.0,
            ),
          ),
          if (isCupertinoStyle) ...<Widget>[
            const SizedBox(width: 4.5),
            AppIcon(
              AppSemanticIcon.chevronRight,
              size: 12.5,
              color: theme.textTheme.bodySmall?.color?.withValues(alpha: 0.52),
            ),
          ],
        ],
      ),
    );
  }
}

class _EmptyStateRow extends StatelessWidget {
  const _EmptyStateRow({
    required this.icon,
    required this.title,
    required this.subtitle,
  });

  final AppSemanticIcon icon;
  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) {
    final isCupertinoStyle = Theme.of(context).platform == TargetPlatform.iOS;
    return Padding(
      padding: EdgeInsets.symmetric(
        horizontal: isCupertinoStyle ? 14 : AppTokens.spacingLg,
        vertical: isCupertinoStyle ? 14 : 18,
      ),
      child: Row(
        children: <Widget>[
          Container(
            width: isCupertinoStyle ? 26 : 30,
            height: isCupertinoStyle ? 26 : 30,
            decoration: BoxDecoration(
              color: AppPalette.mistBlue.withValues(alpha: 0.12),
              borderRadius: BorderRadius.circular(isCupertinoStyle ? 7 : 10),
            ),
            alignment: Alignment.center,
            child: AppIcon(
              icon,
              size: isCupertinoStyle ? 14 : 16,
              color: AppPalette.mistBlue,
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  title,
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 2),
                Text(subtitle, style: Theme.of(context).textTheme.bodySmall),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

String _visualModeSubtitle({
  required VisualPreference preference,
  required VisualTier resolvedVisualTier,
}) {
  return switch (preference) {
    VisualPreference.auto => switch (resolvedVisualTier) {
      VisualTier.enhancedGlass => '跟随设备能力，当前为增强',
      VisualTier.standard => '跟随设备能力，当前为标准',
      VisualTier.auto => '跟随设备能力',
    },
    VisualPreference.forceStandard => '固定使用标准模式',
    VisualPreference.forceEnhanced => '固定使用增强模式',
  };
}

String _visualModeShortLabel(VisualPreference preference) {
  return switch (preference) {
    VisualPreference.auto => '自动',
    VisualPreference.forceStandard => '标准',
    VisualPreference.forceEnhanced => '增强',
  };
}
