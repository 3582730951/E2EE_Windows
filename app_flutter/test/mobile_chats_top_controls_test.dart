import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/application/chat_providers.dart';
import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'package:mi_e2ee_im_app/domain/entities/models.dart';
import 'fakes/fake_sdk_client.dart';
import 'package:mi_e2ee_im_app/presentation/screens/mobile_chats_screen.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';
import 'package:mi_e2ee_im_app/presentation/theme/visual_tier.dart';
import 'package:mi_e2ee_im_app/presentation/widgets/shell_chrome.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('iOS chat home shows search compose and pull-down filters', (
    WidgetTester tester,
  ) async {
    await _pumpChatsScreen(tester, TargetPlatform.iOS);

    expect(
      find.byKey(const Key('mobile-chats-top-chrome-cupertino')),
      findsOneWidget,
    );
    expect(find.text('聊天'), findsWidgets);
    expect(
      find.byKey(const Key('mobile-chats-search-cupertino')),
      findsOneWidget,
    );
    expect(find.byKey(const Key('mobile-chats-nav-compose')), findsOneWidget);
    expect(find.byKey(const Key('mobile-chats-nav-edit')), findsNothing);
    expect(find.byKey(const Key('mobile-chats-nav-more')), findsNothing);
    expect(find.byKey(const ValueKey('chat-home-filter-all')), findsNothing);

    await tester.drag(
      find.byKey(const Key('mobile-chats-list')),
      const Offset(0, 92),
    );
    await tester.pumpAndSettle();

    expect(find.byKey(const ValueKey('chat-home-filter-all')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('chat-home-filter-unread')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('chat-home-filter-mentions')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('chat-home-filter-groups')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('chat-home-filter-files')),
      findsOneWidget,
    );
    expect(find.text('Aurora 设计群'), findsOneWidget);
    expect(find.text('草稿'), findsWidgets);
  });

  testWidgets('iOS enhanced chat search stays matte inside top chrome', (
    WidgetTester tester,
  ) async {
    await _pumpChatsScreen(
      tester,
      TargetPlatform.iOS,
      visualTier: VisualTier.enhancedGlass,
    );

    final search = find.byKey(const Key('mobile-chats-search-cupertino'));
    expect(search, findsOneWidget);
    expect(find.byKey(const Key('mobile-chats-top-glass-shell')), findsNothing);
    expect(
      find.byKey(const Key('mobile-chats-top-matte-shell-cupertino')),
      findsOneWidget,
    );
    expect(
      find.descendant(of: search, matching: find.byType(LiquidGlassPanel)),
      findsNothing,
    );
    expect(
      find.ancestor(of: search, matching: find.byType(LiquidGlassPanel)),
      findsNothing,
    );
  });

  testWidgets('Android chat home keeps filters hidden until pull-down', (
    WidgetTester tester,
  ) async {
    await _pumpChatsScreen(
      tester,
      TargetPlatform.android,
      visualTier: VisualTier.enhancedGlass,
    );

    expect(
      find.byKey(const Key('mobile-chats-top-chrome-material')),
      findsOneWidget,
    );
    final chromeRect = tester.getRect(
      find.byKey(const Key('mobile-chats-top-chrome-material')),
    );
    expect(chromeRect.height, lessThanOrEqualTo(48));
    expect(find.byKey(const Key('mobile-chats-nav-search')), findsOneWidget);
    expect(find.byKey(const Key('mobile-chats-search-material')), findsNothing);
    expect(find.byKey(const Key('mobile-chats-nav-compose')), findsNothing);
    expect(find.byKey(const Key('mobile-chats-nav-more')), findsNothing);
    expect(find.byKey(const Key('mobile-chats-fab-compose')), findsOneWidget);
    expect(find.byType(LiquidGlassPanel), findsNothing);
    expect(find.byKey(const ValueKey('chat-home-filter-all')), findsNothing);

    await tester.drag(
      find.byKey(const Key('mobile-chats-list')),
      const Offset(0, 96),
    );
    await tester.pumpAndSettle();

    expect(find.byKey(const ValueKey('chat-home-filter-all')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('chat-home-filter-unread')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('chat-home-filter-mentions')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('chat-home-filter-groups')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('chat-home-filter-files')),
      findsOneWidget,
    );

    await tester.tap(find.byKey(const ValueKey('chat-home-filter-mentions')));
    await tester.pumpAndSettle();

    expect(find.text('质量回归'), findsOneWidget);
    expect(find.text('Bob Chen'), findsNothing);
  });
}

Future<void> _pumpChatsScreen(
  WidgetTester tester,
  TargetPlatform platform, {
  VisualTier visualTier = VisualTier.standard,
}) async {
  final previousPlatform = debugDefaultTargetPlatformOverride;
  debugDefaultTargetPlatformOverride = platform;
  try {
    final fakeClient = FakeSdkClient();
    await fakeClient.initialize();
    addTearDown(fakeClient.dispose);

    tester.view.physicalSize = platform == TargetPlatform.android
        ? const Size(412, 915)
        : const Size(393, 852);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(
      ProviderScope(
        overrides: [
          sdkClientProvider.overrideWith((_) => fakeClient),
          conversationsProvider.overrideWith(
            (ref) =>
                Stream<List<ConversationSummary>>.value(_denseConversations()),
          ),
        ],
        child: MaterialApp(
          theme: AppTheme.light(visualTier: visualTier),
          home: const Scaffold(body: MobileChatsScreen()),
        ),
      ),
    );
    await tester.pumpAndSettle();
  } finally {
    debugDefaultTargetPlatformOverride = previousPlatform;
  }
}

List<ConversationSummary> _denseConversations() {
  final now = DateTime(2026, 4, 21, 9);
  return <ConversationSummary>[
    ConversationSummary(
      id: 'group-design',
      title: 'Aurora 设计群',
      preview: 'Alice: 新版顶部栏先收窄再验证',
      lastUpdated: now,
      kind: ConversationKind.group,
      isGroup: true,
      unreadCount: 6,
      isPinned: true,
      memberCount: 128,
      onlineCount: 19,
      lastSenderLabel: 'Alice',
    ),
    ConversationSummary(
      id: 'file-helper',
      title: '文件传输助手',
      preview: 'UI-review-v12.fig',
      lastUpdated: now.subtract(const Duration(minutes: 4)),
      kind: ConversationKind.fileAssistant,
      unreadCount: 1,
      lastSenderLabel: '文件',
    ),
    ConversationSummary(
      id: 'group-qa',
      title: '质量回归',
      preview: '@你 Android 截图流程已回归通过',
      lastUpdated: now.subtract(const Duration(minutes: 8)),
      kind: ConversationKind.group,
      isGroup: true,
      unreadCount: 3,
      mentionCount: 1,
      lastSenderLabel: 'QA Bot',
    ),
    ConversationSummary(
      id: 'chat-bob',
      title: 'Bob Chen',
      preview: '今晚把登录页再收一轮',
      lastUpdated: now.subtract(const Duration(minutes: 11)),
      isOnline: true,
      lastSenderLabel: 'Bob',
    ),
    ConversationSummary(
      id: 'chat-mina',
      title: 'Mina Xu',
      preview: '草稿：文件卡片密度要继续压',
      lastUpdated: now.subtract(const Duration(minutes: 14)),
      draftText: '文件卡片密度要继续压',
      isOnline: true,
      lastSenderLabel: 'Mina',
    ),
  ];
}
