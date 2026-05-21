import 'dart:ui';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../bootstrap/app_providers.dart';
import '../theme/app_icons.dart';
import '../theme/app_theme.dart';
import '../theme/app_tokens.dart';

class LoginScreen extends ConsumerStatefulWidget {
  const LoginScreen({super.key});

  @override
  ConsumerState<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends ConsumerState<LoginScreen> {
  final TextEditingController _usernameController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();

  @override
  void dispose() {
    _usernameController.dispose();
    _passwordController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(sessionControllerProvider);
    final bootstrap = ref.watch(appBootstrapProvider);
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final screenSize = MediaQuery.sizeOf(context);
    final isWide = screenSize.width >= 760;
    final isCompactMobile = screenSize.height <= 920;
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final isAndroidLogin = !isWide && !isCupertinoStyle;
    final canSubmit =
        !session.isBusy && bootstrap.hasValue && !bootstrap.isLoading;
    final contentMaxWidth = isWide ? 446.0 : (isCupertinoStyle ? 324.0 : 356.0);
    final entryMaxWidth = isWide ? 320.0 : (isCupertinoStyle ? 274.0 : 296.0);
    final panelPadding = EdgeInsets.fromLTRB(
      isWide ? 28 : (isCupertinoStyle ? 16 : 18),
      isWide ? 18 : (isCupertinoStyle ? 10 : 12),
      isWide ? 28 : (isCupertinoStyle ? 16 : 18),
      isWide ? 18 : (isCupertinoStyle ? 14 : 16),
    );

    return Scaffold(
      body: Stack(
        children: <Widget>[
          Positioned.fill(
            child: _LoginBackdrop(
              enhancedGlass: material.isGlass,
              isCupertinoStyle: isCupertinoStyle,
              isWide: isWide,
            ),
          ),
          SafeArea(
            child: Align(
              alignment: isWide
                  ? const Alignment(0, -0.02)
                  : (isCupertinoStyle
                        ? const Alignment(0, -0.08)
                        : const Alignment(0, -0.06)),
              child: SingleChildScrollView(
                padding: EdgeInsets.fromLTRB(
                  isWide ? 28 : 20,
                  isWide
                      ? 22
                      : (isCupertinoStyle ? 18 : (isCompactMobile ? 24 : 28)),
                  isWide ? 28 : 20,
                  isWide ? 18 : (isCupertinoStyle ? 18 : 18),
                ),
                child: ConstrainedBox(
                  constraints: BoxConstraints(maxWidth: contentMaxWidth),
                  child: _LoginGlassPanel(
                    cupertinoStyle: isCupertinoStyle,
                    emphasizeDesktop: isWide,
                    padding: panelPadding,
                    child: Center(
                      child: ConstrainedBox(
                        constraints: BoxConstraints(maxWidth: entryMaxWidth),
                        child: Column(
                          key: const ValueKey('login-entry-stack'),
                          crossAxisAlignment: CrossAxisAlignment.stretch,
                          mainAxisSize: MainAxisSize.min,
                          children: <Widget>[
                            ValueListenableBuilder<TextEditingValue>(
                              valueListenable: _usernameController,
                              builder: (context, value, _) {
                                final accountLabel = value.text.trim();
                                return _LoginIdentityStatusCard(
                                  key: const ValueKey(
                                    'login-identity-status-card',
                                  ),
                                  accountLabel: accountLabel,
                                  emphasizeDesktop: isWide,
                                  cupertinoStyle: isCupertinoStyle,
                                  androidLayout: isAndroidLogin,
                                  bootstrap: bootstrap,
                                  onRetry: () =>
                                      ref.invalidate(appBootstrapProvider),
                                );
                              },
                            ),
                            SizedBox(
                              height: isWide
                                  ? 14
                                  : isCupertinoStyle
                                  ? 12
                                  : 11,
                            ),
                            Column(
                              key: const ValueKey('login-form-column'),
                              crossAxisAlignment: CrossAxisAlignment.stretch,
                              children: <Widget>[
                                const _LoginModeTabs(),
                                const SizedBox(height: 9),
                                _LoginQrPreview(
                                  key: const ValueKey('login-qr-preview'),
                                  cupertinoStyle: isCupertinoStyle,
                                  emphasizeDesktop: isWide,
                                ),
                                const SizedBox(height: 9),
                                _LoginFormSurface(
                                  key: const ValueKey('login-form-surface'),
                                  cupertinoStyle: isCupertinoStyle,
                                  emphasizeDesktop: isWide,
                                  child: _LoginCredentialFields(
                                    usernameController: _usernameController,
                                    passwordController: _passwordController,
                                    canSubmit: canSubmit,
                                    cupertinoStyle: isCupertinoStyle,
                                    onSubmit: onSubmit,
                                  ),
                                ),
                                if (bootstrap.isLoading) ...<Widget>[
                                  const SizedBox(height: 8),
                                  const LinearProgressIndicator(minHeight: 3),
                                ],
                                SizedBox(
                                  height: isWide
                                      ? 12
                                      : isCupertinoStyle
                                      ? 11
                                      : 10,
                                ),
                                if (session.errorMessage
                                    case final message?) ...<Widget>[
                                  Padding(
                                    padding: const EdgeInsets.only(bottom: 8),
                                    child: Text(
                                      message,
                                      textAlign: TextAlign.center,
                                      style: theme.textTheme.bodySmall
                                          ?.copyWith(
                                            color: theme.colorScheme.error,
                                          ),
                                    ),
                                  ),
                                ],
                                SizedBox(
                                  width: double.infinity,
                                  child: _LoginPrimaryButton(
                                    buttonKey: const ValueKey(
                                      'login-continue-button',
                                    ),
                                    onPressed: canSubmit
                                        ? () => onSubmit()
                                        : null,
                                    label: Text(
                                      bootstrap.isLoading
                                          ? '准备中'
                                          : session.isBusy
                                          ? '登录中'
                                          : '登录',
                                    ),
                                    cupertinoStyle: isCupertinoStyle,
                                    emphasizeDesktop: isWide,
                                  ),
                                ),
                                const SizedBox(height: 10),
                                const _LoginHelpLinks(),
                              ],
                            ),
                          ],
                        ),
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ),
          if (session.isBusy)
            Container(
              color: Colors.black.withValues(alpha: 0.12),
              alignment: Alignment.center,
              child: const CircularProgressIndicator(),
            ),
        ],
      ),
    );
  }

  Future<void> onSubmit() async {
    final ok = await ref
        .read(sessionControllerProvider.notifier)
        .signIn(
          username: _usernameController.text,
          password: _passwordController.text,
        );
    if (ok && mounted) {
      context.go('/app/chats');
    }
  }
}

class _LoginBackdrop extends StatelessWidget {
  const _LoginBackdrop({
    required this.enhancedGlass,
    required this.isCupertinoStyle,
    required this.isWide,
  });

  final bool enhancedGlass;
  final bool isCupertinoStyle;
  final bool isWide;

  @override
  Widget build(BuildContext context) {
    final stageWidth = isWide ? 820.0 : (isCupertinoStyle ? 312.0 : 392.0);
    final stageHeight = isWide ? 520.0 : (isCupertinoStyle ? 236.0 : 270.0);
    return DecoratedBox(
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: enhancedGlass
              ? isCupertinoStyle
                    ? const <Color>[
                        Color(0xFF0B121A),
                        Color(0xFF0F1821),
                        Color(0xFF16212B),
                      ]
                    : const <Color>[
                        Color(0xFF0A1219),
                        Color(0xFF101A24),
                        Color(0xFF17222D),
                      ]
              : const <Color>[
                  Color(0xFF0A1118),
                  Color(0xFF111B25),
                  Color(0xFF172433),
                ],
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
        ),
      ),
      child: Stack(
        children: <Widget>[
          Align(
            alignment: isWide
                ? const Alignment(0, -0.01)
                : (isCupertinoStyle
                      ? const Alignment(0, -0.08)
                      : const Alignment(0, -0.06)),
            child: IgnorePointer(
              child: Container(
                key: const ValueKey('login-stage-glow'),
                width: stageWidth,
                height: stageHeight,
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(isWide ? 240 : 142),
                  gradient: RadialGradient(
                    center: isWide
                        ? const Alignment(0, -0.05)
                        : (isCupertinoStyle
                              ? const Alignment(0, -0.22)
                              : const Alignment(0, -0.16)),
                    radius: isWide ? 1.14 : 1.04,
                    colors: <Color>[
                      AppPalette.mistBlue.withValues(
                        alpha: isWide
                            ? (enhancedGlass ? 0.042 : 0.032)
                            : isCupertinoStyle
                            ? (enhancedGlass ? 0.060 : 0.022)
                            : (enhancedGlass ? 0.072 : 0.03),
                      ),
                      Colors.white.withValues(
                        alpha: isWide
                            ? (enhancedGlass ? 0.016 : 0.012)
                            : isCupertinoStyle
                            ? (enhancedGlass ? 0.014 : 0.004)
                            : (enhancedGlass ? 0.018 : 0.007),
                      ),
                      Colors.transparent,
                    ],
                    stops: const <double>[0, 0.44, 1],
                  ),
                ),
              ),
            ),
          ),
          Positioned.fill(
            child: IgnorePointer(
              child: DecoratedBox(
                decoration: BoxDecoration(
                  gradient: RadialGradient(
                    center: isWide
                        ? const Alignment(0, -0.72)
                        : const Alignment(0, -0.86),
                    radius: isWide ? 1.22 : 1.08,
                    colors: <Color>[
                      Colors.white.withValues(
                        alpha: enhancedGlass ? 0.024 : 0.014,
                      ),
                      AppPalette.mistBlue.withValues(
                        alpha: enhancedGlass ? 0.018 : 0.010,
                      ),
                      Colors.transparent,
                    ],
                    stops: const <double>[0, 0.42, 1],
                  ),
                ),
              ),
            ),
          ),
          Positioned.fill(
            child: IgnorePointer(
              child: DecoratedBox(
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                    colors: <Color>[
                      Colors.white.withValues(
                        alpha: enhancedGlass ? 0.012 : 0.008,
                      ),
                      Colors.transparent,
                      Colors.black.withValues(
                        alpha: enhancedGlass ? 0.048 : 0.042,
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _LoginGlassPanel extends StatelessWidget {
  const _LoginGlassPanel({
    required this.child,
    required this.padding,
    required this.cupertinoStyle,
    required this.emphasizeDesktop,
  });

  final Widget child;
  final EdgeInsetsGeometry padding;
  final bool cupertinoStyle;
  final bool emphasizeDesktop;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final isAndroidShell = !emphasizeDesktop && !cupertinoStyle;
    final isIosShell = cupertinoStyle && !emphasizeDesktop;
    final mobileGlassShell = material.isGlass && (isIosShell || isAndroidShell);
    final shellRadius = emphasizeDesktop
        ? 32.0
        : isIosShell
        ? 22.0
        : isAndroidShell
        ? 24.0
        : 24.0;
    final shellDecoration = emphasizeDesktop
        ? BoxDecoration(
            color: Color.alphaBlend(
              Colors.white.withValues(alpha: material.isGlass ? 0.022 : 0.015),
              theme.colorScheme.surface.withValues(alpha: 0.18),
            ),
            borderRadius: BorderRadius.circular(shellRadius),
            border: Border.all(
              color: Colors.white.withValues(
                alpha: material.isGlass ? 0.052 : 0.04,
              ),
            ),
            gradient: LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment.bottomCenter,
              colors: <Color>[
                Colors.white.withValues(alpha: material.isGlass ? 0.016 : 0.01),
                Colors.white.withValues(
                  alpha: material.isGlass ? 0.004 : 0.002,
                ),
              ],
            ),
            boxShadow: <BoxShadow>[
              BoxShadow(
                color: Colors.black.withValues(alpha: 0.022),
                blurRadius: 28,
                offset: const Offset(0, 12),
              ),
            ],
          )
        : isIosShell
        ? BoxDecoration(
            color: Color.alphaBlend(
              Colors.white.withValues(alpha: material.isGlass ? 0.024 : 0.006),
              theme.colorScheme.surface.withValues(alpha: 0.104),
            ),
            borderRadius: BorderRadius.circular(shellRadius),
            border: Border.all(
              color: Colors.white.withValues(
                alpha: material.isGlass ? 0.044 : 0.016,
              ),
            ),
            gradient: LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment.bottomCenter,
              colors: <Color>[
                Colors.white.withValues(
                  alpha: material.isGlass ? 0.030 : 0.0045,
                ),
                Colors.transparent,
              ],
            ),
            boxShadow: <BoxShadow>[
              BoxShadow(
                color: Colors.black.withValues(
                  alpha: material.isGlass ? 0.012 : 0.005,
                ),
                blurRadius: material.isGlass ? 18 : 8,
                offset: const Offset(0, 6),
              ),
            ],
          )
        : isAndroidShell
        ? BoxDecoration(
            color: Color.alphaBlend(
              Colors.white.withValues(alpha: material.isGlass ? 0.020 : 0.004),
              theme.colorScheme.surface.withValues(alpha: 0.108),
            ),
            borderRadius: BorderRadius.circular(shellRadius),
            border: Border.all(
              color: Colors.white.withValues(
                alpha: material.isGlass ? 0.040 : 0.016,
              ),
            ),
            gradient: LinearGradient(
              begin: Alignment.topCenter,
              end: Alignment.bottomCenter,
              colors: <Color>[
                Colors.white.withValues(
                  alpha: material.isGlass ? 0.028 : 0.005,
                ),
                Colors.transparent,
              ],
            ),
            boxShadow: <BoxShadow>[
              BoxShadow(
                color: Colors.black.withValues(
                  alpha: material.isGlass ? 0.014 : 0.009,
                ),
                blurRadius: material.isGlass ? 18 : 8,
                offset: const Offset(0, 6),
              ),
            ],
          )
        : null;

    final shell = AnimatedContainer(
      key: const ValueKey('login-panel-shell'),
      duration: const Duration(milliseconds: 180),
      alignment: emphasizeDesktop
          ? Alignment.center
          : isAndroidShell
          ? Alignment.center
          : Alignment.topCenter,
      constraints: BoxConstraints(
        minHeight: emphasizeDesktop
            ? 320
            : (isAndroidShell
                  ? 270
                  : isIosShell
                  ? 236
                  : 0),
      ),
      padding: padding,
      decoration: shellDecoration,
      child: Transform.translate(offset: Offset.zero, child: child),
    );

    if (!mobileGlassShell) {
      return shell;
    }

    return ClipRRect(
      borderRadius: BorderRadius.circular(shellRadius),
      child: BackdropFilter(
        filter: ImageFilter.blur(
          sigmaX: isIosShell ? 20 : 16,
          sigmaY: isIosShell ? 20 : 16,
        ),
        child: shell,
      ),
    );
  }
}

class _LoginTopBar extends StatelessWidget {
  const _LoginTopBar({
    required this.cupertinoStyle,
    required this.emphasizeDesktop,
    required this.accountLabel,
  });

  final bool cupertinoStyle;
  final bool emphasizeDesktop;
  final String accountLabel;

  @override
  Widget build(BuildContext context) {
    final size = emphasizeDesktop ? 44.0 : (cupertinoStyle ? 43.0 : 44.0);
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final hasAccount = accountLabel.isNotEmpty;
    final mobileGlassBadge = material.isGlass && !emphasizeDesktop;
    final badge = AnimatedContainer(
      duration: const Duration(milliseconds: 180),
      key: const ValueKey('login-identity-header'),
      width: size,
      height: size,
      decoration: BoxDecoration(
        shape: BoxShape.circle,
        gradient: mobileGlassBadge
            ? LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: <Color>[
                  Colors.white.withValues(alpha: cupertinoStyle ? 0.08 : 0.065),
                  Colors.white.withValues(alpha: cupertinoStyle ? 0.024 : 0.02),
                ],
              )
            : LinearGradient(
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
                colors: <Color>[
                  theme.colorScheme.primary.withValues(
                    alpha: cupertinoStyle ? 0.26 : 0.24,
                  ),
                  theme.colorScheme.primary.withValues(
                    alpha: cupertinoStyle ? 0.08 : 0.06,
                  ),
                ],
              ),
        border: Border.all(
          color: mobileGlassBadge
              ? Colors.white.withValues(alpha: cupertinoStyle ? 0.11 : 0.085)
              : material.isGlass
              ? Colors.white.withValues(alpha: cupertinoStyle ? 0.04 : 0.032)
              : theme.colorScheme.outline.withValues(alpha: 0.08),
        ),
        boxShadow: <BoxShadow>[
          BoxShadow(
            color: Colors.black.withValues(
              alpha: mobileGlassBadge
                  ? 0.016
                  : material.isGlass
                  ? 0.009
                  : 0.008,
            ),
            blurRadius: mobileGlassBadge ? 12 : (material.isGlass ? 6 : 5),
            offset: Offset(0, mobileGlassBadge ? 4 : 2),
          ),
        ],
      ),
      child: Center(
        child: hasAccount
            ? Text(
                accountLabel.characters.first.toUpperCase(),
                style: theme.textTheme.titleMedium?.copyWith(
                  color: Colors.white.withValues(
                    alpha: mobileGlassBadge ? 0.94 : 1,
                  ),
                  fontWeight: FontWeight.w600,
                  fontSize: emphasizeDesktop
                      ? 18.0
                      : (cupertinoStyle ? 16.2 : 16.8),
                ),
              )
            : AppIcon(
                AppSemanticIcon.sessions,
                size: emphasizeDesktop ? 18.0 : (cupertinoStyle ? 18.6 : 19.2),
                color: Colors.white.withValues(
                  alpha: mobileGlassBadge ? 0.92 : 1,
                ),
              ),
      ),
    );

    if (!mobileGlassBadge) {
      return badge;
    }

    return ClipOval(
      child: BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 10, sigmaY: 10),
        child: badge,
      ),
    );
  }
}

class _LoginIdentityStatusCard extends StatelessWidget {
  const _LoginIdentityStatusCard({
    super.key,
    required this.accountLabel,
    required this.emphasizeDesktop,
    required this.cupertinoStyle,
    required this.androidLayout,
    required this.bootstrap,
    required this.onRetry,
  });

  final String accountLabel;
  final bool emphasizeDesktop;
  final bool cupertinoStyle;
  final bool androidLayout;
  final AsyncValue<void> bootstrap;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final statusTint = bootstrap.hasError
        ? theme.colorScheme.error
        : bootstrap.isLoading
        ? AppPalette.warning
        : theme.colorScheme.primary;
    final statusIcon = bootstrap.hasError
        ? AppSemanticIcon.alert
        : bootstrap.isLoading
        ? AppSemanticIcon.sync
        : AppSemanticIcon.verified;
    final statusLabel = bootstrap.isLoading
        ? '同步中'
        : bootstrap.hasError
        ? '重试'
        : '已同步';
    final showStatus =
        bootstrap.isLoading ||
        bootstrap.hasError ||
        (emphasizeDesktop && accountLabel.isNotEmpty);
    final zoneMinHeight = emphasizeDesktop
        ? 0.0
        : (cupertinoStyle ? 54.0 : 60.0);
    final titleStyle = theme.textTheme.titleMedium?.copyWith(
      color: Colors.white,
      fontSize: cupertinoStyle ? 16.2 : (emphasizeDesktop ? 19.0 : 15.6),
      height: 1.0,
      fontWeight: FontWeight.w600,
      letterSpacing: cupertinoStyle ? -0.16 : -0.12,
    );
    return ConstrainedBox(
      key: const ValueKey('login-identity-zone'),
      constraints: BoxConstraints(minHeight: zoneMinHeight),
      child: Column(
        key: const ValueKey('login-hero-stack'),
        mainAxisAlignment: MainAxisAlignment.center,
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          _LoginTopBar(
            cupertinoStyle: cupertinoStyle,
            emphasizeDesktop: emphasizeDesktop,
            accountLabel: accountLabel,
          ),
          SizedBox(height: cupertinoStyle ? 5.5 : (emphasizeDesktop ? 9 : 6)),
          Text(
            'MI Chat',
            key: const ValueKey('login-entry-title'),
            textAlign: TextAlign.center,
            style: titleStyle,
          ),
          const SizedBox(height: 5),
          Text(
            '消息、群聊与文件同步',
            key: const ValueKey('login-entry-subtitle'),
            textAlign: TextAlign.center,
            style: theme.textTheme.bodySmall?.copyWith(
              color: Colors.white.withValues(alpha: 0.72),
              fontSize: cupertinoStyle ? 11.4 : 12.0,
              height: 1.0,
            ),
          ),
          if (showStatus) ...<Widget>[
            const SizedBox(height: 5),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              mainAxisSize: MainAxisSize.min,
              children: <Widget>[
                _LoginStatusPill(
                  label: statusLabel,
                  tint: statusTint,
                  icon: statusIcon,
                  cupertinoStyle: cupertinoStyle,
                ),
                if (bootstrap.hasError) ...<Widget>[
                  const SizedBox(width: 2),
                  _LoginHeroActionButton(
                    cupertinoStyle: cupertinoStyle,
                    icon: AppSemanticIcon.refresh,
                    color: theme.colorScheme.error,
                    onPressed: onRetry,
                  ),
                ],
              ],
            ),
          ],
        ],
      ),
    );
  }
}

class _LoginHeroActionButton extends StatelessWidget {
  const _LoginHeroActionButton({
    required this.cupertinoStyle,
    required this.icon,
    required this.color,
    required this.onPressed,
  });

  final bool cupertinoStyle;
  final AppSemanticIcon icon;
  final Color color;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final widget = AppIcon(icon, size: cupertinoStyle ? 15 : 16, color: color);
    if (cupertinoStyle) {
      return CupertinoButton(
        padding: EdgeInsets.zero,
        minimumSize: const Size(30, 30),
        onPressed: onPressed,
        child: widget,
      );
    }

    return IconButton(
      onPressed: onPressed,
      icon: widget,
      splashRadius: 18,
      padding: EdgeInsets.zero,
      constraints: const BoxConstraints.tightFor(width: 30, height: 30),
      visualDensity: VisualDensity.compact,
    );
  }
}

class _LoginPrimaryButton extends StatelessWidget {
  const _LoginPrimaryButton({
    required this.label,
    required this.onPressed,
    required this.cupertinoStyle,
    required this.emphasizeDesktop,
    this.buttonKey,
  });

  final Widget label;
  final VoidCallback? onPressed;
  final bool cupertinoStyle;
  final bool emphasizeDesktop;
  final Key? buttonKey;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final backgroundColor = Color.alphaBlend(
      Colors.white.withValues(alpha: cupertinoStyle ? 0.01 : 0.008),
      theme.colorScheme.primary.withValues(
        alpha: cupertinoStyle ? 0.72 : (emphasizeDesktop ? 0.76 : 0.82),
      ),
    );
    final useMobileGlassButton = material.isGlass && !emphasizeDesktop;

    if (useMobileGlassButton) {
      final radius = BorderRadius.circular(cupertinoStyle ? 14 : 15);
      final buttonSurface = BoxDecoration(
        borderRadius: radius,
        border: Border.all(
          color: Colors.white.withValues(alpha: cupertinoStyle ? 0.1 : 0.08),
        ),
        gradient: LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: <Color>[
            Color.alphaBlend(
              Colors.white.withValues(alpha: cupertinoStyle ? 0.08 : 0.06),
              theme.colorScheme.primary.withValues(alpha: 0.9),
            ),
            Color.alphaBlend(
              Colors.white.withValues(alpha: 0.012),
              theme.colorScheme.primary.withValues(alpha: 0.8),
            ),
          ],
        ),
        boxShadow: <BoxShadow>[
          BoxShadow(
            color: theme.colorScheme.primary.withValues(alpha: 0.12),
            blurRadius: 16,
            offset: const Offset(0, 6),
          ),
        ],
      );
      final buttonChild = DefaultTextStyle.merge(
        style:
            theme.textTheme.titleMedium?.copyWith(
              color: Colors.white,
              fontWeight: FontWeight.w600,
              fontSize: 15.4,
            ) ??
            const TextStyle(
              color: Colors.white,
              fontWeight: FontWeight.w600,
              fontSize: 15.4,
            ),
        child: label,
      );

      if (cupertinoStyle) {
        return Opacity(
          opacity: onPressed == null ? 0.54 : 1,
          child: DecoratedBox(
            decoration: buttonSurface,
            child: CupertinoButton(
              key: buttonKey,
              padding: EdgeInsets.zero,
              minimumSize: const Size.fromHeight(48),
              borderRadius: radius,
              color: Colors.transparent,
              disabledColor: Colors.transparent,
              onPressed: onPressed,
              child: buttonChild,
            ),
          ),
        );
      }

      return Opacity(
        opacity: onPressed == null ? 0.54 : 1,
        child: DecoratedBox(
          decoration: buttonSurface,
          child: Material(
            color: Colors.transparent,
            child: InkWell(
              key: buttonKey,
              borderRadius: radius,
              onTap: onPressed,
              child: SizedBox(height: 48, child: Center(child: buttonChild)),
            ),
          ),
        ),
      );
    }

    if (cupertinoStyle) {
      return CupertinoButton(
        key: buttonKey,
        padding: EdgeInsets.zero,
        minimumSize: const Size.fromHeight(48),
        borderRadius: BorderRadius.circular(12),
        color: backgroundColor,
        disabledColor: backgroundColor.withValues(alpha: 0.42),
        onPressed: onPressed,
        child: DefaultTextStyle.merge(
          style:
              theme.textTheme.titleMedium?.copyWith(
                color: Colors.white,
                fontWeight: FontWeight.w600,
                fontSize: 15.6,
              ) ??
              const TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.w600,
                fontSize: 15.6,
              ),
          child: label,
        ),
      );
    }

    return FilledButton(
      key: buttonKey,
      onPressed: onPressed,
      style: FilledButton.styleFrom(
        backgroundColor: backgroundColor,
        disabledBackgroundColor: backgroundColor.withValues(alpha: 0.42),
        minimumSize: Size.fromHeight(emphasizeDesktop ? 48 : 48),
        fixedSize: emphasizeDesktop ? const Size.fromHeight(48) : null,
        padding: const EdgeInsets.symmetric(horizontal: 16),
        tapTargetSize: emphasizeDesktop
            ? MaterialTapTargetSize.shrinkWrap
            : MaterialTapTargetSize.padded,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(emphasizeDesktop ? 13 : 14),
        ),
        textStyle: theme.textTheme.titleMedium?.copyWith(
          color: Colors.white,
          fontWeight: FontWeight.w600,
          fontSize: emphasizeDesktop ? 15.1 : 15.2,
        ),
      ),
      child: label,
    );
  }
}

class _LoginStatusPill extends StatelessWidget {
  const _LoginStatusPill({
    required this.label,
    required this.tint,
    required this.icon,
    this.cupertinoStyle = false,
  });

  final String label;
  final Color tint;
  final AppSemanticIcon icon;
  final bool cupertinoStyle;

  @override
  Widget build(BuildContext context) {
    final material = Theme.of(context).extension<AppShellMaterial>()!;
    return Container(
      padding: EdgeInsets.symmetric(
        horizontal: cupertinoStyle ? 6.5 : 7.5,
        vertical: cupertinoStyle ? 3.5 : 4.0,
      ),
      decoration: BoxDecoration(
        color: tint.withValues(alpha: material.isGlass ? 0.08 : 0.09),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(
          color: material.isGlass
              ? Colors.white.withValues(alpha: 0.04)
              : tint.withValues(alpha: 0.14),
        ),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          AppIcon(icon, size: cupertinoStyle ? 10.5 : 11.5, color: tint),
          const SizedBox(width: 4),
          Text(
            label,
            style: Theme.of(context).textTheme.labelSmall?.copyWith(
              color: tint,
              fontWeight: FontWeight.w600,
              fontSize: cupertinoStyle ? 10 : 10.2,
            ),
          ),
        ],
      ),
    );
  }
}

class _LoginModeTabs extends StatelessWidget {
  const _LoginModeTabs();

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Container(
      key: const ValueKey('login-mode-tabs'),
      height: 38,
      padding: const EdgeInsets.all(3),
      decoration: BoxDecoration(
        color: palette.surfaceVariant.withValues(alpha: 0.18),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: Colors.white.withValues(alpha: 0.05)),
      ),
      child: Row(
        children: <Widget>[
          Expanded(child: _LoginModeTab(label: '扫码登录', selected: false)),
          Expanded(child: _LoginModeTab(label: '账号登录', selected: true)),
        ],
      ),
    );
  }
}

class _LoginModeTab extends StatelessWidget {
  const _LoginModeTab({required this.label, required this.selected});

  final String label;
  final bool selected;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Container(
      alignment: Alignment.center,
      decoration: BoxDecoration(
        color: selected
            ? Colors.white.withValues(alpha: 0.13)
            : Colors.transparent,
        borderRadius: BorderRadius.circular(11),
      ),
      child: Text(
        label,
        style: theme.textTheme.labelLarge?.copyWith(
          color: Colors.white.withValues(alpha: selected ? 0.92 : 0.62),
          fontWeight: selected ? FontWeight.w800 : FontWeight.w600,
        ),
      ),
    );
  }
}

class _LoginHelpLinks extends StatelessWidget {
  const _LoginHelpLinks();

  @override
  Widget build(BuildContext context) {
    final style = Theme.of(context).textTheme.labelMedium?.copyWith(
      color: Colors.white.withValues(alpha: 0.66),
      fontWeight: FontWeight.w600,
    );
    return Wrap(
      key: const ValueKey('login-help-links'),
      alignment: WrapAlignment.center,
      spacing: 12,
      runSpacing: 6,
      children: <Widget>[
        Text('注册账号', style: style),
        Text('忘记密码', style: style),
        Text('代理设置', style: style),
      ],
    );
  }
}

class _LoginQrPreview extends StatelessWidget {
  const _LoginQrPreview({
    super.key,
    required this.cupertinoStyle,
    required this.emphasizeDesktop,
  });

  final bool cupertinoStyle;
  final bool emphasizeDesktop;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final height = emphasizeDesktop ? 72.0 : (cupertinoStyle ? 64.0 : 68.0);
    final qrSize = emphasizeDesktop ? 52.0 : (cupertinoStyle ? 48.0 : 50.0);
    final surface = Color.alphaBlend(
      Colors.white.withValues(alpha: emphasizeDesktop ? 0.018 : 0.014),
      palette.surfaceVariant.withValues(alpha: emphasizeDesktop ? 0.16 : 0.18),
    );
    return Container(
      height: height,
      padding: EdgeInsets.fromLTRB(
        emphasizeDesktop ? 8 : 7,
        emphasizeDesktop ? 8 : 7,
        emphasizeDesktop ? 10 : 9,
        emphasizeDesktop ? 8 : 7,
      ),
      decoration: BoxDecoration(
        color: surface,
        borderRadius: BorderRadius.circular(emphasizeDesktop ? 16 : 15),
        border: Border.all(color: Colors.white.withValues(alpha: 0.045)),
      ),
      child: Row(
        children: <Widget>[
          Container(
            key: const ValueKey('login-qr-code-surface'),
            width: qrSize,
            height: qrSize,
            decoration: BoxDecoration(
              color: Colors.white.withValues(alpha: 0.92),
              borderRadius: BorderRadius.circular(10),
            ),
            padding: const EdgeInsets.all(5),
            child: CustomPaint(painter: const _LoginQrPainter()),
          ),
          SizedBox(width: emphasizeDesktop ? 10 : 9),
          Expanded(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Row(
                  children: <Widget>[
                    Expanded(
                      child: Text(
                        '扫码登录',
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: theme.textTheme.labelLarge?.copyWith(
                          color: Colors.white.withValues(alpha: 0.90),
                          fontWeight: FontWeight.w800,
                          fontSize: emphasizeDesktop ? 12.4 : 12.2,
                          height: 1.0,
                        ),
                      ),
                    ),
                    Container(
                      key: const ValueKey('login-qr-refresh-chip'),
                      padding: const EdgeInsets.symmetric(
                        horizontal: 5,
                        vertical: 2,
                      ),
                      decoration: BoxDecoration(
                        color: theme.colorScheme.primary.withValues(
                          alpha: 0.12,
                        ),
                        borderRadius: BorderRadius.circular(999),
                      ),
                      child: Text(
                        '刷新',
                        style: theme.textTheme.labelSmall?.copyWith(
                          color: theme.colorScheme.primary,
                          fontSize: 9.0,
                          height: 1.0,
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 5),
                Text(
                  '手机确认后同步消息与文件',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: Colors.white.withValues(alpha: 0.62),
                    fontSize: emphasizeDesktop ? 10.8 : 10.4,
                    height: 1.0,
                  ),
                ),
                const SizedBox(height: 6),
                Row(
                  children: <Widget>[
                    Container(
                      width: 6,
                      height: 6,
                      decoration: BoxDecoration(
                        color: theme.colorScheme.primary,
                        shape: BoxShape.circle,
                      ),
                    ),
                    const SizedBox(width: 5),
                    Expanded(
                      child: Text(
                        '等待设备确认',
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: theme.textTheme.labelSmall?.copyWith(
                          color: Colors.white.withValues(alpha: 0.58),
                          fontSize: 9.8,
                          height: 1.0,
                        ),
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _LoginQrPainter extends CustomPainter {
  const _LoginQrPainter();

  @override
  void paint(Canvas canvas, Size size) {
    final dark = Paint()..color = const Color(0xFF0E1A24);
    final blue = Paint()..color = AppPalette.brandBlue;
    final cell = size.width / 9;

    void square(int x, int y, int span, Paint paint) {
      final rect = Rect.fromLTWH(x * cell, y * cell, span * cell, span * cell);
      canvas.drawRRect(
        RRect.fromRectAndRadius(rect, Radius.circular(cell * 0.32)),
        paint,
      );
    }

    for (final origin in const <Offset>[
      Offset(0, 0),
      Offset(6, 0),
      Offset(0, 6),
    ]) {
      square(origin.dx.toInt(), origin.dy.toInt(), 3, dark);
      square(
        origin.dx.toInt() + 1,
        origin.dy.toInt() + 1,
        1,
        Paint()..color = Colors.white,
      );
    }

    const dots = <Offset>[
      Offset(4, 0),
      Offset(5, 1),
      Offset(4, 2),
      Offset(3, 3),
      Offset(5, 3),
      Offset(7, 4),
      Offset(1, 4),
      Offset(3, 5),
      Offset(5, 5),
      Offset(8, 6),
      Offset(4, 7),
      Offset(6, 8),
    ];
    for (final dot in dots) {
      final x = dot.dx.toInt();
      square(x, dot.dy.toInt(), 1, x.isEven ? blue : dark);
    }
  }

  @override
  bool shouldRepaint(covariant _LoginQrPainter oldDelegate) => false;
}

class _LoginFormSurface extends StatelessWidget {
  const _LoginFormSurface({
    super.key,
    required this.child,
    required this.cupertinoStyle,
    required this.emphasizeDesktop,
  });

  final Widget child;
  final bool cupertinoStyle;
  final bool emphasizeDesktop;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final palette = theme.extension<AppShellColors>()!;
    final isDesktopIntegrated = emphasizeDesktop && !cupertinoStyle;
    final isMobileMaterial = !emphasizeDesktop && !cupertinoStyle;

    final backgroundColor = cupertinoStyle
        ? Color.alphaBlend(
            Colors.white.withValues(alpha: material.isGlass ? 0.02 : 0.008),
            palette.surfaceVariant.withValues(alpha: 0.34),
          )
        : isDesktopIntegrated
        ? Color.alphaBlend(
            Colors.white.withValues(alpha: material.isGlass ? 0.012 : 0.009),
            palette.surfaceVariant.withValues(alpha: 0.22),
          )
        : isMobileMaterial
        ? Colors.transparent
        : Color.alphaBlend(
            Colors.white.withValues(alpha: material.isGlass ? 0.014 : 0.006),
            palette.surfaceVariant.withValues(alpha: 0.42),
          );
    final borderColor = cupertinoStyle
        ? Colors.white.withValues(alpha: material.isGlass ? 0.06 : 0.038)
        : isMobileMaterial
        ? Colors.transparent
        : Colors.white.withValues(
            alpha: isDesktopIntegrated
                ? 0.05
                : (material.isGlass ? 0.042 : 0.032),
          );
    final radius = BorderRadius.circular(
      cupertinoStyle ? 14 : (emphasizeDesktop ? 18 : 20),
    );
    final surface = AnimatedContainer(
      key: cupertinoStyle
          ? const ValueKey('login-ios-form-shell')
          : emphasizeDesktop
          ? const ValueKey('login-windows-form-shell')
          : const ValueKey('login-android-form-shell'),
      duration: const Duration(milliseconds: 180),
      padding: EdgeInsets.all(
        cupertinoStyle ? 1.5 : (isDesktopIntegrated ? 4 : 0),
      ),
      decoration: BoxDecoration(
        color: backgroundColor,
        borderRadius: radius,
        border: Border.all(color: borderColor),
        gradient: LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: <Color>[
            Colors.white.withValues(
              alpha: cupertinoStyle
                  ? 0.004
                  : (isDesktopIntegrated ? 0.01 : 0.006),
            ),
            Colors.transparent,
          ],
        ),
        boxShadow: <BoxShadow>[
          if (!cupertinoStyle && !isMobileMaterial)
            BoxShadow(
              color: Colors.black.withValues(
                alpha: cupertinoStyle
                    ? 0.002
                    : (emphasizeDesktop ? 0.012 : 0.008),
              ),
              blurRadius: cupertinoStyle ? 2.5 : (emphasizeDesktop ? 10 : 6),
              offset: Offset(
                0,
                cupertinoStyle ? 1 : (emphasizeDesktop ? 4 : 3),
              ),
            ),
        ],
      ),
      child: child,
    );

    if (cupertinoStyle) {
      return ClipRRect(
        borderRadius: radius,
        child: BackdropFilter(
          key: const ValueKey('login-ios-frost-layer'),
          filter: ImageFilter.blur(sigmaX: 14, sigmaY: 14),
          child: surface,
        ),
      );
    }

    return surface;
  }
}

class _LoginCredentialFields extends StatelessWidget {
  const _LoginCredentialFields({
    required this.usernameController,
    required this.passwordController,
    required this.canSubmit,
    required this.cupertinoStyle,
    required this.onSubmit,
  });

  final TextEditingController usernameController;
  final TextEditingController passwordController;
  final bool canSubmit;
  final bool cupertinoStyle;
  final Future<void> Function() onSubmit;

  @override
  Widget build(BuildContext context) {
    if (cupertinoStyle) {
      return _CupertinoLoginCredentialGroup(
        usernameController: usernameController,
        passwordController: passwordController,
        canSubmit: canSubmit,
        onSubmit: onSubmit,
      );
    }

    return _MaterialLoginCredentialGroup(
      usernameController: usernameController,
      passwordController: passwordController,
      canSubmit: canSubmit,
      onSubmit: onSubmit,
    );
  }
}

class _MaterialLoginCredentialGroup extends StatelessWidget {
  const _MaterialLoginCredentialGroup({
    required this.usernameController,
    required this.passwordController,
    required this.canSubmit,
    required this.onSubmit,
  });

  final TextEditingController usernameController;
  final TextEditingController passwordController;
  final bool canSubmit;
  final Future<void> Function() onSubmit;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final palette = theme.extension<AppShellColors>()!;
    final isWide = MediaQuery.sizeOf(context).width >= 760;
    final fieldSurface = Color.alphaBlend(
      Colors.white.withValues(alpha: isWide ? 0.052 : 0.04),
      theme.colorScheme.surface.withValues(alpha: isWide ? 0.24 : 0.24),
    );
    final fieldBorder = Colors.white.withValues(alpha: isWide ? 0.082 : 0.058);
    final dividerColor = material.isGlass
        ? palette.divider.withValues(alpha: isWide ? 0.32 : 0.17)
        : Colors.white.withValues(alpha: 0.05);
    return Container(
      decoration: BoxDecoration(
        color: fieldSurface,
        borderRadius: BorderRadius.circular(isWide ? 14 : 15),
        border: Border.all(color: fieldBorder),
      ),
      child: Column(
        children: <Widget>[
          _MaterialLoginField(
            controller: usernameController,
            placeholder: '账号',
            semanticIcon: AppSemanticIcon.account,
            textInputAction: TextInputAction.next,
          ),
          Container(
            height: 1,
            margin: EdgeInsets.only(left: isWide ? 42 : 42),
            color: dividerColor,
          ),
          _MaterialLoginField(
            controller: passwordController,
            placeholder: '密码',
            semanticIcon: AppSemanticIcon.lock,
            obscureText: true,
            textInputAction: TextInputAction.done,
            onSubmitted: (_) {
              if (canSubmit) {
                onSubmit();
              }
            },
          ),
        ],
      ),
    );
  }
}

class _MaterialLoginField extends StatelessWidget {
  const _MaterialLoginField({
    required this.controller,
    required this.placeholder,
    required this.semanticIcon,
    required this.textInputAction,
    this.obscureText = false,
    this.onSubmitted,
  });

  final TextEditingController controller;
  final String placeholder;
  final AppSemanticIcon semanticIcon;
  final TextInputAction textInputAction;
  final bool obscureText;
  final ValueChanged<String>? onSubmitted;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isWide = MediaQuery.sizeOf(context).width >= 760;
    return SizedBox(
      height: isWide ? 50 : 48,
      child: Row(
        children: <Widget>[
          SizedBox(
            width: isWide ? 42 : 42,
            child: Center(
              child: AppIcon(
                semanticIcon,
                size: isWide ? 15.6 : 15,
                color: Colors.white.withValues(alpha: isWide ? 0.88 : 0.68),
              ),
            ),
          ),
          Expanded(
            child: TextField(
              controller: controller,
              obscureText: obscureText,
              keyboardType: obscureText
                  ? TextInputType.visiblePassword
                  : TextInputType.name,
              textInputAction: textInputAction,
              enableSuggestions: false,
              enableIMEPersonalizedLearning: false,
              autocorrect: false,
              smartDashesType: SmartDashesType.disabled,
              smartQuotesType: SmartQuotesType.disabled,
              onSubmitted: onSubmitted,
              cursorColor: Colors.white,
              style: theme.textTheme.titleMedium?.copyWith(
                color: Colors.white,
                fontWeight: FontWeight.w500,
                fontSize: isWide ? 15.0 : 15.0,
              ),
              decoration: InputDecoration(
                isCollapsed: true,
                border: InputBorder.none,
                hintText: placeholder,
                hintStyle: theme.textTheme.titleMedium?.copyWith(
                  color: Colors.white.withValues(alpha: isWide ? 0.66 : 0.44),
                  fontWeight: FontWeight.w500,
                  fontSize: isWide ? 15.0 : 15.0,
                ),
                contentPadding: EdgeInsets.only(
                  right: 11,
                  top: isWide ? 16 : 16,
                  bottom: isWide ? 16 : 16,
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _CupertinoLoginCredentialGroup extends StatelessWidget {
  const _CupertinoLoginCredentialGroup({
    required this.usernameController,
    required this.passwordController,
    required this.canSubmit,
    required this.onSubmit,
  });

  final TextEditingController usernameController;
  final TextEditingController passwordController;
  final bool canSubmit;
  final Future<void> Function() onSubmit;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final palette = theme.extension<AppShellColors>()!;
    final dividerColor = material.isGlass
        ? palette.divider.withValues(alpha: 0.14)
        : Colors.white.withValues(alpha: 0.06);
    return Container(
      decoration: BoxDecoration(
        color: Color.alphaBlend(
          Colors.white.withValues(alpha: material.isGlass ? 0.032 : 0.018),
          theme.colorScheme.surface.withValues(alpha: 0.28),
        ),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: Colors.white.withValues(
            alpha: material.isGlass ? 0.052 : 0.034,
          ),
        ),
      ),
      child: Column(
        children: <Widget>[
          _CupertinoLoginField(
            controller: usernameController,
            placeholder: '账号',
            semanticIcon: AppSemanticIcon.account,
            textInputAction: TextInputAction.next,
          ),
          Container(
            height: 1,
            margin: const EdgeInsets.only(left: 36),
            color: dividerColor,
          ),
          _CupertinoLoginField(
            controller: passwordController,
            placeholder: '密码',
            semanticIcon: AppSemanticIcon.lock,
            obscureText: true,
            textInputAction: TextInputAction.done,
            onSubmitted: (_) {
              if (canSubmit) {
                onSubmit();
              }
            },
          ),
        ],
      ),
    );
  }
}

class _CupertinoLoginField extends StatelessWidget {
  const _CupertinoLoginField({
    required this.controller,
    required this.placeholder,
    required this.semanticIcon,
    required this.textInputAction,
    this.obscureText = false,
    this.onSubmitted,
  });

  final TextEditingController controller;
  final String placeholder;
  final AppSemanticIcon semanticIcon;
  final TextInputAction textInputAction;
  final bool obscureText;
  final ValueChanged<String>? onSubmitted;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return SizedBox(
      height: 40,
      child: Row(
        children: <Widget>[
          SizedBox(
            width: 36,
            child: Center(
              child: AppIcon(
                semanticIcon,
                size: 13.5,
                color: Colors.white.withValues(alpha: 0.62),
              ),
            ),
          ),
          Expanded(
            child: CupertinoTextField.borderless(
              controller: controller,
              obscureText: obscureText,
              keyboardType: obscureText
                  ? TextInputType.visiblePassword
                  : TextInputType.name,
              textInputAction: textInputAction,
              enableSuggestions: false,
              enableIMEPersonalizedLearning: false,
              autocorrect: false,
              smartDashesType: SmartDashesType.disabled,
              smartQuotesType: SmartQuotesType.disabled,
              onSubmitted: onSubmitted,
              padding: const EdgeInsets.only(right: 12, top: 10, bottom: 10),
              placeholder: placeholder,
              style: theme.textTheme.titleMedium?.copyWith(
                color: Colors.white,
                fontWeight: FontWeight.w500,
                fontSize: 14.5,
              ),
              placeholderStyle: theme.textTheme.titleMedium?.copyWith(
                color: Colors.white.withValues(alpha: 0.56),
                fontWeight: FontWeight.w500,
                fontSize: 14.5,
              ),
              decoration: null,
            ),
          ),
        ],
      ),
    );
  }
}
