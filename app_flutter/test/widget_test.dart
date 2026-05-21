import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/app.dart';
import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'fakes/fake_sdk_client.dart';

void main() {
  testWidgets('app signs in to the chat-first mobile shell', (
    WidgetTester tester,
  ) async {
    await _pumpApp(tester);
    await _signIn(tester);

    expect(find.text('聊天'), findsWidgets);
    expect(find.byKey(const Key('mobile-chats-nav-search')), findsOneWidget);
    expect(find.text('搜索聊天、群、频道和文件'), findsNothing);
    expect(find.text('Aurora 设计群'), findsOneWidget);
    expect(find.text('文件传输助手'), findsOneWidget);
    expect(find.text('登录提醒'), findsOneWidget);
    expect(find.textContaining('风控'), findsNothing);
    expect(find.byKey(const ValueKey('chat-home-filter-all')), findsNothing);
    expect(
      find.byKey(const ValueKey('chat-home-filter-mentions')),
      findsNothing,
    );
  });

  testWidgets(
    'contacts and chat detail remain reachable from shell navigation',
    (WidgetTester tester) async {
      await _pumpApp(tester);
      await _signIn(tester);

      await tester.tap(find.text('联系人').last);
      await tester.pumpAndSettle();

      expect(find.text('新的朋友'), findsWidgets);
      expect(find.text('群聊'), findsWidgets);
      expect(find.text('我的好友'), findsOneWidget);
      expect(find.text('公众号或服务号'), findsOneWidget);

      await tester.tap(find.text('会话').last);
      await tester.pumpAndSettle();
      await tester.tap(find.text('Bob Chen').first);
      await tester.pumpAndSettle();

      expect(
        find.byKey(const Key('mobile-chat-detail-header')),
        findsOneWidget,
      );
      await tester.enterText(
        find.byKey(const Key('mobile-chat-composer-input-material')),
        '来自 smoke 测试',
      );
      await tester.pumpAndSettle();
      await tester.tap(
        find.byKey(const Key('mobile-chat-composer-action-send')),
      );
      await tester.pumpAndSettle();

      expect(find.text('来自 smoke 测试'), findsOneWidget);
    },
  );
}

Future<void> _pumpApp(WidgetTester tester) async {
  final fakeClient = FakeSdkClient();
  await fakeClient.initialize();
  addTearDown(fakeClient.dispose);

  tester.view.physicalSize = const Size(412, 915);
  tester.view.devicePixelRatio = 1.0;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);

  await tester.pumpWidget(
    ProviderScope(
      overrides: [sdkClientProvider.overrideWith((_) => fakeClient)],
      child: const MyApp(),
    ),
  );
  await tester.pumpAndSettle();
}

Future<void> _signIn(WidgetTester tester) async {
  final fields = find.byType(EditableText);
  final suffix = DateTime.now().microsecondsSinceEpoch;
  await tester.enterText(fields.first, 'widget_$suffix');
  await tester.enterText(fields.at(1), 'widget_${suffix}_password');
  await tester.tap(find.byKey(const ValueKey('login-continue-button')));
  await tester.pumpAndSettle();
}
