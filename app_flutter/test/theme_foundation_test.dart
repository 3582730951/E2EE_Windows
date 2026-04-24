import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_tokens.dart';
import 'package:mi_e2ee_im_app/presentation/theme/visual_tier.dart';

void main() {
  test('tokens expose additive spacing radius and size aliases', () {
    expect(AppTokens.space2, 2);
    expect(AppTokens.space4, AppTokens.spacingXs);
    expect(AppTokens.space8, AppTokens.spacingSm);
    expect(AppTokens.space12, AppTokens.spacingMd);
    expect(AppTokens.space16, AppTokens.spacingLg);
    expect(AppTokens.space24, AppTokens.spacingXl);
    expect(AppTokens.space32, AppTokens.spacingXxl);

    expect(AppTokens.radiusXl, AppTokens.radiusCard);
    expect(AppTokens.radiusLg, AppTokens.radiusBubble);
    expect(AppTokens.radiusField, 18);

    expect(AppTokens.iconSm, lessThan(AppTokens.iconMd));
    expect(AppTokens.iconMd, lessThan(AppTokens.iconLg));
    expect(AppTokens.avatarMd, lessThan(AppTokens.avatarLg));

    expect(AppPalette.brandBlue, isNot(AppPalette.socialGreen));
    expect(AppPalette.mentionRed, isNot(AppPalette.channelCyan));
    expect(AppTokens.chromeGlassAlpha, greaterThan(0));
    expect(AppTokens.composerGlassAlpha, greaterThan(0));
    expect(AppTokens.desktopMicaAlpha, lessThan(AppTokens.contentSurfaceAlpha));
  });

  test('IM metrics pin chat density and desktop panel constraints', () {
    expect(ImUiMetrics.androidChatListTop, 48);
    expect(ImUiMetrics.androidChatDetailChromeHeight, 44);
    expect(ImUiMetrics.androidChatDetailTopFade, lessThanOrEqualTo(52));
    expect(ImUiMetrics.androidChatDetailBoundaryMask, lessThanOrEqualTo(16));
    expect(ImUiMetrics.androidConversationRowHeight, inInclusiveRange(72, 76));
    expect(ImUiMetrics.iosConversationRowHeight, inInclusiveRange(70, 74));
    expect(ImUiMetrics.mobileBottomNavVisibleMax, lessThanOrEqualTo(60));
    expect(ImUiMetrics.desktopLeftRailCompact, inInclusiveRange(304, 320));
    expect(ImUiMetrics.desktopLeftRailWide, inInclusiveRange(304, 320));
    expect(ImUiMetrics.desktopRightPanel, inInclusiveRange(320, 340));
    expect(ImUiMetrics.desktopOutgoingRightInset, inInclusiveRange(5, 8));
  });

  test(
    'light and dark themes expose layered shell colors and text hierarchy',
    () {
      final light = AppTheme.light();
      final dark = AppTheme.dark();

      final lightColors = light.extension<AppShellColors>()!;
      final darkColors = dark.extension<AppShellColors>()!;

      expect(light.scaffoldBackgroundColor, lightColors.bg0);
      expect(dark.scaffoldBackgroundColor, darkColors.bg0);

      expect(lightColors.bg0, isNot(lightColors.bg1));
      expect(lightColors.bg1, isNot(lightColors.bg2));
      expect(lightColors.bg2, isNot(lightColors.bg3));
      expect(lightColors.bg3, isNot(lightColors.bg4));
      expect(darkColors.bg0, isNot(darkColors.bg3));

      expect(lightColors.surfaceVariant, lightColors.bg2);
      expect(darkColors.surfaceVariant, darkColors.bg2);

      expect(lightColors.textPrimary, light.textTheme.bodyLarge?.color);
      expect(lightColors.textSecondary, light.textTheme.bodyMedium?.color);
      expect(lightColors.textTertiary, light.textTheme.bodySmall?.color);
      expect(darkColors.textPrimary, dark.textTheme.bodyLarge?.color);
      expect(darkColors.textSecondary, dark.textTheme.bodyMedium?.color);
      expect(darkColors.textTertiary, dark.textTheme.bodySmall?.color);
      expect(
        light.dialogTheme.backgroundColor,
        light.extension<AppShellMaterial>()!.floatingSurfaceColor,
      );
      expect(
        dark.dialogTheme.backgroundColor,
        dark.extension<AppShellMaterial>()!.floatingSurfaceColor,
      );
    },
  );

  test(
    'enhanced glass lifts chrome and floating layers while keeping body surfaces solid',
    () {
      final standard = AppTheme.dark(visualTier: VisualTier.standard);
      final glass = AppTheme.dark(visualTier: VisualTier.enhancedGlass);

      final standardColors = standard.extension<AppShellColors>()!;
      final glassColors = glass.extension<AppShellColors>()!;
      final standardMaterial = standard.extension<AppShellMaterial>()!;
      final glassMaterial = glass.extension<AppShellMaterial>()!;

      expect(glassMaterial.isGlass, isTrue);
      expect(standardMaterial.isGlass, isFalse);
      expect(glassMaterial.blurSigma, greaterThan(standardMaterial.blurSigma));

      expect(glassMaterial.topBarColor, isNot(standardMaterial.topBarColor));
      expect(
        glassMaterial.searchBarColor,
        isNot(standardMaterial.searchBarColor),
      );
      expect(glassMaterial.sideBarColor, isNot(standardMaterial.sideBarColor));
      expect(
        glassMaterial.bottomBarColor,
        isNot(standardMaterial.bottomBarColor),
      );
      expect(
        glassMaterial.floatingSurfaceColor,
        isNot(standardMaterial.floatingSurfaceColor),
      );

      expect(glassColors.bg0, standardColors.bg0);
      expect(glassColors.bg1, standardColors.bg1);
      expect(glassColors.bg2, standardColors.bg2);
      expect(glassColors.bubbleIncoming, standardColors.bubbleIncoming);
      expect(glassColors.bubbleOutgoing, standardColors.bubbleOutgoing);
      expect(glassColors.fileSurface, standardColors.fileSurface);
    },
  );
}
