import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'package:mi_e2ee_im_app/application/chat_providers.dart';
import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'package:mi_e2ee_im_app/domain/entities/models.dart';
import 'package:mi_e2ee_im_app/native_sdk/sdk_client.dart';

void main() {
  test(
    'activeMessagesProvider marks the opened conversation as read',
    () async {
      final client = _RecordingSdkClient();
      final container = ProviderContainer(
        overrides: [sdkClientProvider.overrideWith((_) => client)],
      );
      addTearDown(container.dispose);

      await _awaitProviderValue(container, conversationsProvider);
      container.read(chatActionsProvider).selectConversation('chat-bob');

      final messages = await _awaitProviderValue(
        container,
        activeMessagesProvider,
      );

      expect(messages, hasLength(1));
      expect(client.markReadCalls, <String>['chat-bob']);
    },
  );

  test(
    'chatActions routes all send entrypoints through the same sdk method',
    () async {
      final client = _RecordingSdkClient();
      final container = ProviderContainer(
        overrides: [sdkClientProvider.overrideWith((_) => client)],
      );
      addTearDown(container.dispose);

      final actions = container.read(chatActionsProvider);
      actions.selectConversation('chat-bob');

      await actions.sendCurrentConversationMessage('来自当前会话入口');
      await actions.sendConversationMessage(
        conversationId: 'group-design',
        text: '来自显式会话入口',
      );

      expect(client.sendCalls, <_SendCall>[
        const _SendCall('chat-bob', '来自当前会话入口'),
        const _SendCall('group-design', '来自显式会话入口'),
      ]);
    },
  );

  test('filters conversation presets and keeps chat priority ordering', () {
    final now = DateTime(2026, 4, 21, 9);
    final conversations = <ConversationSummary>[
      ConversationSummary(
        id: 'direct',
        title: 'Bob',
        preview: 'hi',
        lastUpdated: now,
      ),
      ConversationSummary(
        id: 'group',
        title: 'Group',
        preview: '@你 看这里',
        lastUpdated: now.subtract(const Duration(minutes: 4)),
        kind: ConversationKind.group,
        isGroup: true,
        unreadCount: 3,
        mentionCount: 1,
      ),
      ConversationSummary(
        id: 'file',
        title: '文件传输助手',
        preview: 'file',
        lastUpdated: now.subtract(const Duration(minutes: 1)),
        kind: ConversationKind.fileAssistant,
        unreadCount: 1,
      ),
      ConversationSummary(
        id: 'channel',
        title: '公告',
        preview: 'news',
        lastUpdated: now.subtract(const Duration(minutes: 2)),
        kind: ConversationKind.channel,
      ),
    ];

    expect(
      filterConversationsByPreset(
        conversations,
        ConversationFilterPreset.unread,
      ).map((item) => item.id),
      <String>['group', 'file'],
    );
    expect(
      filterConversationsByPreset(
        conversations,
        ConversationFilterPreset.mentions,
      ).map((item) => item.id),
      <String>['group'],
    );
    expect(
      filterConversationsByPreset(
        conversations,
        ConversationFilterPreset.groups,
      ).map((item) => item.id),
      <String>['group'],
    );
    expect(
      filterConversationsByPreset(
        conversations,
        ConversationFilterPreset.channels,
      ).map((item) => item.id),
      <String>['channel'],
    );
    expect(
      filterConversationsByPreset(
        conversations,
        ConversationFilterPreset.files,
      ).map((item) => item.id),
      <String>['file'],
    );
  });
}

class _RecordingSdkClient implements NativeSdkClient {
  final List<String> markReadCalls = <String>[];
  final List<_SendCall> sendCalls = <_SendCall>[];

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
      subtitle: 'stub',
    );
  }

  @override
  Future<void> logout() async {}

  @override
  Future<void> markConversationRead({required String conversationId}) async {
    markReadCalls.add(conversationId);
  }

  @override
  Stream<List<ConversationSummary>> watchConversations() {
    return Stream<List<ConversationSummary>>.value(<ConversationSummary>[
      ConversationSummary(
        id: 'chat-bob',
        title: 'Bob',
        preview: 'hello',
        lastUpdated: DateTime(2026, 4, 19, 9, 0),
        unreadCount: 2,
      ),
      ConversationSummary(
        id: 'group-design',
        title: 'Design',
        preview: 'group',
        lastUpdated: DateTime(2026, 4, 19, 8, 0),
        isGroup: true,
      ),
    ]);
  }

  @override
  Stream<List<ChatMessage>> watchMessages(String conversationId) {
    return Stream<List<ChatMessage>>.value(<ChatMessage>[
      ChatMessage(
        id: 'm1',
        conversationId: conversationId,
        senderLabel: 'Bob',
        text: 'hello',
        timestamp: DateTime(2026, 4, 19, 9, 0),
        direction: MessageDirection.incoming,
        status: MessageDeliveryStatus.delivered,
      ),
    ]);
  }

  @override
  Stream<List<ContactProfile>> watchContacts() {
    return const Stream<List<ContactProfile>>.empty();
  }

  @override
  Stream<List<DeviceTrustInfo>> watchDevices() {
    return const Stream<List<DeviceTrustInfo>>.empty();
  }

  @override
  Future<void> sendMessage({
    required String conversationId,
    required String text,
  }) async {
    sendCalls.add(_SendCall(conversationId, text));
  }

  @override
  void dispose() {}
}

Future<T> _awaitProviderValue<T>(
  ProviderContainer container,
  dynamic provider,
) async {
  final completer = Completer<T>();
  late final ProviderSubscription<AsyncValue<T>> subscription;
  subscription = container.listen<AsyncValue<T>>(provider, (_, next) {
    if (next.hasValue && !completer.isCompleted) {
      completer.complete(next.requireValue);
    } else if (next.hasError && !completer.isCompleted) {
      completer.completeError(next.error!, next.stackTrace);
    }
  }, fireImmediately: true);

  try {
    return await completer.future;
  } finally {
    subscription.close();
  }
}

class _SendCall {
  const _SendCall(this.conversationId, this.text);

  final String conversationId;
  final String text;

  @override
  bool operator ==(Object other) {
    return other is _SendCall &&
        other.conversationId == conversationId &&
        other.text == text;
  }

  @override
  int get hashCode => Object.hash(conversationId, text);

  @override
  String toString() => '_SendCall($conversationId, $text)';
}
