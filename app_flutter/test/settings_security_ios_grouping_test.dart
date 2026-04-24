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

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  testWidgets(
    'keeps iOS settings on inset grouped rows and thin control chrome',
    (WidgetTester tester) async {
      await _pumpSettingsScreen(tester);

      final headerShell = find.byKey(
        const Key('settings-security-header-shell-cupertino'),
      );
      final navTitle = find.byKey(
        const Key('settings-security-nav-title-cupertino'),
      );
      final accountGroup = find.byKey(
        const Key('settings-security-group-account'),
      );
      final messagesGroup = find.byKey(
        const Key('settings-security-group-messages'),
      );
      final privacyGroup = find.byKey(
        const Key('settings-security-group-privacy'),
      );
      final devicesGroup = find.byKey(
        const Key('settings-security-group-devices'),
      );
      final visualControl = find.byKey(
        const ValueKey('visual-mode-segmented-control'),
      );
      final scrollable = find.byType(Scrollable).first;

      expect(headerShell, findsOneWidget);
      expect(navTitle, findsOneWidget);
      expect(find.text('设置'), findsWidgets);
      expect(find.text('隐私与安全'), findsOneWidget);
      expect(find.textContaining('待复核'), findsNothing);
      expect(find.textContaining('最近轮换'), findsNothing);
      expect(accountGroup, findsOneWidget);
      expect(messagesGroup, findsOneWidget);
      expect(privacyGroup, findsOneWidget);
      expect(visualControl, findsOneWidget);
      expect(
        find.descendant(of: headerShell, matching: find.byType(BackdropFilter)),
        findsNothing,
      );

      final accountLeft = tester.getTopLeft(accountGroup).dx;
      final messagesTop = tester.getTopLeft(messagesGroup).dy;
      await tester.dragUntilVisible(
        devicesGroup,
        scrollable,
        const Offset(0, -240),
      );
      await tester.pumpAndSettle();

      expect(devicesGroup, findsOneWidget);
      expect(tester.getSize(headerShell).height, lessThanOrEqualTo(52));
      expect(accountLeft, greaterThanOrEqualTo(12));
      expect(messagesTop, lessThan(tester.getTopLeft(devicesGroup).dy));
    },
  );
}

Future<void> _pumpSettingsScreen(WidgetTester tester) async {
  final previousPlatform = debugDefaultTargetPlatformOverride;
  debugDefaultTargetPlatformOverride = TargetPlatform.iOS;
  try {
    final fakeClient = FakeSdkClient();
    await fakeClient.initialize();
    addTearDown(fakeClient.dispose);

    tester.view.physicalSize = const Size(393, 852);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(
      ProviderScope(
        overrides: [sdkClientProvider.overrideWith((_) => fakeClient)],
        child: MaterialApp(
          theme: AppTheme.light(visualTier: VisualTier.enhancedGlass),
          home: const Scaffold(
            body: MobileShellScreen(section: AppSection.settings),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();
  } finally {
    debugDefaultTargetPlatformOverride = previousPlatform;
  }
}
