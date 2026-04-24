import 'dart:typed_data';

enum MessageDirection { incoming, outgoing }

enum MessageDeliveryStatus { sending, sent, delivered, read, failed }

enum ChatAttachmentKind { file, image, video }

enum ConversationKind { direct, group, channel, bot, system, fileAssistant }

enum ConversationFilterPreset { all, unread, mentions, groups, channels, files }

enum ChatAttachmentSensitivity { none, spoiler, viewOnce }

enum ChatAttachmentState {
  pendingSend,
  uploading,
  downloading,
  completed,
  failed,
  expired,
  offlineQueued,
  retryReady,
}

class SessionProfile {
  const SessionProfile({
    required this.username,
    required this.displayName,
    required this.subtitle,
  });

  final String username;
  final String displayName;
  final String subtitle;
}

class ConversationSummary {
  const ConversationSummary({
    required this.id,
    required this.title,
    required this.preview,
    required this.lastUpdated,
    this.kind = ConversationKind.direct,
    this.unreadCount = 0,
    this.mentionCount = 0,
    this.isMuted = false,
    this.isPinned = false,
    this.isTyping = false,
    this.isOnline = false,
    this.isGroup = false,
    this.isArchived = false,
    this.isVerified = false,
    this.isOfficial = false,
    this.memberCount = 0,
    this.onlineCount = 0,
    this.statusEmoji,
    this.lastSenderLabel,
    this.draftText,
    this.pillLabel,
    this.previewBadge,
    this.avatarSeed,
    this.avatarInitials,
    this.presenceLabel,
    this.membersPreview,
    this.isServiceAccount = false,
    this.lastActivityVerb,
    this.backendKind,
    this.serverConversationKey,
    this.lastMessageId,
    this.unreadLocalWatermark = 0,
    this.memberRole,
    this.callState,
  });

  final String id;
  final String title;
  final String preview;
  final DateTime lastUpdated;
  final ConversationKind kind;
  final int unreadCount;
  final int mentionCount;
  final bool isMuted;
  final bool isPinned;
  final bool isTyping;
  final bool isOnline;
  final bool isGroup;
  final bool isArchived;
  final bool isVerified;
  final bool isOfficial;
  final int memberCount;
  final int onlineCount;
  final String? statusEmoji;
  final String? lastSenderLabel;
  final String? draftText;
  final String? pillLabel;
  final String? previewBadge;
  final String? avatarSeed;
  final String? avatarInitials;
  final String? presenceLabel;
  final String? membersPreview;
  final bool isServiceAccount;
  final String? lastActivityVerb;
  final String? backendKind;
  final String? serverConversationKey;
  final String? lastMessageId;
  final int unreadLocalWatermark;
  final String? memberRole;
  final String? callState;

  ConversationSummary copyWith({
    String? preview,
    DateTime? lastUpdated,
    int? unreadCount,
    int? mentionCount,
    bool? isTyping,
    String? draftText,
    ConversationKind? kind,
    bool? isMuted,
    bool? isPinned,
    bool? isOnline,
    bool? isGroup,
    bool? isArchived,
    bool? isVerified,
    bool? isOfficial,
    int? memberCount,
    int? onlineCount,
    String? statusEmoji,
    String? lastSenderLabel,
    String? pillLabel,
    String? previewBadge,
    String? avatarSeed,
    String? avatarInitials,
    String? presenceLabel,
    String? membersPreview,
    bool? isServiceAccount,
    String? lastActivityVerb,
    String? backendKind,
    String? serverConversationKey,
    String? lastMessageId,
    int? unreadLocalWatermark,
    String? memberRole,
    String? callState,
  }) {
    return ConversationSummary(
      id: id,
      title: title,
      preview: preview ?? this.preview,
      lastUpdated: lastUpdated ?? this.lastUpdated,
      kind: kind ?? this.kind,
      unreadCount: unreadCount ?? this.unreadCount,
      mentionCount: mentionCount ?? this.mentionCount,
      isMuted: isMuted ?? this.isMuted,
      isPinned: isPinned ?? this.isPinned,
      isTyping: isTyping ?? this.isTyping,
      isOnline: isOnline ?? this.isOnline,
      isGroup: isGroup ?? this.isGroup,
      isArchived: isArchived ?? this.isArchived,
      isVerified: isVerified ?? this.isVerified,
      isOfficial: isOfficial ?? this.isOfficial,
      memberCount: memberCount ?? this.memberCount,
      onlineCount: onlineCount ?? this.onlineCount,
      statusEmoji: statusEmoji ?? this.statusEmoji,
      lastSenderLabel: lastSenderLabel ?? this.lastSenderLabel,
      draftText: draftText ?? this.draftText,
      pillLabel: pillLabel ?? this.pillLabel,
      previewBadge: previewBadge ?? this.previewBadge,
      avatarSeed: avatarSeed ?? this.avatarSeed,
      avatarInitials: avatarInitials ?? this.avatarInitials,
      presenceLabel: presenceLabel ?? this.presenceLabel,
      membersPreview: membersPreview ?? this.membersPreview,
      isServiceAccount: isServiceAccount ?? this.isServiceAccount,
      lastActivityVerb: lastActivityVerb ?? this.lastActivityVerb,
      backendKind: backendKind ?? this.backendKind,
      serverConversationKey:
          serverConversationKey ?? this.serverConversationKey,
      lastMessageId: lastMessageId ?? this.lastMessageId,
      unreadLocalWatermark: unreadLocalWatermark ?? this.unreadLocalWatermark,
      memberRole: memberRole ?? this.memberRole,
      callState: callState ?? this.callState,
    );
  }

  bool get hasDraft => draftText != null && draftText!.trim().isNotEmpty;
  bool get isGroupConversation => isGroup || kind == ConversationKind.group;
  bool get isFileConversation => kind == ConversationKind.fileAssistant;
  bool get isChannelConversation => kind == ConversationKind.channel;
}

class ChatMessage {
  const ChatMessage({
    required this.id,
    required this.conversationId,
    required this.senderLabel,
    required this.text,
    required this.timestamp,
    required this.direction,
    required this.status,
    this.attachment,
    this.replyPreview,
    this.reactions = const <MessageReactionSummary>[],
    this.isEdited = false,
    this.isForwarded = false,
  });

  final String id;
  final String conversationId;
  final String senderLabel;
  final String text;
  final DateTime timestamp;
  final MessageDirection direction;
  final MessageDeliveryStatus status;
  final ChatAttachment? attachment;
  final MessageReplyPreview? replyPreview;
  final List<MessageReactionSummary> reactions;
  final bool isEdited;
  final bool isForwarded;

  bool get hasText => text.trim().isNotEmpty;
  bool get isSystemNotice => senderLabel == '系统';

  ChatMessage copyWith({
    MessageDeliveryStatus? status,
    String? text,
    ChatAttachment? attachment,
    MessageReplyPreview? replyPreview,
    List<MessageReactionSummary>? reactions,
    bool? isEdited,
    bool? isForwarded,
  }) {
    return ChatMessage(
      id: id,
      conversationId: conversationId,
      senderLabel: senderLabel,
      text: text ?? this.text,
      timestamp: timestamp,
      direction: direction,
      status: status ?? this.status,
      attachment: attachment ?? this.attachment,
      replyPreview: replyPreview ?? this.replyPreview,
      reactions: reactions ?? this.reactions,
      isEdited: isEdited ?? this.isEdited,
      isForwarded: isForwarded ?? this.isForwarded,
    );
  }
}

class MessageReactionSummary {
  const MessageReactionSummary({
    required this.emoji,
    required this.count,
    this.selected = false,
  });

  final String emoji;
  final int count;
  final bool selected;
}

class MessageReplyPreview {
  const MessageReplyPreview({required this.senderLabel, required this.text});

  final String senderLabel;
  final String text;
}

class ChatAttachment {
  const ChatAttachment({
    required this.kind,
    required this.title,
    required this.detail,
    required this.actionLabel,
    required this.state,
    this.progress,
    this.sensitivity = ChatAttachmentSensitivity.none,
    this.fileExtension,
    this.thumbnailSeed,
    this.caption,
    this.durationLabel,
    this.waveformSeed,
    this.albumCount = 1,
    this.fileId,
    this.fileKey,
    this.fileSizeBytes,
    this.localPath,
    this.transferProgress,
    this.transferError,
    this.downloadable = false,
  });

  final ChatAttachmentKind kind;
  final String title;
  final String detail;
  final String actionLabel;
  final ChatAttachmentState state;
  final double? progress;
  final ChatAttachmentSensitivity sensitivity;
  final String? fileExtension;
  final String? thumbnailSeed;
  final String? caption;
  final String? durationLabel;
  final String? waveformSeed;
  final int albumCount;
  final String? fileId;
  final Uint8List? fileKey;
  final int? fileSizeBytes;
  final String? localPath;
  final double? transferProgress;
  final String? transferError;
  final bool downloadable;

  bool get isVisualMedia =>
      kind == ChatAttachmentKind.image || kind == ChatAttachmentKind.video;
}

extension ChatAttachmentSensitivityX on ChatAttachmentSensitivity {
  String get label => switch (this) {
    ChatAttachmentSensitivity.none => '',
    ChatAttachmentSensitivity.spoiler => '剧透',
    ChatAttachmentSensitivity.viewOnce => '限时查看',
  };
}

extension ChatAttachmentStateX on ChatAttachmentState {
  String get label => switch (this) {
    ChatAttachmentState.pendingSend => '待发送',
    ChatAttachmentState.uploading => '上传中',
    ChatAttachmentState.downloading => '下载中',
    ChatAttachmentState.completed => '已完成',
    ChatAttachmentState.failed => '失败',
    ChatAttachmentState.expired => '已过期',
    ChatAttachmentState.offlineQueued => '离线待发',
    ChatAttachmentState.retryReady => '重试入口',
  };

  bool get hasProgress =>
      this == ChatAttachmentState.uploading ||
      this == ChatAttachmentState.downloading ||
      this == ChatAttachmentState.pendingSend ||
      this == ChatAttachmentState.offlineQueued;

  bool get isTerminal =>
      this == ChatAttachmentState.completed ||
      this == ChatAttachmentState.expired;
}

class ContactProfile {
  const ContactProfile({
    required this.id,
    required this.displayName,
    required this.handle,
    required this.statusLabel,
    this.online = false,
    this.groupLabel = '我的好友',
    this.statusEmoji,
    this.lastSeenLabel,
    this.mutualGroupCount = 0,
    this.avatarSeed,
    this.signature,
    this.relationLabel,
    this.requestStatus,
  });

  final String id;
  final String displayName;
  final String handle;
  final String statusLabel;
  final bool online;
  final String groupLabel;
  final String? statusEmoji;
  final String? lastSeenLabel;
  final int mutualGroupCount;
  final String? avatarSeed;
  final String? signature;
  final String? relationLabel;
  final String? requestStatus;
}

class DeviceTrustInfo {
  const DeviceTrustInfo({
    required this.name,
    required this.fingerprint,
    required this.lastSeenLabel,
    required this.trustLabel,
  });

  final String name;
  final String fingerprint;
  final String lastSeenLabel;
  final String trustLabel;
}

class FriendRequestProfile {
  const FriendRequestProfile({
    required this.username,
    required this.displayName,
    this.remark = '',
    this.incoming = true,
    this.status = 'pending',
  });

  final String username;
  final String displayName;
  final String remark;
  final bool incoming;
  final String status;
}

class GroupMemberProfile {
  const GroupMemberProfile({
    required this.username,
    required this.displayName,
    required this.role,
    this.online = false,
  });

  final String username;
  final String displayName;
  final String role;
  final bool online;
}

class TrustPrompt {
  const TrustPrompt({
    required this.kind,
    required this.fingerprint,
    required this.pin,
    this.username,
  });

  final String kind;
  final String fingerprint;
  final String pin;
  final String? username;
}

class LinkedDeviceRequest {
  const LinkedDeviceRequest({
    required this.deviceId,
    required this.displayName,
    required this.requestIdHex,
  });

  final String deviceId;
  final String displayName;
  final String requestIdHex;
}

class QrLoginChallenge {
  const QrLoginChallenge({
    required this.payload,
    this.username,
    this.completed = false,
    this.errorMessage,
  });

  final String payload;
  final String? username;
  final bool completed;
  final String? errorMessage;
}

class CallParticipant {
  const CallParticipant({
    required this.username,
    required this.displayName,
    this.speaking = false,
    this.videoEnabled = false,
  });

  final String username;
  final String displayName;
  final bool speaking;
  final bool videoEnabled;
}

class MediaRelayState {
  const MediaRelayState({
    required this.callId,
    this.groupId,
    this.packetCount = 0,
    this.lastSender,
    this.lastError,
  });

  final Uint8List callId;
  final String? groupId;
  final int packetCount;
  final String? lastSender;
  final String? lastError;
}

class SdkHealthStatus {
  const SdkHealthStatus({
    required this.remoteOk,
    required this.remoteMode,
    required this.capabilities,
    this.errorMessage = '',
  });

  final bool remoteOk;
  final bool remoteMode;
  final int capabilities;
  final String errorMessage;
}
