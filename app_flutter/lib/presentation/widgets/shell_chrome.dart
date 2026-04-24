import 'dart:ui';

import 'package:flutter/material.dart';

import '../theme/app_icons.dart';
import '../theme/app_theme.dart';
import '../theme/app_tokens.dart';

enum ShellChromeRole {
  topBar,
  bottomBar,
  toolBar,
  searchField,
  controlCluster,
  composerDock,
  desktopPanel,
}

class ImSurfacePolicy {
  const ImSurfacePolicy._();

  static bool allowsGlass({
    required TargetPlatform platform,
    required ShellChromeRole role,
  }) {
    return switch (platform) {
      TargetPlatform.iOS => role == ShellChromeRole.bottomBar,
      TargetPlatform.macOS =>
        role == ShellChromeRole.bottomBar ||
            role == ShellChromeRole.desktopPanel,
      TargetPlatform.windows ||
      TargetPlatform.linux => role == ShellChromeRole.desktopPanel,
      TargetPlatform.android || TargetPlatform.fuchsia => false,
    };
  }

  static bool forcesSolid({
    required TargetPlatform platform,
    required ShellChromeRole role,
  }) {
    return !allowsGlass(platform: platform, role: role);
  }
}

class ShellPageHeader extends StatelessWidget {
  const ShellPageHeader({
    super.key,
    required this.title,
    this.subtitle,
    this.centerTitle = false,
    this.eyebrow,
    this.trailing,
  });

  final String title;
  final String? subtitle;
  final bool centerTitle;
  final String? eyebrow;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final palette = theme.extension<AppShellColors>()!;
    final crossAxisAlignment = centerTitle
        ? CrossAxisAlignment.center
        : CrossAxisAlignment.start;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Expanded(
          child: Column(
            crossAxisAlignment: crossAxisAlignment,
            children: <Widget>[
              if (eyebrow case final eyebrowText?) ...<Widget>[
                _HeaderCapsule(
                  isGlass: material.isGlass,
                  backgroundColor: material.isGlass
                      ? material.controlSurfaceColor
                      : palette.bg2,
                  borderColor: material.isGlass
                      ? material.controlBorderColor
                      : palette.divider.withValues(alpha: 0.8),
                  child: Text(
                    eyebrowText,
                    textAlign: centerTitle ? TextAlign.center : TextAlign.start,
                    style: theme.textTheme.labelMedium?.copyWith(
                      fontWeight: FontWeight.w700,
                      color: material.isGlass
                          ? palette.textSecondary
                          : palette.textTertiary,
                      letterSpacing: 0.2,
                    ),
                  ),
                ),
                const SizedBox(height: AppTokens.spacingXs),
              ],
              Text(
                title,
                textAlign: centerTitle ? TextAlign.center : TextAlign.start,
                style: theme.textTheme.headlineMedium?.copyWith(
                  fontWeight: FontWeight.w600,
                  letterSpacing: material.isGlass ? -0.3 : -0.1,
                ),
              ),
              if (subtitle case final subtitleText?
                  when subtitleText.isNotEmpty) ...<Widget>[
                const SizedBox(height: AppTokens.spacingXs),
                Text(
                  subtitleText,
                  textAlign: centerTitle ? TextAlign.center : TextAlign.start,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: material.isGlass
                        ? palette.textSecondary.withValues(alpha: 0.86)
                        : palette.textTertiary,
                  ),
                ),
              ],
            ],
          ),
        ),
        if (trailing case final trailingWidget?) ...<Widget>[
          const SizedBox(width: AppTokens.spacingMd),
          trailingWidget,
        ],
      ],
    );
  }
}

class ShellStatusPill extends StatelessWidget {
  const ShellStatusPill({
    super.key,
    required this.label,
    required this.tint,
    this.icon,
  });

  final String label;
  final Color tint;
  final AppSemanticIcon? icon;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final pillContent = Row(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        if (icon case final semanticIcon?) ...<Widget>[
          AppIcon(semanticIcon, size: 14, color: tint),
          const SizedBox(width: 6),
        ],
        Text(
          label,
          style: theme.textTheme.labelMedium?.copyWith(
            color: tint,
            fontWeight: FontWeight.w700,
          ),
        ),
      ],
    );

    if (!material.isGlass) {
      return Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
        decoration: BoxDecoration(
          color: tint.withValues(alpha: 0.12),
          borderRadius: BorderRadius.circular(999),
        ),
        child: pillContent,
      );
    }

    return LiquidGlassPanel(
      enabled: true,
      role: ShellChromeRole.controlCluster,
      radius: AppTokens.radiusFull,
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
      backgroundColor: Color.alphaBlend(
        tint.withValues(alpha: 0.06),
        material.controlSurfaceColor,
      ),
      borderColor: Color.alphaBlend(
        tint.withValues(alpha: 0.14),
        material.controlBorderColor,
      ),
      shadowOpacity: 0.06,
      child: pillContent,
    );
  }
}

class ShellSectionLabel extends StatelessWidget {
  const ShellSectionLabel({
    super.key,
    required this.title,
    this.padding = const EdgeInsets.symmetric(horizontal: 4),
  });

  final String title;
  final EdgeInsetsGeometry padding;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: padding,
      child: Text(
        title,
        style: Theme.of(
          context,
        ).textTheme.labelLarge?.copyWith(fontWeight: FontWeight.w700),
      ),
    );
  }
}

class ShellMetricCard extends StatelessWidget {
  const ShellMetricCard({
    super.key,
    required this.label,
    required this.value,
    this.icon,
    this.semanticIcon,
    required this.tint,
  }) : assert(icon != null || semanticIcon != null);

  final String label;
  final String value;
  final IconData? icon;
  final AppSemanticIcon? semanticIcon;
  final Color tint;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final palette = theme.extension<AppShellColors>()!;
    return Container(
      padding: const EdgeInsets.all(AppTokens.spacingMd),
      decoration: BoxDecoration(
        color: material.surfaceColor,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(
          color: material.isGlass
              ? material.borderColor
              : tint.withValues(alpha: 0.22),
        ),
        boxShadow: <BoxShadow>[
          BoxShadow(
            color: material.shadowColor.withValues(
              alpha: material.isGlass ? 0.72 : 1,
            ),
            blurRadius: material.isGlass ? 14 : 18,
            offset: Offset(0, material.isGlass ? 8 : 10),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          semanticIcon != null
              ? AppIcon(semanticIcon!, size: 18, color: tint)
              : Icon(icon!, size: 18, color: tint),
          const SizedBox(height: AppTokens.spacingSm),
          Text(
            label,
            style: Theme.of(
              context,
            ).textTheme.labelMedium?.copyWith(color: palette.textSecondary),
          ),
          const SizedBox(height: AppTokens.spacingXs),
          Text(value, style: Theme.of(context).textTheme.titleMedium),
        ],
      ),
    );
  }
}

class LiquidGlassStyleProfile {
  const LiquidGlassStyleProfile({
    required this.panelBlurSigma,
    required this.shadowAlpha,
    required this.shadowBlurRadius,
    required this.shadowOffsetY,
    required this.surfaceLiftAlpha,
    required this.surfaceDepthAlpha,
    required this.radialHighlightAlpha,
    required this.topEdgeAlpha,
    required this.sideEdgeAlpha,
    required this.innerStrokeAlpha,
    required this.usesBottomGlow,
    required this.usesColorBias,
  });

  final double panelBlurSigma;
  final double shadowAlpha;
  final double shadowBlurRadius;
  final double shadowOffsetY;
  final double surfaceLiftAlpha;
  final double surfaceDepthAlpha;
  final double radialHighlightAlpha;
  final double topEdgeAlpha;
  final double sideEdgeAlpha;
  final double innerStrokeAlpha;
  final bool usesBottomGlow;
  final bool usesColorBias;
}

@visibleForTesting
LiquidGlassStyleProfile resolveLiquidGlassStyleProfile({
  required AppShellMaterial material,
  required ShellChromeRole role,
  required TargetPlatform targetPlatform,
  required bool enabled,
  double shadowOpacity = 0.12,
  double? blurSigma,
}) {
  if (!enabled) {
    return LiquidGlassStyleProfile(
      panelBlurSigma: 0,
      shadowAlpha: shadowOpacity * 0.22,
      shadowBlurRadius: 12,
      shadowOffsetY: 5,
      surfaceLiftAlpha: 0,
      surfaceDepthAlpha: 0,
      radialHighlightAlpha: 0,
      topEdgeAlpha: 0,
      sideEdgeAlpha: 0,
      innerStrokeAlpha: 0,
      usesBottomGlow: false,
      usesColorBias: false,
    );
  }

  final isAppleGlass =
      targetPlatform == TargetPlatform.iOS ||
      targetPlatform == TargetPlatform.macOS;
  final isDesktopQuiet =
      targetPlatform == TargetPlatform.windows ||
      targetPlatform == TargetPlatform.linux;

  final blurFactor = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 1.32,
    (true, _, ShellChromeRole.bottomBar) => 1.30,
    (true, _, ShellChromeRole.composerDock) => 0.86,
    (true, _, ShellChromeRole.desktopPanel) => 0.42,
    (true, _, ShellChromeRole.toolBar) => 1.12,
    (true, _, ShellChromeRole.searchField) => 0.82,
    (true, _, ShellChromeRole.controlCluster) => 0.64,
    (_, true, ShellChromeRole.topBar) => 0.50,
    (_, true, ShellChromeRole.bottomBar) => 0.54,
    (_, true, ShellChromeRole.composerDock) => 0.48,
    (_, true, ShellChromeRole.desktopPanel) => 0.34,
    (_, true, ShellChromeRole.toolBar) => 0.48,
    (_, true, ShellChromeRole.searchField) => 0.46,
    (_, true, ShellChromeRole.controlCluster) => 0.42,
    (_, _, ShellChromeRole.topBar) => 0.64,
    (_, _, ShellChromeRole.bottomBar) => 0.68,
    (_, _, ShellChromeRole.composerDock) => 0.58,
    (_, _, ShellChromeRole.desktopPanel) => 0.42,
    (_, _, ShellChromeRole.toolBar) => 0.60,
    (_, _, ShellChromeRole.searchField) => 0.58,
    (_, _, ShellChromeRole.controlCluster) => 0.52,
  };
  final shadowFactor = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 0.05,
    (true, _, ShellChromeRole.bottomBar) => 0.02,
    (true, _, ShellChromeRole.composerDock) => 0.028,
    (true, _, ShellChromeRole.desktopPanel) => 0.020,
    (true, _, ShellChromeRole.toolBar) => 0.08,
    (true, _, ShellChromeRole.searchField) => 0.024,
    (true, _, ShellChromeRole.controlCluster) => 0.028,
    (_, true, ShellChromeRole.topBar) => 0.18,
    (_, true, ShellChromeRole.bottomBar) => 0.19,
    (_, true, ShellChromeRole.composerDock) => 0.18,
    (_, true, ShellChromeRole.desktopPanel) => 0.10,
    (_, true, ShellChromeRole.toolBar) => 0.18,
    (_, true, ShellChromeRole.searchField) => 0.19,
    (_, true, ShellChromeRole.controlCluster) => 0.20,
    (_, _, ShellChromeRole.topBar) => 0.21,
    (_, _, ShellChromeRole.bottomBar) => 0.22,
    (_, _, ShellChromeRole.composerDock) => 0.22,
    (_, _, ShellChromeRole.desktopPanel) => 0.14,
    (_, _, ShellChromeRole.toolBar) => 0.21,
    (_, _, ShellChromeRole.searchField) => 0.23,
    (_, _, ShellChromeRole.controlCluster) => 0.24,
  };
  final surfaceLiftAlpha = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 0.032,
    (true, _, ShellChromeRole.bottomBar) => 0.026,
    (true, _, ShellChromeRole.composerDock) => 0.014,
    (true, _, ShellChromeRole.desktopPanel) => 0.010,
    (true, _, ShellChromeRole.toolBar) => 0.023,
    (true, _, ShellChromeRole.searchField) => 0.011,
    (true, _, ShellChromeRole.controlCluster) => 0.009,
    (_, true, ShellChromeRole.topBar) => 0.012,
    (_, true, ShellChromeRole.bottomBar) => 0.014,
    (_, true, ShellChromeRole.composerDock) => 0.011,
    (_, true, ShellChromeRole.desktopPanel) => 0.008,
    (_, true, ShellChromeRole.toolBar) => 0.012,
    (_, true, ShellChromeRole.searchField) => 0.010,
    (_, true, ShellChromeRole.controlCluster) => 0.008,
    (_, _, ShellChromeRole.topBar) => 0.016,
    (_, _, ShellChromeRole.bottomBar) => 0.018,
    (_, _, ShellChromeRole.composerDock) => 0.014,
    (_, _, ShellChromeRole.desktopPanel) => 0.009,
    (_, _, ShellChromeRole.toolBar) => 0.015,
    (_, _, ShellChromeRole.searchField) => 0.012,
    (_, _, ShellChromeRole.controlCluster) => 0.010,
  };
  final surfaceDepthAlpha = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 0.010,
    (true, _, ShellChromeRole.bottomBar) => 0.008,
    (true, _, ShellChromeRole.composerDock) => 0.004,
    (true, _, ShellChromeRole.desktopPanel) => 0.004,
    (true, _, ShellChromeRole.toolBar) => 0.007,
    (true, _, ShellChromeRole.searchField) => 0.003,
    (true, _, ShellChromeRole.controlCluster) => 0.003,
    (_, true, _) => 0.006,
    (_, _, ShellChromeRole.topBar) => 0.008,
    (_, _, ShellChromeRole.bottomBar) => 0.010,
    (_, _, ShellChromeRole.composerDock) => 0.007,
    (_, _, ShellChromeRole.desktopPanel) => 0.005,
    (_, _, ShellChromeRole.toolBar) => 0.008,
    (_, _, ShellChromeRole.searchField) => 0.007,
    (_, _, ShellChromeRole.controlCluster) => 0.006,
  };
  final highlightAlpha = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 0.018,
    (true, _, ShellChromeRole.bottomBar) => 0.014,
    (true, _, ShellChromeRole.composerDock) => 0.006,
    (true, _, ShellChromeRole.desktopPanel) => 0.004,
    (true, _, ShellChromeRole.toolBar) => 0.012,
    (true, _, ShellChromeRole.searchField) => 0.004,
    (true, _, ShellChromeRole.controlCluster) => 0.003,
    (_, true, _) => 0.008,
    (_, _, ShellChromeRole.topBar) => 0.010,
    (_, _, ShellChromeRole.bottomBar) => 0.012,
    (_, _, ShellChromeRole.composerDock) => 0.008,
    (_, _, ShellChromeRole.desktopPanel) => 0.006,
    (_, _, ShellChromeRole.toolBar) => 0.010,
    (_, _, ShellChromeRole.searchField) => 0.009,
    (_, _, ShellChromeRole.controlCluster) => 0.008,
  };
  final topEdgeAlpha = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 0.048,
    (true, _, ShellChromeRole.bottomBar) => 0.038,
    (true, _, ShellChromeRole.composerDock) => 0.022,
    (true, _, ShellChromeRole.desktopPanel) => 0.014,
    (true, _, ShellChromeRole.toolBar) => 0.034,
    (true, _, ShellChromeRole.searchField) => 0.018,
    (true, _, ShellChromeRole.controlCluster) => 0.015,
    (_, true, _) => 0.007,
    (_, _, ShellChromeRole.topBar) => 0.009,
    (_, _, ShellChromeRole.bottomBar) => 0.010,
    (_, _, ShellChromeRole.composerDock) => 0.008,
    (_, _, ShellChromeRole.desktopPanel) => 0.005,
    (_, _, ShellChromeRole.toolBar) => 0.009,
    (_, _, ShellChromeRole.searchField) => 0.008,
    (_, _, ShellChromeRole.controlCluster) => 0.007,
  };
  final sideEdgeAlpha = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 0.024,
    (true, _, ShellChromeRole.bottomBar) => 0.018,
    (true, _, ShellChromeRole.composerDock) => 0.008,
    (true, _, ShellChromeRole.desktopPanel) => 0.006,
    (true, _, ShellChromeRole.searchField) => 0.007,
    (true, _, ShellChromeRole.controlCluster) => 0.006,
    (true, _, _) => 0.018,
    (_, true, _) => 0.003,
    (_, _, _) => 0.004,
  };
  final innerStrokeAlpha = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 0.020,
    (true, _, ShellChromeRole.bottomBar) => 0.018,
    (true, _, ShellChromeRole.composerDock) => 0.011,
    (true, _, ShellChromeRole.desktopPanel) => 0.006,
    (true, _, ShellChromeRole.searchField) => 0.010,
    (true, _, ShellChromeRole.controlCluster) => 0.009,
    (true, _, _) => 0.016,
    (_, true, _) => 0.003,
    (_, _, _) => 0.004,
  };

  final shadowBlurRadius = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 6.4,
    (true, _, ShellChromeRole.bottomBar) => 4.2,
    (true, _, ShellChromeRole.composerDock) => 3.4,
    (true, _, ShellChromeRole.desktopPanel) => 4.0,
    (true, _, ShellChromeRole.searchField) => 3.0,
    (true, _, ShellChromeRole.controlCluster) => 3.0,
    (true, _, _) => 5.0,
    (_, true, _) => 8.5,
    (_, _, _) => 9.0,
  };
  final shadowOffsetY = switch ((isAppleGlass, isDesktopQuiet, role)) {
    (true, _, ShellChromeRole.topBar) => 1.2,
    (true, _, ShellChromeRole.bottomBar) => 0.6,
    (true, _, ShellChromeRole.composerDock) => 0.45,
    (true, _, ShellChromeRole.desktopPanel) => 1.2,
    (true, _, ShellChromeRole.searchField) => 0.35,
    (true, _, ShellChromeRole.controlCluster) => 0.35,
    (true, _, _) => 1.0,
    (_, true, _) => 2.5,
    (_, _, _) => 3.0,
  };

  return LiquidGlassStyleProfile(
    panelBlurSigma: (blurSigma ?? material.blurSigma) * blurFactor,
    shadowAlpha: shadowOpacity * shadowFactor,
    shadowBlurRadius: shadowBlurRadius,
    shadowOffsetY: shadowOffsetY,
    surfaceLiftAlpha: surfaceLiftAlpha,
    surfaceDepthAlpha: surfaceDepthAlpha,
    radialHighlightAlpha: highlightAlpha,
    topEdgeAlpha: topEdgeAlpha,
    sideEdgeAlpha: sideEdgeAlpha,
    innerStrokeAlpha: innerStrokeAlpha,
    usesBottomGlow: false,
    usesColorBias: false,
  );
}

class LiquidGlassPanel extends StatelessWidget {
  // Use this widget only for shell chrome: navigation, search, composer docks,
  // control clusters, and desktop side/detail panels. Reading surfaces such as
  // message bubbles, attachment cards, and list rows must stay solid.
  const LiquidGlassPanel({
    super.key,
    required this.child,
    required this.enabled,
    this.role = ShellChromeRole.controlCluster,
    this.radius = 24,
    this.padding = EdgeInsets.zero,
    this.backgroundColor,
    this.borderColor,
    this.blurSigma,
    this.shadowOpacity = 0.12,
  });

  final Widget child;
  final bool enabled;
  final ShellChromeRole role;
  final double radius;
  final EdgeInsetsGeometry padding;
  final Color? backgroundColor;
  final Color? borderColor;
  final double? blurSigma;
  final double shadowOpacity;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final isAppleGlass =
        theme.platform == TargetPlatform.iOS ||
        theme.platform == TargetPlatform.macOS;
    final glassEnabled =
        enabled &&
        ImSurfacePolicy.allowsGlass(platform: theme.platform, role: role);
    final surface =
        backgroundColor ??
        (glassEnabled ? _defaultSurface(material) : material.surfaceColor);
    final edgeColor =
        borderColor ??
        (glassEnabled ? _defaultBorder(material) : material.borderColor);
    final resolvedSurface = glassEnabled && isAppleGlass
        ? Color.alphaBlend(
            Colors.white.withValues(
              alpha: switch (role) {
                ShellChromeRole.bottomBar => 0.018,
                ShellChromeRole.composerDock => 0.010,
                ShellChromeRole.desktopPanel => 0.008,
                ShellChromeRole.searchField => 0.006,
                ShellChromeRole.controlCluster => 0.005,
                ShellChromeRole.topBar => 0.020,
                _ => 0.012,
              },
            ),
            surface,
          )
        : surface;
    final resolvedEdgeColor = glassEnabled && isAppleGlass
        ? Color.alphaBlend(
            Colors.white.withValues(
              alpha: switch (role) {
                ShellChromeRole.bottomBar => 0.060,
                ShellChromeRole.composerDock => 0.042,
                ShellChromeRole.desktopPanel => 0.030,
                ShellChromeRole.searchField => 0.034,
                ShellChromeRole.controlCluster => 0.030,
                ShellChromeRole.topBar => 0.076,
                _ => 0.058,
              },
            ),
            edgeColor,
          )
        : edgeColor;
    final borderRadius = BorderRadius.circular(radius);
    final style = resolveLiquidGlassStyleProfile(
      material: material,
      role: role,
      targetPlatform: theme.platform,
      enabled: glassEnabled,
      blurSigma: blurSigma,
      shadowOpacity: shadowOpacity,
    );
    final content = Container(
      decoration: BoxDecoration(
        color: glassEnabled ? null : surface,
        gradient: glassEnabled
            ? LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: <Color>[
                  Color.alphaBlend(
                    Colors.white.withValues(alpha: style.surfaceLiftAlpha),
                    resolvedSurface,
                  ),
                  resolvedSurface,
                  Color.alphaBlend(
                    Colors.black.withValues(alpha: style.surfaceDepthAlpha),
                    resolvedSurface,
                  ),
                ],
                stops: const <double>[0, 0.58, 1],
              )
            : null,
        borderRadius: borderRadius,
        border: Border.all(
          color: resolvedEdgeColor,
          width: glassEnabled && isAppleGlass
              ? switch (role) {
                  ShellChromeRole.bottomBar => 0.68,
                  ShellChromeRole.composerDock => 0.62,
                  ShellChromeRole.desktopPanel => 0.60,
                  ShellChromeRole.searchField => 0.58,
                  ShellChromeRole.controlCluster => 0.54,
                  _ => 0.78,
                }
              : 1.0,
        ),
        boxShadow: style.shadowAlpha <= 0
            ? const <BoxShadow>[]
            : <BoxShadow>[
                BoxShadow(
                  color: Colors.black.withValues(alpha: style.shadowAlpha),
                  blurRadius: style.shadowBlurRadius,
                  offset: Offset(0, style.shadowOffsetY),
                ),
              ],
      ),
      child: Stack(
        children: <Widget>[
          if (glassEnabled && isAppleGlass && style.topEdgeAlpha > 0)
            Positioned.fill(
              child: IgnorePointer(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    borderRadius: borderRadius,
                    gradient: LinearGradient(
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                      colors: <Color>[
                        Colors.white.withValues(
                          alpha: style.topEdgeAlpha * 0.95,
                        ),
                        Colors.white.withValues(
                          alpha:
                              style.topEdgeAlpha *
                              switch (role) {
                                ShellChromeRole.bottomBar => 0.06,
                                ShellChromeRole.searchField => 0.05,
                                ShellChromeRole.controlCluster => 0.05,
                                _ => 0.14,
                              },
                        ),
                        Colors.transparent,
                      ],
                      stops: switch (role) {
                        ShellChromeRole.bottomBar => const <double>[
                          0,
                          0.025,
                          0.07,
                        ],
                        ShellChromeRole.searchField ||
                        ShellChromeRole.controlCluster => const <double>[
                          0,
                          0.03,
                          0.08,
                        ],
                        _ => const <double>[0, 0.05, 0.14],
                      },
                    ),
                  ),
                ),
              ),
            ),
          if (glassEnabled &&
              style.radialHighlightAlpha > 0 &&
              role == ShellChromeRole.topBar)
            Positioned.fill(
              child: IgnorePointer(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    borderRadius: borderRadius,
                    gradient: RadialGradient(
                      center: const Alignment(-0.36, -0.94),
                      radius: 1.1,
                      colors: <Color>[
                        Colors.white.withValues(
                          alpha: style.radialHighlightAlpha,
                        ),
                        Colors.white.withValues(
                          alpha: style.radialHighlightAlpha * 0.24,
                        ),
                        Colors.white.withValues(
                          alpha: style.radialHighlightAlpha * 0.08,
                        ),
                        Colors.transparent,
                      ],
                      stops: const <double>[0, 0.18, 0.42, 1],
                    ),
                  ),
                ),
              ),
            ),
          if (glassEnabled &&
              isAppleGlass &&
              role == ShellChromeRole.bottomBar &&
              style.radialHighlightAlpha > 0)
            Positioned.fill(
              child: IgnorePointer(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    borderRadius: borderRadius,
                    gradient: RadialGradient(
                      center: const Alignment(0, -0.9),
                      radius: 1.12,
                      colors: <Color>[
                        Colors.white.withValues(
                          alpha: style.radialHighlightAlpha * 2.35,
                        ),
                        Colors.white.withValues(
                          alpha: style.radialHighlightAlpha * 0.62,
                        ),
                        Colors.transparent,
                      ],
                      stops: const <double>[0, 0.34, 1],
                    ),
                  ),
                ),
              ),
            ),
          if (glassEnabled &&
              style.sideEdgeAlpha > 0 &&
              role != ShellChromeRole.bottomBar &&
              role != ShellChromeRole.searchField &&
              role != ShellChromeRole.controlCluster)
            Positioned.fill(
              child: IgnorePointer(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    borderRadius: borderRadius,
                    gradient: LinearGradient(
                      begin: Alignment.centerLeft,
                      end: Alignment.centerRight,
                      colors: <Color>[
                        Colors.white.withValues(alpha: style.sideEdgeAlpha),
                        Colors.transparent,
                        Colors.white.withValues(
                          alpha: style.sideEdgeAlpha * 0.52,
                        ),
                      ],
                      stops: const <double>[0, 0.32, 1],
                    ),
                  ),
                ),
              ),
            ),
          if (glassEnabled &&
              isAppleGlass &&
              style.sideEdgeAlpha > 0 &&
              role == ShellChromeRole.bottomBar)
            Positioned.fill(
              child: IgnorePointer(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    borderRadius: borderRadius,
                    gradient: LinearGradient(
                      begin: Alignment.centerLeft,
                      end: Alignment.centerRight,
                      colors: <Color>[
                        Colors.white.withValues(
                          alpha: style.sideEdgeAlpha * 1.05,
                        ),
                        Colors.transparent,
                        Colors.white.withValues(
                          alpha: style.sideEdgeAlpha * 0.66,
                        ),
                      ],
                      stops: const <double>[0, 0.38, 1],
                    ),
                  ),
                ),
              ),
            ),
          if (glassEnabled && style.innerStrokeAlpha > 0)
            Positioned.fill(
              child: IgnorePointer(
                child: Padding(
                  padding: const EdgeInsets.all(0.5),
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(radius - 0.45),
                      border: Border.all(
                        color: Colors.white.withValues(
                          alpha: style.innerStrokeAlpha,
                        ),
                        width: isAppleGlass
                            ? switch (role) {
                                ShellChromeRole.searchField ||
                                ShellChromeRole.controlCluster ||
                                ShellChromeRole.composerDock ||
                                ShellChromeRole.desktopPanel => 0.6,
                                ShellChromeRole.bottomBar => 0.68,
                                _ => 0.76,
                              }
                            : 0.8,
                      ),
                    ),
                  ),
                ),
              ),
            ),
          if (glassEnabled && style.topEdgeAlpha > 0)
            Positioned(
              left: switch (role) {
                ShellChromeRole.bottomBar => 18,
                ShellChromeRole.searchField ||
                ShellChromeRole.controlCluster => 14,
                _ => 20,
              },
              right: switch (role) {
                ShellChromeRole.bottomBar => 18,
                ShellChromeRole.searchField ||
                ShellChromeRole.controlCluster => 14,
                _ => 20,
              },
              top: 0,
              child: IgnorePointer(
                child: Container(
                  height: 1,
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      colors: <Color>[
                        Colors.white.withValues(alpha: 0),
                        Colors.white.withValues(alpha: style.topEdgeAlpha),
                        Colors.white.withValues(alpha: 0),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          Padding(padding: padding, child: child),
        ],
      ),
    );

    if (!glassEnabled) {
      return content;
    }

    return ClipRRect(
      borderRadius: borderRadius,
      child: BackdropFilter(
        filter: ImageFilter.blur(
          sigmaX: style.panelBlurSigma,
          sigmaY: style.panelBlurSigma,
        ),
        child: content,
      ),
    );
  }

  Color _defaultSurface(AppShellMaterial material) {
    if (!enabled) {
      return material.surfaceColor;
    }
    return switch (role) {
      ShellChromeRole.topBar => material.topBarColor,
      ShellChromeRole.bottomBar => material.bottomBarColor,
      ShellChromeRole.toolBar => material.chromeSurfaceColor,
      ShellChromeRole.searchField => material.searchBarColor,
      ShellChromeRole.controlCluster => material.controlSurfaceColor,
      ShellChromeRole.composerDock => material.bottomBarColor,
      ShellChromeRole.desktopPanel => material.sideBarColor,
    };
  }

  Color _defaultBorder(AppShellMaterial material) {
    if (!enabled) {
      return material.borderColor;
    }
    return switch (role) {
      ShellChromeRole.topBar ||
      ShellChromeRole.bottomBar ||
      ShellChromeRole.toolBar ||
      ShellChromeRole.composerDock ||
      ShellChromeRole.desktopPanel => material.chromeBorderColor,
      ShellChromeRole.searchField ||
      ShellChromeRole.controlCluster => material.controlBorderColor,
    };
  }
}

class _HeaderCapsule extends StatelessWidget {
  const _HeaderCapsule({
    required this.child,
    required this.isGlass,
    required this.backgroundColor,
    required this.borderColor,
  });

  final Widget child;
  final bool isGlass;
  final Color backgroundColor;
  final Color borderColor;

  @override
  Widget build(BuildContext context) {
    final capsule = Container(
      padding: const EdgeInsets.symmetric(
        horizontal: AppTokens.spacingSm,
        vertical: 5,
      ),
      decoration: BoxDecoration(
        color: backgroundColor,
        borderRadius: BorderRadius.circular(AppTokens.radiusFull),
        border: Border.all(color: borderColor),
      ),
      child: child,
    );

    if (!isGlass) {
      return capsule;
    }

    return ClipRRect(
      borderRadius: BorderRadius.circular(AppTokens.radiusFull),
      child: BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 4.5, sigmaY: 4.5),
        child: capsule,
      ),
    );
  }
}
