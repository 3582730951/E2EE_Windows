import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/domain/entities/models.dart';
import '../fakes/fake_sdk_client.dart';

void main() {
  test(
    'markConversationRead clears unread summary and refreshes message states',
    () async {
      final client = FakeSdkClient();
      addTearDown(client.dispose);

      await client.initialize();

      final before = await client.watchConversations().first;
      final beforeSummary = before.firstWhere(
        (conversation) => conversation.id == 'chat-bob',
      );
      expect(beforeSummary.unreadCount, 2);

      final beforeMessages = await client.watchMessages('chat-bob').first;
      expect(beforeMessages.last.status, MessageDeliveryStatus.delivered);

      await client.markConversationRead(conversationId: 'chat-bob');

      final after = await client.watchConversations().first;
      final afterSummary = after.firstWhere(
        (conversation) => conversation.id == 'chat-bob',
      );
      expect(afterSummary.unreadCount, 0);
      expect(afterSummary.mentionCount, 0);

      final afterMessages = await client.watchMessages('chat-bob').first;
      expect(afterMessages.last.status, MessageDeliveryStatus.read);
      expect(
        afterMessages
            .where((message) => message.direction == MessageDirection.incoming)
            .every((message) => message.status == MessageDeliveryStatus.read),
        isTrue,
      );
    },
  );
}
