import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/domain/entities/models.dart';
import 'package:mi_e2ee_im_app/presentation/screens/mobile_chat_detail_screen.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';
import 'package:mi_e2ee_im_app/presentation/theme/visual_tier.dart';
import 'package:mi_e2ee_im_app/presentation/widgets/shell_chrome.dart';

void main() {
  testWidgets('chat detail exposes title, online info and rich composer', (
    WidgetTester tester,
  ) async {
    await _pumpDetail(
      tester,
      TargetPlatform.android,
      visualTier: VisualTier.enhancedGlass,
    );

    expect(find.byKey(const Key('mobile-chat-detail-header')), findsOneWidget);
    expect(find.text('Aurora 设计群'), findsOneWidget);
    expect(find.text('128 位成员 · 19 在线'), findsOneWidget);
    expect(find.byKey(const Key('mobile-chat-header-search')), findsOneWidget);
    expect(find.byKey(const Key('mobile-chat-header-more')), findsOneWidget);
    final timelineFinder = find.byKey(const Key('mobile-chat-timeline'));
    final timeline = tester.widget<ListView>(timelineFinder);
    final timelinePadding = timeline.padding! as EdgeInsets;
    expect(tester.getTopLeft(timelineFinder).dy, lessThanOrEqualTo(46));
    expect(timelinePadding.top, lessThanOrEqualTo(8));
    expect(
      find.byKey(const Key('mobile-chat-top-boundary-mask-material')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-chat-composer-shortcut-image')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-chat-composer-shortcut-file')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-chat-composer-shortcut-emoji')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-chat-composer-shortcut-voice')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-chat-composer-action-send')),
      findsNothing,
    );
    expect(find.byType(LiquidGlassPanel), findsNothing);

    await tester.enterText(
      find.byKey(const Key('mobile-chat-composer-input-material')),
      '收到',
    );
    await tester.pumpAndSettle();

    expect(
      find.byKey(const Key('mobile-chat-composer-action-send')),
      findsOneWidget,
    );
  });

  testWidgets('messages render reply previews reactions and action sheet', (
    WidgetTester tester,
  ) async {
    await _pumpDetail(tester, TargetPlatform.android);

    expect(find.byKey(const Key('mobile-chat-reply-strip')), findsOneWidget);
    expect(find.byKey(const Key('message-reaction-bar')), findsOneWidget);
    expect(find.textContaining('已编辑'), findsOneWidget);
    expect(find.text('转发'), findsOneWidget);

    await tester.longPress(
      find.byKey(const Key('mobile-chat-bubble-material')).first,
    );
    await tester.pumpAndSettle();

    expect(find.text('回复'), findsOneWidget);
    expect(find.text('转发'), findsWidgets);
    expect(find.text('复制'), findsOneWidget);
    expect(find.text('删除'), findsOneWidget);
    expect(find.text('更多表情'), findsOneWidget);
  });

  testWidgets('iOS uses cupertino header and composer fields', (
    WidgetTester tester,
  ) async {
    await _pumpDetail(tester, TargetPlatform.iOS);

    expect(
      find.byKey(const Key('mobile-chat-detail-header-cupertino')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-chat-composer-input-cupertino')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-chat-top-boundary-mask-material')),
      findsNothing,
    );
    expect(find.textContaining('端到端'), findsNothing);
  });
}

Future<void> _pumpDetail(
  WidgetTester tester,
  TargetPlatform platform, {
  VisualTier visualTier = VisualTier.standard,
}) async {
  final previousPlatform = debugDefaultTargetPlatformOverride;
  debugDefaultTargetPlatformOverride = platform;
  try {
    tester.view.physicalSize = platform == TargetPlatform.iOS
        ? const Size(393, 852)
        : const Size(412, 915);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(
      ProviderScope(
        child: MaterialApp(
          theme: AppTheme.light(visualTier: visualTier),
          home: Scaffold(
            body: MobileChatDetailScreen(
              conversation: ConversationSummary(
                id: 'group-design',
                title: 'Aurora 设计群',
                preview: '你: [图片]',
                lastUpdated: DateTime(2026, 4, 21, 9),
                kind: ConversationKind.group,
                isGroup: true,
                memberCount: 128,
                onlineCount: 19,
              ),
              messages: _messages(),
              onBack: () {},
              autoScrollToLatest: false,
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();
  } finally {
    debugDefaultTargetPlatformOverride = previousPlatform;
  }
}

List<ChatMessage> _messages() {
  final now = DateTime(2026, 4, 21, 9);
  return <ChatMessage>[
    ChatMessage(
      id: 'm1',
      conversationId: 'group-design',
      senderLabel: 'Mina',
      text: '我把说明补上。',
      timestamp: now,
      direction: MessageDirection.incoming,
      status: MessageDeliveryStatus.read,
      isEdited: true,
      reactions: const <MessageReactionSummary>[
        MessageReactionSummary(emoji: '👍', count: 3),
      ],
    ),
    ChatMessage(
      id: 'm2',
      conversationId: 'group-design',
      senderLabel: 'Leo',
      text: '新版附件卡整理好了。',
      timestamp: now.add(const Duration(minutes: 1)),
      direction: MessageDirection.incoming,
      status: MessageDeliveryStatus.delivered,
      isForwarded: true,
      attachment: const ChatAttachment(
        kind: ChatAttachmentKind.file,
        title: '聊天详情规范.pdf',
        detail: '5.8 MB · PDF',
        actionLabel: '打开',
        state: ChatAttachmentState.completed,
      ),
    ),
    ChatMessage(
      id: 'm3',
      conversationId: 'group-design',
      senderLabel: '你',
      text: '可以，Android 截图就出这版。',
      timestamp: now.add(const Duration(minutes: 2)),
      direction: MessageDirection.outgoing,
      status: MessageDeliveryStatus.read,
      replyPreview: const MessageReplyPreview(
        senderLabel: 'Mina',
        text: '我把说明补上。',
      ),
    ),
  ];
}
