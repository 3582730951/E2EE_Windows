import 'dart:typed_data';

import '../domain/entities/models.dart';

class SdkEventType {
  const SdkEventType._();

  static const int chatText = 1;
  static const int chatFile = 2;
  static const int chatSticker = 3;
  static const int groupText = 4;
  static const int groupFile = 5;
  static const int groupInvite = 6;
  static const int groupNotice = 7;
  static const int outgoingText = 8;
  static const int outgoingFile = 9;
  static const int outgoingSticker = 10;
  static const int outgoingGroupText = 11;
  static const int outgoingGroupFile = 12;
  static const int delivery = 13;
  static const int readReceipt = 14;
  static const int typing = 15;
  static const int presence = 16;
  static const int groupCall = 17;
  static const int mediaRelay = 18;
  static const int groupMediaRelay = 19;
  static const int offlinePayload = 20;
}

class SdkHistoryKind {
  const SdkHistoryKind._();

  static const int text = 1;
  static const int file = 2;
  static const int sticker = 3;
  static const int system = 4;
}

class SdkHistoryStatus {
  const SdkHistoryStatus._();

  static const int sent = 0;
  static const int delivered = 1;
  static const int read = 2;
  static const int failed = 3;
}

class SdkFriendSnapshot {
  const SdkFriendSnapshot({required this.username, required this.remark});

  final String username;
  final String remark;
}

class SdkDeviceSnapshot {
  const SdkDeviceSnapshot({
    required this.deviceId,
    required this.displayId,
    required this.lastSeenSec,
  });

  final String deviceId;
  final String displayId;
  final int lastSeenSec;
}

class SdkHistorySnapshot {
  const SdkHistorySnapshot({
    required this.kind,
    required this.status,
    required this.isGroup,
    required this.outgoing,
    required this.timestampSec,
    required this.conversationKey,
    required this.sender,
    required this.messageId,
    this.text = '',
    this.fileId = '',
    this.fileKey,
    this.fileName = '',
    this.fileSize = 0,
    this.stickerId = '',
    this.canonicalEnvelope,
    this.messageType = 0,
  });

  final int kind;
  final int status;
  final bool isGroup;
  final bool outgoing;
  final int timestampSec;
  final String conversationKey;
  final String sender;
  final String messageId;
  final String text;
  final String fileId;
  final Uint8List? fileKey;
  final String fileName;
  final int fileSize;
  final String stickerId;
  final Uint8List? canonicalEnvelope;
  final int messageType;
}

class SdkEventSnapshot {
  const SdkEventSnapshot({
    required this.type,
    required this.timestampMs,
    this.peer = '',
    this.sender = '',
    this.groupId = '',
    this.messageId = '',
    this.text = '',
    this.fileId = '',
    this.fileKey,
    this.fileName = '',
    this.fileSize = 0,
    this.stickerId = '',
    this.typing = false,
    this.online = false,
    this.callId,
    this.callKeyId = 0,
    this.callOp = 0,
    this.callMediaFlags = 0,
    this.payload,
  });

  final int type;
  final int timestampMs;
  final String peer;
  final String sender;
  final String groupId;
  final String messageId;
  final String text;
  final String fileId;
  final Uint8List? fileKey;
  final String fileName;
  final int fileSize;
  final String stickerId;
  final bool typing;
  final bool online;
  final Uint8List? callId;
  final int callKeyId;
  final int callOp;
  final int callMediaFlags;
  final Uint8List? payload;
}

class ProjectedSdkEventMessage {
  const ProjectedSdkEventMessage({
    required this.message,
    required this.isGroup,
  });

  final ChatMessage message;
  final bool isGroup;
}

String directConversationId(String username) => username;

String groupConversationId(String groupId) => 'group:$groupId';

bool isGroupConversationId(String conversationId) {
  return conversationId.startsWith('group:');
}

String sdkConversationKey(String conversationId) {
  if (isGroupConversationId(conversationId)) {
    return conversationId.substring('group:'.length);
  }
  return conversationId;
}

ContactProfile contactFromFriend(
  SdkFriendSnapshot friend, {
  required bool online,
}) {
  final username = friend.username.trim();
  final remark = friend.remark.trim();
  final displayName = remark.isEmpty ? username : remark;
  return ContactProfile(
    id: username,
    displayName: displayName,
    handle: '@$username',
    statusLabel: online ? '在线' : '离线',
    online: online,
    avatarSeed: username,
    signature: online ? '在线' : '离线',
    relationLabel: '好友',
  );
}

DeviceTrustInfo deviceFromSnapshot(
  SdkDeviceSnapshot device, {
  required String currentDeviceId,
  required DateTime now,
}) {
  final displayName = device.displayId.trim().isEmpty
      ? device.deviceId
      : device.displayId.trim();
  return DeviceTrustInfo(
    name: displayName,
    fingerprint: _formatFingerprint(device.deviceId),
    lastSeenLabel: _relativeLastSeenLabel(now, device.lastSeenSec),
    trustLabel: device.deviceId == currentDeviceId ? '当前设备' : '已链接',
  );
}

ChatMessage historyToMessage(SdkHistorySnapshot entry) {
  final isGroup = entry.isGroup;
  final conversationId = isGroup
      ? groupConversationId(entry.conversationKey)
      : directConversationId(entry.conversationKey);
  final direction = entry.outgoing
      ? MessageDirection.outgoing
      : MessageDirection.incoming;
  return ChatMessage(
    id: entry.messageId,
    conversationId: conversationId,
    senderLabel: entry.outgoing
        ? '你'
        : (entry.sender.isEmpty ? entry.conversationKey : entry.sender),
    text: _messageText(
      kind: entry.kind,
      text: entry.text,
      fileName: entry.fileName,
      stickerId: entry.stickerId,
    ),
    timestamp: DateTime.fromMillisecondsSinceEpoch(entry.timestampSec * 1000),
    direction: direction,
    status: _deliveryStatus(entry.status),
    attachment: _historyAttachment(entry),
  );
}

ProjectedSdkEventMessage? eventToMessage(SdkEventSnapshot event) {
  final isGroup = switch (event.type) {
    SdkEventType.groupText ||
    SdkEventType.groupFile ||
    SdkEventType.groupInvite ||
    SdkEventType.groupNotice ||
    SdkEventType.outgoingGroupText ||
    SdkEventType.outgoingGroupFile => true,
    _ => false,
  };
  final isOutgoing = switch (event.type) {
    SdkEventType.outgoingText ||
    SdkEventType.outgoingFile ||
    SdkEventType.outgoingSticker ||
    SdkEventType.outgoingGroupText ||
    SdkEventType.outgoingGroupFile => true,
    _ => false,
  };
  final conversationId = switch (event.type) {
    SdkEventType.chatText ||
    SdkEventType.chatFile ||
    SdkEventType.chatSticker ||
    SdkEventType.outgoingText ||
    SdkEventType.outgoingFile ||
    SdkEventType.outgoingSticker => directConversationId(
      isOutgoing ? event.peer : event.peer,
    ),
    SdkEventType.groupText ||
    SdkEventType.groupFile ||
    SdkEventType.groupInvite ||
    SdkEventType.groupNotice ||
    SdkEventType.outgoingGroupText ||
    SdkEventType.outgoingGroupFile => groupConversationId(event.groupId),
    _ => '',
  };
  if (conversationId.isEmpty) {
    return null;
  }

  final text = switch (event.type) {
    SdkEventType.chatFile ||
    SdkEventType.groupFile ||
    SdkEventType.outgoingFile ||
    SdkEventType.outgoingGroupFile => '[文件] ${event.fileName}'.trim(),
    SdkEventType.chatSticker ||
    SdkEventType.outgoingSticker => '[贴纸] ${event.stickerId}'.trim(),
    SdkEventType.groupInvite => '邀请加入群聊',
    SdkEventType.groupNotice => '群组状态已更新',
    _ => event.text,
  };
  return ProjectedSdkEventMessage(
    isGroup: isGroup,
    message: ChatMessage(
      id: event.messageId,
      conversationId: conversationId,
      senderLabel: isOutgoing
          ? '你'
          : (event.sender.isEmpty
                ? (isGroup ? event.groupId : event.peer)
                : event.sender),
      text: text,
      timestamp: DateTime.fromMillisecondsSinceEpoch(event.timestampMs),
      direction: isOutgoing
          ? MessageDirection.outgoing
          : MessageDirection.incoming,
      status: isOutgoing
          ? MessageDeliveryStatus.sent
          : MessageDeliveryStatus.delivered,
      attachment: _eventAttachment(event),
    ),
  );
}

ConversationSummary buildConversationSummary({
  required String conversationId,
  required String title,
  required List<ChatMessage> messages,
  required bool isGroup,
  bool isOnline = false,
  bool isTyping = false,
}) {
  final latest = messages.isEmpty
      ? null
      : (List<ChatMessage>.from(
              messages,
            )..sort((left, right) => right.timestamp.compareTo(left.timestamp)))
            .first;
  return ConversationSummary(
    id: conversationId,
    title: title,
    preview: _conversationPreview(latest, isGroup: isGroup),
    lastUpdated: latest?.timestamp ?? DateTime.fromMillisecondsSinceEpoch(0),
    kind: isGroup ? ConversationKind.group : ConversationKind.direct,
    isGroup: isGroup,
    isOnline: isOnline,
    isTyping: isTyping,
    avatarSeed: conversationId,
    avatarInitials: _projectionInitials(title),
    presenceLabel: isGroup
        ? '群聊'
        : isOnline
        ? '在线'
        : null,
    membersPreview: isGroup ? '群成员' : null,
    lastActivityVerb: latest == null ? null : '发来消息',
    backendKind: isGroup ? 'group' : 'direct',
    serverConversationKey: sdkConversationKey(conversationId),
    lastMessageId: latest?.id,
  );
}

ChatAttachment? _historyAttachment(SdkHistorySnapshot entry) {
  if (entry.kind != SdkHistoryKind.file) {
    return null;
  }
  return _fileAttachment(
    fileId: entry.fileId,
    fileKey: entry.fileKey,
    fileName: entry.fileName,
    fileSize: entry.fileSize,
    completed: true,
  );
}

ChatAttachment? _eventAttachment(SdkEventSnapshot event) {
  final isFile =
      event.type == SdkEventType.chatFile ||
      event.type == SdkEventType.groupFile ||
      event.type == SdkEventType.outgoingFile ||
      event.type == SdkEventType.outgoingGroupFile;
  if (!isFile) {
    return null;
  }
  return _fileAttachment(
    fileId: event.fileId,
    fileKey: event.fileKey,
    fileName: event.fileName,
    fileSize: event.fileSize,
    completed:
        event.type == SdkEventType.chatFile ||
        event.type == SdkEventType.groupFile,
  );
}

ChatAttachment _fileAttachment({
  required String fileId,
  required Uint8List? fileKey,
  required String fileName,
  required int fileSize,
  required bool completed,
}) {
  final title = fileName.trim().isEmpty ? '未命名文件' : fileName.trim();
  final kind = _attachmentKindFromName(title);
  return ChatAttachment(
    kind: kind,
    title: title,
    detail: _fileSizeLabel(fileSize),
    actionLabel: completed ? '下载' : '发送中',
    state: completed
        ? ChatAttachmentState.completed
        : ChatAttachmentState.uploading,
    fileExtension: _fileExtension(title).toUpperCase(),
    fileId: fileId.trim().isEmpty ? null : fileId.trim(),
    fileKey: fileKey,
    fileSizeBytes: fileSize > 0 ? fileSize : null,
    downloadable: fileId.trim().isNotEmpty && fileKey != null,
  );
}

String? _projectionInitials(String title) {
  final trimmed = title.trim();
  if (trimmed.isEmpty) {
    return null;
  }
  return String.fromCharCodes(trimmed.runes.take(2)).toUpperCase();
}

MessageDeliveryStatus _deliveryStatus(int status) {
  return switch (status) {
    SdkHistoryStatus.sent => MessageDeliveryStatus.sent,
    SdkHistoryStatus.delivered => MessageDeliveryStatus.delivered,
    SdkHistoryStatus.read => MessageDeliveryStatus.read,
    SdkHistoryStatus.failed => MessageDeliveryStatus.failed,
    _ => MessageDeliveryStatus.sent,
  };
}

String _messageText({
  required int kind,
  required String text,
  required String fileName,
  required String stickerId,
}) {
  return switch (kind) {
    SdkHistoryKind.file => _fileMessageLabel(fileName),
    SdkHistoryKind.sticker => '[贴纸] $stickerId'.trim(),
    SdkHistoryKind.system => text.isEmpty ? '系统消息' : text,
    _ => text,
  };
}

String _fileMessageLabel(String fileName) {
  final normalized = fileName.trim();
  if (normalized.isEmpty) {
    return '[文件]';
  }
  final dotIndex = normalized.lastIndexOf('.');
  final extension = dotIndex == -1
      ? ''
      : normalized.substring(dotIndex + 1).toLowerCase();
  if (_imageExtensions.contains(extension)) {
    return '[图片]';
  }
  if (_videoExtensions.contains(extension)) {
    return '[视频]';
  }
  if (_audioExtensions.contains(extension)) {
    return '[语音]';
  }
  return '[文件] $normalized'.trim();
}

ChatAttachmentKind _attachmentKindFromName(String fileName) {
  final extension = _fileExtension(fileName);
  if (_imageExtensions.contains(extension)) {
    return ChatAttachmentKind.image;
  }
  if (_videoExtensions.contains(extension)) {
    return ChatAttachmentKind.video;
  }
  if (_audioExtensions.contains(extension)) {
    return ChatAttachmentKind.audio;
  }
  return ChatAttachmentKind.file;
}

String _fileExtension(String fileName) {
  final normalized = fileName.trim();
  final dotIndex = normalized.lastIndexOf('.');
  if (dotIndex == -1 || dotIndex == normalized.length - 1) {
    return '';
  }
  return normalized.substring(dotIndex + 1).toLowerCase();
}

String _fileSizeLabel(int size) {
  if (size <= 0) {
    return '未知大小';
  }
  const units = <String>['B', 'KB', 'MB', 'GB'];
  var value = size.toDouble();
  var unitIndex = 0;
  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex += 1;
  }
  final text = unitIndex == 0
      ? value.toStringAsFixed(0)
      : value.toStringAsFixed(1);
  return '$text ${units[unitIndex]}';
}

const Set<String> _imageExtensions = <String>{
  'png',
  'jpg',
  'jpeg',
  'gif',
  'webp',
  'bmp',
  'heic',
};

const Set<String> _videoExtensions = <String>{'mp4', 'mov', 'm4v', 'webm'};

const Set<String> _audioExtensions = <String>{
  'm4a',
  'aac',
  'mp3',
  'wav',
  'ogg',
  'opus',
  'amr',
};

String _conversationPreview(ChatMessage? latest, {required bool isGroup}) {
  if (latest == null) {
    return '开始新对话';
  }
  final previewBody = _conversationPreviewBody(latest);
  if (isGroup) {
    return '${latest.senderLabel}: $previewBody';
  }
  return previewBody;
}

String _conversationPreviewBody(ChatMessage latest) {
  final trimmedText = latest.text.trim();
  if (trimmedText.isNotEmpty) {
    return trimmedText;
  }
  if (latest.attachment case final attachment?) {
    return _attachmentConversationPreview(attachment);
  }
  return '新消息';
}

String _attachmentConversationPreview(ChatAttachment attachment) {
  final resolvedKind = _resolvedAttachmentKind(attachment);
  if (resolvedKind == ChatAttachmentKind.image ||
      resolvedKind == ChatAttachmentKind.video) {
    return _visualMediaConversationPreview(
      kind: resolvedKind,
      sensitivity: attachment.sensitivity,
    );
  }
  if (resolvedKind == ChatAttachmentKind.audio) {
    return '[语音]';
  }
  final normalizedTitle = attachment.title.trim();
  if (normalizedTitle.isEmpty) {
    return '[文件]';
  }
  return '[文件] $normalizedTitle';
}

String _visualMediaConversationPreview({
  required ChatAttachmentKind kind,
  required ChatAttachmentSensitivity sensitivity,
}) {
  final noun = kind == ChatAttachmentKind.video ? '视频' : '图片';
  return '[$noun]';
}

ChatAttachmentKind _resolvedAttachmentKind(ChatAttachment attachment) {
  if (attachment.kind != ChatAttachmentKind.file) {
    return attachment.kind;
  }
  final normalizedTitle = attachment.title.trim();
  final dotIndex = normalizedTitle.lastIndexOf('.');
  final extension = dotIndex == -1
      ? ''
      : normalizedTitle.substring(dotIndex + 1).toLowerCase();
  if (_imageExtensions.contains(extension)) {
    return ChatAttachmentKind.image;
  }
  if (_videoExtensions.contains(extension)) {
    return ChatAttachmentKind.video;
  }
  if (_audioExtensions.contains(extension)) {
    return ChatAttachmentKind.audio;
  }
  return ChatAttachmentKind.file;
}

String _relativeLastSeenLabel(DateTime now, int lastSeenSec) {
  final nowSec = now.millisecondsSinceEpoch ~/ 1000;
  final diff = (nowSec - lastSeenSec).clamp(0, 1 << 31);
  if (diff < 60) {
    return '刚刚活跃';
  }
  if (diff < 3600) {
    return '${diff ~/ 60} 分钟前';
  }
  if (diff < 86400) {
    return '${diff ~/ 3600} 小时前';
  }
  return '${diff ~/ 86400} 天前';
}

String _formatFingerprint(String raw) {
  final normalized = raw.trim().toUpperCase();
  if (normalized.isEmpty) {
    return '未知设备';
  }
  final chunks = <String>[];
  for (var index = 0; index < normalized.length; index += 4) {
    final end = (index + 4 < normalized.length) ? index + 4 : normalized.length;
    chunks.add(normalized.substring(index, end));
  }
  return chunks.join(' ');
}
