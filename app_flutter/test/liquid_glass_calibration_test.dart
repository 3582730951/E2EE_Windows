import 'package:flutter/cupertino.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/application/chat_providers.dart';
import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'package:mi_e2ee_im_app/native_sdk/fake_sdk_client.dart';
import 'package:mi_e2ee_im_app/presentation/screens/mobile_shell_screen.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';
import 'package:mi_e2ee_im_app/presentation/theme/visual_tier.dart';
import 'package:mi_e2ee_im_app/presentation/widgets/shell_chrome.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('dark enhanced glass keeps iOS chrome thinner than Android chrome', () {
    final iosMaterial = _themeForPlatform(
      TargetPlatform.iOS,
    ).extension<AppShellMaterial>()!;
    final androidMaterial = _themeForPlatform(
      TargetPlatform.android,
    ).extension<AppShellMaterial>()!;
    final windowsMaterial = _themeForPlatform(
      TargetPlatform.windows,
    ).extension<AppShellMaterial>()!;

    expect(
      _normalizedAlpha(iosMaterial.topBarColor),
      inInclusiveRange(0.08, 0.10),
    );
    expect(
      _normalizedAlpha(iosMaterial.searchBarColor),
      inInclusiveRange(0.06, 0.08),
    );
    expect(
      _normalizedAlpha(iosMaterial.bottomBarColor),
      inInclusiveRange(0.10, 0.12),
    );
    expect(
      _normalizedAlpha(iosMaterial.chromeBorderColor),
      inInclusiveRange(0.09, 0.11),
    );
    expect(
      _normalizedAlpha(iosMaterial.controlBorderColor),
      inInclusiveRange(0.09, 0.10),
    );
    expect(
      _normalizedAlpha(androidMaterial.topBarColor),
      greaterThan(_normalizedAlpha(iosMaterial.topBarColor)),
    );
    expect(
      _normalizedAlpha(androidMaterial.bottomBarColor),
      greaterThan(_normalizedAlpha(iosMaterial.bottomBarColor)),
    );
    expect(
      _normalizedAlpha(windowsMaterial.bottomBarColor),
      greaterThanOrEqualTo(_normalizedAlpha(androidMaterial.bottomBarColor)),
    );
    expect(iosMaterial.blurSigma, inInclusiveRange(12.0, 13.0));
    expect(iosMaterial.blurSigma, greaterThan(androidMaterial.blurSigma));
    expect(
      androidMaterial.blurSigma,
      greaterThanOrEqualTo(windowsMaterial.blurSigma),
    );
  });

  test('apple platform typography is lighter than android typography', () {
    final iosTheme = _themeForPlatform(TargetPlatform.iOS);
    final androidTheme = _themeForPlatform(TargetPlatform.android);

    expect(iosTheme.textTheme.titleLarge?.fontFamily, 'SourceHanSansSC');
    expect(iosTheme.textTheme.titleLarge?.fontWeight, FontWeight.w500);
    expect(iosTheme.textTheme.labelMedium?.fontWeight, FontWeight.w400);
    expect(iosTheme.textTheme.labelSmall?.fontWeight, FontWeight.w400);

    expect(androidTheme.textTheme.titleLarge?.fontFamily, 'SourceHanSansSC');
    expect(androidTheme.textTheme.titleLarge?.fontWeight, FontWeight.w600);
    expect(androidTheme.textTheme.labelMedium?.fontWeight, FontWeight.w500);
    expect(androidTheme.textTheme.labelSmall?.fontWeight, FontWeight.w500);
  });

  test(
    'liquid glass profile favors iOS navigation blur over glossy overlays',
    () {
      final material = _themeForPlatform(
        TargetPlatform.iOS,
      ).extension<AppShellMaterial>()!;
      final androidMaterial = _themeForPlatform(
        TargetPlatform.android,
      ).extension<AppShellMaterial>()!;

      for (final role in <ShellChromeRole>[
        ShellChromeRole.topBar,
        ShellChromeRole.searchField,
        ShellChromeRole.bottomBar,
      ]) {
        final iosProfile = resolveLiquidGlassStyleProfile(
          material: material,
          role: role,
          targetPlatform: TargetPlatform.iOS,
          enabled: true,
          shadowOpacity: 0.12,
        );
        final androidProfile = resolveLiquidGlassStyleProfile(
          material: androidMaterial,
          role: role,
          targetPlatform: TargetPlatform.android,
          enabled: true,
          shadowOpacity: 0.12,
        );

        expect(
          iosProfile.panelBlurSigma,
          greaterThan(androidProfile.panelBlurSigma),
        );
        expect(iosProfile.shadowAlpha, lessThan(androidProfile.shadowAlpha));
        expect(
          iosProfile.topEdgeAlpha,
          greaterThan(androidProfile.topEdgeAlpha),
        );
        final expectedTopEdgeRange = switch (role) {
          ShellChromeRole.bottomBar => inInclusiveRange(0.037, 0.039),
          ShellChromeRole.searchField => inInclusiveRange(0.017, 0.019),
          _ => inInclusiveRange(0.047, 0.049),
        };
        expect(iosProfile.topEdgeAlpha, expectedTopEdgeRange);
        final expectedHighlightCeiling = switch (role) {
          ShellChromeRole.bottomBar => 0.014,
          ShellChromeRole.searchField => 0.004,
          _ => 0.018,
        };
        expect(
          iosProfile.radialHighlightAlpha,
          lessThanOrEqualTo(expectedHighlightCeiling),
        );
        expect(
          iosProfile.innerStrokeAlpha,
          greaterThanOrEqualTo(androidProfile.innerStrokeAlpha),
        );
        final expectedInnerStrokeRange = switch (role) {
          ShellChromeRole.bottomBar => inInclusiveRange(0.017, 0.019),
          ShellChromeRole.searchField => inInclusiveRange(0.009, 0.011),
          _ => inInclusiveRange(0.019, 0.021),
        };
        expect(iosProfile.innerStrokeAlpha, expectedInnerStrokeRange);
        expect(iosProfile.usesBottomGlow, isFalse);
      }
    },
  );

  test('liquid glass roles stay scoped to chrome surfaces', () {
    expect(
      ShellChromeRole.values,
      containsAll(<ShellChromeRole>[
        ShellChromeRole.topBar,
        ShellChromeRole.bottomBar,
        ShellChromeRole.searchField,
        ShellChromeRole.controlCluster,
        ShellChromeRole.composerDock,
        ShellChromeRole.desktopPanel,
      ]),
    );
    expect(
      ShellChromeRole.values.map((role) => role.name),
      isNot(contains('messageBubble')),
    );
    expect(
      ShellChromeRole.values.map((role) => role.name),
      isNot(contains('attachmentCard')),
    );
    expect(
      ImSurfacePolicy.allowsGlass(
        platform: TargetPlatform.iOS,
        role: ShellChromeRole.searchField,
      ),
      isFalse,
    );
    expect(
      ImSurfacePolicy.allowsGlass(
        platform: TargetPlatform.iOS,
        role: ShellChromeRole.composerDock,
      ),
      isFalse,
    );
    expect(
      ImSurfacePolicy.allowsGlass(
        platform: TargetPlatform.android,
        role: ShellChromeRole.bottomBar,
      ),
      isFalse,
    );
    expect(
      ImSurfacePolicy.allowsGlass(
        platform: TargetPlatform.windows,
        role: ShellChromeRole.desktopPanel,
      ),
      isTrue,
    );
  });

  testWidgets(
    'enhanced glass iOS shell adds dedicated navigation environment fields',
    (WidgetTester tester) async {
      await _pumpShell(tester, TargetPlatform.iOS);

      final bottomPanel = find.byWidgetPredicate(
        (widget) =>
            widget is LiquidGlassPanel &&
            widget.role == ShellChromeRole.bottomBar,
      );

      expect(bottomPanel, findsOneWidget);
      expect(find.byType(CupertinoTabBar), findsOneWidget);
      expect(
        find.byKey(const ValueKey('mobile-shell-environment-top-lens')),
        findsOneWidget,
      );
      expect(
        find.byKey(const ValueKey('mobile-shell-environment-middle')),
        findsNothing,
      );
      expect(
        find.byKey(const ValueKey('mobile-shell-environment-bottom-dock')),
        findsOneWidget,
      );
      expect(tester.getSize(bottomPanel).height, inInclusiveRange(74.0, 80.0));
      expect(
        tester
            .getSize(
              find.byKey(
                const ValueKey('mobile-shell-cupertino-tab-glyph-联系人-selected'),
              ),
            )
            .height,
        inInclusiveRange(52.0, 60.0),
      );
      final bottomPanelRect = tester.getRect(bottomPanel);
      final selectedGlyphRect = tester.getRect(
        find.byKey(
          const ValueKey('mobile-shell-cupertino-tab-glyph-联系人-selected'),
        ),
      );
      expect(
        (selectedGlyphRect.center.dy - bottomPanelRect.center.dy).abs(),
        lessThanOrEqualTo(3),
      );
      expect(
        tester.view.physicalSize.width - tester.getSize(bottomPanel).width,
        greaterThanOrEqualTo(28),
      );

      final panel = tester.widget<LiquidGlassPanel>(bottomPanel);
      expect(panel.shadowOpacity, lessThanOrEqualTo(0.045));
    },
  );

  testWidgets(
    'enhanced glass Android shell avoids iOS-specific environment fields',
    (WidgetTester tester) async {
      await _pumpShell(tester, TargetPlatform.android);

      expect(find.byType(NavigationBar), findsOneWidget);
      expect(
        find.byKey(const ValueKey('mobile-shell-environment-top-lens')),
        findsNothing,
      );
      expect(
        find.byKey(const ValueKey('mobile-shell-environment-middle')),
        findsNothing,
      );
      expect(
        find.byKey(const ValueKey('mobile-shell-environment-bottom-dock')),
        findsNothing,
      );
    },
  );
}

Future<void> _pumpShell(WidgetTester tester, TargetPlatform platform) async {
  final previousPlatform = debugDefaultTargetPlatformOverride;
  debugDefaultTargetPlatformOverride = platform;
  try {
    final fakeClient = FakeSdkClient();
    await fakeClient.initialize();
    addTearDown(fakeClient.dispose);

    tester.view.physicalSize = platform == TargetPlatform.iOS
        ? const Size(393, 852)
        : const Size(412, 915);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(
      ProviderScope(
        overrides: [sdkClientProvider.overrideWith((_) => fakeClient)],
        child: MaterialApp(
          theme: AppTheme.dark(visualTier: VisualTier.enhancedGlass),
          home: const MobileShellScreen(section: AppSection.contacts),
        ),
      ),
    );
    await tester.pumpAndSettle();
  } finally {
    debugDefaultTargetPlatformOverride = previousPlatform;
  }
}

double _normalizedAlpha(Color color) {
  return ((color.toARGB32() >> 24) & 0xFF) / 255.0;
}

ThemeData _themeForPlatform(TargetPlatform platform) {
  final previousPlatform = debugDefaultTargetPlatformOverride;
  debugDefaultTargetPlatformOverride = platform;
  try {
    return AppTheme.dark(visualTier: VisualTier.enhancedGlass);
  } finally {
    debugDefaultTargetPlatformOverride = previousPlatform;
  }
}
