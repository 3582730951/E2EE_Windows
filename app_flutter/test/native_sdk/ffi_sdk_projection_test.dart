import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/domain/entities/models.dart';
import 'package:mi_e2ee_im_app/native_sdk/ffi_sdk_projection.dart';

void main() {
  test('maps friend snapshots into contact rows', () {
    final contact = contactFromFriend(
      const SdkFriendSnapshot(username: 'bob', remark: 'Bob Chen'),
      online: true,
    );

    expect(contact.id, 'bob');
    expect(contact.displayName, 'Bob Chen');
    expect(contact.handle, '@bob');
    expect(contact.statusLabel, '在线');
    expect(contact.online, isTrue);
  });

  test('maps device snapshots into trust cards with readable time', () {
    final device = deviceFromSnapshot(
      const SdkDeviceSnapshot(
        deviceId: 'A1B2C3D4E5F6',
        displayId: 'Windows 主端',
        lastSeenSec: 60 * 5,
      ),
      currentDeviceId: 'A1B2C3D4E5F6',
      now: DateTime.fromMillisecondsSinceEpoch(60 * 60 * 1000),
    );

    expect(device.name, 'Windows 主端');
    expect(device.fingerprint, 'A1B2 C3D4 E5F6');
    expect(device.lastSeenLabel, '55 分钟前');
    expect(device.trustLabel, '当前设备');
  });

  test('maps history entries into chat messages and delivery states', () {
    final message = historyToMessage(
      const SdkHistorySnapshot(
        kind: SdkHistoryKind.file,
        status: SdkHistoryStatus.delivered,
        isGroup: false,
        outgoing: false,
        timestampSec: 1_710_000_000,
        conversationKey: 'bob',
        sender: 'bob',
        messageId: 'm-1',
        fileName: 'spec.pdf',
      ),
    );

    expect(message.id, 'm-1');
    expect(message.conversationId, 'bob');
    expect(message.senderLabel, 'bob');
    expect(message.text, '[文件] spec.pdf');
    expect(message.direction, MessageDirection.incoming);
    expect(message.status, MessageDeliveryStatus.delivered);
    expect(
      message.timestamp,
      DateTime.fromMillisecondsSinceEpoch(1710000000000),
    );
  });

  test('maps live group events into outgoing chat messages', () {
    final projected = eventToMessage(
      const SdkEventSnapshot(
        type: SdkEventType.outgoingGroupText,
        timestampMs: 1_710_000_321_000,
        groupId: 'design',
        messageId: 'g-1',
        text: '今晚把群聊资料页也收一下。',
      ),
    );

    expect(projected, isNotNull);
    expect(projected!.isGroup, isTrue);
    expect(projected.message.conversationId, 'group:design');
    expect(projected.message.senderLabel, '你');
    expect(projected.message.text, '今晚把群聊资料页也收一下。');
    expect(projected.message.direction, MessageDirection.outgoing);
    expect(projected.message.status, MessageDeliveryStatus.sent);
  });

  test('builds conversation summaries from latest message state', () {
    final summary = buildConversationSummary(
      conversationId: 'group:design',
      title: 'Aurora 设计群',
      isGroup: true,
      isTyping: true,
      messages: <ChatMessage>[
        ChatMessage(
          id: '1',
          conversationId: 'group:design',
          senderLabel: 'Mina',
          text: '新版本动效已经收敛了。',
          timestamp: DateTime.fromMillisecondsSinceEpoch(1710000000000),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.read,
        ),
      ],
    );

    expect(summary.id, 'group:design');
    expect(summary.title, 'Aurora 设计群');
    expect(summary.preview, 'Mina: 新版本动效已经收敛了。');
    expect(
      summary.lastUpdated,
      DateTime.fromMillisecondsSinceEpoch(1710000000000),
    );
    expect(summary.isGroup, isTrue);
    expect(summary.isTyping, isTrue);
  });

  test('builds spoiler image summaries with protected media copy', () {
    final summary = buildConversationSummary(
      conversationId: 'dm:mina',
      title: 'Mina',
      isGroup: false,
      messages: <ChatMessage>[
        ChatMessage(
          id: '2',
          conversationId: 'dm:mina',
          senderLabel: 'Mina',
          text: '',
          timestamp: DateTime.fromMillisecondsSinceEpoch(1710000001000),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.image,
            title: 'hero-cover-preview.png',
            detail: '2.4 MB · 图片',
            actionLabel: '查看',
            state: ChatAttachmentState.completed,
            sensitivity: ChatAttachmentSensitivity.spoiler,
          ),
        ),
      ],
    );

    expect(summary.preview, '[图片]');
  });

  test('builds group view-once video summaries with sender prefix', () {
    final summary = buildConversationSummary(
      conversationId: 'group:design',
      title: 'Aurora 设计群',
      isGroup: true,
      messages: <ChatMessage>[
        ChatMessage(
          id: '3',
          conversationId: 'group:design',
          senderLabel: 'Leo',
          text: '',
          timestamp: DateTime.fromMillisecondsSinceEpoch(1710000002000),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.video,
            title: 'teaser-cut.mp4',
            detail: '8.2 MB · 视频',
            actionLabel: '查看',
            state: ChatAttachmentState.completed,
            sensitivity: ChatAttachmentSensitivity.viewOnce,
          ),
        ),
      ],
    );

    expect(summary.preview, 'Leo: [视频]');
  });
}
