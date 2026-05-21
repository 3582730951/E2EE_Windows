import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/application/chat_providers.dart';
import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'package:mi_e2ee_im_app/domain/entities/models.dart';
import 'package:mi_e2ee_im_app/native_sdk/sdk_client.dart';
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

  testWidgets('desktop rail filters and searches live conversations', (
    WidgetTester tester,
  ) async {
    await _pumpDesktopShell(tester);

    await tester.tap(find.byKey(const ValueKey('chat-home-filter-files')));
    await tester.pumpAndSettle();

    expect(
      find.byKey(const ValueKey('desktop-inbox-tile-file-helper')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('desktop-inbox-tile-chat-bob')),
      findsNothing,
    );

    await tester.tap(find.byKey(const ValueKey('chat-home-filter-all')));
    await tester.pumpAndSettle();
    await tester.enterText(
      find.byKey(const ValueKey('desktop-inbox-search-input')),
      '质量',
    );
    await tester.pumpAndSettle();

    expect(
      find.byKey(const ValueKey('desktop-inbox-tile-group-qa')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('desktop-inbox-tile-group-design')),
      findsNothing,
    );
    expect(
      find.byKey(const ValueKey('desktop-inbox-search-clear')),
      findsOneWidget,
    );

    await tester.tap(find.byKey(const ValueKey('desktop-inbox-search-clear')));
    await tester.pumpAndSettle();

    expect(find.text('Aurora 设计群'), findsOneWidget);

    await tester.sendKeyDownEvent(LogicalKeyboardKey.controlLeft);
    await tester.sendKeyEvent(LogicalKeyboardKey.keyK);
    await tester.sendKeyUpEvent(LogicalKeyboardKey.controlLeft);
    await tester.pump();

    final searchEditable = tester.widget<EditableText>(
      find.descendant(
        of: find.byKey(const ValueKey('desktop-inbox-search-input')),
        matching: find.byType(EditableText),
      ),
    );
    expect(searchEditable.focusNode.hasFocus, isTrue);
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

  testWidgets('desktop stage search attach and clear use sdk-backed state', (
    WidgetTester tester,
  ) async {
    await _pumpDesktopShell(tester);

    expect(
      find.byKey(const ValueKey('desktop-message-bubble-m3')),
      findsOneWidget,
    );

    await tester.tap(find.byKey(const ValueKey('desktop-stage-search-action')));
    await tester.pumpAndSettle();
    await tester.enterText(
      find.byKey(const ValueKey('desktop-message-search-input')),
      '媒体面板',
    );
    await tester.pumpAndSettle();

    expect(
      find.byKey(const ValueKey('desktop-message-search-result-m3')),
      findsOneWidget,
    );

    await tester.tap(find.text('关闭'));
    await tester.pumpAndSettle();

    const filePickerChannel = MethodChannel(
      'miguelruivo.flutter.plugins.filepicker',
      StandardMethodCodec(),
    );
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(filePickerChannel, (call) async {
          if (call.method == 'any') {
            return <Map<String, Object?>>[
              <String, Object?>{
                'name': 'desktop-plan.pdf',
                'path': r'C:\tmp\desktop-plan.pdf',
                'size': 8192,
                'bytes': null,
                'identifier': null,
              },
            ];
          }
          return null;
        });
    addTearDown(
      () => TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(filePickerChannel, null),
    );

    await tester.tap(
      find.byKey(const ValueKey('desktop-composer-attach-button')),
    );
    await tester.pumpAndSettle();

    expect(find.text('desktop-plan.pdf'), findsOneWidget);

    await tester.tap(find.byKey(const ValueKey('desktop-stage-more-action')));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('desktop-stage-menu-clear')));
    await tester.pumpAndSettle();
    await tester.tap(
      find.byKey(const ValueKey('desktop-clear-history-confirm')),
    );
    await tester.pumpAndSettle();

    expect(
      find.byKey(const ValueKey('desktop-message-bubble-m3')),
      findsNothing,
    );

    await tester.tap(find.byKey(const ValueKey('desktop-stage-search-action')));
    await tester.pumpAndSettle();
    await tester.enterText(
      find.byKey(const ValueKey('desktop-message-search-input')),
      'desktop-plan.pdf',
    );
    await tester.pumpAndSettle();

    expect(find.text('没有找到相关消息'), findsOneWidget);
  });

  testWidgets('desktop empty state actions focus add contact and create chat', (
    WidgetTester tester,
  ) async {
    await _pumpDesktopShell(tester, client: _EmptyShellSdkClient());

    expect(
      find.byKey(const ValueKey('desktop-empty-state-surface')),
      findsOneWidget,
    );

    await tester.tap(find.text('选择一个聊天'));
    await tester.pump();

    final searchEditable = tester.widget<EditableText>(
      find.descendant(
        of: find.byKey(const ValueKey('desktop-inbox-search-input')),
        matching: find.byType(EditableText),
      ),
    );
    expect(searchEditable.focusNode.hasFocus, isTrue);

    await tester.tap(find.text('导入联系人'));
    await tester.pumpAndSettle();
    await tester.enterText(
      find.byKey(const ValueKey('desktop-add-friend-username')),
      'charlie',
    );
    await tester.enterText(
      find.byKey(const ValueKey('desktop-add-friend-remark')),
      'Charlie',
    );
    await tester.tap(find.byKey(const ValueKey('desktop-add-friend-submit')));
    await tester.pumpAndSettle();

    expect(find.text('好友请求已发送'), findsOneWidget);

    await tester.tap(find.text('新建聊天'));
    await tester.pumpAndSettle();

    expect(find.text('新群聊'), findsAtLeastNWidgets(1));
    expect(
      find.byKey(const ValueKey('desktop-empty-state-surface')),
      findsNothing,
    );
  });
}

Future<void> _pumpDesktopShell(
  WidgetTester tester, {
  NativeSdkClient? client,
}) async {
  final sdkClient = client ?? FakeSdkClient();
  await sdkClient.initialize();
  addTearDown(sdkClient.dispose);

  tester.view.physicalSize = const Size(1440, 960);
  tester.view.devicePixelRatio = 1.0;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);

  await tester.pumpWidget(
    ProviderScope(
      overrides: [sdkClientProvider.overrideWith((_) => sdkClient)],
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

class _EmptyShellSdkClient implements NativeSdkClient {
  final StreamController<List<ConversationSummary>> _conversationController =
      StreamController<List<ConversationSummary>>.broadcast();
  final StreamController<List<ContactProfile>> _contactController =
      StreamController<List<ContactProfile>>.broadcast();
  final StreamController<List<DeviceTrustInfo>> _deviceController =
      StreamController<List<DeviceTrustInfo>>.broadcast();
  final Map<String, StreamController<List<ChatMessage>>> _messageControllers =
      <String, StreamController<List<ChatMessage>>>{};
  List<ConversationSummary> _conversations = const <ConversationSummary>[];
  List<ContactProfile> _contacts = const <ContactProfile>[];
  var _groupSequence = 1;

  @override
  Future<void> initialize() async {}

  @override
  Future<SessionProfile?> login({
    required String username,
    required String password,
  }) async {
    return const SessionProfile(
      username: 'alice',
      displayName: 'Alice',
      subtitle: 'desktop test',
    );
  }

  @override
  Future<void> logout() async {}

  @override
  Stream<List<ConversationSummary>> watchConversations() async* {
    yield _conversations;
    yield* _conversationController.stream;
  }

  @override
  Stream<List<ChatMessage>> watchMessages(String conversationId) async* {
    yield const <ChatMessage>[];
    yield* _messageController(conversationId).stream;
  }

  @override
  Stream<List<ContactProfile>> watchContacts() async* {
    yield _contacts;
    yield* _contactController.stream;
  }

  @override
  Stream<List<DeviceTrustInfo>> watchDevices() async* {
    yield const <DeviceTrustInfo>[];
    yield* _deviceController.stream;
  }

  @override
  Future<void> markConversationRead({required String conversationId}) async {}

  @override
  Future<void> sendMessage({
    required String conversationId,
    required String text,
  }) async {}

  @override
  Future<void> sendFile({
    required String conversationId,
    required String path,
  }) async {}

  @override
  Future<String?> createGroup() async {
    final now = DateTime(2026, 5, 21, 9, _groupSequence);
    final id = 'group-empty-${_groupSequence++}';
    final conversation = ConversationSummary(
      id: id,
      title: '新群聊',
      preview: '群聊已创建',
      lastUpdated: now,
      kind: ConversationKind.group,
      isGroup: true,
      memberCount: 1,
      onlineCount: 1,
      lastSenderLabel: '系统',
      pillLabel: '群聊',
      previewBadge: '群聊',
    );
    _conversations = <ConversationSummary>[conversation, ..._conversations];
    _conversationController.add(
      List<ConversationSummary>.unmodifiable(_conversations),
    );
    _messageController(id).add(const <ChatMessage>[]);
    return id;
  }

  @override
  Future<void> sendFriendRequest({
    required String accountId,
    String remark = '',
  }) async {
    final trimmed = accountId.trim();
    if (trimmed.isEmpty) {
      return;
    }
    final contact = ContactProfile(
      id: trimmed,
      displayName: remark.trim().isEmpty ? trimmed : remark.trim(),
      handle: '@$trimmed',
      statusLabel: '好友请求已发送',
      groupLabel: '新的朋友',
      relationLabel: '等待验证',
      requestStatus: '已发送',
    );
    _contacts = <ContactProfile>[contact, ..._contacts];
    _contactController.add(List<ContactProfile>.unmodifiable(_contacts));
  }

  @override
  Future<void> clearConversationHistory({
    required String conversationId,
  }) async {
    _messageController(conversationId).add(const <ChatMessage>[]);
  }

  @override
  void dispose() {
    _conversationController.close();
    _contactController.close();
    _deviceController.close();
    for (final controller in _messageControllers.values) {
      controller.close();
    }
  }

  StreamController<List<ChatMessage>> _messageController(
    String conversationId,
  ) {
    return _messageControllers.putIfAbsent(
      conversationId,
      () => StreamController<List<ChatMessage>>.broadcast(),
    );
  }
}
