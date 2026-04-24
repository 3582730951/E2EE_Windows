import 'dart:ui';

import 'package:flutter/foundation.dart' show defaultTargetPlatform;
import 'package:flutter/material.dart';

import 'app_tokens.dart';
import 'visual_tier.dart';

class AppTheme {
  const AppTheme._();

  static ThemeData light({VisualTier visualTier = VisualTier.standard}) {
    return _baseTheme(brightness: Brightness.light, visualTier: visualTier);
  }

  static ThemeData dark({VisualTier visualTier = VisualTier.standard}) {
    return _baseTheme(brightness: Brightness.dark, visualTier: visualTier);
  }

  static ThemeData _baseTheme({
    required Brightness brightness,
    required VisualTier visualTier,
  }) {
    final shellColors = _shellColors(brightness);
    final targetPlatform = defaultTargetPlatform;
    final isApplePlatform = _isApplePlatform(targetPlatform);
    final material = _materialLayer(
      brightness: brightness,
      colors: shellColors,
      visualTier: visualTier,
    );
    final scheme =
        ColorScheme.fromSeed(
          seedColor: AppPalette.brandBlue,
          brightness: brightness,
        ).copyWith(
          primary: AppPalette.brandBlue,
          onPrimary: Colors.white,
          secondary: AppPalette.socialGreen,
          onSecondary: Colors.white,
          error: AppPalette.danger,
          onError: Colors.white,
          surface: shellColors.bg1,
          onSurface: shellColors.textPrimary,
          onSurfaceVariant: shellColors.textSecondary,
          outline: shellColors.divider,
          shadow: Colors.black,
          surfaceTint: Colors.transparent,
        );
    final base = ThemeData(
      useMaterial3: true,
      platform: targetPlatform,
      brightness: brightness,
      colorScheme: scheme,
      scaffoldBackgroundColor: shellColors.bg0,
      canvasColor: shellColors.bg0,
      dividerColor: shellColors.divider,
      cardColor: material.surfaceColor,
      fontFamily: _fontFamilyFor(targetPlatform),
      textTheme: _textTheme(
        shellColors.textPrimary,
        shellColors.textSecondary,
        shellColors.textTertiary,
        platform: targetPlatform,
      ),
      iconTheme: IconThemeData(
        color: shellColors.textSecondary,
        size: AppTokens.iconMd,
      ),
    );
    return base.copyWith(
      appBarTheme: AppBarTheme(
        backgroundColor: material.topBarColor,
        foregroundColor: shellColors.textPrimary,
        elevation: 0,
        scrolledUnderElevation: 0,
        centerTitle: false,
        surfaceTintColor: Colors.transparent,
        toolbarHeight: AppTokens.topBarHeight,
        titleTextStyle: base.textTheme.titleLarge,
      ),
      cardTheme: CardThemeData(
        elevation: 0,
        color: material.surfaceColor,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(AppTokens.radiusCard),
        ),
      ),
      chipTheme: base.chipTheme.copyWith(
        backgroundColor: shellColors.bg2,
        selectedColor: AppPalette.mistBlue.withValues(alpha: 0.16),
        disabledColor: shellColors.bg2,
        side: BorderSide(color: shellColors.divider.withValues(alpha: 0.64)),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(AppTokens.radiusFull),
        ),
        labelStyle: base.textTheme.labelMedium?.copyWith(
          color: shellColors.textSecondary,
        ),
        secondaryLabelStyle: base.textTheme.labelMedium?.copyWith(
          color: shellColors.textPrimary,
        ),
      ),
      dialogTheme: DialogThemeData(
        backgroundColor: material.surfaceColor,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(AppTokens.radius2xl),
        ),
      ),
      drawerTheme: DrawerThemeData(
        backgroundColor: material.sideBarColor,
        surfaceTintColor: Colors.transparent,
      ),
      bottomSheetTheme: BottomSheetThemeData(
        backgroundColor: material.surfaceColor,
        modalBackgroundColor: material.surfaceColor,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(AppTokens.radius2xl),
        ),
      ),
      dividerTheme: DividerThemeData(
        color: shellColors.divider,
        thickness: 0.8,
        space: 1,
      ),
      listTileTheme: ListTileThemeData(
        iconColor: shellColors.textSecondary,
        textColor: shellColors.textPrimary,
        tileColor: Colors.transparent,
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: material.searchBarColor,
        prefixIconColor: shellColors.textSecondary,
        suffixIconColor: shellColors.textSecondary,
        hintStyle: base.textTheme.bodyMedium?.copyWith(
          color: shellColors.textTertiary,
        ),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(AppTokens.radiusField),
          borderSide: BorderSide.none,
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(AppTokens.radiusField),
          borderSide: BorderSide(color: material.controlBorderColor),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(AppTokens.radiusField),
          borderSide: const BorderSide(color: AppPalette.mistBlue, width: 1.5),
        ),
        contentPadding: const EdgeInsets.symmetric(
          horizontal: AppTokens.spacingLg,
          vertical: AppTokens.spacingLg,
        ),
      ),
      searchBarTheme: SearchBarThemeData(
        backgroundColor: WidgetStatePropertyAll(material.searchBarColor),
        elevation: const WidgetStatePropertyAll(0),
        shadowColor: const WidgetStatePropertyAll(Colors.transparent),
        side: WidgetStatePropertyAll(
          BorderSide(color: material.controlBorderColor),
        ),
        surfaceTintColor: const WidgetStatePropertyAll(Colors.transparent),
        padding: const WidgetStatePropertyAll(
          EdgeInsets.symmetric(
            horizontal: AppTokens.spacingLg,
            vertical: AppTokens.spacingXs,
          ),
        ),
        shape: WidgetStatePropertyAll(
          RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(AppTokens.radiusField),
            side: BorderSide(color: material.controlBorderColor),
          ),
        ),
        textStyle: WidgetStatePropertyAll(base.textTheme.bodyMedium),
        hintStyle: WidgetStatePropertyAll(
          base.textTheme.bodyMedium?.copyWith(color: shellColors.textTertiary),
        ),
      ),
      navigationBarTheme: NavigationBarThemeData(
        backgroundColor: material.bottomBarColor,
        indicatorColor: AppPalette.mistBlue.withValues(
          alpha: material.isGlass ? 0.12 : 0.18,
        ),
        shadowColor: material.shadowColor,
        labelTextStyle: WidgetStatePropertyAll(
          base.textTheme.labelMedium?.copyWith(
            fontWeight: isApplePlatform ? FontWeight.w500 : FontWeight.w600,
            color: shellColors.textSecondary,
          ),
        ),
      ),
      extensions: <ThemeExtension<dynamic>>[shellColors, material],
    );
  }

  static AppShellColors _shellColors(Brightness brightness) {
    if (brightness == Brightness.dark) {
      return const AppShellColors(
        bg0: AppPalette.darkBg0,
        bg1: AppPalette.darkBg1,
        bg2: AppPalette.darkBg2,
        bg3: AppPalette.darkBg3,
        bg4: AppPalette.darkBg4,
        bubbleIncoming: AppPalette.bubbleIncomingDark,
        bubbleOutgoing: AppPalette.bubbleOutgoingDark,
        fileSurface: AppPalette.fileSurfaceDark,
        textPrimary: AppPalette.textOnDark,
        textSecondary: AppPalette.textSecondaryOnDark,
        textTertiary: AppPalette.textMutedOnDark,
        textInverse: AppPalette.textInverse,
        divider: AppPalette.dividerDark,
      );
    }

    return const AppShellColors(
      bg0: AppPalette.lightBg0,
      bg1: AppPalette.lightBg1,
      bg2: AppPalette.lightBg2,
      bg3: AppPalette.lightBg3,
      bg4: AppPalette.lightBg4,
      bubbleIncoming: AppPalette.bubbleIncomingLight,
      bubbleOutgoing: AppPalette.bubbleOutgoingLight,
      fileSurface: AppPalette.fileSurfaceLight,
      textPrimary: AppPalette.textOnLight,
      textSecondary: AppPalette.textSecondaryOnLight,
      textTertiary: AppPalette.textMutedOnLight,
      textInverse: AppPalette.textInverse,
      divider: AppPalette.dividerLight,
    );
  }

  static AppShellMaterial _materialLayer({
    required Brightness brightness,
    required AppShellColors colors,
    required VisualTier visualTier,
  }) {
    final isGlass = visualTier == VisualTier.enhancedGlass;
    final targetPlatform = defaultTargetPlatform;
    final isAppleGlass = isGlass && _isApplePlatform(targetPlatform);
    final isDesktopQuiet =
        isGlass &&
        (targetPlatform == TargetPlatform.windows ||
            targetPlatform == TargetPlatform.linux);
    final chromeTone = switch ((brightness, isAppleGlass, isDesktopQuiet)) {
      (Brightness.dark, true, _) => Color.lerp(colors.bg1, Colors.white, 0.30)!,
      (Brightness.dark, _, true) => Color.lerp(
        colors.bg1,
        AppPalette.silverBlue,
        0.08,
      )!,
      (Brightness.dark, _, _) => Color.lerp(
        colors.bg1,
        AppPalette.mistBlue,
        0.08,
      )!,
      (Brightness.light, true, _) => Color.lerp(
        colors.bg1,
        Colors.white,
        0.18,
      )!,
      (Brightness.light, _, true) => Color.lerp(
        colors.bg1,
        AppPalette.silverBlue,
        0.08,
      )!,
      (Brightness.light, _, _) => Color.lerp(
        colors.bg1,
        AppPalette.mistBlue,
        0.08,
      )!,
    };
    final controlTone = switch ((brightness, isAppleGlass, isDesktopQuiet)) {
      (Brightness.dark, true, _) => Color.lerp(colors.bg2, Colors.white, 0.20)!,
      (Brightness.dark, _, true) => Color.lerp(
        colors.bg2,
        AppPalette.silverBlue,
        0.05,
      )!,
      (Brightness.dark, _, _) => Color.lerp(
        colors.bg2,
        AppPalette.mistBlue,
        0.05,
      )!,
      (Brightness.light, true, _) => Color.lerp(
        colors.bg1,
        Colors.white,
        0.12,
      )!,
      (Brightness.light, _, true) => Color.lerp(
        colors.bg1,
        AppPalette.silverBlue,
        0.05,
      )!,
      (Brightness.light, _, _) => Color.lerp(
        colors.bg1,
        AppPalette.mistBlue,
        0.05,
      )!,
    };
    final floatingTone = switch ((brightness, isAppleGlass, isDesktopQuiet)) {
      (Brightness.dark, true, _) => Color.lerp(colors.bg2, Colors.white, 0.18)!,
      (Brightness.dark, _, true) => Color.lerp(
        colors.bg2,
        AppPalette.silverBlue,
        0.04,
      )!,
      (Brightness.dark, _, _) => Color.lerp(
        colors.bg2,
        AppPalette.mistBlue,
        0.04,
      )!,
      (Brightness.light, true, _) => Color.lerp(
        colors.bg1,
        Colors.white,
        0.12,
      )!,
      (Brightness.light, _, true) => Color.lerp(
        colors.bg1,
        AppPalette.silverBlue,
        0.03,
      )!,
      (Brightness.light, _, _) => Color.lerp(
        colors.bg1,
        AppPalette.mistBlue,
        0.03,
      )!,
    };
    final surfaceColor = switch ((brightness, isGlass)) {
      (Brightness.dark, true) => colors.bg2,
      (Brightness.dark, false) => colors.bg2,
      (Brightness.light, true) => colors.bg1,
      (Brightness.light, false) => colors.bg1,
    };
    final topBarColor = switch ((brightness, isGlass)) {
      (Brightness.dark, true) => chromeTone.withValues(
        alpha: isAppleGlass ? 0.088 : (isDesktopQuiet ? 0.14 : 0.13),
      ),
      (Brightness.dark, false) => colors.bg1.withValues(alpha: 0.96),
      (Brightness.light, true) => chromeTone.withValues(
        alpha: isAppleGlass ? 0.104 : (isDesktopQuiet ? 0.17 : 0.16),
      ),
      (Brightness.light, false) => colors.bg1.withValues(alpha: 0.98),
    };
    final searchBarColor = switch ((brightness, isGlass)) {
      (Brightness.dark, true) => controlTone.withValues(
        alpha: isAppleGlass ? 0.070 : (isDesktopQuiet ? 0.12 : 0.11),
      ),
      (Brightness.dark, false) => colors.bg2,
      (Brightness.light, true) => controlTone.withValues(
        alpha: isAppleGlass ? 0.082 : (isDesktopQuiet ? 0.14 : 0.13),
      ),
      (Brightness.light, false) => colors.bg2,
    };
    final sideBarColor = switch ((brightness, isGlass)) {
      (Brightness.dark, true) => chromeTone.withValues(
        alpha: isAppleGlass
            ? 0.70
            : (isDesktopQuiet ? AppTokens.desktopMicaAlpha : 0.86),
      ),
      (Brightness.dark, false) => colors.bg1,
      (Brightness.light, true) => chromeTone.withValues(
        alpha: isAppleGlass
            ? 0.82
            : (isDesktopQuiet ? AppTokens.desktopMicaAlpha + 0.06 : 0.92),
      ),
      (Brightness.light, false) => colors.bg1,
    };
    final bottomBarColor = switch ((brightness, isGlass)) {
      (Brightness.dark, true) => chromeTone.withValues(
        alpha: isAppleGlass ? 0.115 : (isDesktopQuiet ? 0.15 : 0.14),
      ),
      (Brightness.dark, false) => colors.bg1.withValues(alpha: 0.98),
      (Brightness.light, true) => chromeTone.withValues(
        alpha: isAppleGlass ? 0.108 : (isDesktopQuiet ? 0.18 : 0.17),
      ),
      (Brightness.light, false) => colors.bg1.withValues(alpha: 0.98),
    };
    final floatingSurfaceColor = switch ((brightness, isGlass)) {
      (Brightness.dark, true) => floatingTone.withValues(
        alpha: isAppleGlass ? 0.67 : AppTokens.contentSurfaceAlpha,
      ),
      (Brightness.dark, false) => colors.bg2,
      (Brightness.light, true) => floatingTone.withValues(
        alpha: isAppleGlass ? 0.81 : 0.97,
      ),
      (Brightness.light, false) => colors.bg1,
    };
    final floatingBorderColor = isGlass
        ? colors.divider.withValues(
            alpha: brightness == Brightness.dark
                ? (isAppleGlass ? 0.068 : (isDesktopQuiet ? 0.12 : 0.11))
                : (isAppleGlass ? 0.09 : (isDesktopQuiet ? 0.16 : 0.14)),
          )
        : colors.divider.withValues(
            alpha: brightness == Brightness.dark ? 0.82 : 0.92,
          );
    final shadowColor = Colors.black.withValues(
      alpha: brightness == Brightness.dark
          ? (isGlass
                ? (isAppleGlass ? 0.008 : (isDesktopQuiet ? 0.032 : 0.030))
                : 0.24)
          : (isGlass
                ? (isAppleGlass ? 0.004 : (isDesktopQuiet ? 0.026 : 0.020))
                : 0.08),
    );
    final chromeSurfaceColor = switch ((brightness, isGlass)) {
      (Brightness.dark, true) => chromeTone.withValues(
        alpha: isAppleGlass ? 0.070 : (isDesktopQuiet ? 0.14 : 0.13),
      ),
      (Brightness.dark, false) => colors.bg2,
      (Brightness.light, true) => chromeTone.withValues(
        alpha: isAppleGlass ? 0.092 : (isDesktopQuiet ? 0.17 : 0.16),
      ),
      (Brightness.light, false) => colors.bg1,
    };
    final chromeBorderColor = isGlass
        ? colors.divider.withValues(
            alpha: brightness == Brightness.dark
                ? (isAppleGlass ? 0.104 : (isDesktopQuiet ? 0.12 : 0.10))
                : (isAppleGlass ? 0.088 : (isDesktopQuiet ? 0.15 : 0.13)),
          )
        : colors.divider.withValues(
            alpha: brightness == Brightness.dark ? 0.82 : 0.92,
          );
    final controlSurfaceColor = switch ((brightness, isGlass)) {
      (Brightness.dark, true) => controlTone.withValues(
        alpha: isAppleGlass ? 0.060 : (isDesktopQuiet ? 0.12 : 0.11),
      ),
      (Brightness.dark, false) => colors.bg2,
      (Brightness.light, true) => controlTone.withValues(
        alpha: isAppleGlass ? 0.072 : (isDesktopQuiet ? 0.14 : 0.13),
      ),
      (Brightness.light, false) => colors.bg2,
    };
    final controlBorderColor = isGlass
        ? colors.divider.withValues(
            alpha: brightness == Brightness.dark
                ? (isAppleGlass ? 0.096 : (isDesktopQuiet ? 0.11 : 0.09))
                : (isAppleGlass ? 0.082 : (isDesktopQuiet ? 0.14 : 0.12)),
          )
        : colors.divider.withValues(
            alpha: brightness == Brightness.dark ? 0.82 : 0.92,
          );

    return AppShellMaterial(
      tier: visualTier,
      surfaceColor: surfaceColor,
      borderColor: floatingBorderColor,
      shadowColor: shadowColor,
      blurSigma: isGlass
          ? (isAppleGlass ? 12.5 : (isDesktopQuiet ? 7.0 : 8.5))
          : 0,
      topBarColor: topBarColor,
      searchBarColor: searchBarColor,
      sideBarColor: sideBarColor,
      bottomBarColor: bottomBarColor,
      floatingSurfaceColor: floatingSurfaceColor,
      floatingBorderColor: floatingBorderColor,
      chromeSurfaceColor: chromeSurfaceColor,
      chromeBorderColor: chromeBorderColor,
      controlSurfaceColor: controlSurfaceColor,
      controlBorderColor: controlBorderColor,
    );
  }

  static TextTheme _textTheme(
    Color primary,
    Color secondary,
    Color tertiary, {
    required TargetPlatform platform,
  }) {
    final isApplePlatform = _isApplePlatform(platform);
    final fontFamily = _fontFamilyFor(platform);
    final fontFamilyFallback = _fontFamilyFallback(platform);
    final headingWeight = isApplePlatform ? FontWeight.w500 : FontWeight.w600;
    final labelLargeWeight = isApplePlatform
        ? FontWeight.w500
        : FontWeight.w600;
    final labelMediumWeight = isApplePlatform
        ? FontWeight.w400
        : FontWeight.w500;
    final labelSmallWeight = isApplePlatform
        ? FontWeight.w400
        : FontWeight.w500;

    return TextTheme(
      headlineLarge: TextStyle(
        fontFamily: fontFamily,
        fontFamilyFallback: fontFamilyFallback,
        fontWeight: headingWeight,
        fontSize: 28,
        height: 1.2,
        color: primary,
      ),
      headlineMedium: TextStyle(
        fontFamily: fontFamily,
        fontFamilyFallback: fontFamilyFallback,
        fontWeight: headingWeight,
        fontSize: 24,
        height: 1.25,
        color: primary,
      ),
      titleLarge: TextStyle(
        fontFamily: fontFamily,
        fontFamilyFallback: fontFamilyFallback,
        fontWeight: headingWeight,
        fontSize: 20,
        height: 1.25,
        color: primary,
      ),
      titleMedium: TextStyle(
        fontFamily: fontFamily,
        fontFamilyFallback: fontFamilyFallback,
        fontWeight: headingWeight,
        fontSize: 16,
        height: 1.3,
        color: primary,
      ),
      bodyLarge: TextStyle(
        fontFamily: fontFamily,
        fontSize: 15,
        height: 1.45,
        color: primary,
        fontFamilyFallback: fontFamilyFallback,
      ),
      bodyMedium: TextStyle(
        fontFamily: fontFamily,
        fontSize: 14,
        height: 1.4,
        color: secondary,
        fontFamilyFallback: fontFamilyFallback,
      ),
      bodySmall: TextStyle(
        fontFamily: fontFamily,
        fontSize: 12,
        height: 1.35,
        color: tertiary,
        fontFamilyFallback: fontFamilyFallback,
      ),
      labelLarge: TextStyle(
        fontFamily: fontFamily,
        fontWeight: labelLargeWeight,
        fontSize: 14,
        color: secondary,
        fontFamilyFallback: fontFamilyFallback,
      ),
      labelMedium: TextStyle(
        fontFamily: fontFamily,
        fontWeight: labelMediumWeight,
        fontSize: 12,
        color: tertiary,
        fontFamilyFallback: fontFamilyFallback,
      ),
      labelSmall: TextStyle(
        fontFamily: fontFamily,
        fontWeight: labelSmallWeight,
        fontSize: 11,
        height: 1.25,
        color: tertiary,
        fontFamilyFallback: fontFamilyFallback,
      ),
    );
  }

  static bool _isApplePlatform(TargetPlatform platform) {
    return platform == TargetPlatform.iOS || platform == TargetPlatform.macOS;
  }

  static String? _fontFamilyFor(TargetPlatform platform) {
    return 'SourceHanSansSC';
  }

  static List<String> _fontFamilyFallback(TargetPlatform platform) {
    return _isApplePlatform(platform)
        ? const <String>[
            'PingFang SC',
            'Heiti SC',
            'SourceHanSansSC',
            'SourceSans3',
          ]
        : const <String>['SourceSans3'];
  }
}

class AppShellColors extends ThemeExtension<AppShellColors> {
  const AppShellColors({
    required this.bg0,
    required this.bg1,
    required this.bg2,
    required this.bg3,
    required this.bg4,
    required this.bubbleIncoming,
    required this.bubbleOutgoing,
    required this.fileSurface,
    required this.textPrimary,
    required this.textSecondary,
    required this.textTertiary,
    required this.textInverse,
    required this.divider,
  }) : surfaceVariant = bg2;

  final Color bg0;
  final Color bg1;
  final Color bg2;
  final Color bg3;
  final Color bg4;
  final Color surfaceVariant;
  final Color bubbleIncoming;
  final Color bubbleOutgoing;
  final Color fileSurface;
  final Color textPrimary;
  final Color textSecondary;
  final Color textTertiary;
  final Color textInverse;
  final Color divider;

  @override
  AppShellColors copyWith({
    Color? bg0,
    Color? bg1,
    Color? bg2,
    Color? bg3,
    Color? bg4,
    Color? surfaceVariant,
    Color? bubbleIncoming,
    Color? bubbleOutgoing,
    Color? fileSurface,
    Color? textPrimary,
    Color? textSecondary,
    Color? textTertiary,
    Color? textInverse,
    Color? divider,
  }) {
    return AppShellColors(
      bg0: bg0 ?? this.bg0,
      bg1: bg1 ?? this.bg1,
      bg2: surfaceVariant ?? bg2 ?? this.bg2,
      bg3: bg3 ?? this.bg3,
      bg4: bg4 ?? this.bg4,
      bubbleIncoming: bubbleIncoming ?? this.bubbleIncoming,
      bubbleOutgoing: bubbleOutgoing ?? this.bubbleOutgoing,
      fileSurface: fileSurface ?? this.fileSurface,
      textPrimary: textPrimary ?? this.textPrimary,
      textSecondary: textSecondary ?? this.textSecondary,
      textTertiary: textTertiary ?? this.textTertiary,
      textInverse: textInverse ?? this.textInverse,
      divider: divider ?? this.divider,
    );
  }

  @override
  AppShellColors lerp(ThemeExtension<AppShellColors>? other, double t) {
    if (other is! AppShellColors) {
      return this;
    }
    return AppShellColors(
      bg0: Color.lerp(bg0, other.bg0, t)!,
      bg1: Color.lerp(bg1, other.bg1, t)!,
      bg2: Color.lerp(surfaceVariant, other.surfaceVariant, t)!,
      bg3: Color.lerp(bg3, other.bg3, t)!,
      bg4: Color.lerp(bg4, other.bg4, t)!,
      bubbleIncoming: Color.lerp(bubbleIncoming, other.bubbleIncoming, t)!,
      bubbleOutgoing: Color.lerp(bubbleOutgoing, other.bubbleOutgoing, t)!,
      fileSurface: Color.lerp(fileSurface, other.fileSurface, t)!,
      textPrimary: Color.lerp(textPrimary, other.textPrimary, t)!,
      textSecondary: Color.lerp(textSecondary, other.textSecondary, t)!,
      textTertiary: Color.lerp(textTertiary, other.textTertiary, t)!,
      textInverse: Color.lerp(textInverse, other.textInverse, t)!,
      divider: Color.lerp(divider, other.divider, t)!,
    );
  }
}

class AppShellMaterial extends ThemeExtension<AppShellMaterial> {
  const AppShellMaterial({
    required this.tier,
    required this.surfaceColor,
    required this.borderColor,
    required this.shadowColor,
    required this.blurSigma,
    required this.topBarColor,
    required this.searchBarColor,
    required this.sideBarColor,
    required this.bottomBarColor,
    required this.floatingSurfaceColor,
    required this.floatingBorderColor,
    required this.chromeSurfaceColor,
    required this.chromeBorderColor,
    required this.controlSurfaceColor,
    required this.controlBorderColor,
  });

  final VisualTier tier;
  final Color surfaceColor;
  final Color borderColor;
  final Color shadowColor;
  final double blurSigma;
  final Color topBarColor;
  final Color searchBarColor;
  final Color sideBarColor;
  final Color bottomBarColor;
  final Color floatingSurfaceColor;
  final Color floatingBorderColor;
  final Color chromeSurfaceColor;
  final Color chromeBorderColor;
  final Color controlSurfaceColor;
  final Color controlBorderColor;

  bool get isGlass => tier == VisualTier.enhancedGlass;

  @override
  AppShellMaterial copyWith({
    VisualTier? tier,
    Color? surfaceColor,
    Color? borderColor,
    Color? shadowColor,
    double? blurSigma,
    Color? topBarColor,
    Color? searchBarColor,
    Color? sideBarColor,
    Color? bottomBarColor,
    Color? floatingSurfaceColor,
    Color? floatingBorderColor,
    Color? chromeSurfaceColor,
    Color? chromeBorderColor,
    Color? controlSurfaceColor,
    Color? controlBorderColor,
  }) {
    return AppShellMaterial(
      tier: tier ?? this.tier,
      surfaceColor: surfaceColor ?? this.surfaceColor,
      borderColor: borderColor ?? this.borderColor,
      shadowColor: shadowColor ?? this.shadowColor,
      blurSigma: blurSigma ?? this.blurSigma,
      topBarColor: topBarColor ?? this.topBarColor,
      searchBarColor: searchBarColor ?? this.searchBarColor,
      sideBarColor: sideBarColor ?? this.sideBarColor,
      bottomBarColor: bottomBarColor ?? this.bottomBarColor,
      floatingSurfaceColor: floatingSurfaceColor ?? this.floatingSurfaceColor,
      floatingBorderColor: floatingBorderColor ?? this.floatingBorderColor,
      chromeSurfaceColor: chromeSurfaceColor ?? this.chromeSurfaceColor,
      chromeBorderColor: chromeBorderColor ?? this.chromeBorderColor,
      controlSurfaceColor: controlSurfaceColor ?? this.controlSurfaceColor,
      controlBorderColor: controlBorderColor ?? this.controlBorderColor,
    );
  }

  @override
  AppShellMaterial lerp(ThemeExtension<AppShellMaterial>? other, double t) {
    if (other is! AppShellMaterial) {
      return this;
    }
    return AppShellMaterial(
      tier: t < 0.5 ? tier : other.tier,
      surfaceColor: Color.lerp(surfaceColor, other.surfaceColor, t)!,
      borderColor: Color.lerp(borderColor, other.borderColor, t)!,
      shadowColor: Color.lerp(shadowColor, other.shadowColor, t)!,
      blurSigma: lerpDouble(blurSigma, other.blurSigma, t)!,
      topBarColor: Color.lerp(topBarColor, other.topBarColor, t)!,
      searchBarColor: Color.lerp(searchBarColor, other.searchBarColor, t)!,
      sideBarColor: Color.lerp(sideBarColor, other.sideBarColor, t)!,
      bottomBarColor: Color.lerp(bottomBarColor, other.bottomBarColor, t)!,
      floatingSurfaceColor: Color.lerp(
        floatingSurfaceColor,
        other.floatingSurfaceColor,
        t,
      )!,
      floatingBorderColor: Color.lerp(
        floatingBorderColor,
        other.floatingBorderColor,
        t,
      )!,
      chromeSurfaceColor: Color.lerp(
        chromeSurfaceColor,
        other.chromeSurfaceColor,
        t,
      )!,
      chromeBorderColor: Color.lerp(
        chromeBorderColor,
        other.chromeBorderColor,
        t,
      )!,
      controlSurfaceColor: Color.lerp(
        controlSurfaceColor,
        other.controlSurfaceColor,
        t,
      )!,
      controlBorderColor: Color.lerp(
        controlBorderColor,
        other.controlBorderColor,
        t,
      )!,
    );
  }
}
