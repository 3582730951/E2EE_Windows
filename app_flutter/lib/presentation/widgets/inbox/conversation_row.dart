import 'package:flutter/material.dart';

import '../../../domain/entities/models.dart';
import '../../theme/app_icons.dart';
import '../../theme/app_theme.dart';
import '../../theme/app_tokens.dart';
import '../social_avatar.dart';

class InboxConversationRow extends StatelessWidget {
  const InboxConversationRow({
    super.key,
    required this.conversation,
    required this.onTap,
    this.selected = false,
    this.compact = false,
    this.desktop = false,
  });

  final ConversationSummary conversation;
  final VoidCallback onTap;
  final bool selected;
  final bool compact;
  final bool desktop;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final hasUnread = conversation.unreadCount > 0;
    final hasMention = conversation.mentionCount > 0;
    final hasDraft = conversation.hasDraft;
    final preview = hasDraft
        ? conversation.draftText!.trim()
        : conversation.isTyping
        ? '正在输入...'
        : conversation.preview;
    final senderLabel = hasDraft
        ? '草稿'
        : conversation.lastSenderLabel?.trim().isNotEmpty == true
        ? conversation.lastSenderLabel!.trim()
        : _defaultSenderLabel(conversation);
    final accent = hasMention
        ? AppPalette.mentionRed
        : hasDraft
        ? theme.colorScheme.error
        : hasUnread
        ? theme.colorScheme.primary
        : conversation.isOnline
        ? AppPalette.socialGreen
        : palette.textTertiary;
    final avatarSize = desktop ? 40.0 : (compact ? 44.0 : 50.0);
    final titleSize = desktop ? 13.6 : (compact ? 14.6 : 15.8);
    final previewSize = desktop ? 11.2 : (compact ? 11.8 : 12.6);
    final rowPadding = desktop
        ? const EdgeInsets.symmetric(horizontal: 8, vertical: 6)
        : EdgeInsets.symmetric(
            horizontal: compact ? 12 : 14,
            vertical: compact ? 8 : 10,
          );
    final rowHeight = desktop
        ? ImUiMetrics.desktopConversationRowHeight
        : compact
        ? ImUiMetrics.iosConversationRowHeight
        : ImUiMetrics.androidConversationRowHeight;

    return Material(
      color: Colors.transparent,
      child: InkWell(
        borderRadius: BorderRadius.circular(desktop ? 10 : 14),
        onTap: onTap,
        child: AnimatedContainer(
          key: ValueKey<String>(
            selected
                ? 'conversation-row-selected-${conversation.id}'
                : 'conversation-row-${conversation.id}',
          ),
          duration: const Duration(milliseconds: 160),
          curve: Curves.easeOutCubic,
          padding: rowPadding,
          constraints: BoxConstraints(minHeight: rowHeight),
          decoration: BoxDecoration(
            color: selected
                ? theme.colorScheme.primary.withValues(alpha: 0.07)
                : Colors.transparent,
            borderRadius: BorderRadius.circular(desktop ? 10 : 14),
            border: selected
                ? Border.all(
                    color: theme.colorScheme.primary.withValues(alpha: 0.12),
                  )
                : null,
          ),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: <Widget>[
              SocialAvatar.conversation(
                conversation,
                size: avatarSize,
                selected: selected,
              ),
              SizedBox(width: desktop ? 9 : 11),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: <Widget>[
                    Row(
                      children: <Widget>[
                        Expanded(
                          child: Row(
                            children: <Widget>[
                              Flexible(
                                child: Text(
                                  conversation.title,
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                  style: theme.textTheme.titleMedium?.copyWith(
                                    fontSize: titleSize,
                                    fontWeight:
                                        hasUnread || hasMention || selected
                                        ? FontWeight.w700
                                        : FontWeight.w600,
                                    letterSpacing: 0,
                                  ),
                                ),
                              ),
                              if (conversation.isOfficial ||
                                  conversation.isVerified) ...<Widget>[
                                const SizedBox(width: 4),
                                AppIcon(
                                  AppSemanticIcon.verified,
                                  size: desktop ? 11 : 12,
                                  color: AppPalette.channelCyan,
                                ),
                              ],
                              if (conversation.isPinned) ...<Widget>[
                                const SizedBox(width: 4),
                                AppIcon(
                                  AppSemanticIcon.pinned,
                                  size: desktop ? 10 : 11,
                                  color: palette.textTertiary.withValues(
                                    alpha: 0.64,
                                  ),
                                ),
                              ],
                              if (conversation.isMuted) ...<Widget>[
                                const SizedBox(width: 4),
                                AppIcon(
                                  AppSemanticIcon.muted,
                                  size: desktop ? 10 : 11,
                                  color: palette.textTertiary.withValues(
                                    alpha: 0.58,
                                  ),
                                ),
                              ],
                            ],
                          ),
                        ),
                        const SizedBox(width: 8),
                        Text(
                          _formatClock(conversation.lastUpdated),
                          style: theme.textTheme.labelSmall?.copyWith(
                            fontSize: desktop ? 9.2 : 10.2,
                            fontWeight: hasUnread
                                ? FontWeight.w700
                                : FontWeight.w500,
                            color: hasUnread
                                ? theme.colorScheme.primary
                                : palette.textTertiary,
                            fontFeatures: const <FontFeature>[
                              FontFeature.tabularFigures(),
                            ],
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 3),
                    Row(
                      children: <Widget>[
                        Expanded(
                          child: Text.rich(
                            TextSpan(
                              children: <InlineSpan>[
                                if (senderLabel != null)
                                  TextSpan(
                                    text: '$senderLabel: ',
                                    style: theme.textTheme.bodySmall?.copyWith(
                                      fontSize: previewSize,
                                      fontWeight: FontWeight.w700,
                                      color: hasDraft
                                          ? theme.colorScheme.error
                                          : accent.withValues(alpha: 0.84),
                                    ),
                                  ),
                                TextSpan(text: preview),
                              ],
                            ),
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: theme.textTheme.bodySmall?.copyWith(
                              fontSize: previewSize,
                              fontWeight: hasUnread
                                  ? FontWeight.w500
                                  : FontWeight.w400,
                              color: hasDraft
                                  ? theme.colorScheme.error.withValues(
                                      alpha: 0.86,
                                    )
                                  : palette.textSecondary.withValues(
                                      alpha: hasUnread ? 0.92 : 0.76,
                                    ),
                            ),
                          ),
                        ),
                        const SizedBox(width: 8),
                        _ConversationStateBadge(conversation: conversation),
                      ],
                    ),
                    if (!desktop) ...<Widget>[
                      const SizedBox(height: 3),
                      _ConversationMetaLine(conversation: conversation),
                    ],
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _ConversationMetaLine extends StatelessWidget {
  const _ConversationMetaLine({required this.conversation});

  final ConversationSummary conversation;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final parts = <String>[
      conversation.kind.label,
      if (conversation.memberCount > 0)
        '${conversation.memberCount} 人'
      else if (conversation.isOnline)
        '在线',
      if (conversation.onlineCount > 0) '${conversation.onlineCount} 在线',
    ];

    return Text(
      parts.join(' · '),
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: theme.textTheme.labelSmall?.copyWith(
        fontSize: 10.2,
        color: palette.textTertiary.withValues(alpha: 0.78),
      ),
    );
  }
}

class _ConversationStateBadge extends StatelessWidget {
  const _ConversationStateBadge({required this.conversation});

  final ConversationSummary conversation;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final hasMention = conversation.mentionCount > 0;
    final hasUnread = conversation.unreadCount > 0;
    final hasDraft = conversation.hasDraft;

    if (hasDraft) {
      return _Badge(label: '草稿', tint: theme.colorScheme.error);
    }
    if (hasMention) {
      return _Badge(
        label: conversation.mentionCount > 1
            ? '@${conversation.mentionCount}'
            : '@',
        tint: AppPalette.mentionRed,
      );
    }
    if (hasUnread) {
      return _Badge(
        label: conversation.unreadCount > 99
            ? '99+'
            : '${conversation.unreadCount}',
        tint: conversation.isMuted
            ? theme.colorScheme.onSurface.withValues(alpha: 0.52)
            : theme.colorScheme.primary,
        filled: !conversation.isMuted,
      );
    }
    return const SizedBox(width: 1, height: 1);
  }
}

class _Badge extends StatelessWidget {
  const _Badge({required this.label, required this.tint, this.filled = false});

  final String label;
  final Color tint;
  final bool filled;

  @override
  Widget build(BuildContext context) {
    return Container(
      constraints: const BoxConstraints(minWidth: 21),
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2.2),
      decoration: BoxDecoration(
        color: filled ? tint : tint.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(AppTokens.radiusFull),
      ),
      alignment: Alignment.center,
      child: Text(
        label,
        style: Theme.of(context).textTheme.labelSmall?.copyWith(
          fontSize: 9.2,
          height: 1.0,
          fontWeight: FontWeight.w800,
          color: filled ? Colors.white : tint,
        ),
      ),
    );
  }
}

String? _defaultSenderLabel(ConversationSummary conversation) {
  if (conversation.kind == ConversationKind.group &&
      conversation.preview.contains(':')) {
    return conversation.preview.split(':').first.trim();
  }
  if (conversation.kind == ConversationKind.channel) {
    return '频道';
  }
  if (conversation.kind == ConversationKind.fileAssistant) {
    return '文件';
  }
  if (conversation.kind == ConversationKind.system) {
    return '服务';
  }
  return null;
}

String _formatClock(DateTime time) {
  final hour = time.hour.toString().padLeft(2, '0');
  final minute = time.minute.toString().padLeft(2, '0');
  return '$hour:$minute';
}

extension ConversationKindLabel on ConversationKind {
  String get label => switch (this) {
    ConversationKind.direct => '私聊',
    ConversationKind.group => '群聊',
    ConversationKind.channel => '频道',
    ConversationKind.bot => '服务号',
    ConversationKind.system => '服务通知',
    ConversationKind.fileAssistant => '文件',
  };
}
