import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/application/chat_providers.dart';
import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'fakes/fake_sdk_client.dart';
import 'package:mi_e2ee_im_app/presentation/screens/desktop_shell_screen.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';

void main() {
  testWidgets('desktop shell keeps detail panel behind the header menu', (
    WidgetTester tester,
  ) async {
    await _pumpDesktopShell(tester);

    expect(
      find.byKey(const ValueKey('desktop-inbox-search-field')),
      findsOneWidget,
    );
    expect(find.byKey(const ValueKey('desktop-folder-strip')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('desktop-conversation-stage')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('desktop-conversation-sidebar')),
      findsNothing,
    );
    expect(
      find.byKey(const ValueKey('desktop-stage-more-action')),
      findsOneWidget,
    );
    expect(find.text('全部'), findsOneWidget);
    expect(find.text('未读'), findsOneWidget);
    expect(find.text('群聊'), findsWidgets);
    expect(find.text('频道'), findsOneWidget);
    expect(find.text('文件'), findsWidgets);

    await _openDesktopContextPanel(tester);

    expect(
      find.byKey(const ValueKey('desktop-conversation-sidebar')),
      findsOneWidget,
    );
    expect(find.text('媒体'), findsAtLeastNWidgets(1));
    expect(find.text('链接'), findsAtLeastNWidgets(1));
    expect(find.text('通知设置'), findsOneWidget);
  });

  testWidgets('desktop stage pins outgoing messages near the right edge', (
    WidgetTester tester,
  ) async {
    await _pumpDesktopShell(tester);

    await tester.tap(find.text('Aurora 设计群').first);
    await tester.pumpAndSettle();

    final stage = find.byKey(const ValueKey('desktop-stage-canvas'));
    final timeline = find.byKey(const ValueKey('desktop-chat-timeline'));
    final outgoingBubble = find.byKey(
      const ValueKey('desktop-message-bubble-g6'),
    );

    expect(stage, findsOneWidget);
    expect(timeline, findsOneWidget);
    expect(
      (tester.getSize(stage).width - tester.getSize(timeline).width).abs(),
      lessThanOrEqualTo(2),
    );
    expect(outgoingBubble, findsOneWidget);
    final stageRect = tester.getRect(stage);
    final outgoingRect = tester.getRect(outgoingBubble);
    expect(stageRect.right - outgoingRect.right, inInclusiveRange(5, 8));
    expect(
      find.byKey(const ValueKey('desktop-conversation-sidebar')),
      findsNothing,
    );

    await _openDesktopContextPanel(tester);

    expect(tester.getSize(timeline).width, lessThanOrEqualTo(780));
    expect(find.text('成员'), findsAtLeastNWidgets(1));
    expect(find.text('媒体'), findsAtLeastNWidgets(1));
    expect(find.text('文件'), findsWidgets);
  });
}

Future<void> _pumpDesktopShell(WidgetTester tester) async {
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
        theme: AppTheme.light(),
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
