import 'package:flutter/cupertino.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_riverpod/misc.dart' show Override;
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'package:mi_e2ee_im_app/domain/entities/models.dart';
import 'package:mi_e2ee_im_app/main.dart';
import 'package:mi_e2ee_im_app/native_sdk/sdk_client.dart';

import 'test_capabilities.dart';

String _captureUsername() {
  const configured = String.fromEnvironment('MI_E2EE_CAPTURE_USERNAME');
  if (configured.isNotEmpty) {
    return configured;
  }
  return 'capture_${DateTime.now().microsecondsSinceEpoch}';
}

String _capturePassword() {
  const configured = String.fromEnvironment('MI_E2EE_CAPTURE_PASSWORD');
  if (configured.isNotEmpty) {
    return configured;
  }
  return 'capture_${DateTime.now().microsecondsSinceEpoch}_local';
}

enum UiCaptureVariant { standard, enhanced }

typedef UiCaptureAction = Future<void> Function(WidgetTester tester);

class UiCaptureScenario {
  const UiCaptureScenario({
    required this.name,
    required this.platform,
    required this.size,
    required this.outputPath,
    required this.variant,
    required this.action,
    this.sdkClient,
  });

  final String name;
  final TargetPlatform platform;
  final Size size;
  final String outputPath;
  final UiCaptureVariant variant;
  final NativeSdkClient? sdkClient;
  final UiCaptureAction action;

  Future<void> capture(WidgetTester tester) async {
    final previousPlatform = debugDefaultTargetPlatformOverride;
    debugDefaultTargetPlatformOverride = platform;
    tester.view.physicalSize = size;
    tester.view.devicePixelRatio = 1.0;

    try {
      final overrides = <Override>[
        ...buildTestCapabilityOverrides(
          platform: platform,
          logicalSize: size,
          devicePixelRatio: 1.0,
          disableAnimations: variant == UiCaptureVariant.standard,
          androidIsLowRamDevice: platform == TargetPlatform.android
              ? false
              : null,
          androidPhysicalRamSizeMb: platform == TargetPlatform.android
              ? 8192
              : null,
          iosPhysicalRamSizeMb: platform == TargetPlatform.iOS ? 8192 : null,
          desktopSystemMemoryInMegabytes: switch (platform) {
            TargetPlatform.macOS ||
            TargetPlatform.windows ||
            TargetPlatform.linux => 16384,
            _ => null,
          },
        ),
      ];
      if (sdkClient != null) {
        overrides.add(sdkClientProvider.overrideWith((_) => sdkClient!));
      }

      await tester.pumpWidget(
        ProviderScope(overrides: overrides, child: const MyApp()),
      );
      await tester.pumpAndSettle();

      await action(tester);

      expect(find.byType(Scaffold), findsWidgets);
      await expectLater(
        find.byType(Scaffold).last,
        matchesGoldenFile(outputPath),
      );
    } finally {
      debugDefaultTargetPlatformOverride = previousPlatform;
      tester.view.resetPhysicalSize();
      tester.view.resetDevicePixelRatio();
      sdkClient?.dispose();
    }
  }
}

Future<void> captureLoginScreen(WidgetTester tester) async {
  _expectLoginScreen();
}

Future<void> captureMobileChatList(WidgetTester tester) async {
  await _signIn(tester);
  _expectMobileChatList();
}

Future<void> captureMobileChatDetail(WidgetTester tester) async {
  await _signIn(tester);
  await _openConversation(tester, 'Aurora 设计群');
  _expectMobileChatDetail();
}

Future<void> captureMobileContacts(WidgetTester tester) async {
  await _signIn(tester);
  await _openSection(tester, '联系人');
  _expectMobileContacts();
}

Future<void> captureMobileSettingsSecurity(WidgetTester tester) async {
  await _signIn(tester);
  await _openSection(tester, '设置');
  await _expectMobileSettingsSecurity(tester);
}

Future<void> captureDesktopChatShell(WidgetTester tester) async {
  await _signIn(tester);
  await _openConversation(tester, 'Aurora 设计群');
  _expectDesktopChatShell();
}

Future<void> captureDesktopChatShellClosed(WidgetTester tester) async {
  await _signIn(tester);
  await _openConversation(tester, 'Aurora 设计群');
  _expectDesktopChatShellClosed();
}

Future<void> captureDesktopChatPanelOpen(WidgetTester tester) async {
  await _signIn(tester);
  await _openConversation(tester, 'Aurora 设计群');
  await _openDesktopContextPanel(tester);
  _expectDesktopChatPanelOpen();
}

Future<void> captureDesktopEmptyState(WidgetTester tester) async {
  await _signIn(tester);
  _expectDesktopEmptyState();
}

void _expectLoginScreen() {
  expect(find.text('MI Chat'), findsOneWidget);
  expect(find.text('消息、群聊与文件同步'), findsOneWidget);
  expect(find.byKey(const ValueKey('login-mode-tabs')), findsOneWidget);
  expect(find.text('扫码登录'), findsAtLeastNWidgets(1));
  expect(find.text('账号登录'), findsOneWidget);
  expect(find.byKey(const ValueKey('login-qr-preview')), findsOneWidget);
  expect(find.byKey(const ValueKey('login-qr-code-surface')), findsOneWidget);
  expect(find.byKey(const ValueKey('login-help-links')), findsOneWidget);
  expect(find.text('注册账号'), findsOneWidget);
  expect(find.text('忘记密码'), findsOneWidget);
  expect(find.text('代理设置'), findsOneWidget);
  expect(find.text('登录'), findsOneWidget);
  expect(find.text('聊天'), findsNothing);
  expect(find.text('Aurora 设计群'), findsNothing);
  expect(find.text('欢迎回来'), findsNothing);
  expect(find.text('回到最近会话'), findsNothing);
  expect(
    find.byKey(const ValueKey('login-identity-status-card')),
    findsOneWidget,
  );
  expect(find.text('账号'), findsOneWidget);
  expect(find.text('密码'), findsOneWidget);
  expect(find.textContaining('Root Auth'), findsNothing);
}

Future<void> _signIn(WidgetTester tester) async {
  final continueButton = find.byKey(const ValueKey('login-continue-button'));
  if (continueButton.evaluate().isEmpty) {
    return;
  }

  final formColumn = find.byKey(const ValueKey('login-form-column'));
  final fields = find.descendant(
    of: formColumn,
    matching: find.byType(EditableText),
  );
  expect(fields, findsAtLeastNWidgets(2));

  await tester.enterText(fields.first, _captureUsername());
  await tester.enterText(fields.at(1), _capturePassword());
  await tester.ensureVisible(continueButton);
  await tester.tap(continueButton);
  await tester.pumpAndSettle();
}

Future<void> _openConversation(WidgetTester tester, String title) async {
  const conversationIds = <String, String>{
    'Aurora 设计群': 'group-design',
    'Bob Chen': 'chat-bob',
    '文件传输助手': 'file-helper',
    '质量回归': 'group-qa',
    'Mina Xu': 'chat-mina',
    '项目推进群': 'group-project',
    '登录提醒': 'service-notice',
  };
  final conversationId = conversationIds[title];
  if (conversationId != null) {
    final keyedTargets = <Finder>[
      find.byKey(ValueKey('conversation-row-$conversationId')),
      find.byKey(ValueKey('desktop-inbox-tile-$conversationId')),
      find.byKey(ValueKey('desktop-inbox-tile-selected-$conversationId')),
    ];
    for (final target in keyedTargets) {
      if (target.evaluate().isNotEmpty) {
        await tester.ensureVisible(target.first);
        await tester.tap(target.first);
        await tester.pumpAndSettle();
        return;
      }
    }
  }

  await tester.ensureVisible(find.text(title).first);
  await tester.tap(find.text(title).first);
  await tester.pumpAndSettle();
}

Future<void> _openDesktopContextPanel(WidgetTester tester) async {
  await tester.tap(find.byKey(const ValueKey('desktop-stage-more-action')));
  await tester.pumpAndSettle();
  await tester.tap(find.byKey(const ValueKey('desktop-stage-menu-profile')));
  await tester.pumpAndSettle();
}

Future<void> _openSection(WidgetTester tester, String label) async {
  final navigationBar = find.byType(NavigationBar);
  final tabBar = find.byType(CupertinoTabBar);

  if (navigationBar.evaluate().isNotEmpty) {
    await tester.tap(
      find.descendant(of: navigationBar, matching: find.text(label)).first,
    );
  } else if (tabBar.evaluate().isNotEmpty) {
    await tester.tap(
      find.descendant(of: tabBar, matching: find.text(label)).first,
    );
  } else {
    throw StateError('No shell navigation bar was found.');
  }

  await tester.pumpAndSettle();
}

void _expectMobileChatList() {
  final tabBar = find.byType(CupertinoTabBar);
  final navigationBar = find.byType(NavigationBar);
  final topChrome = find.byKey(const Key('mobile-chats-top-chrome-cupertino'));
  final topChromeMaterial = find.byKey(
    const Key('mobile-chats-top-chrome-material'),
  );
  final cupertinoHeader = find.byKey(
    const Key('mobile-chats-header-cupertino'),
  );
  final materialHeader = find.byKey(const Key('mobile-chats-header-material'));
  final searchField = find.byKey(const Key('mobile-chats-search-cupertino'));
  final searchFieldMaterial = find.byKey(
    const Key('mobile-chats-search-material'),
  );
  final materialSearchAction = find.byKey(const Key('mobile-chats-nav-search'));
  expect(
    cupertinoHeader.evaluate().isNotEmpty ||
        materialHeader.evaluate().isNotEmpty,
    isTrue,
  );
  if (cupertinoHeader.evaluate().isNotEmpty) {
    expect(
      find.byKey(const Key('mobile-chats-large-title-cupertino')),
      findsOneWidget,
    );
    expect(find.text('最近消息'), findsNothing);
  } else {
    expect(materialHeader, findsOneWidget);
    expect(
      find.descendant(of: materialHeader, matching: find.text('聊天')),
      findsOneWidget,
    );
  }
  expect(find.textContaining('个会话'), findsNothing);
  expect(
    topChrome.evaluate().isNotEmpty || topChromeMaterial.evaluate().isNotEmpty,
    isTrue,
  );
  if (searchField.evaluate().isNotEmpty) {
    expect(
      find.descendant(of: searchField, matching: find.text('搜索聊天、群、频道和文件')),
      findsOneWidget,
    );
  } else {
    expect(searchFieldMaterial, findsNothing);
    expect(materialSearchAction, findsOneWidget);
  }
  expect(find.byKey(const ValueKey('chat-home-filter-all')), findsNothing);
  expect(find.byKey(const ValueKey('chat-home-filter-groups')), findsNothing);
  expect(find.byKey(const ValueKey('chat-home-filter-unread')), findsNothing);
  expect(
    find.byKey(const ValueKey('conversation-row-group-design')),
    findsOneWidget,
  );
  expect(find.text('Aurora 设计群'), findsOneWidget);
  expect(find.text('Bob Chen'), findsOneWidget);
  expect(
    tabBar.evaluate().isNotEmpty || navigationBar.evaluate().isNotEmpty,
    isTrue,
  );
  expect(find.byKey(const Key('mobile-chat-detail-header')), findsNothing);
}

void _expectMobileChatDetail() {
  expect(find.byKey(const Key('mobile-chat-detail-header')), findsOneWidget);
  expect(find.byKey(const Key('mobile-chat-timeline')), findsOneWidget);
  expect(find.byKey(const Key('mobile-chat-composer')), findsOneWidget);
  expect(find.text('Aurora 设计群'), findsOneWidget);
  expect(find.byKey(const Key('mobile-chat-header-back')), findsOneWidget);
  expect(find.byType(CupertinoTabBar), findsNothing);
  expect(find.byType(NavigationBar), findsNothing);
}

void _expectMobileContacts() {
  expect(find.byKey(const ValueKey('contacts-search-field')), findsOneWidget);
  expect(find.text('联系人'), findsAtLeastNWidgets(1));
  expect(find.text('新的朋友'), findsWidgets);
  expect(find.text('我的好友'), findsOneWidget);
  expect(find.text('Bob Chen'), findsAtLeastNWidgets(1));
  expect(find.text('Mina Xu'), findsAtLeastNWidgets(1));
  expect(
    find.byType(CupertinoTabBar).evaluate().isNotEmpty ||
        find.byType(NavigationBar).evaluate().isNotEmpty,
    isTrue,
  );
}

Future<void> _expectMobileSettingsSecurity(WidgetTester tester) async {
  expect(find.text('设置'), findsAtLeastNWidgets(1));
  expect(
    find
            .byKey(const Key('settings-security-header-shell-cupertino'))
            .evaluate()
            .isNotEmpty ||
        find.byType(NavigationBar).evaluate().isNotEmpty,
    isTrue,
  );
  if (find
      .byKey(const Key('settings-security-header-shell-cupertino'))
      .evaluate()
      .isNotEmpty) {
    expect(
      find.byKey(const Key('settings-security-nav-title-cupertino')),
      findsOneWidget,
    );
    expect(find.text('账号与隐私'), findsNothing);
  }
  expect(find.text('当前账号'), findsOneWidget);
  expect(find.text('通知'), findsOneWidget);
  expect(find.text('通知与提醒'), findsOneWidget);
  expect(find.text('聊天'), findsOneWidget);
  expect(find.text('会话锁定'), findsOneWidget);
  expect(find.text('外观'), findsOneWidget);
  expect(find.text('视觉模式'), findsOneWidget);
  expect(
    find.byKey(const ValueKey('visual-mode-segmented-control')),
    findsOneWidget,
  );
  expect(
    find.byType(CupertinoTabBar).evaluate().isNotEmpty ||
        find.byType(NavigationBar).evaluate().isNotEmpty,
    isTrue,
  );
}

void _expectDesktopChatShell() {
  expect(
    find.byKey(const ValueKey('desktop-conversation-stage')),
    findsOneWidget,
  );
  expect(
    find.byKey(const ValueKey('desktop-inbox-search-field')),
    findsOneWidget,
  );
  expect(find.text('Alice Lin'), findsOneWidget);
  expect(
    find.descendant(
      of: find.byKey(const ValueKey('desktop-conversation-stage')),
      matching: find.text('Aurora 设计群'),
    ),
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
}

void _expectDesktopChatShellClosed() {
  expect(
    find.byKey(const ValueKey('desktop-conversation-stage')),
    findsOneWidget,
  );
  expect(
    find.byKey(const ValueKey('desktop-inbox-search-field')),
    findsOneWidget,
  );
  expect(
    find.descendant(
      of: find.byKey(const ValueKey('desktop-conversation-stage')),
      matching: find.text('Aurora 设计群'),
    ),
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
}

void _expectDesktopChatPanelOpen() {
  expect(
    find.byKey(const ValueKey('desktop-conversation-stage')),
    findsOneWidget,
  );
  expect(
    find.byKey(const ValueKey('desktop-inbox-search-field')),
    findsOneWidget,
  );
  expect(
    find.descendant(
      of: find.byKey(const ValueKey('desktop-conversation-stage')),
      matching: find.text('Aurora 设计群'),
    ),
    findsOneWidget,
  );
  expect(
    find.byKey(const ValueKey('desktop-conversation-sidebar')),
    findsOneWidget,
  );
  expect(find.text('成员'), findsAtLeastNWidgets(1));
  expect(find.text('媒体'), findsAtLeastNWidgets(1));
  expect(
    find.byKey(const ValueKey('desktop-context-media-grid')),
    findsOneWidget,
  );
  expect(find.text('文件'), findsWidgets);
  expect(find.text('链接'), findsAtLeastNWidgets(1));
  expect(find.text('通知设置'), findsOneWidget);
}

void _expectDesktopEmptyState() {
  expect(
    find.byKey(const ValueKey('desktop-conversation-stage')),
    findsOneWidget,
  );
  expect(
    find.byKey(const ValueKey('desktop-conversation-sidebar')),
    findsNothing,
  );
  expect(find.text('从左侧打开一个聊天'), findsOneWidget);
  expect(find.text('从左侧开始。'), findsNothing);
  expect(find.text('从左侧选择一个会话，消息会稳定地停留在这里。'), findsNothing);
  expect(find.textContaining('最近消息、文件和群聊会在这里展开'), findsOneWidget);
  expect(
    find.byKey(const ValueKey('desktop-empty-state-actions')),
    findsOneWidget,
  );
  expect(find.text('新建聊天'), findsOneWidget);
  expect(find.text('导入联系人'), findsOneWidget);
  expect(find.text('打开文件助手'), findsOneWidget);
}

class EmptyCaptureSdkClient implements NativeSdkClient {
  @override
  Future<void> initialize() async {}

  @override
  Future<SessionProfile?> login({
    required String username,
    required String password,
  }) async {
    if (username.trim().isEmpty || password.trim().isEmpty) {
      return null;
    }
    return const SessionProfile(
      username: 'alice',
      displayName: 'Alice Lin',
      subtitle: '消息、群聊与文件同步中',
    );
  }

  @override
  Future<void> logout() async {}

  @override
  Stream<List<ConversationSummary>> watchConversations() {
    return Stream<List<ConversationSummary>>.value(
      const <ConversationSummary>[],
    );
  }

  @override
  Stream<List<ChatMessage>> watchMessages(String conversationId) {
    return Stream<List<ChatMessage>>.value(const <ChatMessage>[]);
  }

  @override
  Stream<List<ContactProfile>> watchContacts() {
    return Stream<List<ContactProfile>>.value(const <ContactProfile>[]);
  }

  @override
  Stream<List<DeviceTrustInfo>> watchDevices() {
    return Stream<List<DeviceTrustInfo>>.value(const <DeviceTrustInfo>[]);
  }

  @override
  Future<void> markConversationRead({required String conversationId}) async {}

  @override
  Future<void> sendMessage({
    required String conversationId,
    required String text,
  }) async {}

  @override
  void dispose() {}
}
