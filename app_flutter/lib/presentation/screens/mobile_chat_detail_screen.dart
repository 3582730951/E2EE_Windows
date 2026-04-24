import 'dart:ui';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/chat_providers.dart';
import '../../domain/entities/models.dart';
import '../theme/app_icons.dart';
import '../theme/app_tokens.dart';
import '../theme/app_theme.dart';
import '../widgets/components.dart' show AttachmentMessageCard;
import '../widgets/chat/message_action_sheet.dart';
import '../widgets/chat/reaction_bar.dart';
import '../widgets/social_avatar.dart';

part 'mobile_chat_detail_parts.dart';

class MobileChatDetailScreen extends ConsumerWidget {
  const MobileChatDetailScreen({
    super.key,
    required this.conversation,
    required this.messages,
    required this.onBack,
    this.autoScrollToLatest = true,
  });

  final ConversationSummary conversation;
  final List<ChatMessage> messages;
  final VoidCallback onBack;
  final bool autoScrollToLatest;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final openedUnreadSnapshots = ref.watch(
      openedConversationUnreadSnapshotProvider,
    );
    return _MobileChatDetailContent(
      conversation: conversation,
      messages: messages,
      onBack: onBack,
      autoScrollToLatest: autoScrollToLatest,
      openingUnreadCount:
          openedUnreadSnapshots[conversation.id] ?? conversation.unreadCount,
      onSend: (text) => ref
          .read(chatActionsProvider)
          .sendConversationMessage(conversationId: conversation.id, text: text),
    );
  }
}
