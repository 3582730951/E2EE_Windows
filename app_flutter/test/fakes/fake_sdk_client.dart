import 'dart:async';

import 'package:mi_e2ee_im_app/domain/entities/models.dart';
import 'package:mi_e2ee_im_app/native_sdk/sdk_client.dart';

class FakeSdkClient implements NativeSdkClient {
  FakeSdkClient() {
    final today = DateTime.now();
    final now = DateTime(today.year, today.month, today.day, 21);
    _session = const SessionProfile(
      username: 'alice',
      displayName: 'Alice Lin',
      subtitle: '消息、群聊与文件同步中',
    );
    _conversations = <ConversationSummary>[
      ConversationSummary(
        id: 'chat-bob',
        title: 'Bob Chen',
        preview: '晚上把媒体面板再收一下，层级已经很顺了。',
        lastUpdated: now.subtract(const Duration(minutes: 3)),
        kind: ConversationKind.direct,
        unreadCount: 2,
        isOnline: true,
        isVerified: true,
        statusEmoji: '💻',
        lastSenderLabel: 'Bob',
        avatarSeed: 'bob-product-designer',
        avatarInitials: 'BC',
        presenceLabel: '在线',
        lastActivityVerb: '发来消息',
      ),
      ConversationSummary(
        id: 'group-design',
        title: 'Aurora 设计群',
        preview: '你: [图片]',
        lastUpdated: now.subtract(const Duration(minutes: 13)),
        kind: ConversationKind.group,
        unreadCount: 6,
        isPinned: true,
        isGroup: true,
        memberCount: 128,
        onlineCount: 19,
        statusEmoji: '✨',
        lastSenderLabel: '你',
        pillLabel: '群聊',
        previewBadge: '群聊',
        avatarSeed: 'aurora-design-group',
        avatarInitials: 'AD',
        membersPreview: 'Mina、Leo、Bob 等 128 人',
        presenceLabel: '19 人在线',
        lastActivityVerb: '发送图片',
      ),
      ConversationSummary(
        id: 'channel-product',
        title: 'MI Chat 公告',
        preview: '新版桌面端支持右侧资料面板与媒体入口。',
        lastUpdated: now.subtract(const Duration(minutes: 5)),
        kind: ConversationKind.channel,
        unreadCount: 1,
        isOfficial: true,
        statusEmoji: '📣',
        lastSenderLabel: '公告',
        pillLabel: '频道',
        previewBadge: '公告',
        avatarSeed: 'mi-chat-announcement',
        avatarInitials: 'MI',
        presenceLabel: '官方频道',
        lastActivityVerb: '发布公告',
      ),
      ConversationSummary(
        id: 'file-helper',
        title: '文件传输助手',
        preview: '附件状态矩阵已补齐：待发送、上传中、下载中与重试入口都能看到。',
        lastUpdated: now.subtract(const Duration(minutes: 7)),
        kind: ConversationKind.fileAssistant,
        unreadCount: 1,
        statusEmoji: '📁',
        lastSenderLabel: '文件助手',
        pillLabel: '文件',
        previewBadge: '文件',
        avatarSeed: 'file-transfer-helper',
        avatarInitials: 'FT',
        presenceLabel: '跨设备在线',
        lastActivityVerb: '同步附件',
      ),
      ConversationSummary(
        id: 'group-project',
        title: '项目推进群',
        preview: 'Allen: 发布窗口确认在周五，回归清单今晚发出。',
        lastUpdated: now.subtract(const Duration(minutes: 18)),
        kind: ConversationKind.group,
        unreadCount: 3,
        isMuted: true,
        isGroup: true,
        memberCount: 42,
        onlineCount: 11,
        lastSenderLabel: 'Allen',
        pillLabel: '群聊',
        previewBadge: '群聊',
      ),
      ConversationSummary(
        id: 'chat-zoe',
        title: 'Zoe Liu',
        preview: '我把联系人页的在线态整理成统一语义了。',
        lastUpdated: now.subtract(const Duration(minutes: 26)),
        kind: ConversationKind.direct,
        isOnline: true,
        statusEmoji: '🌿',
        lastSenderLabel: 'Zoe',
      ),
      ConversationSummary(
        id: 'group-qa',
        title: '质量回归',
        preview: '@你 Android 截图流程已回归通过，可以继续细调视觉。',
        lastUpdated: now.subtract(const Duration(minutes: 34)),
        kind: ConversationKind.group,
        unreadCount: 1,
        mentionCount: 1,
        isGroup: true,
        memberCount: 24,
        onlineCount: 8,
        lastSenderLabel: 'QA Bot',
        pillLabel: '@你',
        previewBadge: '有人@你',
      ),
      ConversationSummary(
        id: 'chat-mina',
        title: 'Mina Xu',
        preview: '我把输入区和会话页的节奏重新收了一遍。',
        lastUpdated: now.subtract(const Duration(minutes: 48)),
        kind: ConversationKind.direct,
        isOnline: true,
        statusEmoji: '🎨',
        lastSenderLabel: 'Mina',
        draftText: '把 iOS 会话列表的按压反馈和阅读态再收一下。',
      ),
      ConversationSummary(
        id: 'service-notice',
        title: '登录提醒',
        preview: 'iPad 刚刚登录了 MI Chat，可在设置中管理。',
        lastUpdated: now.subtract(const Duration(hours: 1, minutes: 8)),
        kind: ConversationKind.system,
        isOfficial: true,
        statusEmoji: '🔔',
        lastSenderLabel: '服务通知',
        pillLabel: '服务',
        previewBadge: '服务通知',
        avatarSeed: 'login-alert-service',
        avatarInitials: 'LA',
        presenceLabel: '系统服务',
        isServiceAccount: true,
        lastActivityVerb: '提醒登录',
      ),
      ConversationSummary(
        id: 'bot-support',
        title: 'MI 助手',
        preview: '你可以问我怎么找聊天文件、创建群或同步记录。',
        lastUpdated: now.subtract(const Duration(hours: 2, minutes: 18)),
        kind: ConversationKind.bot,
        isOfficial: true,
        statusEmoji: '🤖',
        lastSenderLabel: '助手',
        pillLabel: '服务号',
        avatarSeed: 'mi-assistant-service',
        avatarInitials: 'AI',
        presenceLabel: '服务号',
        isServiceAccount: true,
        lastActivityVerb: '提供帮助',
      ),
    ];
    _messages = <String, List<ChatMessage>>{
      'chat-bob': <ChatMessage>[
        ChatMessage(
          id: 'm1',
          conversationId: 'chat-bob',
          senderLabel: 'Bob',
          text: '我把三栏布局和右侧详情先做成稳定骨架了。',
          timestamp: now.subtract(const Duration(minutes: 18)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.read,
        ),
        ChatMessage(
          id: 'm2',
          conversationId: 'chat-bob',
          senderLabel: '你',
          text: '可以，消息主流程不要被装饰动效打断。',
          timestamp: now.subtract(const Duration(minutes: 15)),
          direction: MessageDirection.outgoing,
          status: MessageDeliveryStatus.read,
          replyPreview: const MessageReplyPreview(
            senderLabel: 'Bob',
            text: '三栏布局已经稳定了',
          ),
          reactions: const <MessageReactionSummary>[
            MessageReactionSummary(emoji: '👌', count: 1, selected: true),
          ],
        ),
        ChatMessage(
          id: 'm3',
          conversationId: 'chat-bob',
          senderLabel: 'Bob',
          text: '晚上把媒体面板再收一下，层级已经很顺了。',
          timestamp: now.subtract(const Duration(minutes: 3)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          isEdited: true,
          reactions: const <MessageReactionSummary>[
            MessageReactionSummary(emoji: '👍', count: 2),
            MessageReactionSummary(emoji: '✨', count: 1),
          ],
        ),
      ],
      'group-design': <ChatMessage>[
        ChatMessage(
          id: 'g1',
          conversationId: 'group-design',
          senderLabel: 'Mina',
          text: '我更喜欢 Telegram 的骨架，但头像和关系感还是要更接近 QQ。',
          timestamp: now.subtract(const Duration(minutes: 32)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          reactions: const <MessageReactionSummary>[
            MessageReactionSummary(emoji: '👍', count: 5),
          ],
        ),
        ChatMessage(
          id: 'g1b',
          conversationId: 'group-design',
          senderLabel: 'Mina',
          text: '头像、关系感和消息留白也已经一起收了，不会再有概念稿味道。',
          timestamp: now.subtract(const Duration(minutes: 31)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
        ),
        ChatMessage(
          id: 'g2',
          conversationId: 'group-design',
          senderLabel: 'Leo',
          text: '新版附件卡和聊天详情样式我一起整理到文档里了。',
          timestamp: now.subtract(const Duration(minutes: 21)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          isForwarded: true,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.file,
            title: '聊天详情规范.pdf',
            detail: '5.8 MB · PDF',
            actionLabel: '预览',
            state: ChatAttachmentState.completed,
            fileExtension: 'PDF',
            caption: '聊天详情规范',
          ),
        ),
        ChatMessage(
          id: 'g2b',
          conversationId: 'group-design',
          senderLabel: '系统',
          text: 'Leo 上传了聊天详情规范.pdf',
          timestamp: now.subtract(const Duration(minutes: 20)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
        ),
        ChatMessage(
          id: 'g3',
          conversationId: 'group-design',
          senderLabel: '你',
          text: '可以，时间分隔和连续消息间距我来收，Android 截图就出这版。',
          timestamp: now.subtract(const Duration(minutes: 16)),
          direction: MessageDirection.outgoing,
          status: MessageDeliveryStatus.read,
          replyPreview: const MessageReplyPreview(
            senderLabel: 'Mina',
            text: '头像、关系感和消息留白也已经一起收了',
          ),
          reactions: const <MessageReactionSummary>[
            MessageReactionSummary(emoji: '🔥', count: 3, selected: true),
          ],
        ),
        ChatMessage(
          id: 'g4',
          conversationId: 'group-design',
          senderLabel: 'Mina',
          text: '我把最终交付包也整理进来了，连续发三份方便你看群聊里的密集附件节奏。',
          timestamp: now.subtract(const Duration(minutes: 15)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.file,
            title: 'Aurora设计群_2026-04-18_超长文件名_最终交付包_v12_不要再改了.pdf',
            detail: '24.8 MB · PDF',
            actionLabel: '打开',
            state: ChatAttachmentState.completed,
            fileExtension: 'PDF',
          ),
        ),
        ChatMessage(
          id: 'g5',
          conversationId: 'group-design',
          senderLabel: 'Leo',
          text: '',
          timestamp: now.subtract(const Duration(minutes: 14)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.file,
            title: '发布说明草案',
            detail: '1.1 MB · 无扩展名',
            actionLabel: '打开',
            state: ChatAttachmentState.completed,
          ),
        ),
        ChatMessage(
          id: 'g6',
          conversationId: 'group-design',
          senderLabel: '你',
          text: '',
          timestamp: now.subtract(const Duration(minutes: 13)),
          direction: MessageDirection.outgoing,
          status: MessageDeliveryStatus.sending,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.image,
            title: '群聊连续文件消息-封面图.png',
            detail: '3.6 MB · 照片发送中',
            actionLabel: '上传中',
            state: ChatAttachmentState.uploading,
            progress: 0.68,
            fileExtension: 'PNG',
            thumbnailSeed: 'aurora-chat-cover',
            caption: '群聊连续文件消息',
            albumCount: 3,
          ),
        ),
      ],
      'channel-product': <ChatMessage>[
        ChatMessage(
          id: 'c1',
          conversationId: 'channel-product',
          senderLabel: '公告',
          text: '新版桌面端支持右侧资料面板与媒体入口。',
          timestamp: now.subtract(const Duration(minutes: 5)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          reactions: const <MessageReactionSummary>[
            MessageReactionSummary(emoji: '📣', count: 18),
            MessageReactionSummary(emoji: '👍', count: 42),
          ],
        ),
      ],
      'service-notice': <ChatMessage>[
        ChatMessage(
          id: 'o1',
          conversationId: 'service-notice',
          senderLabel: '系统',
          text: 'iPad 刚刚登录了 MI Chat，可在设置中管理。',
          timestamp: now.subtract(const Duration(hours: 1, minutes: 8)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
        ),
      ],
      'file-helper': <ChatMessage>[
        ChatMessage(
          id: 'f1',
          conversationId: 'file-helper',
          senderLabel: '文件助手',
          text: '先把文件消息的状态矩阵铺满，方便你验证消息流里的密度和异常态。',
          timestamp: now.subtract(const Duration(minutes: 11)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.file,
            title: '待发送草稿包.zip',
            detail: '14.2 MB · ZIP',
            actionLabel: '排队',
            state: ChatAttachmentState.pendingSend,
          ),
        ),
        ChatMessage(
          id: 'f2',
          conversationId: 'file-helper',
          senderLabel: '你',
          text: '附件卡不能再像一张卡片贴在气泡里，要像真正的消息附件。',
          timestamp: now.subtract(const Duration(minutes: 10)),
          direction: MessageDirection.outgoing,
          status: MessageDeliveryStatus.read,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.file,
            title: '移动端附件状态矩阵_视觉走查版',
            detail: '8.1 MB · 无扩展名',
            actionLabel: '暂停',
            state: ChatAttachmentState.uploading,
            progress: 0.38,
          ),
        ),
        ChatMessage(
          id: 'f3',
          conversationId: 'file-helper',
          senderLabel: '文件助手',
          text: '下载态、失败态和过期态也都补上了。',
          timestamp: now.subtract(const Duration(minutes: 9)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.file,
            title: '视觉走查切图.zip',
            detail: '18 张图 · 8.1 MB',
            actionLabel: '下载',
            state: ChatAttachmentState.downloading,
            progress: 0.62,
          ),
        ),
        ChatMessage(
          id: 'f4',
          conversationId: 'file-helper',
          senderLabel: '文件助手',
          text: '已完成的文件要像正式 IM 一样，给到明确的打开入口。',
          timestamp: now.subtract(const Duration(minutes: 8)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.file,
            title: '产品验收记录.pdf',
            detail: '2.4 MB · PDF',
            actionLabel: '打开',
            state: ChatAttachmentState.completed,
          ),
        ),
        ChatMessage(
          id: 'f5',
          conversationId: 'file-helper',
          senderLabel: '你',
          text: '失败态和重试入口也要是同一个消息结构里的正常分支。',
          timestamp: now.subtract(const Duration(minutes: 6)),
          direction: MessageDirection.outgoing,
          status: MessageDeliveryStatus.failed,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.image,
            title: '断网时的重发样本.png',
            detail: '3.2 MB · 图片发送失败',
            actionLabel: '重试',
            state: ChatAttachmentState.failed,
            fileExtension: 'PNG',
            thumbnailSeed: 'retry-sample',
            caption: '断网时的重发样本',
          ),
        ),
        ChatMessage(
          id: 'f6',
          conversationId: 'file-helper',
          senderLabel: '文件助手',
          text: '过期文件和离线待发文件都已经接上。',
          timestamp: now.subtract(const Duration(minutes: 5)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.video,
            title: '过期验收包.mp4',
            detail: '已失效 · 9.7 MB · 视频',
            actionLabel: '查看原因',
            state: ChatAttachmentState.expired,
            sensitivity: ChatAttachmentSensitivity.viewOnce,
            fileExtension: 'MP4',
            thumbnailSeed: 'expired-acceptance-video',
            durationLabel: '00:12',
          ),
        ),
        ChatMessage(
          id: 'f7',
          conversationId: 'file-helper',
          senderLabel: '你',
          text: '离线的时候先排队，联网后自动发出。',
          timestamp: now.subtract(const Duration(minutes: 4)),
          direction: MessageDirection.outgoing,
          status: MessageDeliveryStatus.sent,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.file,
            title: '离线待发说明.txt',
            detail: '队列中 · 1.1 MB',
            actionLabel: '等待联机',
            state: ChatAttachmentState.offlineQueued,
            progress: 0.24,
          ),
        ),
        ChatMessage(
          id: 'f8',
          conversationId: 'file-helper',
          senderLabel: '文件助手',
          text: '失败后重试入口也作为独立状态暴露出来。',
          timestamp: now.subtract(const Duration(minutes: 3)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
          attachment: const ChatAttachment(
            kind: ChatAttachmentKind.file,
            title: '提交失败回执.csv',
            detail: '42 KB · CSV',
            actionLabel: '重试',
            state: ChatAttachmentState.retryReady,
          ),
        ),
      ],
      'group-project': <ChatMessage>[],
      'chat-zoe': <ChatMessage>[
        ChatMessage(
          id: 'z1',
          conversationId: 'chat-zoe',
          senderLabel: 'Zoe',
          text: '[语音] 00:18',
          timestamp: now.subtract(const Duration(minutes: 26)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
        ),
      ],
      'group-qa': <ChatMessage>[
        ChatMessage(
          id: 'q1',
          conversationId: 'group-qa',
          senderLabel: 'QA Bot',
          text: '@你 Android 截图流程已回归通过，可以继续细调视觉。',
          timestamp: now.subtract(const Duration(minutes: 34)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
        ),
      ],
      'chat-mina': <ChatMessage>[],
      'bot-support': <ChatMessage>[
        ChatMessage(
          id: 'b1',
          conversationId: 'bot-support',
          senderLabel: 'MI 助手',
          text: '你可以问我怎么找聊天文件、创建群或同步记录。',
          timestamp: now.subtract(const Duration(hours: 2, minutes: 18)),
          direction: MessageDirection.incoming,
          status: MessageDeliveryStatus.delivered,
        ),
      ],
    };
    _contacts = const <ContactProfile>[
      ContactProfile(
        id: 'bob',
        displayName: 'Bob Chen',
        handle: '@bobchen',
        statusLabel: '正在整理动效节奏',
        online: true,
        groupLabel: '我的好友',
        statusEmoji: '💻',
        lastSeenLabel: '在线',
        mutualGroupCount: 3,
        avatarSeed: 'bob-product-designer',
        signature: '正在整理动效节奏',
        relationLabel: '好友',
      ),
      ContactProfile(
        id: 'mina',
        displayName: 'Mina Xu',
        handle: '@mina',
        statusLabel: '喜欢干净的聊天动线',
        online: true,
        groupLabel: '我的好友',
        statusEmoji: '🎨',
        lastSeenLabel: '15 分钟前在线',
        mutualGroupCount: 5,
        avatarSeed: 'mina-visual-designer',
        signature: '喜欢干净的聊天动线',
        relationLabel: '好友',
      ),
      ContactProfile(
        id: 'zoe',
        displayName: 'Zoe Liu',
        handle: '@zoe',
        statusLabel: '文档和联系人体验整理中',
        online: true,
        groupLabel: '我的好友',
        statusEmoji: '🌿',
        lastSeenLabel: '在线',
        mutualGroupCount: 2,
        avatarSeed: 'zoe-docs-contact',
        signature: '文档和联系人体验整理中',
        relationLabel: '好友',
      ),
      ContactProfile(
        id: 'group-design',
        displayName: 'Aurora 设计群',
        handle: '#aurora-design',
        statusLabel: '128 位成员 · 19 人在线',
        groupLabel: '群聊',
        statusEmoji: '✨',
        lastSeenLabel: '刚刚活跃',
        mutualGroupCount: 0,
        avatarSeed: 'aurora-design-group',
        signature: '设计、动效、截图验收',
        relationLabel: '群聊',
      ),
      ContactProfile(
        id: 'service-notice',
        displayName: 'MI Chat 服务号',
        handle: '@mi-service',
        statusLabel: '公告、登录提醒和帮助',
        groupLabel: '公众号或服务号',
        statusEmoji: '📣',
        lastSeenLabel: '官方服务',
        avatarSeed: 'mi-service-account',
        signature: '公告、登录提醒和帮助',
        relationLabel: '服务号',
        requestStatus: '官方',
      ),
    ];
    _devices = const <DeviceTrustInfo>[
      DeviceTrustInfo(
        name: 'Windows 主端',
        fingerprint: 'A1 C9 44 7E 2F 9D',
        lastSeenLabel: '刚刚活跃',
        trustLabel: '已验证',
      ),
      DeviceTrustInfo(
        name: 'iPhone 16 Pro',
        fingerprint: '18 6F 3B 9C A7 01',
        lastSeenLabel: '13 分钟前',
        trustLabel: '已验证',
      ),
    ];
  }

  final StreamController<List<ConversationSummary>> _conversationController =
      StreamController<List<ConversationSummary>>.broadcast();
  final Map<String, StreamController<List<ChatMessage>>> _messageControllers =
      <String, StreamController<List<ChatMessage>>>{};
  final StreamController<List<ContactProfile>> _contactController =
      StreamController<List<ContactProfile>>.broadcast();
  final StreamController<List<DeviceTrustInfo>> _deviceController =
      StreamController<List<DeviceTrustInfo>>.broadcast();

  late SessionProfile _session;
  late List<ConversationSummary> _conversations;
  late Map<String, List<ChatMessage>> _messages;
  late List<ContactProfile> _contacts;
  late List<DeviceTrustInfo> _devices;

  @override
  Future<void> initialize() async {
    _conversationController.add(
      List<ConversationSummary>.unmodifiable(_conversations),
    );
    _contactController.add(List<ContactProfile>.unmodifiable(_contacts));
    _deviceController.add(List<DeviceTrustInfo>.unmodifiable(_devices));
    for (final entry in _messages.entries) {
      _controllerFor(
        entry.key,
      ).add(List<ChatMessage>.unmodifiable(entry.value));
    }
  }

  @override
  Future<SessionProfile?> login({
    required String username,
    required String password,
  }) async {
    if (username.trim().isEmpty || password.trim().isEmpty) {
      return null;
    }
    _session = SessionProfile(
      username: username.trim(),
      displayName: 'Alice Lin',
      subtitle: '消息、群聊与文件同步中',
    );
    return _session;
  }

  @override
  Future<void> logout() async {}

  @override
  Stream<List<ConversationSummary>> watchConversations() async* {
    yield List<ConversationSummary>.unmodifiable(_conversations);
    yield* _conversationController.stream;
  }

  @override
  Stream<List<ChatMessage>> watchMessages(String conversationId) async* {
    yield List<ChatMessage>.unmodifiable(
      _messages[conversationId] ?? const <ChatMessage>[],
    );
    yield* _controllerFor(conversationId).stream;
  }

  @override
  Stream<List<ContactProfile>> watchContacts() async* {
    yield List<ContactProfile>.unmodifiable(_contacts);
    yield* _contactController.stream;
  }

  @override
  Stream<List<DeviceTrustInfo>> watchDevices() async* {
    yield List<DeviceTrustInfo>.unmodifiable(_devices);
    yield* _deviceController.stream;
  }

  @override
  Future<void> markConversationRead({required String conversationId}) async {
    final existingMessages = _messages[conversationId];
    if (existingMessages != null) {
      final updatedMessages = existingMessages
          .map(
            (message) =>
                message.direction == MessageDirection.incoming &&
                    message.status != MessageDeliveryStatus.read
                ? _copyMessageWithStatus(message, MessageDeliveryStatus.read)
                : message,
          )
          .toList();
      _messages[conversationId] = updatedMessages;
      _controllerFor(
        conversationId,
      ).add(List<ChatMessage>.unmodifiable(updatedMessages));
    }

    var changed = false;
    _conversations = _conversations.map((conversation) {
      if (conversation.id != conversationId) {
        return conversation;
      }
      if (conversation.unreadCount == 0 &&
          conversation.mentionCount == 0 &&
          !conversation.isTyping) {
        return conversation;
      }
      changed = true;
      return conversation.copyWith(
        unreadCount: 0,
        mentionCount: 0,
        isTyping: false,
      );
    }).toList();
    if (changed) {
      _conversationController.add(
        List<ConversationSummary>.unmodifiable(_conversations),
      );
    }
  }

  @override
  Future<void> sendMessage({
    required String conversationId,
    required String text,
  }) async {
    final trimmed = text.trim();
    if (trimmed.isEmpty) {
      return;
    }
    final now = DateTime.now();
    final message = ChatMessage(
      id: 'local-${now.microsecondsSinceEpoch}',
      conversationId: conversationId,
      senderLabel: '你',
      text: trimmed,
      timestamp: now,
      direction: MessageDirection.outgoing,
      status: MessageDeliveryStatus.read,
    );
    final conversationMessages = List<ChatMessage>.from(
      _messages[conversationId] ?? const <ChatMessage>[],
    )..add(message);
    _messages[conversationId] = conversationMessages;
    _controllerFor(
      conversationId,
    ).add(List<ChatMessage>.unmodifiable(conversationMessages));

    _conversations =
        _conversations
            .map(
              (conversation) => conversation.id == conversationId
                  ? conversation.copyWith(
                      preview: trimmed,
                      lastUpdated: now,
                      unreadCount: 0,
                      mentionCount: 0,
                      isTyping: false,
                      draftText: '',
                      lastSenderLabel: '你',
                    )
                  : conversation,
            )
            .toList()
          ..sort(
            (left, right) => right.lastUpdated.compareTo(left.lastUpdated),
          );
    _conversationController.add(
      List<ConversationSummary>.unmodifiable(_conversations),
    );
  }

  ChatMessage _copyMessageWithStatus(
    ChatMessage message,
    MessageDeliveryStatus status,
  ) {
    return message.copyWith(status: status);
  }

  StreamController<List<ChatMessage>> _controllerFor(String conversationId) {
    return _messageControllers.putIfAbsent(
      conversationId,
      () => StreamController<List<ChatMessage>>.broadcast(),
    );
  }

  @override
  void dispose() {
    _conversationController.close();
    _contactController.close();
    _deviceController.close();
    for (final controller in _messageControllers.values) {
      controller.close();
    }
  }
}
