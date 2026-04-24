import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/domain/entities/models.dart';

void main() {
  test('conversation summary keeps old constructor defaults compatible', () {
    final summary = ConversationSummary(
      id: 'chat-bob',
      title: 'Bob',
      preview: 'hello',
      lastUpdated: DateTime(2026, 4, 21, 9),
    );

    expect(summary.kind, ConversationKind.direct);
    expect(summary.isArchived, isFalse);
    expect(summary.isVerified, isFalse);
    expect(summary.isOfficial, isFalse);
    expect(summary.memberCount, 0);
    expect(summary.onlineCount, 0);
    expect(summary.statusEmoji, isNull);
    expect(summary.lastSenderLabel, isNull);
    expect(summary.avatarSeed, isNull);
    expect(summary.avatarInitials, isNull);
    expect(summary.presenceLabel, isNull);
    expect(summary.membersPreview, isNull);
    expect(summary.isServiceAccount, isFalse);
    expect(summary.lastActivityVerb, isNull);
  });

  test('chat message social fields default to empty values', () {
    final message = ChatMessage(
      id: 'm1',
      conversationId: 'chat-bob',
      senderLabel: 'Bob',
      text: 'hello',
      timestamp: DateTime(2026, 4, 21, 9),
      direction: MessageDirection.incoming,
      status: MessageDeliveryStatus.delivered,
    );

    expect(message.replyPreview, isNull);
    expect(message.reactions, isEmpty);
    expect(message.isEdited, isFalse);
    expect(message.isForwarded, isFalse);
  });

  test('chat attachments keep media defaults compatible', () {
    const attachment = ChatAttachment(
      kind: ChatAttachmentKind.file,
      title: 'report.pdf',
      detail: '2 MB',
      actionLabel: '打开',
      state: ChatAttachmentState.completed,
    );

    expect(attachment.fileExtension, isNull);
    expect(attachment.thumbnailSeed, isNull);
    expect(attachment.caption, isNull);
    expect(attachment.durationLabel, isNull);
    expect(attachment.waveformSeed, isNull);
    expect(attachment.albumCount, 1);
  });

  test('contact profile social fields default to friend semantics', () {
    const contact = ContactProfile(
      id: 'bob',
      displayName: 'Bob',
      handle: '@bob',
      statusLabel: '在线',
    );

    expect(contact.groupLabel, '我的好友');
    expect(contact.statusEmoji, isNull);
    expect(contact.lastSeenLabel, isNull);
    expect(contact.mutualGroupCount, 0);
    expect(contact.avatarSeed, isNull);
    expect(contact.signature, isNull);
    expect(contact.relationLabel, isNull);
    expect(contact.requestStatus, isNull);
  });
}
