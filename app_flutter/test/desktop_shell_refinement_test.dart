import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/application/chat_providers.dart';
import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'fakes/fake_sdk_client.dart';
import 'package:mi_e2ee_im_app/presentation/screens/desktop_shell_screen.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';
import 'package:mi_e2ee_im_app/presentation/theme/visual_tier.dart';
import 'package:mi_e2ee_im_app/presentation/widgets/shell_chrome.dart';

void main() {
  testWidgets('enhanced desktop glass is limited to side and detail panels', (
    WidgetTester tester,
  ) async {
    await _pumpDesktopShell(tester, enhanced: true);

    expect(find.byKey(const ValueKey('desktop-stage-canvas')), findsOneWidget);
    expect(find.byKey(const ValueKey('desktop-folder-strip')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('desktop-conversation-sidebar')),
      findsNothing,
    );

    await _openDesktopContextPanel(tester);

    final desktopPanels = find.byWidgetPredicate(
      (widget) =>
          widget is LiquidGlassPanel &&
          widget.role == ShellChromeRole.desktopPanel,
    );

    expect(desktopPanels, findsAtLeastNWidgets(2));
    expect(
      find.byKey(const ValueKey('desktop-conversation-sidebar')),
      findsOneWidget,
    );
  });

  testWidgets(
    'desktop detail panel exposes media files links and notifications',
    (WidgetTester tester) async {
      await _pumpDesktopShell(tester);

      await tester.tap(find.text('Aurora 设计群').first);
      await tester.pumpAndSettle();
      await _openDesktopContextPanel(tester);

      final sidebar = find.byKey(
        const ValueKey('desktop-conversation-sidebar'),
      );

      expect(sidebar, findsOneWidget);
      expect(
        find.descendant(of: sidebar, matching: find.text('成员')),
        findsAtLeastNWidgets(1),
      );
      expect(
        find.descendant(of: sidebar, matching: find.text('媒体')),
        findsAtLeastNWidgets(1),
      );
      expect(
        find.descendant(
          of: sidebar,
          matching: find.byKey(const ValueKey('desktop-context-media-grid')),
        ),
        findsOneWidget,
      );
      expect(
        find.descendant(of: sidebar, matching: find.text('文件')),
        findsAtLeastNWidgets(1),
      );
      expect(
        find.descendant(of: sidebar, matching: find.text('链接')),
        findsAtLeastNWidgets(1),
      );
      expect(
        find.descendant(of: sidebar, matching: find.text('通知设置')),
        findsOneWidget,
      );

      await tester.tap(find.byKey(const ValueKey('desktop-context-tab-成员')));
      await tester.pumpAndSettle();
      expect(
        find.descendant(
          of: sidebar,
          matching: find.byKey(const ValueKey('desktop-context-member-list')),
        ),
        findsOneWidget,
      );
      expect(
        find.descendant(
          of: sidebar,
          matching: find.byKey(const ValueKey('desktop-context-member-row-0')),
        ),
        findsOneWidget,
      );

      await tester.tap(find.byKey(const ValueKey('desktop-context-tab-媒体')));
      await tester.pumpAndSettle();
      expect(
        find.descendant(
          of: sidebar,
          matching: find.byKey(const ValueKey('desktop-context-media-grid')),
        ),
        findsOneWidget,
      );

      await tester.tap(find.byKey(const ValueKey('desktop-context-tab-文件')));
      await tester.pumpAndSettle();
      expect(
        find.descendant(
          of: sidebar,
          matching: find.byKey(const ValueKey('desktop-context-file-row-0')),
        ),
        findsOneWidget,
      );

      await tester.tap(find.byKey(const ValueKey('desktop-context-tab-链接')));
      await tester.pumpAndSettle();
      expect(
        find.descendant(of: sidebar, matching: find.text('暂无共享链接。')),
        findsOneWidget,
      );
    },
  );

  testWidgets('desktop context panel opens from more action and can close', (
    WidgetTester tester,
  ) async {
    await _pumpDesktopShell(tester);

    expect(
      find.byKey(const ValueKey('desktop-conversation-sidebar')),
      findsNothing,
    );

    await _openDesktopContextPanel(tester);

    expect(
      find.byKey(const ValueKey('desktop-conversation-sidebar')),
      findsOneWidget,
    );

    await tester.tap(find.byKey(const ValueKey('desktop-context-panel-close')));
    await tester.pumpAndSettle();

    expect(
      find.byKey(const ValueKey('desktop-conversation-sidebar')),
      findsNothing,
    );
    expect(
      find.byKey(const ValueKey('desktop-context-panel-reopen')),
      findsNothing,
    );
    expect(find.byKey(const ValueKey('desktop-stage-canvas')), findsOneWidget);
  });
}

Future<void> _pumpDesktopShell(
  WidgetTester tester, {
  bool enhanced = false,
}) async {
  final fakeClient = FakeSdkClient();
  await fakeClient.initialize();
  addTearDown(fakeClient.dispose);

  tester.view.physicalSize = const Size(1440, 960);
  tester.view.devicePixelRatio = 1.0;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);

  await tester.pumpWidget(
    ProviderScope(
      overrides: [sdkClientProvider.overrideWith((_) => fakeClient)],
      child: MaterialApp(
        theme: AppTheme.light(
          visualTier: enhanced ? VisualTier.enhancedGlass : VisualTier.standard,
        ),
        home: const DesktopShellScreen(section: AppSection.chats),
      ),
    ),
  );
  await tester.pumpAndSettle();
}

Future<void> _openDesktopContextPanel(WidgetTester tester) async {
  await tester.tap(find.byKey(const ValueKey('desktop-stage-more-action')));
  await tester.pumpAndSettle();
  expect(
    find.byKey(const ValueKey('desktop-stage-menu-profile')),
    findsOneWidget,
  );
  await tester.tap(find.byKey(const ValueKey('desktop-stage-menu-profile')));
  await tester.pumpAndSettle();
}
