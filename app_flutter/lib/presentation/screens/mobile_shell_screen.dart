import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter/services.dart';
import 'package:go_router/go_router.dart';

import '../../application/chat_providers.dart';
import '../../bootstrap/app_providers.dart';
import '../../domain/entities/models.dart';
import '../theme/app_icons.dart';
import '../theme/app_theme.dart';
import '../theme/app_tokens.dart';
import '../widgets/shell_chrome.dart';
import 'mobile_chat_detail_screen.dart';
import 'mobile_chats_screen.dart';
import 'mobile_contacts_screen.dart';
import 'settings_security_screen.dart';

class MobileShellScreen extends ConsumerWidget {
  const MobileShellScreen({super.key, required this.section});

  final AppSection section;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final activeConversation = ref.watch(activeConversationProvider);
    final activeMessages = ref
        .watch(activeMessagesProvider)
        .maybeWhen(data: (value) => value, orElse: () => const <ChatMessage>[]);
    final selectedConversationId = ref.watch(selectedConversationIdProvider);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final showingChatDetail =
        section == AppSection.chats &&
        selectedConversationId != null &&
        activeConversation != null;
    final bottomNavigationClearance = material.isGlass && !showingChatDetail
        ? (isCupertinoStyle ? 90.0 : 72.0)
        : 0.0;
    final baseOuterPadding = switch (section) {
      AppSection.chats => EdgeInsets.zero,
      AppSection.contacts =>
        isCupertinoStyle
            ? const EdgeInsets.fromLTRB(10, 8, 10, 0)
            : const EdgeInsets.fromLTRB(12, 8, 12, 0),
      AppSection.settings => EdgeInsets.zero,
    };
    final outerPadding = baseOuterPadding.copyWith(
      bottom: baseOuterPadding.bottom + bottomNavigationClearance,
    );
    final shellGradientColors = material.isGlass
        ? isCupertinoStyle
              ? const <Color>[
                  Color(0xFF08111A),
                  Color(0xFF111A25),
                  Color(0xFF192636),
                ]
              : const <Color>[
                  Color(0xFF09121A),
                  Color(0xFF121C27),
                  Color(0xFF1A2836),
                ]
        : isCupertinoStyle
        ? const <Color>[Color(0xFF0D151E), Color(0xFF182330)]
        : const <Color>[Color(0xFF101923), AppPalette.midnight];
    final navigationSurface = material.isGlass
        ? Color.alphaBlend(
            Colors.white.withValues(alpha: isCupertinoStyle ? 0.024 : 0.012),
            material.bottomBarColor,
          )
        : Color.alphaBlend(
            Colors.white.withValues(alpha: isCupertinoStyle ? 0.0 : 0.006),
            theme.cardColor.withValues(alpha: isCupertinoStyle ? 0.95 : 0.92),
          );
    final navigationOuterPadding = material.isGlass
        ? EdgeInsets.fromLTRB(
            isCupertinoStyle ? 18 : 10,
            0,
            isCupertinoStyle ? 18 : 10,
            isCupertinoStyle ? 8 : 8,
          )
        : isCupertinoStyle
        ? EdgeInsets.zero
        : const EdgeInsets.fromLTRB(10, 0, 10, 8);
    final inactiveTabColor =
        theme.textTheme.bodySmall?.color?.withValues(alpha: 0.84) ??
        theme.colorScheme.onSurface.withValues(alpha: 0.62);
    final activeTabColor = theme.colorScheme.primary.withValues(alpha: 0.96);
    final cupertinoItems = <BottomNavigationBarItem>[
      _buildCupertinoNavigationItem(
        icon: AppSemanticIcon.sessions,
        label: '会话',
        activeColor: activeTabColor,
        inactiveColor: inactiveTabColor,
      ),
      _buildCupertinoNavigationItem(
        icon: AppSemanticIcon.contacts,
        label: '联系人',
        activeColor: activeTabColor,
        inactiveColor: inactiveTabColor,
      ),
      _buildCupertinoNavigationItem(
        icon: AppSemanticIcon.settings,
        label: '设置',
        activeColor: activeTabColor,
        inactiveColor: inactiveTabColor,
      ),
    ];

    return AnnotatedRegion<SystemUiOverlayStyle>(
      value: SystemUiOverlayStyle.light.copyWith(
        statusBarColor: Colors.transparent,
        systemNavigationBarColor: material.isGlass
            ? Colors.transparent
            : navigationSurface,
        systemNavigationBarIconBrightness: Brightness.light,
      ),
      child: Scaffold(
        extendBody: material.isGlass,
        body: Stack(
          children: <Widget>[
            DecoratedBox(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  colors: shellGradientColors,
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                ),
              ),
              child: SizedBox.expand(
                child: material.isGlass && isCupertinoStyle
                    ? Stack(
                        fit: StackFit.expand,
                        children: <Widget>[
                          _ShellGlassEnvironment(
                            cupertinoStyle: isCupertinoStyle,
                          ),
                        ],
                      )
                    : const SizedBox.expand(),
              ),
            ),
            SafeArea(
              child: Padding(
                padding: outerPadding,
                child: switch (section) {
                  AppSection.chats =>
                    showingChatDetail
                        ? MobileChatDetailScreen(
                            conversation: activeConversation,
                            messages: activeMessages,
                            onBack: () => ref
                                .read(chatActionsProvider)
                                .selectConversation(null),
                          )
                        : const MobileChatsScreen(),
                  AppSection.contacts => const MobileContactsScreen(),
                  AppSection.settings => const SettingsSecurityScreen(),
                },
              ),
            ),
          ],
        ),
        bottomNavigationBar: showingChatDetail
            ? null
            : Padding(
                padding: navigationOuterPadding,
                child: isCupertinoStyle
                    ? _GlassNavigationFrame(
                        enabled: material.isGlass,
                        radius: 18,
                        backgroundColor: navigationSurface,
                        borderColor: material.chromeBorderColor,
                        blurSigma: material.blurSigma * 1.02,
                        child: Padding(
                          padding: const EdgeInsets.symmetric(vertical: 4),
                          child: CupertinoTabBar(
                            currentIndex: AppSection.values.indexOf(section),
                            backgroundColor: material.isGlass
                                ? Colors.transparent
                                : navigationSurface,
                            activeColor: activeTabColor,
                            inactiveColor: inactiveTabColor,
                            height: 68,
                            iconSize: 1,
                            onTap: (index) {
                              final next = AppSection.values[index];
                              context.go('/app/${next.routeSegment}');
                            },
                            items: cupertinoItems,
                          ),
                        ),
                      )
                    : _GlassNavigationFrame(
                        enabled: material.isGlass,
                        radius: 23,
                        backgroundColor: navigationSurface,
                        borderColor: material.chromeBorderColor.withValues(
                          alpha: material.isGlass ? 1 : 0.80,
                        ),
                        blurSigma: material.blurSigma * 0.86,
                        roundAllCorners: true,
                        child: Padding(
                          padding: EdgeInsets.symmetric(
                            vertical: material.isGlass ? 4 : 2,
                          ),
                          child: NavigationBar(
                            backgroundColor: material.isGlass
                                ? Colors.transparent
                                : navigationSurface,
                            height: material.isGlass
                                ? ImUiMetrics.androidBottomNavGlassHeight
                                : ImUiMetrics.androidBottomNavHeight,
                            selectedIndex: AppSection.values.indexOf(section),
                            onDestinationSelected: (index) {
                              final next = AppSection.values[index];
                              context.go('/app/${next.routeSegment}');
                            },
                            destinations: <NavigationDestination>[
                              _buildMaterialNavigationDestination(
                                icon: AppSemanticIcon.sessions,
                                label: '会话',
                              ),
                              _buildMaterialNavigationDestination(
                                icon: AppSemanticIcon.contacts,
                                label: '联系人',
                              ),
                              _buildMaterialNavigationDestination(
                                icon: AppSemanticIcon.settings,
                                label: '设置',
                              ),
                            ],
                          ),
                        ),
                      ),
              ),
      ),
    );
  }
}

BottomNavigationBarItem _buildCupertinoNavigationItem({
  required AppSemanticIcon icon,
  required String label,
  required Color activeColor,
  required Color inactiveColor,
}) {
  return BottomNavigationBarItem(
    label: null,
    tooltip: label,
    icon: _CupertinoTabGlyph(
      icon: icon,
      label: label,
      color: inactiveColor,
      selected: false,
    ),
    activeIcon: _CupertinoTabGlyph(
      icon: icon,
      label: label,
      color: activeColor,
      selected: true,
    ),
  );
}

NavigationDestination _buildMaterialNavigationDestination({
  required AppSemanticIcon icon,
  required String label,
}) {
  Widget buildGlyph(bool selected) {
    return SizedBox.square(
      dimension: 28,
      child: Center(child: AppIcon(icon, size: selected ? 21.5 : 20.5)),
    );
  }

  return NavigationDestination(
    icon: buildGlyph(false),
    selectedIcon: buildGlyph(true),
    label: label,
  );
}

class _ShellGlassEnvironment extends StatelessWidget {
  const _ShellGlassEnvironment({required this.cupertinoStyle});

  final bool cupertinoStyle;

  @override
  Widget build(BuildContext context) {
    return Stack(
      fit: StackFit.expand,
      children: <Widget>[
        Positioned(
          top: -8,
          left: cupertinoStyle ? 18 : 12,
          right: cupertinoStyle ? 18 : 12,
          child: _ShellEnvironmentField(
            key: const ValueKey('mobile-shell-environment-top'),
            height: cupertinoStyle ? 72 : 144,
            radius: cupertinoStyle ? 92 : 156,
            gradient: RadialGradient(
              center: const Alignment(0, -0.96),
              radius: cupertinoStyle ? 0.94 : 1.02,
              colors: <Color>[
                Colors.white.withValues(alpha: cupertinoStyle ? 0.014 : 0.034),
                AppPalette.silverBlue.withValues(
                  alpha: cupertinoStyle ? 0.009 : 0.018,
                ),
                Colors.transparent,
              ],
              stops: const <double>[0, 0.24, 1],
            ),
          ),
        ),
        if (cupertinoStyle)
          Positioned(
            top: 18,
            left: 58,
            right: 58,
            child: _ShellEnvironmentField(
              key: const ValueKey('mobile-shell-environment-top-lens'),
              height: 38,
              radius: 44,
              gradient: RadialGradient(
                center: const Alignment(0, -0.90),
                radius: 0.88,
                colors: <Color>[
                  Colors.white.withValues(alpha: 0.028),
                  AppPalette.silverBlue.withValues(alpha: 0.011),
                  AppPalette.mistBlue.withValues(alpha: 0.003),
                  Colors.transparent,
                ],
                stops: const <double>[0, 0.14, 0.28, 1],
              ),
            ),
          ),
        if (cupertinoStyle)
          Positioned(
            top: 44,
            right: 18,
            child: Transform.rotate(
              angle: 0.16,
              child: _ShellEnvironmentField(
                key: const ValueKey('mobile-shell-environment-side-lens'),
                height: 96,
                radius: 52,
                gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                  colors: <Color>[
                    Colors.white.withValues(alpha: 0.014),
                    AppPalette.silverBlue.withValues(alpha: 0.006),
                    Colors.transparent,
                  ],
                  stops: const <double>[0, 0.32, 1],
                ),
              ),
            ),
          ),
        Positioned(
          left: cupertinoStyle ? 42 : 0,
          right: cupertinoStyle ? 42 : 0,
          bottom: -2,
          child: _ShellEnvironmentField(
            key: const ValueKey('mobile-shell-environment-bottom'),
            height: cupertinoStyle ? 56 : 150,
            radius: cupertinoStyle ? 64 : 170,
            gradient: RadialGradient(
              center: const Alignment(0, -0.42),
              radius: cupertinoStyle ? 0.84 : 1.0,
              colors: <Color>[
                Colors.white.withValues(alpha: cupertinoStyle ? 0.01 : 0.026),
                AppPalette.mistBlue.withValues(
                  alpha: cupertinoStyle ? 0.005 : 0.014,
                ),
                Colors.transparent,
              ],
              stops: const <double>[0, 0.18, 1],
            ),
          ),
        ),
        if (cupertinoStyle)
          Positioned(
            left: 88,
            right: 88,
            bottom: 4,
            child: _ShellEnvironmentField(
              key: const ValueKey('mobile-shell-environment-bottom-dock'),
              height: 28,
              radius: 38,
              gradient: RadialGradient(
                center: const Alignment(0, -0.52),
                radius: 0.80,
                colors: <Color>[
                  Colors.white.withValues(alpha: 0.016),
                  AppPalette.silverBlue.withValues(alpha: 0.007),
                  AppPalette.mistBlue.withValues(alpha: 0.003),
                  Colors.transparent,
                ],
                stops: const <double>[0, 0.14, 0.28, 1],
              ),
            ),
          ),
        Positioned(
          left: 0,
          right: 0,
          bottom: 0,
          child: IgnorePointer(
            child: Container(
              height: cupertinoStyle ? 28 : 78,
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.bottomCenter,
                  end: Alignment.topCenter,
                  colors: <Color>[
                    Colors.black.withValues(
                      alpha: cupertinoStyle ? 0.002 : 0.06,
                    ),
                    Colors.transparent,
                  ],
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }
}

class _ShellEnvironmentField extends StatelessWidget {
  const _ShellEnvironmentField({
    super.key,
    required this.height,
    required this.radius,
    required this.gradient,
  });

  final double height;
  final double radius;
  final Gradient gradient;

  @override
  Widget build(BuildContext context) {
    return IgnorePointer(
      child: Container(
        height: height,
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(radius),
          gradient: gradient,
        ),
      ),
    );
  }
}

class _CupertinoTabGlyph extends StatelessWidget {
  const _CupertinoTabGlyph({
    required this.icon,
    required this.label,
    required this.color,
    required this.selected,
  });

  final AppSemanticIcon icon;
  final String label;
  final Color color;
  final bool selected;

  @override
  Widget build(BuildContext context) {
    final focusColor = selected
        ? Color.alphaBlend(
            Colors.white.withValues(alpha: 0.045),
            color.withValues(alpha: 0.06),
          )
        : Colors.transparent;
    final focusBorderColor = selected
        ? Colors.white.withValues(alpha: 0.05)
        : Colors.transparent;

    return SizedBox(
      key: ValueKey(
        'mobile-shell-cupertino-tab-glyph-$label-${selected ? 'selected' : 'idle'}',
      ),
      width: 84,
      height: ImUiMetrics.mobileBottomNavVisibleMax,
      child: Stack(
        alignment: Alignment.center,
        children: <Widget>[
          AnimatedContainer(
            duration: const Duration(milliseconds: 180),
            curve: Curves.easeOutCubic,
            width: 60,
            height: 48,
            decoration: BoxDecoration(
              color: focusColor,
              borderRadius: BorderRadius.circular(17),
              border: Border.all(color: focusBorderColor, width: 0.55),
              boxShadow: selected
                  ? <BoxShadow>[
                      BoxShadow(
                        color: color.withValues(alpha: 0.035),
                        blurRadius: 10,
                        spreadRadius: -7,
                        offset: const Offset(0, 2),
                      ),
                    ]
                  : const <BoxShadow>[],
            ),
          ),
          Column(
            mainAxisAlignment: MainAxisAlignment.center,
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              AppIcon(icon, size: selected ? 24.4 : 22.6, color: color),
              const SizedBox(height: 2.6),
              Text(
                label,
                maxLines: 1,
                overflow: TextOverflow.visible,
                strutStyle: const StrutStyle(height: 1, forceStrutHeight: true),
                textHeightBehavior: const TextHeightBehavior(
                  applyHeightToFirstAscent: false,
                  applyHeightToLastDescent: false,
                ),
                style: Theme.of(context).textTheme.labelSmall?.copyWith(
                  color: color,
                  fontSize: 13.4,
                  height: 1.0,
                  fontWeight: selected ? FontWeight.w700 : FontWeight.w500,
                  letterSpacing: 0,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _GlassNavigationFrame extends StatelessWidget {
  const _GlassNavigationFrame({
    required this.enabled,
    required this.radius,
    required this.backgroundColor,
    required this.borderColor,
    required this.blurSigma,
    required this.child,
    this.roundAllCorners = false,
  });

  final bool enabled;
  final double radius;
  final Color backgroundColor;
  final Color borderColor;
  final double blurSigma;
  final Widget child;
  final bool roundAllCorners;

  @override
  Widget build(BuildContext context) {
    final isCupertinoStyle = Theme.of(context).platform == TargetPlatform.iOS;
    final borderRadius = enabled || roundAllCorners
        ? BorderRadius.circular(radius)
        : BorderRadius.vertical(top: Radius.circular(radius));
    final frame = ClipRRect(
      borderRadius: borderRadius,
      child: LiquidGlassPanel(
        enabled: enabled,
        role: ShellChromeRole.bottomBar,
        radius: radius,
        backgroundColor: backgroundColor,
        borderColor: borderColor,
        blurSigma: blurSigma,
        shadowOpacity: 0.024,
        child: child,
      ),
    );

    if (!enabled || !isCupertinoStyle) {
      return frame;
    }

    return Stack(
      clipBehavior: Clip.none,
      alignment: Alignment.bottomCenter,
      children: <Widget>[
        Positioned(
          left: 14,
          right: 14,
          top: -6,
          child: IgnorePointer(
            child: Container(
              height: 30,
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(999),
                gradient: RadialGradient(
                  center: const Alignment(0, -0.9),
                  radius: 1.08,
                  colors: <Color>[
                    Colors.white.withValues(alpha: 0.032),
                    AppPalette.silverBlue.withValues(alpha: 0.014),
                    AppPalette.mistBlue.withValues(alpha: 0.006),
                    Colors.transparent,
                  ],
                  stops: const <double>[0, 0.22, 0.44, 1],
                ),
              ),
            ),
          ),
        ),
        Positioned(
          left: 18,
          right: 18,
          bottom: -4,
          child: IgnorePointer(
            child: Container(
              height: 18,
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(999),
                gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                  colors: <Color>[
                    Colors.white.withValues(alpha: 0.024),
                    AppPalette.silverBlue.withValues(alpha: 0.008),
                    Colors.transparent,
                  ],
                  stops: const <double>[0, 0.42, 1],
                ),
              ),
            ),
          ),
        ),
        frame,
      ],
    );
  }
}
