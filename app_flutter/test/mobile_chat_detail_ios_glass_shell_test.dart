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
  testWidgets('keeps iOS chat chrome matte and separated from the timeline', (
    WidgetTester tester,
  ) async {
    final previousPlatform = debugDefaultTargetPlatformOverride;
    debugDefaultTargetPlatformOverride = TargetPlatform.iOS;
    tester.view.physicalSize = const Size(393, 852);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    try {
      final theme = AppTheme.light(visualTier: VisualTier.enhancedGlass);
      final conversation = ConversationSummary(
        id: 'chat-ios-shell-tighten-01',
        title: 'Mina Xu',
        preview: '控制层继续收薄。',
        lastUpdated: DateTime(2026, 4, 20, 9, 12),
        isOnline: true,
      );
      final messages = <ChatMessage>[
        ChatMessage(
          id: 'ios-shell-tighten-1',
          conversationId: 'chat-ios-shell-tighten-01',
          senderLabel: 'Mina Xu',
          text: '把玻璃高光和阴影都再收一点，消息主体也别再铺得太满，附件插入后要更像成熟 IM 的内容块。',
          timestamp: DateTime(2026, 4, 20, 9, 5),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.read,
        ),
        ChatMessage(
          id: 'ios-shell-tighten-1b',
          conversationId: 'chat-ios-shell-tighten-01',
          senderLabel: '系统',
          text: '已验证新设备会话安全性',
          timestamp: DateTime(2026, 4, 20, 9, 7),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.read,
        ),
        ChatMessage(
          id: 'ios-shell-tighten-2',
          conversationId: 'chat-ios-shell-tighten-01',
          senderLabel: '我',
          text: '收到，消息层继续保持深色哑光，时间戳和发送状态只做轻提示，不和正文抢权重。',
          timestamp: DateTime(2026, 4, 20, 9, 9),
          direction: MessageDirection.outgoing,
          status: MessageDeliveryStatus.delivered,
        ),
      ];

      await tester.pumpWidget(
        ProviderScope(
          child: MaterialApp(
            theme: theme,
            home: Scaffold(
              body: MobileChatDetailScreen(
                conversation: conversation,
                messages: messages,
                onBack: () {},
                autoScrollToLatest: false,
              ),
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final headerFinder = find.byKey(
        const Key('mobile-chat-detail-header-matte-shell'),
      );
      final composerFinder = find.byKey(
        const Key('mobile-chat-composer-shell'),
      );
      final centeredTitleFinder = find.byKey(
        const Key('mobile-chat-detail-title-center'),
      );
      final centeredSubtitleFinder = find.byKey(
        const Key('mobile-chat-detail-subtitle-center'),
      );
      final inputFinder = find.byKey(
        const Key('mobile-chat-composer-input-cupertino'),
      );
      final timelineFinder = find.byKey(const Key('mobile-chat-timeline'));

      final timeline = tester.widget<ListView>(timelineFinder);
      final timelinePadding = timeline.padding! as EdgeInsets;
      final bubbles = tester.widgetList<ConstrainedBox>(
        find.ancestor(
          of: find.byKey(const Key('mobile-chat-bubble-cupertino')),
          matching: find.byType(ConstrainedBox),
        ),
      );
      final titleCenter = tester.getCenter(centeredTitleFinder);
      final headerCenter = tester.getCenter(headerFinder);
      final outgoingMeta = tester.getSize(
        find.byKey(const Key('mobile-chat-message-meta')).last,
      );
      final timeSeparator = tester.getSize(
        find.byKey(const Key('mobile-chat-time-separator')),
      );
      final systemNotice = tester.getSize(
        find.byKey(const ValueKey<String>('mobile-chat-notice-security')),
      );

      expect(headerFinder, findsOneWidget);
      expect(
        find.byKey(const Key('mobile-chat-detail-header-glass-shell')),
        findsNothing,
      );
      expect(
        find.descendant(
          of: headerFinder,
          matching: find.byType(LiquidGlassPanel),
        ),
        findsNothing,
      );
      expect(
        find.ancestor(
          of: headerFinder,
          matching: find.byType(LiquidGlassPanel),
        ),
        findsNothing,
      );
      expect(
        find.byKey(const Key('mobile-chat-composer-glass-shell')),
        findsNothing,
      );
      expect(
        find.descendant(
          of: composerFinder,
          matching: find.byType(BackdropFilter),
        ),
        findsNothing,
      );
      expect(centeredTitleFinder, findsOneWidget);
      expect(centeredSubtitleFinder, findsOneWidget);
      expect(inputFinder, findsOneWidget);
      expect(
        find.descendant(
          of: headerFinder,
          matching: find.byKey(const Key('mobile-chat-detail-header-avatar')),
        ),
        findsNothing,
      );

      expect(timelinePadding.top, lessThan(66));
      expect(timelinePadding.bottom, lessThan(84));
      expect(tester.getSize(inputFinder).height, lessThanOrEqualTo(40));
      expect((titleCenter.dx - headerCenter.dx).abs(), lessThanOrEqualTo(18));
      expect(
        bubbles.every((bubble) => bubble.constraints.maxWidth <= 272),
        isTrue,
      );
      expect(outgoingMeta.height, lessThanOrEqualTo(11));
      expect(timeSeparator.height, lessThanOrEqualTo(17));
      expect(systemNotice.height, lessThanOrEqualTo(21));
      expect(
        find.descendant(
          of: timelineFinder,
          matching: find.byType(LiquidGlassPanel),
        ),
        findsNothing,
      );
      expect(
        find.descendant(
          of: timelineFinder,
          matching: find.byType(BackdropFilter),
        ),
        findsNothing,
      );
      expect(
        tester
            .getSize(
              find.byKey(const Key('mobile-chat-composer-shortcut-image')),
            )
            .width,
        lessThanOrEqualTo(35),
      );
    } finally {
      debugDefaultTargetPlatformOverride = previousPlatform;
    }
  });
}
