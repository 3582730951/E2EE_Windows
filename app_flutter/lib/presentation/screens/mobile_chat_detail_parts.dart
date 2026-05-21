part of 'mobile_chat_detail_screen.dart';

class _MobileChatDetailContent extends StatefulWidget {
  const _MobileChatDetailContent({
    required this.conversation,
    required this.messages,
    required this.onBack,
    required this.autoScrollToLatest,
    required this.openingUnreadCount,
    required this.onSend,
    required this.onSendFile,
    required this.onClearHistory,
  });

  final ConversationSummary conversation;
  final List<ChatMessage> messages;
  final VoidCallback onBack;
  final bool autoScrollToLatest;
  final int openingUnreadCount;
  final ValueChanged<String> onSend;
  final FutureOr<void> Function(String) onSendFile;
  final Future<void> Function() onClearHistory;

  @override
  State<_MobileChatDetailContent> createState() =>
      _MobileChatDetailContentState();
}

class _MobileChatDetailContentState extends State<_MobileChatDetailContent> {
  final GlobalKey<_MobileChatTimelineState> _timelineKey =
      GlobalKey<_MobileChatTimelineState>();

  void _showSearch() {
    _showMobileMessageSearch(
      context,
      widget.conversation,
      widget.messages,
      onOpenMessage: _openMessage,
    );
  }

  void _openMessage(ChatMessage message) {
    _timelineKey.currentState?.revealMessage(message.id);
    ScaffoldMessenger.maybeOf(
      context,
    )?.showSnackBar(SnackBar(content: Text('已定位到 ${message.senderLabel} 的消息')));
  }

  @override
  Widget build(BuildContext context) {
    final isCupertinoStyle = Theme.of(context).platform == TargetPlatform.iOS;
    final viewPadding = MediaQuery.paddingOf(context);
    final topChromeHeight =
        viewPadding.top +
        (isCupertinoStyle
            ? ImUiMetrics.iosChatDetailChromeHeight
            : ImUiMetrics.androidChatDetailChromeHeight);
    final trailingVisualMedia = _hasTrailingVisualMedia(widget.messages);
    final bottomChromeHeight =
        viewPadding.bottom +
        (isCupertinoStyle
            ? ImUiMetrics.iosComposerDockHeight
            : ImUiMetrics.androidComposerDockHeight) +
        (trailingVisualMedia ? (isCupertinoStyle ? 10.0 : 8.0) : 0.0);
    return Stack(
      children: <Widget>[
        Positioned.fill(
          top: topChromeHeight,
          child: _MobileChatTimeline(
            key: _timelineKey,
            conversation: widget.conversation,
            messages: widget.messages,
            initialUnreadCount: widget.openingUnreadCount,
            autoScrollToLatest: widget.autoScrollToLatest,
            contentPadding: EdgeInsets.fromLTRB(
              isCupertinoStyle ? 6 : 4,
              isCupertinoStyle
                  ? ImUiMetrics.iosChatDetailTimelineTopPadding
                  : ImUiMetrics.androidChatDetailTimelineTopPadding,
              isCupertinoStyle ? 6 : 4,
              bottomChromeHeight,
            ),
          ),
        ),
        Positioned(
          top: 0,
          left: 0,
          right: 0,
          child: IgnorePointer(
            child: _ChatEdgeFade(
              top: true,
              cupertinoStyle: isCupertinoStyle,
              extraInset: viewPadding.top,
            ),
          ),
        ),
        if (!isCupertinoStyle)
          Positioned(
            top: topChromeHeight - 1,
            left: 0,
            right: 0,
            child: const IgnorePointer(child: _AndroidTimelineBoundaryMask()),
          ),
        Positioned(
          top: 0,
          left: 0,
          right: 0,
          child: _MobileChatDetailHeader(
            conversation: widget.conversation,
            onBack: widget.onBack,
            onSearchMessages: _showSearch,
            onMoreActions: () => _showMobileConversationActions(
              context,
              conversation: widget.conversation,
              messages: widget.messages,
              onSearchMessages: _showSearch,
              onOpenMessage: _openMessage,
              onClearHistory: widget.onClearHistory,
            ),
          ),
        ),
        Positioned(
          left: 0,
          right: 0,
          bottom: 0,
          child: IgnorePointer(
            child: _ChatEdgeFade(
              top: false,
              cupertinoStyle: isCupertinoStyle,
              extraInset: viewPadding.bottom,
            ),
          ),
        ),
        Positioned(
          left: 0,
          right: 0,
          bottom: 0,
          child: _ChatComposerDock(
            onSend: widget.onSend,
            onSendFile: widget.onSendFile,
          ),
        ),
      ],
    );
  }
}

class _AndroidTimelineBoundaryMask extends StatelessWidget {
  const _AndroidTimelineBoundaryMask();

  @override
  Widget build(BuildContext context) {
    return Container(
      key: const Key('mobile-chat-top-boundary-mask-material'),
      height: ImUiMetrics.androidChatDetailBoundaryMask,
      color: Theme.of(context).colorScheme.surface,
    );
  }
}

bool _hasTrailingVisualMedia(List<ChatMessage> messages) {
  for (var index = messages.length - 1; index >= 0; index -= 1) {
    final message = messages[index];
    if (message.isSystemNotice) {
      continue;
    }
    if (message.attachment case final attachment?) {
      return _isVisualAttachmentKind(_resolvedAttachmentKind(attachment));
    }
    return false;
  }
  return false;
}

class _ChatEdgeFade extends StatelessWidget {
  const _ChatEdgeFade({
    required this.top,
    required this.cupertinoStyle,
    required this.extraInset,
  });

  final bool top;
  final bool cupertinoStyle;
  final double extraInset;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final height = top
        ? extraInset +
              (cupertinoStyle
                  ? ImUiMetrics.iosChatDetailTopFade
                  : ImUiMetrics.androidChatDetailTopFade)
        : extraInset + (cupertinoStyle ? 58.0 : 70.0);
    final colors = top && cupertinoStyle
        ? <Color>[
            theme.colorScheme.surface.withValues(alpha: 0.98),
            Color.alphaBlend(
              AppPalette.mistBlue.withValues(alpha: 0.012),
              theme.colorScheme.surface.withValues(alpha: 0.82),
            ),
            theme.colorScheme.surface.withValues(alpha: 0.46),
            Colors.transparent,
          ]
        : top && !cupertinoStyle
        ? <Color>[
            Color.alphaBlend(
              AppPalette.mistBlue.withValues(alpha: 0.014),
              theme.colorScheme.surface.withValues(alpha: 0.98),
            ),
            theme.colorScheme.surface.withValues(alpha: 0.44),
            Colors.transparent,
          ]
        : <Color>[
            Colors.white.withValues(
              alpha: top
                  ? (cupertinoStyle ? 0.026 : 0.034)
                  : (cupertinoStyle ? 0.014 : 0.02),
            ),
            AppPalette.mistBlue.withValues(
              alpha: top
                  ? (cupertinoStyle ? 0.01 : 0.014)
                  : (cupertinoStyle ? 0.008 : 0.01),
            ),
            Colors.transparent,
          ];
    final backdrop = Container(
      height: height,
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: top ? Alignment.topCenter : Alignment.bottomCenter,
          end: top ? Alignment.bottomCenter : Alignment.topCenter,
          colors: colors,
          stops: top && cupertinoStyle
              ? const <double>[0, 0.34, 0.70, 1]
              : top && !cupertinoStyle
              ? const <double>[0, 0.56, 1]
              : const <double>[0, 0.26, 1],
        ),
      ),
    );
    return backdrop;
  }
}

class _MobileChatDetailHeader extends StatelessWidget {
  const _MobileChatDetailHeader({
    required this.conversation,
    required this.onBack,
    required this.onSearchMessages,
    required this.onMoreActions,
  });

  final ConversationSummary conversation;
  final VoidCallback onBack;
  final VoidCallback onSearchMessages;
  final VoidCallback onMoreActions;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final palette = Theme.of(context).extension<AppShellColors>()!;
    final avatarTint = conversation.isGroupConversation
        ? palette.surfaceVariant.withValues(alpha: 0.92)
        : theme.colorScheme.primary.withValues(alpha: 0.16);
    final leadIcon = AppIcons.conversationLead(conversation);
    final onSurfaceColor = theme.colorScheme.onSurface;
    final headerSummary = _detailHeaderSummary(conversation);

    if (isCupertinoStyle) {
      final shellSurface = Color.alphaBlend(
        palette.surfaceVariant.withValues(
          alpha: material.isGlass ? 0.045 : 0.03,
        ),
        theme.colorScheme.surface.withValues(alpha: 0.985),
      );
      final shellBorder = palette.divider.withValues(
        alpha: material.isGlass ? 0.10 : 0.11,
      );
      return SafeArea(
        bottom: false,
        child: DecoratedBox(
          key: const Key('mobile-chat-detail-header'),
          decoration: BoxDecoration(
            color: shellSurface,
            border: Border(bottom: BorderSide(color: shellBorder)),
          ),
          child: Padding(
            padding: const EdgeInsets.fromLTRB(4, 1, 4, 1),
            child: SizedBox(
              key: const Key('mobile-chat-detail-header-matte-shell'),
              height: 34,
              child: SizedBox(
                key: const Key('mobile-chat-detail-header-cupertino'),
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: <Widget>[
                    SizedBox(
                      width: 78,
                      child: Align(
                        alignment: Alignment.centerLeft,
                        child: _CupertinoBackLabelButton(
                          onPressed: onBack,
                          label: '消息',
                        ),
                      ),
                    ),
                    Expanded(
                      child: IgnorePointer(
                        child: Column(
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: <Widget>[
                            Text(
                              conversation.title,
                              key: const Key('mobile-chat-detail-title-center'),
                              maxLines: 1,
                              textAlign: TextAlign.center,
                              overflow: TextOverflow.ellipsis,
                              style: theme.textTheme.titleMedium?.copyWith(
                                fontWeight: FontWeight.w500,
                                fontSize: 13.8,
                                letterSpacing: -0.2,
                              ),
                            ),
                            const SizedBox(height: 1),
                            Text(
                              headerSummary,
                              key: const Key(
                                'mobile-chat-detail-subtitle-center',
                              ),
                              maxLines: 1,
                              textAlign: TextAlign.center,
                              overflow: TextOverflow.ellipsis,
                              style: theme.textTheme.bodySmall?.copyWith(
                                fontSize: 9.1,
                                height: 1.0,
                                color: theme.textTheme.bodySmall?.color
                                    ?.withValues(alpha: 0.62),
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                    SizedBox(
                      width: 78,
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.end,
                        children: <Widget>[
                          _HeaderActionButton(
                            key: const Key('mobile-chat-header-search'),
                            icon: AppSemanticIcon.search,
                            tooltip: '搜索消息',
                            onPressed: onSearchMessages,
                            plain: true,
                          ),
                          const SizedBox(width: 1),
                          _HeaderActionButton(
                            key: const Key('mobile-chat-header-more'),
                            icon: AppSemanticIcon.more,
                            tooltip: '更多操作',
                            onPressed: onMoreActions,
                            plain: true,
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
    }

    final materialHeaderContent = SizedBox(
      key: const Key('mobile-chat-detail-header-material'),
      height: 41,
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.center,
        children: <Widget>[
          const SizedBox(width: 2),
          _HeaderActionButton(
            key: const Key('mobile-chat-header-back'),
            icon: AppSemanticIcon.back,
            tooltip: '返回',
            onPressed: onBack,
          ),
          const SizedBox(width: 4),
          CircleAvatar(
            key: const Key('mobile-chat-detail-header-avatar'),
            radius: 13,
            backgroundColor: avatarTint,
            child: leadIcon != null
                ? AppIcon(leadIcon, size: 13, color: onSurfaceColor)
                : Text(
                    conversation.title.characters.first.toUpperCase(),
                    style: theme.textTheme.labelLarge?.copyWith(
                      fontWeight: FontWeight.w700,
                      fontSize: 11.8,
                    ),
                  ),
          ),
          const SizedBox(width: 7),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisAlignment: MainAxisAlignment.center,
              children: <Widget>[
                Text(
                  conversation.title,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w700,
                    fontSize: 14.6,
                    letterSpacing: -0.12,
                  ),
                ),
                Text(
                  headerSummary,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(
                    fontSize: 10.0,
                    height: 1.05,
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: 0.72,
                    ),
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 4),
          _HeaderActionButton(
            key: const Key('mobile-chat-header-search'),
            icon: AppSemanticIcon.search,
            tooltip: '搜索消息',
            onPressed: onSearchMessages,
          ),
          const SizedBox(width: 2),
          _HeaderActionButton(
            key: const Key('mobile-chat-header-more'),
            icon: AppSemanticIcon.more,
            tooltip: '更多操作',
            onPressed: onMoreActions,
          ),
          const SizedBox(width: 4),
        ],
      ),
    );

    return SafeArea(
      bottom: false,
      child: Padding(
        key: const Key('mobile-chat-detail-header'),
        padding: material.isGlass
            ? const EdgeInsets.fromLTRB(4, 0, 4, 1)
            : const EdgeInsets.fromLTRB(0, 0, 0, 1),
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: theme.colorScheme.surface.withValues(alpha: 1.0),
            border: Border(
              bottom: BorderSide(
                color: palette.divider.withValues(
                  alpha: material.isGlass ? 0.08 : 0.1,
                ),
              ),
            ),
          ),
          child: materialHeaderContent,
        ),
      ),
    );
  }
}

void _showMobileMessageSearch(
  BuildContext context,
  ConversationSummary conversation,
  List<ChatMessage> messages, {
  required ValueChanged<ChatMessage> onOpenMessage,
}) {
  showModalBottomSheet<void>(
    context: context,
    isScrollControlled: true,
    builder: (context) => _MessageSearchSheet(
      conversation: conversation,
      messages: messages,
      onOpenMessage: onOpenMessage,
    ),
  );
}

void _showMobileConversationActions(
  BuildContext context, {
  required ConversationSummary conversation,
  required List<ChatMessage> messages,
  required VoidCallback onSearchMessages,
  required ValueChanged<ChatMessage> onOpenMessage,
  required Future<void> Function() onClearHistory,
}) {
  final attachmentMessages = messages
      .where((message) => message.attachment != null)
      .toList(growable: false);
  showModalBottomSheet<void>(
    context: context,
    builder: (sheetContext) => SafeArea(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          ListTile(
            key: const Key('mobile-chat-more-search'),
            leading: const AppIcon(AppSemanticIcon.search),
            title: const Text('搜索消息'),
            onTap: () {
              Navigator.of(sheetContext).pop();
              onSearchMessages();
            },
          ),
          ListTile(
            key: const Key('mobile-chat-more-media'),
            leading: const AppIcon(AppSemanticIcon.file),
            title: const Text('媒体与文件'),
            subtitle: Text('${attachmentMessages.length} 个附件'),
            onTap: () {
              Navigator.of(sheetContext).pop();
              _showMobileAttachmentSummary(
                context,
                conversation,
                attachmentMessages,
                onOpenMessage: onOpenMessage,
              );
            },
          ),
          ListTile(
            key: const Key('mobile-chat-more-clear-history'),
            leading: const AppIcon(AppSemanticIcon.alert),
            title: const Text('清空聊天记录'),
            onTap: () async {
              Navigator.of(sheetContext).pop();
              final confirmed = await _confirmMobileClearHistory(
                context,
                conversation.title,
              );
              if (!context.mounted) {
                return;
              }
              if (confirmed) {
                await onClearHistory();
              }
            },
          ),
        ],
      ),
    ),
  );
}

void _showMobileAttachmentSummary(
  BuildContext context,
  ConversationSummary conversation,
  List<ChatMessage> attachmentMessages, {
  required ValueChanged<ChatMessage> onOpenMessage,
}) {
  showModalBottomSheet<void>(
    context: context,
    builder: (sheetContext) => SafeArea(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(18, 14, 18, 18),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text(
              '${conversation.title} 的附件',
              style: Theme.of(
                context,
              ).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w700),
            ),
            const SizedBox(height: 10),
            if (attachmentMessages.isEmpty)
              const Text('当前会话暂无附件')
            else
              for (final message in attachmentMessages.take(6))
                ListTile(
                  key: ValueKey<String>(
                    'mobile-chat-attachment-summary-${message.id}',
                  ),
                  dense: true,
                  contentPadding: EdgeInsets.zero,
                  leading: const AppIcon(AppSemanticIcon.file),
                  title: Text(message.attachment!.title),
                  subtitle: Text(message.attachment!.detail),
                  onTap: () {
                    Navigator.of(sheetContext).pop();
                    onOpenMessage(message);
                  },
                ),
          ],
        ),
      ),
    ),
  );
}

Future<bool> _confirmMobileClearHistory(
  BuildContext context,
  String title,
) async {
  final result = await showDialog<bool>(
    context: context,
    builder: (context) => AlertDialog(
      title: const Text('清空聊天记录'),
      content: Text('清空 $title 的本地聊天记录？'),
      actions: <Widget>[
        TextButton(
          onPressed: () => Navigator.of(context).pop(false),
          child: const Text('取消'),
        ),
        FilledButton(
          key: const Key('mobile-chat-clear-history-confirm'),
          onPressed: () => Navigator.of(context).pop(true),
          child: const Text('清空'),
        ),
      ],
    ),
  );
  return result ?? false;
}

class _MessageSearchSheet extends StatefulWidget {
  const _MessageSearchSheet({
    required this.conversation,
    required this.messages,
    required this.onOpenMessage,
  });

  final ConversationSummary conversation;
  final List<ChatMessage> messages;
  final ValueChanged<ChatMessage> onOpenMessage;

  @override
  State<_MessageSearchSheet> createState() => _MessageSearchSheetState();
}

class _MessageSearchSheetState extends State<_MessageSearchSheet> {
  final TextEditingController _controller = TextEditingController();
  String _query = '';

  @override
  void initState() {
    super.initState();
    _controller.addListener(_syncQuery);
  }

  @override
  void dispose() {
    _controller.removeListener(_syncQuery);
    _controller.dispose();
    super.dispose();
  }

  void _syncQuery() {
    setState(() {
      _query = _controller.text.trim();
    });
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final results = _query.isEmpty
        ? widget.messages
        : widget.messages
              .where(
                (message) => '${message.senderLabel} ${message.text}'
                    .toLowerCase()
                    .contains(_query.toLowerCase()),
              )
              .toList(growable: false);
    return SafeArea(
      child: Padding(
        padding: EdgeInsets.fromLTRB(
          16,
          12,
          16,
          MediaQuery.viewInsetsOf(context).bottom + 16,
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text(
              '搜索 ${widget.conversation.title}',
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 10),
            TextField(
              key: const Key('mobile-chat-message-search-input'),
              controller: _controller,
              autofocus: true,
              textInputAction: TextInputAction.search,
              decoration: const InputDecoration(
                prefixIcon: AppIcon(AppSemanticIcon.search),
                hintText: '搜索消息内容',
              ),
            ),
            const SizedBox(height: 10),
            ConstrainedBox(
              constraints: const BoxConstraints(maxHeight: 300),
              child: results.isEmpty
                  ? const Center(child: Text('没有找到相关消息'))
                  : ListView.builder(
                      shrinkWrap: true,
                      itemCount: results.length,
                      itemBuilder: (context, index) {
                        final message = results[index];
                        return ListTile(
                          key: ValueKey<String>(
                            'mobile-chat-message-search-result-${message.id}',
                          ),
                          dense: true,
                          title: Text(message.text),
                          subtitle: Text(
                            '${message.senderLabel} · ${_formatClock(message.timestamp)}',
                          ),
                          onTap: () {
                            Navigator.of(context).pop();
                            widget.onOpenMessage(message);
                          },
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

class _HeaderActionButton extends StatelessWidget {
  const _HeaderActionButton({
    super.key,
    required this.icon,
    required this.tooltip,
    required this.onPressed,
    this.plain = false,
  });

  final AppSemanticIcon icon;
  final String tooltip;
  final VoidCallback onPressed;
  final bool plain;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final foregroundColor = plain
        ? theme.textTheme.bodyMedium?.color?.withValues(alpha: 0.8) ??
              theme.colorScheme.onSurface.withValues(alpha: 0.8)
        : theme.colorScheme.onSurface.withValues(alpha: 0.76);
    final visualSize = plain ? 24.0 : 30.0;
    final touchTarget = plain ? 38.0 : 34.0;
    final iconSize = plain ? 14.0 : 15.0;
    Widget content = SizedBox(
      width: visualSize,
      height: visualSize,
      child: Center(
        child: AppIcon(icon, size: iconSize, color: foregroundColor),
      ),
    );
    if (!plain) {
      content = DecoratedBox(
        decoration: BoxDecoration(
          color: theme.extension<AppShellColors>()!.surfaceVariant.withValues(
            alpha: 0.07,
          ),
          borderRadius: BorderRadius.circular(visualSize / 2),
        ),
        child: content,
      );
    }
    if (plain) {
      return CupertinoButton(
        onPressed: onPressed,
        padding: EdgeInsets.zero,
        minimumSize: Size.square(touchTarget),
        child: SizedBox.square(
          dimension: touchTarget,
          child: Center(child: content),
        ),
      );
    }
    return Tooltip(
      message: tooltip,
      child: SizedBox.square(
        dimension: touchTarget,
        child: Material(
          color: Colors.transparent,
          child: InkWell(
            borderRadius: BorderRadius.circular(visualSize / 2),
            onTap: onPressed,
            child: Center(child: content),
          ),
        ),
      ),
    );
  }
}

class _CupertinoBackLabelButton extends StatelessWidget {
  const _CupertinoBackLabelButton({
    required this.onPressed,
    required this.label,
  });

  final VoidCallback onPressed;
  final String label;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return CupertinoButton(
      key: const Key('mobile-chat-header-back'),
      padding: EdgeInsets.zero,
      minimumSize: const Size(54, 36),
      onPressed: onPressed,
      child: SizedBox(
        height: 36,
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            AppIcon(
              AppSemanticIcon.back,
              size: 13.0,
              color: theme.colorScheme.primary,
            ),
            const SizedBox(width: 1),
            Text(
              label,
              style: theme.textTheme.bodyMedium?.copyWith(
                color: theme.colorScheme.primary,
                fontWeight: FontWeight.w500,
                fontSize: 13.2,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

String _detailHeaderSummary(ConversationSummary conversation) {
  return _detailSubtitle(conversation);
}

class _MobileChatTimeline extends StatefulWidget {
  const _MobileChatTimeline({
    super.key,
    required this.conversation,
    required this.messages,
    required this.initialUnreadCount,
    required this.autoScrollToLatest,
    required this.contentPadding,
  });

  final ConversationSummary conversation;
  final List<ChatMessage> messages;
  final int initialUnreadCount;
  final bool autoScrollToLatest;
  final EdgeInsets contentPadding;

  @override
  State<_MobileChatTimeline> createState() => _MobileChatTimelineState();
}

class _MobileChatTimelineState extends State<_MobileChatTimeline> {
  late final ScrollController _scrollController;
  late int _openingUnreadCount;

  @override
  void initState() {
    super.initState();
    _scrollController = ScrollController();
    _openingUnreadCount = widget.initialUnreadCount;
    if (widget.autoScrollToLatest) {
      _scheduleJumpToLatest();
    }
  }

  @override
  void didUpdateWidget(covariant _MobileChatTimeline oldWidget) {
    super.didUpdateWidget(oldWidget);
    final switchedConversation =
        oldWidget.conversation.id != widget.conversation.id;
    if (switchedConversation ||
        oldWidget.initialUnreadCount != widget.initialUnreadCount) {
      _openingUnreadCount = widget.initialUnreadCount;
    }
    final messageCountChanged =
        oldWidget.messages.length != widget.messages.length;
    if (widget.autoScrollToLatest &&
        (switchedConversation || (messageCountChanged && _isNearLatest()))) {
      _scheduleJumpToLatest();
    }
  }

  @override
  void dispose() {
    _scrollController.dispose();
    super.dispose();
  }

  bool _isNearLatest() {
    if (!_scrollController.hasClients) {
      return true;
    }
    final position = _scrollController.position;
    return position.maxScrollExtent - position.pixels < 180;
  }

  void _scheduleJumpToLatest() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted || !_scrollController.hasClients) {
        return;
      }
      final maxScrollExtent = _scrollController.position.maxScrollExtent;
      if (maxScrollExtent <= 0) {
        return;
      }
      final boundarySlack = Theme.of(context).platform == TargetPlatform.iOS
          ? 0.0
          : ImUiMetrics.androidChatDetailLatestBoundarySlack;
      _scrollController.jumpTo(
        (maxScrollExtent - boundarySlack).clamp(0.0, maxScrollExtent),
      );
    });
  }

  void revealMessage(String messageId) {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) {
        return;
      }
      final targetContext = GlobalObjectKey(
        'mobile-chat-message-$messageId',
      ).currentContext;
      if (targetContext != null) {
        Scrollable.ensureVisible(
          targetContext,
          duration: const Duration(milliseconds: 260),
          curve: Curves.easeOutCubic,
          alignment: 0.35,
        );
        return;
      }
      final index = widget.messages.indexWhere(
        (message) => message.id == messageId,
      );
      if (index < 0 || !_scrollController.hasClients) {
        return;
      }
      final maxScrollExtent = _scrollController.position.maxScrollExtent;
      final denominator = widget.messages.length <= 1
          ? 1
          : widget.messages.length - 1;
      _scrollController.animateTo(
        maxScrollExtent * index / denominator,
        duration: const Duration(milliseconds: 260),
        curve: Curves.easeOutCubic,
      );
    });
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    if (widget.messages.isEmpty) {
      return _EmptyTimelineState(conversation: widget.conversation);
    }

    final children = <Widget>[];
    ChatMessage? previousVisibleMessage;
    DateTime? lastSeparatorTime;
    final unreadAnchorMessage = _unreadAnchorMessage(
      widget.conversation,
      widget.messages,
      unreadCountOverride: _openingUnreadCount,
    );
    final unreadCount = _effectiveUnreadCount(
      widget.conversation,
      widget.messages,
      unreadCountOverride: _openingUnreadCount,
    );
    var unreadSeparatorInserted = false;

    for (var index = 0; index < widget.messages.length; index += 1) {
      final message = widget.messages[index];
      if (_shouldInsertSeparator(lastSeparatorTime, message.timestamp)) {
        children.add(
          _DetailTimeSeparator(
            label: _formatSeparatorLabel(message.timestamp),
            palette: palette,
          ),
        );
        children.add(const SizedBox(height: 1.5));
        lastSeparatorTime = message.timestamp;
      }

      if (!unreadSeparatorInserted && unreadAnchorMessage?.id == message.id) {
        if (children.isNotEmpty) {
          children.add(const SizedBox(height: 4));
        }
        children.add(
          _UnreadMessageSeparator(label: '$unreadCount 条新消息', palette: palette),
        );
        children.add(const SizedBox(height: 4));
        unreadSeparatorInserted = true;
        previousVisibleMessage = null;
      }

      if (message.isSystemNotice) {
        if (children.isNotEmpty) {
          children.add(const SizedBox(height: 2));
        }
        children.add(_SystemNoticeLine(text: message.text));
        children.add(const SizedBox(height: 3));
        previousVisibleMessage = null;
        continue;
      }

      final isClusterStart = !_belongsToPreviousCluster(
        previousVisibleMessage,
        message,
      );
      final nextVisibleMessage = _nextVisibleMessage(
        widget.messages,
        index + 1,
      );
      final isClusterEnd =
          nextVisibleMessage == null ||
          !_belongsToPreviousCluster(message, nextVisibleMessage);
      children.add(
        _ChatMessageTile(
          message: message,
          conversation: widget.conversation,
          showMeta: isClusterEnd,
          isClusterStart: isClusterStart,
          isClusterEnd: isClusterEnd,
          showAvatar:
              widget.conversation.isGroupConversation &&
              message.direction == MessageDirection.incoming &&
              isClusterStart,
          showSenderLabel:
              widget.conversation.isGroupConversation &&
              message.direction == MessageDirection.incoming &&
              isClusterStart,
        ),
      );
      children.add(SizedBox(height: isClusterEnd ? 7 : 2.25));
      previousVisibleMessage = message;
    }

    if (children.isNotEmpty) {
      children.removeLast();
    }

    return ListView(
      key: const Key('mobile-chat-timeline'),
      controller: _scrollController,
      padding: widget.contentPadding,
      keyboardDismissBehavior: ScrollViewKeyboardDismissBehavior.onDrag,
      children: children,
    );
  }
}

class _ChatMessageTile extends StatelessWidget {
  const _ChatMessageTile({
    required this.message,
    required this.conversation,
    required this.showMeta,
    required this.isClusterStart,
    required this.isClusterEnd,
    required this.showAvatar,
    required this.showSenderLabel,
  });

  final ChatMessage message;
  final ConversationSummary conversation;
  final bool showMeta;
  final bool isClusterStart;
  final bool isClusterEnd;
  final bool showAvatar;
  final bool showSenderLabel;

  @override
  Widget build(BuildContext context) {
    final isOutgoing = message.direction == MessageDirection.outgoing;
    final palette = Theme.of(context).extension<AppShellColors>()!;
    final theme = Theme.of(context);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final hasAttachment = message.attachment != null;
    final bubbleColor = _platformBubbleColor(
      theme: theme,
      palette: palette,
      isOutgoing: isOutgoing,
      cupertinoStyle: isCupertinoStyle,
    );
    final bubbleSurface = hasAttachment
        ? _attachmentBubbleColor(
            theme: theme,
            palette: palette,
            isOutgoing: isOutgoing,
            cupertinoStyle: isCupertinoStyle,
          )
        : bubbleColor;
    final bubbleBorderColor = hasAttachment
        ? palette.divider.withValues(alpha: isCupertinoStyle ? 0.044 : 0.062)
        : Colors.transparent;
    final bubbleBorderRadius = _bubbleBorderRadius(
      isOutgoing: isOutgoing,
      isClusterStart: isClusterStart,
      isClusterEnd: isClusterEnd,
      cupertinoStyle: isCupertinoStyle,
    );
    final maxBubbleWidth =
        MediaQuery.sizeOf(context).width *
        (hasAttachment
            ? (_isImageLikeAttachment(message.attachment!.title)
                  ? (isCupertinoStyle
                        ? (conversation.isGroupConversation
                              ? ImUiMetrics.iosMediaBubbleMaxWidth - 0.02
                              : ImUiMetrics.iosMediaBubbleMaxWidth)
                        : (conversation.isGroupConversation
                              ? ImUiMetrics.androidMediaBubbleMaxWidth - 0.02
                              : ImUiMetrics.androidMediaBubbleMaxWidth))
                  : (isCupertinoStyle
                        ? (conversation.isGroupConversation
                              ? ImUiMetrics.iosBubbleMaxWidth - 0.03
                              : ImUiMetrics.iosBubbleMaxWidth)
                        : (conversation.isGroupConversation
                              ? ImUiMetrics.androidBubbleMaxWidth - 0.03
                              : ImUiMetrics.androidBubbleMaxWidth)))
            : (isCupertinoStyle
                  ? (conversation.isGroupConversation
                        ? ImUiMetrics.iosBubbleMaxWidth - 0.04
                        : ImUiMetrics.iosBubbleMaxWidth)
                  : (conversation.isGroupConversation
                        ? ImUiMetrics.androidBubbleMaxWidth - 0.02
                        : ImUiMetrics.androidBubbleMaxWidth)));
    final messageAlign = isOutgoing
        ? CrossAxisAlignment.end
        : CrossAxisAlignment.start;
    final bubbleHorizontalPadding = hasAttachment
        ? 0.0
        : (isCupertinoStyle ? 11.0 : 10.5);
    final bubbleVerticalPadding = hasAttachment
        ? 0.0
        : (isCupertinoStyle ? 6.0 : 5.5);
    final avatarRadius = isCupertinoStyle ? 12.5 : 13.5;
    const avatarGap = 7.0;
    final avatarGutter =
        conversation.isGroupConversation &&
            message.direction == MessageDirection.incoming &&
            !showAvatar
        ? avatarRadius * 2 + avatarGap
        : 0.0;
    final senderLabelLeftPadding = showAvatar
        ? avatarRadius * 2 + avatarGap
        : 5.0;

    return Padding(
      key: GlobalObjectKey('mobile-chat-message-${message.id}'),
      padding: EdgeInsets.only(top: isClusterStart ? 4.5 : 0.75),
      child: Column(
        crossAxisAlignment: messageAlign,
        children: <Widget>[
          if (!isOutgoing && showSenderLabel)
            Padding(
              padding: EdgeInsets.only(left: senderLabelLeftPadding, bottom: 3),
              child: Text(
                message.senderLabel,
                style: theme.textTheme.labelMedium?.copyWith(
                  fontWeight: FontWeight.w600,
                  fontSize: 9.2,
                  color: theme.textTheme.bodySmall?.color,
                ),
              ),
            ),
          Row(
            mainAxisAlignment: isOutgoing
                ? MainAxisAlignment.end
                : MainAxisAlignment.start,
            crossAxisAlignment: CrossAxisAlignment.end,
            children: <Widget>[
              if (avatarGutter > 0) SizedBox(width: avatarGutter),
              if (showAvatar) ...<Widget>[
                SocialAvatar(
                  id: message.senderLabel,
                  title: message.senderLabel,
                  size: avatarRadius * 2,
                ),
                const SizedBox(width: avatarGap),
              ],
              Flexible(
                child: ConstrainedBox(
                  constraints: BoxConstraints(maxWidth: maxBubbleWidth),
                  child: Column(
                    crossAxisAlignment: messageAlign,
                    children: <Widget>[
                      GestureDetector(
                        onLongPress: () => _showMessageActions(context),
                        key: Key(
                          isCupertinoStyle
                              ? 'mobile-chat-bubble-cupertino'
                              : 'mobile-chat-bubble-material',
                        ),
                        child: Container(
                          clipBehavior: hasAttachment
                              ? Clip.antiAlias
                              : Clip.none,
                          decoration: BoxDecoration(
                            color: bubbleSurface,
                            borderRadius: bubbleBorderRadius,
                            border: hasAttachment
                                ? Border.all(color: bubbleBorderColor)
                                : null,
                          ),
                          child: Padding(
                            padding: EdgeInsets.symmetric(
                              horizontal: bubbleHorizontalPadding,
                              vertical: bubbleVerticalPadding,
                            ),
                            child: _ChatBubbleBody(
                              message: message,
                              isOutgoing: isOutgoing,
                            ),
                          ),
                        ),
                      ),
                      if (message.reactions.isNotEmpty) ...<Widget>[
                        const SizedBox(height: 3),
                        ReactionBar(
                          reactions: message.reactions,
                          isOutgoing: isOutgoing,
                          compact: isCupertinoStyle,
                        ),
                      ],
                      const SizedBox(height: 2),
                      _ChatMessageMeta(
                        message: message,
                        isOutgoing: isOutgoing,
                        showMeta: showMeta,
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  void _showMessageActions(BuildContext context) {
    final cupertinoStyle = Theme.of(context).platform == TargetPlatform.iOS;
    if (cupertinoStyle) {
      showCupertinoModalPopup<void>(
        context: context,
        builder: (context) => const MessageActionSheet(cupertinoStyle: true),
      );
      return;
    }
    showModalBottomSheet<void>(
      context: context,
      builder: (context) => const MessageActionSheet(cupertinoStyle: false),
    );
  }
}

class _ChatBubbleBody extends StatelessWidget {
  const _ChatBubbleBody({required this.message, required this.isOutgoing});

  final ChatMessage message;
  final bool isOutgoing;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final attachmentMetrics = _AttachmentSyntaxMetrics.of(
      theme.platform == TargetPlatform.iOS,
    );
    final textStyle = theme.textTheme.bodyMedium?.copyWith(
      height: 1.28,
      color: isOutgoing
          ? theme.colorScheme.onSurface
          : theme.colorScheme.onSurface,
    );

    final markers = <Widget>[
      if (message.isForwarded) _MessageMiniMarker(label: '转发'),
      if (message.replyPreview case final reply?)
        _MessageReplyStrip(reply: reply, isOutgoing: isOutgoing),
    ];

    if (message.attachment case final attachment?) {
      final useInlineMediaMeta =
          !message.hasText &&
          _isVisualAttachmentKind(_resolvedAttachmentKind(attachment));
      return Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          ...markers,
          if (markers.isNotEmpty) const SizedBox(height: 5),
          if (useInlineMediaMeta)
            Stack(
              clipBehavior: Clip.none,
              children: <Widget>[
                _CompactAttachmentMessageCard(
                  attachment: attachment,
                  isOutgoing: isOutgoing,
                ),
                Positioned(
                  right: 8.5,
                  bottom: attachment.state.hasProgress ? 10 : 9,
                  child: _InlineMediaMetaPill(
                    message: message,
                    isOutgoing: isOutgoing,
                  ),
                ),
              ],
            )
          else
            _CompactAttachmentMessageCard(
              attachment: attachment,
              isOutgoing: isOutgoing,
            ),
          if (message.hasText) ...<Widget>[
            SizedBox(height: attachmentMetrics.captionGap),
            _MessageTextWithFlags(message: message, style: textStyle),
          ],
        ],
      );
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        ...markers,
        if (markers.isNotEmpty) const SizedBox(height: 5),
        _MessageTextWithFlags(message: message, style: textStyle),
      ],
    );
  }
}

class _MessageReplyStrip extends StatelessWidget {
  const _MessageReplyStrip({required this.reply, required this.isOutgoing});

  final MessageReplyPreview reply;
  final bool isOutgoing;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Container(
      key: const Key('mobile-chat-reply-strip'),
      padding: const EdgeInsets.fromLTRB(7, 5, 7, 5),
      decoration: BoxDecoration(
        color: (isOutgoing ? Colors.white : theme.colorScheme.primary)
            .withValues(alpha: isOutgoing ? 0.075 : 0.055),
        borderRadius: BorderRadius.circular(9),
        border: Border(
          left: BorderSide(
            color: isOutgoing
                ? Colors.white.withValues(alpha: 0.34)
                : theme.colorScheme.primary.withValues(alpha: 0.46),
            width: 2,
          ),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Text(
            reply.senderLabel,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: theme.textTheme.labelSmall?.copyWith(
              color: isOutgoing ? Colors.white : theme.colorScheme.primary,
              fontWeight: FontWeight.w800,
              fontSize: 9.4,
            ),
          ),
          const SizedBox(height: 1),
          Text(
            reply.text,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: theme.textTheme.labelSmall?.copyWith(
              color: theme.colorScheme.onSurface.withValues(alpha: 0.70),
              fontSize: 9.4,
            ),
          ),
        ],
      ),
    );
  }
}

class _MessageMiniMarker extends StatelessWidget {
  const _MessageMiniMarker({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) {
    return Text(
      label,
      key: ValueKey<String>('mobile-chat-message-marker-$label'),
      style: Theme.of(context).textTheme.labelSmall?.copyWith(
        fontSize: 9.4,
        fontWeight: FontWeight.w700,
        color: Theme.of(
          context,
        ).textTheme.bodySmall?.color?.withValues(alpha: 0.66),
      ),
    );
  }
}

class _MessageTextWithFlags extends StatelessWidget {
  const _MessageTextWithFlags({required this.message, required this.style});

  final ChatMessage message;
  final TextStyle? style;

  @override
  Widget build(BuildContext context) {
    if (!message.isEdited) {
      return Text(message.text, style: style);
    }
    return Text.rich(
      TextSpan(
        children: <InlineSpan>[
          TextSpan(text: message.text),
          TextSpan(
            text: '  已编辑',
            style: Theme.of(context).textTheme.labelSmall?.copyWith(
              fontSize: 9,
              color: Theme.of(
                context,
              ).textTheme.bodySmall?.color?.withValues(alpha: 0.62),
            ),
          ),
        ],
      ),
      style: style,
    );
  }
}

class _ChatMessageMeta extends StatelessWidget {
  const _ChatMessageMeta({
    required this.message,
    required this.isOutgoing,
    required this.showMeta,
  });

  final ChatMessage message;
  final bool isOutgoing;
  final bool showMeta;

  @override
  Widget build(BuildContext context) {
    if (!showMeta) {
      return const SizedBox.shrink();
    }
    if (message.attachment case final attachment?) {
      if (!message.hasText &&
          _isVisualAttachmentKind(_resolvedAttachmentKind(attachment))) {
        return const SizedBox.shrink();
      }
    }
    final theme = Theme.of(context);
    final mutedColor = theme.textTheme.labelSmall?.color?.withValues(
      alpha: 0.5,
    );
    final rowChildren = <Widget>[
      Text(
        key: const Key('mobile-chat-message-meta-time'),
        _formatClock(message.timestamp),
        style: theme.textTheme.labelSmall?.copyWith(
          color: mutedColor,
          fontSize: 8.5,
          height: 1.0,
        ),
      ),
      if (isOutgoing) ...<Widget>[
        const SizedBox(width: 3.5),
        AppIcon(
          key: const Key('mobile-chat-message-meta-status'),
          AppIcons.messageStatus(message.status),
          size: 9.0,
          color: message.status == MessageDeliveryStatus.read
              ? theme.colorScheme.primary.withValues(alpha: 0.7)
              : mutedColor,
        ),
      ],
    ];

    return Padding(
      key: const Key('mobile-chat-message-meta'),
      padding: const EdgeInsets.symmetric(horizontal: 4.0),
      child: Row(mainAxisSize: MainAxisSize.min, children: rowChildren),
    );
  }
}

class _InlineMediaMetaPill extends StatelessWidget {
  const _InlineMediaMetaPill({required this.message, required this.isOutgoing});

  final ChatMessage message;
  final bool isOutgoing;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return ClipRRect(
      borderRadius: BorderRadius.circular(999),
      child: BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 8, sigmaY: 8),
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 6.5, vertical: 4.0),
          decoration: BoxDecoration(
            color: Colors.black.withValues(alpha: 0.13),
            borderRadius: BorderRadius.circular(999),
            border: Border.all(color: Colors.white.withValues(alpha: 0.08)),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              Text(
                _formatClock(message.timestamp),
                style: theme.textTheme.labelSmall?.copyWith(
                  color: Colors.white.withValues(alpha: 0.9),
                  fontSize: 8.2,
                  height: 1.0,
                ),
              ),
              if (isOutgoing) ...<Widget>[
                const SizedBox(width: 3.5),
                AppIcon(
                  AppIcons.messageStatus(message.status),
                  size: 9.2,
                  color: message.status == MessageDeliveryStatus.read
                      ? theme.colorScheme.primary.withValues(alpha: 0.92)
                      : Colors.white.withValues(alpha: 0.72),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

class _ChatComposerDock extends StatefulWidget {
  const _ChatComposerDock({required this.onSend, required this.onSendFile});

  final ValueChanged<String> onSend;
  final FutureOr<void> Function(String) onSendFile;

  @override
  State<_ChatComposerDock> createState() => _ChatComposerDockState();
}

class _ChatComposerDockState extends State<_ChatComposerDock> {
  final TextEditingController _controller = TextEditingController();
  bool _hasText = false;

  @override
  void initState() {
    super.initState();
    _controller.addListener(_syncComposerState);
    _syncComposerState();
  }

  @override
  void dispose() {
    _controller.removeListener(_syncComposerState);
    _controller.dispose();
    super.dispose();
  }

  void _syncComposerState() {
    final hasText = _controller.text.trim().isNotEmpty;
    if (hasText != _hasText && mounted) {
      setState(() {
        _hasText = hasText;
      });
    } else {
      _hasText = hasText;
    }
  }

  void _submit() {
    final text = _controller.text.trim();
    if (text.isEmpty) {
      return;
    }
    _controller.clear();
    widget.onSend(text);
  }

  void _insertText(String text) {
    final value = _controller.value;
    final start = value.selection.start < 0
        ? value.text.length
        : value.selection.start;
    final end = value.selection.end < 0
        ? value.text.length
        : value.selection.end;
    final nextText = value.text.replaceRange(start, end, text);
    final offset = start + text.length;
    _controller.value = TextEditingValue(
      text: nextText,
      selection: TextSelection.collapsed(offset: offset),
    );
  }

  Future<void> _sendPickedFile() async {
    final path = await _MobileAttachmentBridge.pickFile();
    if (!mounted) {
      return;
    }
    if (path == null || path.trim().isEmpty) {
      return;
    }
    await Future<void>.value(widget.onSendFile(path.trim()));
  }

  Future<void> _sendImage() async {
    final pickedImage = await _MobileAttachmentBridge.pickImage();
    if (!mounted) {
      await pickedImage?.delete();
      return;
    }
    if (pickedImage != null) {
      await _sendEphemeralFile(pickedImage);
    }
  }

  Future<void> _sendVoice() async {
    final recordedPath = await showModalBottomSheet<String>(
      context: context,
      isScrollControlled: true,
      builder: (context) => const _VoiceRecorderSheet(),
    );
    if (!mounted) {
      return;
    }
    if (recordedPath != null && recordedPath.trim().isNotEmpty) {
      await _sendEphemeralFile(
        _EphemeralPickedFile(
          path: recordedPath.trim(),
          cleanupPaths: <String>{recordedPath.trim()},
        ),
      );
    }
  }

  Future<void> _sendEphemeralFile(_EphemeralPickedFile file) async {
    try {
      await Future<void>.value(widget.onSendFile(file.path));
    } finally {
      await file.delete();
    }
  }

  Future<void> _showEmojiPicker() async {
    final emoji = await showModalBottomSheet<String>(
      context: context,
      builder: (context) => const _EmojiPickerSheet(),
    );
    if (emoji == null) {
      return;
    }
    if (!mounted) {
      return;
    }
    _insertText(emoji);
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    Widget sendSwitcher({required bool cupertinoStyle}) {
      return AnimatedSwitcher(
        duration: const Duration(milliseconds: 180),
        switchInCurve: Curves.easeOutCubic,
        switchOutCurve: Curves.easeInCubic,
        transitionBuilder: (Widget child, Animation<double> animation) {
          return FadeTransition(
            opacity: animation,
            child: ScaleTransition(scale: animation, child: child),
          );
        },
        child: _hasText
            ? _ComposerActionButton(
                key: const Key('mobile-chat-composer-action-send'),
                icon: AppSemanticIcon.send,
                filled: true,
                cupertinoStyle: cupertinoStyle,
                onPressed: _submit,
              )
            : const SizedBox(
                key: Key('mobile-chat-composer-action-idle'),
                width: 1,
                height: 34,
              ),
      );
    }

    if (isCupertinoStyle) {
      final inputSurface = Color.alphaBlend(
        palette.surfaceVariant.withValues(alpha: 0.08),
        theme.colorScheme.surface.withValues(alpha: 0.90),
      );
      final inputBorder = palette.divider.withValues(alpha: 0.095);
      final shellSurface = Color.alphaBlend(
        palette.surfaceVariant.withValues(alpha: 0.03),
        theme.colorScheme.surface.withValues(alpha: 0.96),
      );
      final shellBorder = palette.divider.withValues(alpha: 0.105);
      return SafeArea(
        top: false,
        child: Padding(
          padding: const EdgeInsets.fromLTRB(8, 4, 8, 8),
          child: DecoratedBox(
            key: const Key('mobile-chat-composer-shell'),
            decoration: BoxDecoration(
              color: shellSurface,
              borderRadius: BorderRadius.circular(22),
              border: Border.all(color: shellBorder),
            ),
            child: Padding(
              padding: const EdgeInsets.fromLTRB(6, 5, 6, 5),
              child: SizedBox(
                key: const Key('mobile-chat-composer'),
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: <Widget>[
                    _ComposerShortcutButton(
                      key: const Key('mobile-chat-composer-shortcut-image'),
                      label: '图片',
                      icon: AppSemanticIcon.image,
                      cupertinoStyle: true,
                      onPressed: _sendImage,
                    ),
                    const SizedBox(width: 2),
                    _ComposerShortcutButton(
                      key: const Key('mobile-chat-composer-shortcut-file'),
                      label: '文件',
                      icon: AppSemanticIcon.file,
                      cupertinoStyle: true,
                      onPressed: _sendPickedFile,
                    ),
                    const SizedBox(width: 5),
                    Expanded(
                      child: Container(
                        key: const Key('mobile-chat-composer-input-cupertino'),
                        constraints: const BoxConstraints(minHeight: 38),
                        padding: const EdgeInsets.symmetric(horizontal: 11),
                        decoration: BoxDecoration(
                          color: inputSurface,
                          borderRadius: BorderRadius.circular(19),
                          border: Border.all(color: inputBorder),
                        ),
                        child: TextField(
                          controller: _controller,
                          minLines: 1,
                          maxLines: 4,
                          textInputAction: TextInputAction.send,
                          textAlignVertical: TextAlignVertical.center,
                          enableSuggestions: false,
                          autocorrect: false,
                          smartDashesType: SmartDashesType.disabled,
                          smartQuotesType: SmartQuotesType.disabled,
                          cursorColor: theme.colorScheme.primary,
                          onSubmitted: (_) => _submit(),
                          decoration: InputDecoration(
                            hintText: '发送消息',
                            hintStyle: theme.textTheme.bodyMedium?.copyWith(
                              fontSize: 13.0,
                              color: theme.textTheme.bodySmall?.color
                                  ?.withValues(alpha: 0.62),
                            ),
                            border: InputBorder.none,
                            isDense: true,
                            contentPadding: const EdgeInsets.symmetric(
                              vertical: 7,
                            ),
                          ),
                        ),
                      ),
                    ),
                    const SizedBox(width: 4),
                    _ComposerShortcutButton(
                      key: const Key('mobile-chat-composer-shortcut-emoji'),
                      label: '表情',
                      icon: AppSemanticIcon.emoji,
                      cupertinoStyle: true,
                      onPressed: _showEmojiPicker,
                    ),
                    const SizedBox(width: 2),
                    _ComposerShortcutButton(
                      key: const Key('mobile-chat-composer-shortcut-voice'),
                      label: '语音',
                      icon: AppSemanticIcon.voice,
                      cupertinoStyle: true,
                      onPressed: _sendVoice,
                    ),
                    const SizedBox(width: 1),
                    sendSwitcher(cupertinoStyle: true),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
    }
    final materialInputSurface = Color.alphaBlend(
      palette.surfaceVariant.withValues(alpha: 0.08),
      theme.colorScheme.surface.withValues(alpha: 0.84),
    );
    final materialInputBorder = palette.divider.withValues(alpha: 0.09);
    final materialComposerRow = DecoratedBox(
      decoration: BoxDecoration(
        color: theme.colorScheme.surface.withValues(alpha: 0.94),
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: palette.divider.withValues(alpha: 0.08)),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(7, 6, 7, 6),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.center,
          children: <Widget>[
            _ComposerShortcutButton(
              key: const Key('mobile-chat-composer-shortcut-image'),
              label: '图片',
              icon: AppSemanticIcon.image,
              cupertinoStyle: false,
              onPressed: _sendImage,
            ),
            const SizedBox(width: 2),
            _ComposerShortcutButton(
              key: const Key('mobile-chat-composer-shortcut-file'),
              label: '文件',
              icon: AppSemanticIcon.file,
              cupertinoStyle: false,
              onPressed: _sendPickedFile,
            ),
            const SizedBox(width: 5),
            Expanded(
              child: Container(
                key: const Key('mobile-chat-composer-input-material'),
                constraints: const BoxConstraints(minHeight: 38),
                padding: const EdgeInsets.symmetric(horizontal: 10),
                decoration: BoxDecoration(
                  color: materialInputSurface,
                  borderRadius: BorderRadius.circular(19),
                  border: Border.all(color: materialInputBorder),
                ),
                child: TextField(
                  controller: _controller,
                  minLines: 1,
                  maxLines: 4,
                  textInputAction: TextInputAction.send,
                  textAlignVertical: TextAlignVertical.center,
                  enableSuggestions: false,
                  autocorrect: false,
                  smartDashesType: SmartDashesType.disabled,
                  smartQuotesType: SmartQuotesType.disabled,
                  onSubmitted: (_) => _submit(),
                  decoration: InputDecoration(
                    hintText: '发送消息',
                    hintStyle: theme.textTheme.bodyMedium?.copyWith(
                      fontSize: 13.2,
                      color: theme.textTheme.bodySmall?.color?.withValues(
                        alpha: 0.64,
                      ),
                    ),
                    border: InputBorder.none,
                    isDense: true,
                    contentPadding: const EdgeInsets.symmetric(vertical: 7),
                  ),
                ),
              ),
            ),
            const SizedBox(width: 4),
            _ComposerShortcutButton(
              key: const Key('mobile-chat-composer-shortcut-emoji'),
              label: '表情',
              icon: AppSemanticIcon.emoji,
              cupertinoStyle: false,
              onPressed: _showEmojiPicker,
            ),
            const SizedBox(width: 1),
            _ComposerShortcutButton(
              key: const Key('mobile-chat-composer-shortcut-voice'),
              label: '语音',
              icon: AppSemanticIcon.voice,
              cupertinoStyle: false,
              onPressed: _sendVoice,
            ),
            const SizedBox(width: 1),
            sendSwitcher(cupertinoStyle: false),
          ],
        ),
      ),
    );
    return SafeArea(
      top: false,
      child: Padding(
        padding: const EdgeInsets.fromLTRB(8, 4, 8, 7),
        child: SizedBox(
          key: const Key('mobile-chat-composer'),
          child: materialComposerRow,
        ),
      ),
    );
  }
}

class _MobileAttachmentBridge {
  const _MobileAttachmentBridge._();

  static final ImagePicker _imagePicker = ImagePicker();

  static Future<String?> pickFile() async {
    final result = await FilePicker.pickFiles(
      type: FileType.any,
      allowMultiple: false,
      withData: false,
    );
    if (result == null || result.files.isEmpty) {
      return null;
    }
    return result.files.single.path;
  }

  static Future<_EphemeralPickedFile?> pickImage() async {
    final image = await _imagePicker.pickImage(
      source: ImageSource.gallery,
      imageQuality: 96,
    );
    final sourcePath = image?.path.trim();
    if (sourcePath == null || sourcePath.isEmpty) {
      return null;
    }
    final sourceFile = File(sourcePath);
    if (!await sourceFile.exists()) {
      return null;
    }
    final directory = await getTemporaryDirectory();
    final targetPath =
        '${directory.path}/mi_image_${DateTime.now().microsecondsSinceEpoch}${_safeExtension(sourcePath, '.jpg')}';
    await sourceFile.copy(targetPath);
    final cleanupPaths = <String>{targetPath};
    if (_isInsideDirectory(sourcePath, directory.path)) {
      cleanupPaths.add(sourcePath);
    }
    return _EphemeralPickedFile(path: targetPath, cleanupPaths: cleanupPaths);
  }

  static String _safeExtension(String path, String fallback) {
    final slash = path.lastIndexOf('/');
    final backslash = path.lastIndexOf(r'\');
    final separator = slash > backslash ? slash : backslash;
    final dot = path.lastIndexOf('.');
    if (dot <= separator || dot == path.length - 1 || path.length - dot > 12) {
      return fallback;
    }
    return path.substring(dot).toLowerCase();
  }

  static bool _isInsideDirectory(String path, String directory) {
    final normalizedPath = path.replaceAll(r'\', '/');
    var normalizedDirectory = directory.replaceAll(r'\', '/');
    while (normalizedDirectory.endsWith('/')) {
      normalizedDirectory = normalizedDirectory.substring(
        0,
        normalizedDirectory.length - 1,
      );
    }
    return normalizedPath == normalizedDirectory ||
        normalizedPath.startsWith('$normalizedDirectory/');
  }
}

class _EphemeralPickedFile {
  const _EphemeralPickedFile({required this.path, required this.cleanupPaths});

  final String path;
  final Set<String> cleanupPaths;

  Future<void> delete() async {
    for (final cleanupPath in cleanupPaths) {
      await _wipeAndDeleteFile(cleanupPath);
    }
  }
}

Future<void> _wipeAndDeleteFile(String path) async {
  final trimmedPath = path.trim();
  if (trimmedPath.isEmpty) {
    return;
  }
  final file = File(trimmedPath);
  try {
    if (!await file.exists()) {
      return;
    }
    final length = await file.length();
    if (length > 0) {
      final sink = file.openWrite(mode: FileMode.write);
      var remaining = length;
      final chunk = List<int>.filled(65536, 0);
      while (remaining > 0) {
        final writeLength = remaining > chunk.length ? chunk.length : remaining;
        sink.add(chunk.take(writeLength).toList(growable: false));
        remaining -= writeLength;
      }
      await sink.flush();
      await sink.close();
    }
    await file.delete();
  } on FileSystemException {
    return;
  }
}

class _VoiceRecorderSheet extends StatefulWidget {
  const _VoiceRecorderSheet();

  @override
  State<_VoiceRecorderSheet> createState() => _VoiceRecorderSheetState();
}

class _VoiceRecorderSheetState extends State<_VoiceRecorderSheet> {
  final AudioRecorder _recorder = AudioRecorder();
  Timer? _timer;
  Duration _elapsed = Duration.zero;
  String? _recordedPath;
  String? _errorText;
  bool _recording = false;
  bool _busy = false;
  bool _pathClaimed = false;

  @override
  void dispose() {
    _timer?.cancel();
    final path = _recordedPath;
    if (!_pathClaimed && path != null) {
      unawaited(_wipeAndDeleteFile(path));
    }
    _recorder.dispose();
    super.dispose();
  }

  Future<void> _start() async {
    if (_busy || _recording) {
      return;
    }
    setState(() {
      _busy = true;
      _errorText = null;
    });
    final previousPath = _recordedPath;
    _recordedPath = null;
    if (previousPath != null) {
      unawaited(_wipeAndDeleteFile(previousPath));
    }
    final hasPermission = await _recorder.hasPermission();
    if (!mounted) {
      return;
    }
    if (!hasPermission) {
      setState(() {
        _busy = false;
        _errorText = '未获得麦克风权限';
      });
      return;
    }
    final directory = await getTemporaryDirectory();
    if (!mounted) {
      return;
    }
    final path =
        '${directory.path}/mi_voice_${DateTime.now().microsecondsSinceEpoch}.m4a';
    await _recorder.start(
      const RecordConfig(encoder: AudioEncoder.aacLc),
      path: path,
    );
    if (!mounted) {
      return;
    }
    _timer?.cancel();
    _timer = Timer.periodic(const Duration(seconds: 1), (_) {
      if (!mounted) {
        return;
      }
      setState(() {
        _elapsed += const Duration(seconds: 1);
      });
    });
    setState(() {
      _recording = true;
      _busy = false;
      _recordedPath = path;
      _elapsed = Duration.zero;
    });
  }

  Future<void> _cancel() async {
    if (_busy) {
      return;
    }
    setState(() {
      _busy = true;
      _errorText = null;
    });
    if (_recording) {
      try {
        await _recorder.stop();
      } on Object {
        _recording = false;
      }
      _timer?.cancel();
    }
    final path = _recordedPath;
    _recordedPath = null;
    _recording = false;
    if (path != null) {
      await _wipeAndDeleteFile(path);
    }
    if (!mounted) {
      return;
    }
    Navigator.of(context).pop();
  }

  void _send() {
    final path = _recordedPath;
    if (path == null) {
      return;
    }
    _pathClaimed = true;
    Navigator.of(context).pop(path);
  }

  Future<void> _stop() async {
    if (_busy || !_recording) {
      return;
    }
    setState(() {
      _busy = true;
    });
    final path = await _recorder.stop();
    _timer?.cancel();
    if (!mounted) {
      return;
    }
    setState(() {
      _recording = false;
      _busy = false;
      _recordedPath = path ?? _recordedPath;
    });
  }

  String get _elapsedLabel {
    final minutes = _elapsed.inMinutes.remainder(60).toString().padLeft(2, '0');
    final seconds = _elapsed.inSeconds.remainder(60).toString().padLeft(2, '0');
    return '$minutes:$seconds';
  }

  @override
  Widget build(BuildContext context) {
    return PopScope<String?>(
      canPop: !_busy && !_recording,
      child: SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(18, 16, 18, 18),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                '语音消息',
                style: Theme.of(
                  context,
                ).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w700),
              ),
              const SizedBox(height: 10),
              Text(_recording ? '正在录音 $_elapsedLabel' : '录制完成后发送语音消息'),
              if (_errorText != null) ...<Widget>[
                const SizedBox(height: 8),
                Text(
                  _errorText!,
                  style: TextStyle(color: Theme.of(context).colorScheme.error),
                ),
              ],
              const SizedBox(height: 14),
              Row(
                children: <Widget>[
                  FilledButton(
                    key: const Key('mobile-chat-voice-record-toggle'),
                    onPressed: _busy ? null : (_recording ? _stop : _start),
                    child: Text(_recording ? '停止录音' : '开始录音'),
                  ),
                  const SizedBox(width: 10),
                  if (_recordedPath != null && !_recording)
                    FilledButton.tonal(
                      key: const Key('mobile-chat-voice-record-send'),
                      onPressed: _send,
                      child: const Text('发送语音'),
                    ),
                  const Spacer(),
                  TextButton(
                    key: const Key('mobile-chat-voice-record-cancel'),
                    onPressed: _busy ? null : _cancel,
                    child: const Text('取消'),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _EmojiPickerSheet extends StatelessWidget {
  const _EmojiPickerSheet();

  static const List<String> _emojis = <String>[
    '👍',
    '👌',
    '🙏',
    '😂',
    '🔥',
    '✨',
    '❤️',
    '🎉',
  ];

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 14, 16, 18),
        child: Wrap(
          key: const Key('mobile-chat-emoji-sheet'),
          spacing: 12,
          runSpacing: 12,
          children: <Widget>[
            for (final emoji in _emojis)
              ActionChip(
                label: Text(emoji),
                onPressed: () => Navigator.of(context).pop(emoji),
              ),
          ],
        ),
      ),
    );
  }
}

class _ComposerShortcutButton extends StatelessWidget {
  const _ComposerShortcutButton({
    super.key,
    required this.label,
    required this.icon,
    required this.onPressed,
    this.cupertinoStyle = false,
  });

  final String label;
  final AppSemanticIcon icon;
  final VoidCallback onPressed;
  final bool cupertinoStyle;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    if (cupertinoStyle) {
      return CupertinoButton(
        padding: EdgeInsets.zero,
        minimumSize: const Size(32, 32),
        onPressed: onPressed,
        child: SizedBox.square(
          dimension: 24,
          child: Center(
            child: AppIcon(
              icon,
              size: 15.0,
              color: theme.textTheme.bodyMedium?.color?.withValues(alpha: 0.74),
            ),
          ),
        ),
      );
    }
    return SizedBox(
      width: 30,
      height: 30,
      child: Center(
        child: Material(
          color: Colors.transparent,
          child: InkWell(
            customBorder: const CircleBorder(),
            onTap: onPressed,
            child: SizedBox(
              width: 30,
              height: 30,
              child: Center(
                child: AppIcon(
                  icon,
                  size: 14,
                  color: theme.colorScheme.onSurface.withValues(alpha: 0.74),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _ComposerActionButton extends StatelessWidget {
  const _ComposerActionButton({
    super.key,
    required this.icon,
    required this.filled,
    required this.onPressed,
    this.cupertinoStyle = false,
  });

  final AppSemanticIcon icon;
  final bool filled;
  final VoidCallback onPressed;
  final bool cupertinoStyle;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    if (cupertinoStyle) {
      final iconColor = filled
          ? Colors.white
          : theme.colorScheme.primary.withValues(alpha: 0.74);
      final backgroundColor = filled
          ? theme.colorScheme.primary.withValues(alpha: 0.96)
          : Colors.transparent;
      return CupertinoButton(
        padding: EdgeInsets.zero,
        minimumSize: const Size(32, 32),
        onPressed: onPressed,
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: backgroundColor,
            shape: BoxShape.circle,
          ),
          child: SizedBox.square(
            dimension: filled ? 28 : 24,
            child: Center(
              child: AppIcon(
                icon,
                size: filled ? 15.5 : 15.0,
                color: iconColor,
              ),
            ),
          ),
        ),
      );
    }
    final iconColor = filled
        ? Colors.white
        : theme.colorScheme.onSurface.withValues(alpha: 0.74);
    return Material(
      color: filled
          ? theme.colorScheme.primary.withValues(alpha: 0.94)
          : Colors.transparent,
      shape: const CircleBorder(),
      child: InkWell(
        customBorder: const CircleBorder(),
        onTap: onPressed,
        child: SizedBox(
          width: 30,
          height: 30,
          child: Center(
            child: AppIcon(icon, size: filled ? 15 : 14, color: iconColor),
          ),
        ),
      ),
    );
  }
}

class _AttachmentSyntaxMetrics {
  const _AttachmentSyntaxMetrics._({
    required this.cardInsetX,
    required this.cardInsetY,
    required this.contentGap,
    required this.titleFontSize,
    required this.extensionFontSize,
    required this.stateFontSize,
    required this.detailFontSize,
    required this.rowGap,
    required this.sectionGap,
    required this.imagePreviewSize,
    required this.imagePreviewRadius,
    required this.imageCardMinWidth,
    required this.imageCardMaxWidth,
    required this.imageMinTextWidth,
    required this.imageMaxTextWidth,
    required this.documentSurfaceWidth,
    required this.documentSurfaceHeight,
    required this.documentSurfaceRadius,
    required this.documentIconSize,
    required this.captionGap,
  });

  factory _AttachmentSyntaxMetrics.of(bool cupertinoStyle) {
    return cupertinoStyle
        ? const _AttachmentSyntaxMetrics._(
            cardInsetX: 6.4,
            cardInsetY: 5.8,
            contentGap: 6.6,
            titleFontSize: 10.2,
            extensionFontSize: 6.1,
            stateFontSize: 6.7,
            detailFontSize: 7.4,
            rowGap: 1.9,
            sectionGap: 3.5,
            imagePreviewSize: 42.0,
            imagePreviewRadius: 9.5,
            imageCardMinWidth: 176.0,
            imageCardMaxWidth: 218.0,
            imageMinTextWidth: 108.0,
            imageMaxTextWidth: 154.0,
            documentSurfaceWidth: 22.0,
            documentSurfaceHeight: 26.0,
            documentSurfaceRadius: 9.0,
            documentIconSize: 11.2,
            captionGap: 6.0,
          )
        : const _AttachmentSyntaxMetrics._(
            cardInsetX: 6.8,
            cardInsetY: 6.2,
            contentGap: 7.0,
            titleFontSize: 11.2,
            extensionFontSize: 6.8,
            stateFontSize: 7.4,
            detailFontSize: 8.2,
            rowGap: 1.9,
            sectionGap: 3.8,
            imagePreviewSize: 48.0,
            imagePreviewRadius: 11.0,
            imageCardMinWidth: 192.0,
            imageCardMaxWidth: 240.0,
            imageMinTextWidth: 118.0,
            imageMaxTextWidth: 172.0,
            documentSurfaceWidth: 26.0,
            documentSurfaceHeight: 31.0,
            documentSurfaceRadius: 10.0,
            documentIconSize: 13.0,
            captionGap: 6.2,
          );
  }

  final double cardInsetX;
  final double cardInsetY;
  final double contentGap;
  final double titleFontSize;
  final double extensionFontSize;
  final double stateFontSize;
  final double detailFontSize;
  final double rowGap;
  final double sectionGap;
  final double imagePreviewSize;
  final double imagePreviewRadius;
  final double imageCardMinWidth;
  final double imageCardMaxWidth;
  final double imageMinTextWidth;
  final double imageMaxTextWidth;
  final double documentSurfaceWidth;
  final double documentSurfaceHeight;
  final double documentSurfaceRadius;
  final double documentIconSize;
  final double captionGap;

  double get horizontalInsetsTotal => cardInsetX * 2;
}

class _CompactAttachmentMessageCard extends StatelessWidget {
  const _CompactAttachmentMessageCard({
    required this.attachment,
    required this.isOutgoing,
  });

  final ChatAttachment attachment;
  final bool isOutgoing;

  @override
  Widget build(BuildContext context) {
    final isCupertinoStyle = Theme.of(context).platform == TargetPlatform.iOS;
    final metrics = _AttachmentSyntaxMetrics.of(isCupertinoStyle);
    final resolvedKind = _resolvedAttachmentKind(attachment);
    final isImageLike = _isVisualAttachmentKind(resolvedKind);

    final child = Align(
      alignment: isOutgoing ? Alignment.centerRight : Alignment.centerLeft,
      widthFactor: 1,
      child: ConstrainedBox(
        constraints: BoxConstraints(
          maxWidth: isImageLike ? metrics.imageCardMaxWidth : double.infinity,
        ),
        child: isImageLike
            ? AttachmentMessageCard(
                attachment: attachment,
                isOutgoing: isOutgoing,
              )
            : _CompactDocumentAttachmentCard(
                attachment: attachment,
                isOutgoing: isOutgoing,
                stateColor: _attachmentStateColor(
                  context,
                  attachment.state,
                  isOutgoing,
                ),
                extensionLabel: _attachmentExtensionLabel(attachment.title),
                showProgress:
                    attachment.progress != null && attachment.state.hasProgress,
                progress: attachment.progress,
              ),
      ),
    );

    if (isImageLike) {
      return child;
    }

    return Container(key: const Key('attachment-document-card'), child: child);
  }
}

class _CompactDocumentAttachmentCard extends StatelessWidget {
  const _CompactDocumentAttachmentCard({
    required this.attachment,
    required this.isOutgoing,
    required this.stateColor,
    required this.extensionLabel,
    required this.showProgress,
    required this.progress,
  });

  final ChatAttachment attachment;
  final bool isOutgoing;
  final Color stateColor;
  final String extensionLabel;
  final bool showProgress;
  final double? progress;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final metrics = _AttachmentSyntaxMetrics.of(isCupertinoStyle);
    final accentColor = isOutgoing ? Colors.white : theme.colorScheme.primary;
    final mutedColor = theme.textTheme.bodySmall?.color?.withValues(
      alpha: 0.68,
    );

    return Padding(
      padding: EdgeInsets.fromLTRB(
        metrics.cardInsetX,
        metrics.cardInsetY,
        metrics.cardInsetX,
        metrics.cardInsetY,
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Container(
                width: metrics.documentSurfaceWidth,
                height: metrics.documentSurfaceHeight,
                decoration: BoxDecoration(
                  color: accentColor.withValues(
                    alpha: isOutgoing ? 0.09 : 0.04,
                  ),
                  borderRadius: BorderRadius.circular(
                    metrics.documentSurfaceRadius,
                  ),
                ),
                child: Center(
                  child: AppIcon(
                    AppSemanticIcon.file,
                    size: metrics.documentIconSize,
                    color: accentColor.withValues(alpha: 0.82),
                  ),
                ),
              ),
              SizedBox(width: metrics.contentGap),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: <Widget>[
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.center,
                      children: <Widget>[
                        Expanded(
                          child: Text(
                            attachment.title,
                            maxLines: 2,
                            overflow: TextOverflow.ellipsis,
                            style: theme.textTheme.titleMedium?.copyWith(
                              fontSize: metrics.titleFontSize,
                              fontWeight: FontWeight.w500,
                              height: 1.1,
                            ),
                          ),
                        ),
                      ],
                    ),
                    SizedBox(height: metrics.rowGap),
                    Row(
                      key: const Key('attachment-file-meta-row'),
                      children: <Widget>[
                        Text(
                          key: const Key('attachment-state-pill'),
                          attachment.state.label,
                          style: theme.textTheme.labelSmall?.copyWith(
                            color: stateColor,
                            fontWeight: FontWeight.w600,
                            fontSize: metrics.stateFontSize,
                            height: 1.0,
                          ),
                        ),
                        const SizedBox(width: 2.5),
                        Expanded(
                          child: Row(
                            children: <Widget>[
                              Expanded(
                                child: Text(
                                  attachment.detail,
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                  style: theme.textTheme.bodySmall?.copyWith(
                                    fontSize: metrics.detailFontSize,
                                    height: 1.1,
                                    color: mutedColor,
                                  ),
                                ),
                              ),
                              const SizedBox(width: 4),
                              DefaultTextStyle(
                                key: const Key('attachment-extension-pill'),
                                style: theme.textTheme.labelSmall!.copyWith(
                                  color: accentColor.withValues(alpha: 0.5),
                                  fontWeight: FontWeight.w600,
                                  fontSize: metrics.extensionFontSize,
                                  height: 1.0,
                                ),
                                child: Text(extensionLabel),
                              ),
                            ],
                          ),
                        ),
                      ],
                    ),
                    if (showProgress) ...<Widget>[
                      SizedBox(height: metrics.sectionGap),
                      ClipRRect(
                        borderRadius: BorderRadius.circular(999),
                        child: LinearProgressIndicator(
                          minHeight: 1.0,
                          value: progress,
                          backgroundColor: stateColor.withValues(alpha: 0.14),
                          valueColor: AlwaysStoppedAnimation<Color>(stateColor),
                        ),
                      ),
                    ],
                  ],
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

Color _attachmentStateColor(
  BuildContext context,
  ChatAttachmentState state,
  bool isOutgoing,
) {
  final theme = Theme.of(context);
  final neutralColor =
      theme.textTheme.bodySmall?.color?.withValues(alpha: 0.76) ??
      theme.colorScheme.onSurface.withValues(alpha: 0.76);
  return switch (state) {
    ChatAttachmentState.pendingSend => neutralColor,
    ChatAttachmentState.uploading => theme.colorScheme.primary,
    ChatAttachmentState.downloading => theme.colorScheme.primary,
    ChatAttachmentState.completed => theme.colorScheme.primary,
    ChatAttachmentState.failed => theme.colorScheme.error,
    ChatAttachmentState.expired => neutralColor,
    ChatAttachmentState.offlineQueued => neutralColor,
    ChatAttachmentState.retryReady => theme.colorScheme.error,
  };
}

class _DetailTimeSeparator extends StatelessWidget {
  const _DetailTimeSeparator({required this.label, required this.palette});

  final String label;
  final AppShellColors palette;

  @override
  Widget build(BuildContext context) {
    final isCupertinoStyle = Theme.of(context).platform == TargetPlatform.iOS;
    return Center(
      child: Container(
        key: const Key('mobile-chat-time-separator'),
        padding: EdgeInsets.symmetric(
          horizontal: isCupertinoStyle ? 3.5 : 5.0,
          vertical: isCupertinoStyle ? 0.9 : 1.0,
        ),
        decoration: BoxDecoration(
          color: palette.surfaceVariant.withValues(
            alpha: isCupertinoStyle ? 0.03 : 0.055,
          ),
          borderRadius: BorderRadius.circular(999),
        ),
        child: Text(
          label,
          style: Theme.of(context).textTheme.labelSmall?.copyWith(
            color: Theme.of(
              context,
            ).textTheme.labelSmall?.color?.withValues(alpha: 0.5),
            fontSize: isCupertinoStyle ? 7.2 : 8.0,
            fontWeight: FontWeight.w500,
            height: 1.0,
          ),
        ),
      ),
    );
  }
}

class _UnreadMessageSeparator extends StatelessWidget {
  const _UnreadMessageSeparator({required this.label, required this.palette});

  final String label;
  final AppShellColors palette;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final accentColor = theme.colorScheme.primary.withValues(
      alpha: isCupertinoStyle ? 0.66 : 0.74,
    );
    final lineColor = palette.divider.withValues(
      alpha: isCupertinoStyle ? 0.08 : 0.12,
    );
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 10),
      child: Row(
        children: <Widget>[
          Expanded(
            child: Container(
              height: isCupertinoStyle ? 0.75 : 1,
              color: lineColor,
            ),
          ),
          Container(
            key: const Key('mobile-chat-new-message-separator'),
            margin: const EdgeInsets.symmetric(horizontal: 8),
            padding: EdgeInsets.symmetric(
              horizontal: isCupertinoStyle ? 5.5 : 6.0,
              vertical: isCupertinoStyle ? 1.0 : 1.25,
            ),
            decoration: BoxDecoration(
              color: theme.colorScheme.primary.withValues(
                alpha: isCupertinoStyle ? 0.026 : 0.034,
              ),
              borderRadius: BorderRadius.circular(999),
            ),
            child: Text(
              label,
              style: theme.textTheme.labelMedium?.copyWith(
                color: accentColor,
                fontWeight: FontWeight.w500,
                fontSize: isCupertinoStyle ? 7.5 : 8.2,
                height: 1.0,
              ),
            ),
          ),
          Expanded(
            child: Container(
              height: isCupertinoStyle ? 0.75 : 1,
              color: lineColor,
            ),
          ),
        ],
      ),
    );
  }
}

enum _TimelineNoticeTone { system, security }

class _SystemNoticeLine extends StatelessWidget {
  const _SystemNoticeLine({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final tone = _noticeToneFor(text);
    final isSecurity = tone == _TimelineNoticeTone.security;
    final accentColor = isSecurity
        ? theme.colorScheme.primary.withValues(alpha: 0.78)
        : theme.textTheme.labelMedium?.color?.withValues(alpha: 0.6) ??
              theme.colorScheme.onSurface.withValues(alpha: 0.6);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    return Center(
      child: ConstrainedBox(
        constraints: BoxConstraints(
          maxWidth: MediaQuery.sizeOf(context).width * 0.6,
        ),
        child: Container(
          key: ValueKey<String>('mobile-chat-notice-${tone.name}'),
          padding: EdgeInsets.symmetric(
            horizontal: isCupertinoStyle ? 5.0 : 6.0,
            vertical: isCupertinoStyle ? 2.0 : 2.4,
          ),
          child: Text(
            text,
            maxLines: 2,
            textAlign: TextAlign.center,
            overflow: TextOverflow.ellipsis,
            style: theme.textTheme.labelMedium?.copyWith(
              color: accentColor.withValues(alpha: isSecurity ? 0.56 : 0.48),
              fontWeight: isSecurity ? FontWeight.w500 : FontWeight.w400,
              fontSize: isCupertinoStyle ? 7.1 : 8.0,
              height: 1.04,
            ),
          ),
        ),
      ),
    );
  }
}

class _EmptyTimelineState extends StatelessWidget {
  const _EmptyTimelineState({required this.conversation});

  final ConversationSummary conversation;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 24),
        child: Text(
          '${conversation.title} 暂时没有消息',
          textAlign: TextAlign.center,
          style: Theme.of(context).textTheme.bodySmall,
        ),
      ),
    );
  }
}

String _detailSubtitle(ConversationSummary conversation) {
  if (conversation.id == 'file-helper') {
    return '文件助手';
  }
  if (conversation.isGroupConversation) {
    if (conversation.memberCount > 0 && conversation.onlineCount > 0) {
      return '${conversation.memberCount} 位成员 · ${conversation.onlineCount} 在线';
    }
    if (conversation.memberCount > 0) {
      return '${conversation.memberCount} 位成员';
    }
    return '群聊';
  }
  if (conversation.kind == ConversationKind.channel) {
    return conversation.isOfficial ? '官方频道' : '频道';
  }
  if (conversation.kind == ConversationKind.bot) {
    return '服务号';
  }
  if (conversation.isOnline) {
    return conversation.lastSenderLabel == null ? '在线' : '在线 · 最近活跃';
  }
  return conversation.lastSenderLabel == null
      ? '最近在线'
      : '${conversation.lastSenderLabel} 的聊天';
}

bool _shouldInsertSeparator(DateTime? previous, DateTime current) {
  if (previous == null) {
    return true;
  }
  return current.difference(previous).inMinutes >= 8;
}

bool _belongsToPreviousCluster(ChatMessage? previous, ChatMessage current) {
  if (previous == null) {
    return false;
  }
  if (previous.isSystemNotice || current.isSystemNotice) {
    return false;
  }
  if (previous.direction != current.direction) {
    return false;
  }
  if (previous.senderLabel != current.senderLabel) {
    return false;
  }
  return current.timestamp.difference(previous.timestamp).inMinutes <= 6;
}

BorderRadius _bubbleBorderRadius({
  required bool isOutgoing,
  required bool isClusterStart,
  required bool isClusterEnd,
  required bool cupertinoStyle,
}) {
  if (cupertinoStyle) {
    final leadingTop = isClusterStart ? 18.0 : 10.0;
    final leadingBottom = isClusterEnd ? 16.0 : 5.0;
    if (isOutgoing) {
      return BorderRadius.only(
        topLeft: const Radius.circular(18),
        topRight: Radius.circular(leadingTop),
        bottomLeft: const Radius.circular(18),
        bottomRight: Radius.circular(leadingBottom),
      );
    }
    return BorderRadius.only(
      topLeft: Radius.circular(leadingTop),
      topRight: const Radius.circular(18),
      bottomLeft: Radius.circular(leadingBottom),
      bottomRight: const Radius.circular(18),
    );
  }
  final leadingTop = isClusterStart ? 16.0 : 8.0;
  final leadingBottom = isClusterEnd ? 9.0 : 4.0;
  if (isOutgoing) {
    return BorderRadius.only(
      topLeft: const Radius.circular(16),
      topRight: Radius.circular(leadingTop),
      bottomLeft: const Radius.circular(16),
      bottomRight: Radius.circular(leadingBottom),
    );
  }
  return BorderRadius.only(
    topLeft: Radius.circular(leadingTop),
    topRight: const Radius.circular(16),
    bottomLeft: Radius.circular(leadingBottom),
    bottomRight: const Radius.circular(16),
  );
}

Color _platformBubbleColor({
  required ThemeData theme,
  required AppShellColors palette,
  required bool isOutgoing,
  required bool cupertinoStyle,
}) {
  final baseColor = isOutgoing
      ? palette.bubbleOutgoing
      : palette.bubbleIncoming;
  if (!cupertinoStyle) {
    return baseColor;
  }
  return Color.alphaBlend(
    theme.colorScheme.surface.withValues(alpha: isOutgoing ? 0.04 : 0.14),
    baseColor,
  );
}

Color _attachmentBubbleColor({
  required ThemeData theme,
  required AppShellColors palette,
  required bool isOutgoing,
  required bool cupertinoStyle,
}) {
  final baseBubble = _platformBubbleColor(
    theme: theme,
    palette: palette,
    isOutgoing: isOutgoing,
    cupertinoStyle: cupertinoStyle,
  );
  final tint = isOutgoing
      ? theme.colorScheme.primary.withValues(
          alpha: cupertinoStyle ? 0.02 : 0.026,
        )
      : palette.surfaceVariant.withValues(alpha: cupertinoStyle ? 0.042 : 0.05);
  return Color.alphaBlend(
    tint,
    Color.alphaBlend(
      theme.colorScheme.surface.withValues(
        alpha: cupertinoStyle
            ? (isOutgoing ? 0.024 : 0.06)
            : (isOutgoing ? 0.026 : 0.06),
      ),
      baseBubble,
    ),
  );
}

bool _isImageLikeAttachment(String title) {
  const imageExtensions = <String>{
    'png',
    'jpg',
    'jpeg',
    'gif',
    'webp',
    'bmp',
    'heic',
  };
  final extension = _attachmentExtensionLabel(title).toLowerCase();
  return imageExtensions.contains(extension);
}

ChatAttachmentKind _resolvedAttachmentKind(ChatAttachment attachment) {
  if (attachment.kind != ChatAttachmentKind.file) {
    return attachment.kind;
  }
  if (_isImageLikeAttachment(attachment.title)) {
    return ChatAttachmentKind.image;
  }
  if (_isVideoLikeAttachment(attachment.title)) {
    return ChatAttachmentKind.video;
  }
  if (_isAudioLikeAttachment(attachment.title)) {
    return ChatAttachmentKind.audio;
  }
  return ChatAttachmentKind.file;
}

bool _isVisualAttachmentKind(ChatAttachmentKind kind) {
  return kind == ChatAttachmentKind.image || kind == ChatAttachmentKind.video;
}

bool _isVideoLikeAttachment(String title) {
  const videoExtensions = <String>{'mp4', 'mov', 'm4v', 'webm'};
  final extension = _attachmentExtensionLabel(title).toLowerCase();
  return videoExtensions.contains(extension);
}

bool _isAudioLikeAttachment(String title) {
  const audioExtensions = <String>{
    'm4a',
    'aac',
    'mp3',
    'wav',
    'ogg',
    'opus',
    'amr',
  };
  final extension = _attachmentExtensionLabel(title).toLowerCase();
  return audioExtensions.contains(extension);
}

String _attachmentExtensionLabel(String title) {
  final trimmedTitle = title.trim();
  final separatorIndex = trimmedTitle.lastIndexOf('.');
  if (separatorIndex <= 0 || separatorIndex == trimmedTitle.length - 1) {
    return '文件';
  }
  final extension = trimmedTitle.substring(separatorIndex + 1).trim();
  if (extension.isEmpty) {
    return '文件';
  }
  return extension.toUpperCase();
}

int _effectiveUnreadCount(
  ConversationSummary conversation,
  List<ChatMessage> messages, {
  int? unreadCountOverride,
}) {
  final unreadCount = unreadCountOverride ?? conversation.unreadCount;
  if (unreadCount <= 0) {
    return 0;
  }
  final visibleMessageCount = messages
      .where((message) => !message.isSystemNotice)
      .length;
  if (visibleMessageCount == 0) {
    return 0;
  }
  if (unreadCount > visibleMessageCount) {
    return visibleMessageCount;
  }
  return unreadCount;
}

ChatMessage? _unreadAnchorMessage(
  ConversationSummary conversation,
  List<ChatMessage> messages, {
  int? unreadCountOverride,
}) {
  final unreadCount = _effectiveUnreadCount(
    conversation,
    messages,
    unreadCountOverride: unreadCountOverride,
  );
  if (unreadCount == 0) {
    return null;
  }
  final visibleMessages = messages
      .where((message) => !message.isSystemNotice)
      .toList(growable: false);
  return visibleMessages[visibleMessages.length - unreadCount];
}

_TimelineNoticeTone _noticeToneFor(String text) {
  const securityKeywords = <String>['加密', '验证', '密钥', '会话', '设备', '指纹', '安全'];
  for (final keyword in securityKeywords) {
    if (text.contains(keyword)) {
      return _TimelineNoticeTone.security;
    }
  }
  return _TimelineNoticeTone.system;
}

String _formatSeparatorLabel(DateTime time) {
  final hour = time.hour.toString().padLeft(2, '0');
  final minute = time.minute.toString().padLeft(2, '0');
  return '今天 $hour:$minute';
}

String _formatClock(DateTime time) {
  final hour = time.hour.toString().padLeft(2, '0');
  final minute = time.minute.toString().padLeft(2, '0');
  return '$hour:$minute';
}

ChatMessage? _nextVisibleMessage(List<ChatMessage> messages, int startIndex) {
  if (startIndex >= messages.length) {
    return null;
  }
  return messages[startIndex];
}
