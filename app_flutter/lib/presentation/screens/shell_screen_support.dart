import '../../domain/entities/models.dart';

bool isFileConversation(ConversationSummary? conversation) {
  if (conversation == null) {
    return false;
  }
  return conversation.isFileConversation ||
      conversation.kind == ConversationKind.fileAssistant ||
      conversation.pillLabel == '文件' ||
      conversation.id == 'file-helper';
}
