import 'package:flutter/cupertino.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/application/chat_providers.dart';
import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'package:mi_e2ee_im_app/native_sdk/fake_sdk_client.dart';
import 'package:mi_e2ee_im_app/presentation/screens/mobile_chats_screen.dart';
import 'package:mi_e2ee_im_app/presentation/screens/mobile_shell_screen.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('iOS chat home is list-first with hidden optional filters', (
    WidgetTester tester,
  ) async {
    await _pumpChatsScreen(tester, TargetPlatform.iOS);

    expect(
      find.byKey(const Key('mobile-chats-large-title-cupertino')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-chats-header-cupertino')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-chats-search-cupertino')),
      findsOneWidget,
    );
    expect(find.byKey(const ValueKey('chat-home-filter-all')), findsNothing);
    expect(find.byKey(const ValueKey('chat-home-filter-unread')), findsNothing);
    expect(
      find.byKey(const ValueKey('chat-home-filter-mentions')),
      findsNothing,
    );
    expect(find.byType(CupertinoTabBar), findsNothing);
    expect(find.text('Aurora 设计群'), findsOneWidget);
    expect(find.textContaining('风控'), findsNothing);
  });

  testWidgets('mobile shell keeps iOS tab bar around chat list', (
    WidgetTester tester,
  ) async {
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
            theme: AppTheme.light(),
            home: const MobileShellScreen(section: AppSection.chats),
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.byType(CupertinoTabBar), findsOneWidget);
      expect(
        find.byKey(const ValueKey('conversation-row-group-design')),
        findsOneWidget,
      );
    } finally {
      debugDefaultTargetPlatformOverride = previousPlatform;
    }
  });

  testWidgets('Android chat home exposes plus and FAB compose affordances', (
    WidgetTester tester,
  ) async {
    await _pumpChatsScreen(tester, TargetPlatform.android);

    expect(
      find.byKey(const Key('mobile-chats-header-material')),
      findsOneWidget,
    );
    final chromeRect = tester.getRect(
      find.byKey(const Key('mobile-chats-top-chrome-material')),
    );
    expect(chromeRect.height, lessThanOrEqualTo(48));
    expect(find.byKey(const Key('mobile-chats-nav-search')), findsOneWidget);
    expect(find.byKey(const Key('mobile-chats-search-material')), findsNothing);
    expect(find.byKey(const Key('mobile-chats-nav-compose')), findsNothing);
    expect(find.byKey(const Key('mobile-chats-fab-compose')), findsOneWidget);
    expect(find.byKey(const ValueKey('chat-home-filter-files')), findsNothing);
  });

  testWidgets('Android shell keeps a compact floating solid bottom nav', (
    WidgetTester tester,
  ) async {
    final previousPlatform = debugDefaultTargetPlatformOverride;
    debugDefaultTargetPlatformOverride = TargetPlatform.android;
    try {
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
          child: MaterialApp(
            theme: AppTheme.light(),
            home: const MobileShellScreen(section: AppSection.chats),
          ),
        ),
      );
      await tester.pumpAndSettle();

      final navigationBar = find.byType(NavigationBar);
      expect(navigationBar, findsOneWidget);
      final rect = tester.getRect(navigationBar);
      expect(rect.left, greaterThanOrEqualTo(9));
      expect(412 - rect.right, greaterThanOrEqualTo(9));
      expect(rect.height, lessThanOrEqualTo(56));
    } finally {
      debugDefaultTargetPlatformOverride = previousPlatform;
    }
  });
}

Future<void> _pumpChatsScreen(
  WidgetTester tester,
  TargetPlatform platform,
) async {
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
          theme: AppTheme.light(),
          home: const Scaffold(body: MobileChatsScreen()),
        ),
      ),
    );
    await tester.pumpAndSettle();
  } finally {
    debugDefaultTargetPlatformOverride = previousPlatform;
  }
}
