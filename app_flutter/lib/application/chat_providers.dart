import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../bootstrap/app_providers.dart';
import '../domain/entities/models.dart';

enum AppSection { chats, contacts, settings }

extension AppSectionX on AppSection {
  String get routeSegment => switch (this) {
    AppSection.chats => 'chats',
    AppSection.contacts => 'contacts',
    AppSection.settings => 'settings',
  };

  String get label => switch (this) {
    AppSection.chats => '会话',
    AppSection.contacts => '联系人',
    AppSection.settings => '设置',
  };
}

final conversationsProvider = StreamProvider<List<ConversationSummary>>((ref) {
  return ref.watch(sdkClientProvider).watchConversations();
});

List<ConversationSummary> filterConversationsByPreset(
  Iterable<ConversationSummary> conversations,
  ConversationFilterPreset preset,
) {
  return prioritizeChatConversations(
    conversations.where((conversation) {
      return switch (preset) {
        ConversationFilterPreset.all => !conversation.isArchived,
        ConversationFilterPreset.unread =>
          !conversation.isArchived && conversation.unreadCount > 0,
        ConversationFilterPreset.mentions =>
          !conversation.isArchived && conversation.mentionCount > 0,
        ConversationFilterPreset.groups =>
          !conversation.isArchived &&
              (conversation.isGroupConversation ||
                  conversation.kind == ConversationKind.group),
        ConversationFilterPreset.channels =>
          !conversation.isArchived &&
              conversation.kind == ConversationKind.channel,
        ConversationFilterPreset.files =>
          !conversation.isArchived &&
              (conversation.isFileConversation ||
                  conversation.kind == ConversationKind.fileAssistant ||
                  conversation.pillLabel == '文件' ||
                  conversation.previewBadge == '文件'),
      };
    }),
  );
}

List<ConversationSummary> filterConversationsBySearch(
  Iterable<ConversationSummary> conversations,
  String query,
) {
  final normalizedQuery = _normalizeConversationSearchToken(query);
  if (normalizedQuery.isEmpty) {
    return prioritizeChatConversations(conversations);
  }
  return prioritizeChatConversations(
    conversations.where(
      (conversation) =>
          _conversationSearchIndex(conversation).contains(normalizedQuery),
    ),
  );
}

String _conversationSearchIndex(ConversationSummary conversation) {
  final fields = <String>[
    conversation.title,
    conversation.preview,
    conversation.draftText ?? '',
    conversation.lastSenderLabel ?? '',
    conversation.pillLabel ?? '',
    conversation.previewBadge ?? '',
    conversation.presenceLabel ?? '',
    conversation.membersPreview ?? '',
    conversation.kind.name,
  ];
  return _normalizeConversationSearchToken(fields.join(' '));
}

String _normalizeConversationSearchToken(String value) {
  return value.trim().toLowerCase().replaceAll(RegExp(r'\s+'), ' ');
}

List<ConversationSummary> prioritizeChatConversations(
  Iterable<ConversationSummary> conversations,
) {
  final items = conversations.toList();
  items.sort(compareChatConversationPriority);
  return items;
}

int compareChatConversationPriority(
  ConversationSummary left,
  ConversationSummary right,
) {
  final bucketDelta =
      _chatConversationPriorityBucket(left) -
      _chatConversationPriorityBucket(right);
  if (bucketDelta != 0) {
    return bucketDelta;
  }

  final updatedDelta = right.lastUpdated.compareTo(left.lastUpdated);
  if (updatedDelta != 0) {
    return updatedDelta;
  }

  if (left.unreadCount != right.unreadCount) {
    return right.unreadCount.compareTo(left.unreadCount);
  }

  return left.title.compareTo(right.title);
}

int _chatConversationPriorityBucket(ConversationSummary conversation) {
  if (conversation.isPinned) {
    return 0;
  }
  if (conversation.mentionCount > 0) {
    return 1;
  }
  if (conversation.unreadCount > 0) {
    return 2;
  }
  if (conversation.hasDraft) {
    return 3;
  }
  if (conversation.isMuted) {
    return 5;
  }
  return 4;
}

final contactsProvider = StreamProvider<List<ContactProfile>>((ref) {
  return ref.watch(sdkClientProvider).watchContacts();
});

final devicesProvider = StreamProvider<List<DeviceTrustInfo>>((ref) {
  return ref.watch(sdkClientProvider).watchDevices();
});

final activeConversationProvider = Provider<ConversationSummary?>((ref) {
  final conversations = ref
      .watch(conversationsProvider)
      .maybeWhen(
        data: (value) => value,
        orElse: () => const <ConversationSummary>[],
      );
  final selectedId = ref.watch(selectedConversationIdProvider);
  if (conversations.isEmpty) {
    return null;
  }
  if (selectedId == null) {
    return conversations.first;
  }
  for (final conversation in conversations) {
    if (conversation.id == selectedId) {
      return conversation;
    }
  }
  return conversations.first;
});

final activeMessagesProvider = StreamProvider<List<ChatMessage>>((ref) {
  final conversation = ref.watch(activeConversationProvider);
  final client = ref.watch(sdkClientProvider);
  if (conversation == null) {
    return Stream<List<ChatMessage>>.value(const <ChatMessage>[]);
  }
  return (() async* {
    ref
        .read(openedConversationUnreadSnapshotProvider.notifier)
        .remember(conversation.id, conversation.unreadCount);
    await client.markConversationRead(conversationId: conversation.id);
    yield* client.watchMessages(conversation.id);
  })();
});

final chatActionsProvider = Provider<ChatActions>((ref) {
  return ChatActions(ref);
});

final openedConversationUnreadSnapshotProvider =
    NotifierProvider<
      OpenedConversationUnreadSnapshotController,
      Map<String, int>
    >(OpenedConversationUnreadSnapshotController.new);

class ChatActions {
  const ChatActions(this.ref);

  final Ref ref;

  Future<void> sendCurrentConversationMessage(String text) async {
    final selectedConversationId = ref.read(selectedConversationIdProvider);
    final conversationId =
        selectedConversationId ?? ref.read(activeConversationProvider)?.id;
    if (conversationId == null) {
      return;
    }
    await sendConversationMessage(conversationId: conversationId, text: text);
  }

  Future<void> sendConversationMessage({
    required String conversationId,
    required String text,
  }) async {
    await ref
        .read(sdkClientProvider)
        .sendMessage(conversationId: conversationId, text: text);
  }

  Future<void> sendConversationFile({
    required String conversationId,
    required String path,
  }) async {
    await ref
        .read(sdkClientProvider)
        .sendFile(conversationId: conversationId, path: path);
  }

  Future<String?> createGroup() async {
    final conversationId = await ref.read(sdkClientProvider).createGroup();
    if (conversationId != null) {
      selectConversation(conversationId);
    }
    return conversationId;
  }

  Future<void> sendFriendRequest({
    required String accountId,
    String remark = '',
  }) async {
    final targetAccountId = accountId.trim();
    if (targetAccountId.isEmpty) {
      return;
    }
    await ref
        .read(sdkClientProvider)
        .sendFriendRequest(accountId: targetAccountId, remark: remark);
  }

  Future<void> clearConversationHistory({
    required String conversationId,
  }) async {
    await ref
        .read(sdkClientProvider)
        .clearConversationHistory(conversationId: conversationId);
  }

  void selectConversation(String? conversationId) {
    if (conversationId != null) {
      final conversations = ref
          .read(conversationsProvider)
          .maybeWhen(
            data: (value) => value,
            orElse: () => const <ConversationSummary>[],
          );
      ConversationSummary? selected;
      for (final conversation in conversations) {
        if (conversation.id == conversationId) {
          selected = conversation;
          break;
        }
      }
      if (selected != null) {
        ref
            .read(openedConversationUnreadSnapshotProvider.notifier)
            .remember(selected.id, selected.unreadCount);
      }
    }
    ref.read(selectedConversationIdProvider.notifier).select(conversationId);
  }
}

class OpenedConversationUnreadSnapshotController
    extends Notifier<Map<String, int>> {
  @override
  Map<String, int> build() => <String, int>{};

  void remember(String conversationId, int unreadCount) {
    if (conversationId.trim().isEmpty) {
      return;
    }
    if (state[conversationId] == unreadCount) {
      return;
    }
    state = <String, int>{...state, conversationId: unreadCount};
  }
}
