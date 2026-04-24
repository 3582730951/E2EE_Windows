import 'dart:math' as math;
import 'dart:ui';

import 'package:flutter/material.dart';

import '../../domain/entities/models.dart';
import '../theme/app_icons.dart';
import '../theme/app_theme.dart';
import '../theme/app_tokens.dart';

class AppBarPrimary extends StatelessWidget {
  const AppBarPrimary({
    super.key,
    required this.title,
    this.subtitle,
    this.trailing,
  });

  final String title;
  final String? subtitle;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) {
    final trailingChildren = trailing == null
        ? const <Widget>[]
        : <Widget>[trailing!];
    return Row(
      children: <Widget>[
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(title, style: Theme.of(context).textTheme.titleLarge),
              if (subtitle case final subtitleText?)
                Padding(
                  padding: const EdgeInsets.only(top: AppTokens.spacingXs),
                  child: Text(
                    subtitleText,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ),
            ],
          ),
        ),
        ...trailingChildren,
      ],
    );
  }
}

class SectionCard extends StatelessWidget {
  const SectionCard({
    super.key,
    required this.child,
    this.padding = const EdgeInsets.all(AppTokens.spacingLg),
    this.decorativeChrome = false,
  });

  final Widget child;
  final EdgeInsetsGeometry padding;
  final bool decorativeChrome;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final borderRadius = BorderRadius.circular(AppTokens.radiusCard);
    final isGlass =
        decorativeChrome && material.isGlass && material.blurSigma > 0;
    final card = Container(
      decoration: BoxDecoration(
        color: isGlass ? null : material.surfaceColor,
        gradient: isGlass
            ? LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: <Color>[
                  Colors.white.withValues(
                    alpha: theme.brightness == Brightness.dark ? 0.05 : 0.22,
                  ),
                  material.floatingSurfaceColor,
                  material.floatingSurfaceColor.withValues(alpha: 0.96),
                ],
                stops: const <double>[0, 0.18, 1],
              )
            : null,
        borderRadius: borderRadius,
        border: Border.all(
          color: isGlass ? material.floatingBorderColor : material.borderColor,
        ),
        boxShadow: <BoxShadow>[
          BoxShadow(
            color: material.shadowColor.withValues(alpha: isGlass ? 0.82 : 1),
            blurRadius: isGlass ? 18 : 24,
            offset: Offset(0, isGlass ? 10 : 14),
          ),
        ],
      ),
      child: Stack(
        children: <Widget>[
          if (isGlass)
            Positioned(
              left: 18,
              right: 18,
              top: 0,
              child: IgnorePointer(
                child: Container(
                  height: 1,
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      colors: <Color>[
                        Colors.white.withValues(alpha: 0),
                        Colors.white.withValues(alpha: 0.16),
                        Colors.white.withValues(alpha: 0),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          Padding(padding: padding, child: child),
        ],
      ),
    );
    return isGlass
        ? ClipRRect(
            borderRadius: borderRadius,
            child: BackdropFilter(
              filter: ImageFilter.blur(
                sigmaX: material.blurSigma * 0.45,
                sigmaY: material.blurSigma * 0.45,
              ),
              child: card,
            ),
          )
        : card;
  }
}

class StatusBanner extends StatelessWidget {
  const StatusBanner({
    super.key,
    required this.label,
    required this.detail,
    required this.color,
  });

  final String label;
  final String detail;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(
        horizontal: AppTokens.spacingLg,
        vertical: AppTokens.spacingMd,
      ),
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(18),
        color: color.withValues(alpha: 0.15),
      ),
      child: Row(
        children: <Widget>[
          AppIcon(AppSemanticIcon.security, color: color, size: 18),
          const SizedBox(width: AppTokens.spacingSm),
          Expanded(
            child: RichText(
              text: TextSpan(
                style: Theme.of(context).textTheme.bodySmall,
                children: <InlineSpan>[
                  TextSpan(
                    text: '$label  ',
                    style: Theme.of(
                      context,
                    ).textTheme.labelLarge?.copyWith(color: color),
                  ),
                  TextSpan(text: detail),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class SecurityBadge extends StatelessWidget {
  const SecurityBadge({super.key, required this.label, required this.color});

  final String label;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(
        horizontal: AppTokens.spacingMd,
        vertical: AppTokens.spacingXs + 2,
      ),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.16),
        borderRadius: BorderRadius.circular(999),
      ),
      child: Text(
        label,
        style: Theme.of(context).textTheme.labelMedium?.copyWith(
          color: color,
          fontWeight: FontWeight.w700,
        ),
      ),
    );
  }
}

class ConversationRow extends StatelessWidget {
  const ConversationRow({
    super.key,
    required this.conversation,
    required this.selected,
    required this.onTap,
    this.dense = false,
  });

  final ConversationSummary conversation;
  final bool selected;
  final VoidCallback onTap;
  final bool dense;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final palette = theme.extension<AppShellColors>()!;
    final hasDraft = conversation.hasDraft;
    final previewPrefix = _conversationPreviewPrefix(conversation);
    final preview = hasDraft
        ? conversation.draftText!.trim()
        : conversation.isTyping
        ? '正在输入…'
        : conversation.preview;
    final isMediaPreview = _isConversationMediaPreview(preview);
    final hasUnread = conversation.unreadCount > 0;
    final hasMentions = conversation.mentionCount > 0;
    final primaryState = _buildConversationPrimaryState(
      context: context,
      conversation: conversation,
      hasDraft: hasDraft,
      hasUnread: hasUnread,
      hasMentions: hasMentions,
      dense: dense,
      selected: selected,
    );
    final showPinnedIndicator =
        conversation.isPinned && (hasDraft || hasMentions || hasUnread);
    final showMutedIndicator =
        conversation.isMuted &&
        (hasDraft || hasMentions || hasUnread || conversation.isPinned);
    final avatarRadius = dense
        ? (isCupertinoStyle ? 18.5 : 20.0)
        : (isCupertinoStyle ? 21.0 : 22.0);
    final contentPadding = dense
        ? EdgeInsets.symmetric(
            horizontal: isCupertinoStyle ? 10 : 12,
            vertical: 7.5,
          )
        : EdgeInsets.symmetric(
            horizontal: isCupertinoStyle ? 13 : 14,
            vertical: isCupertinoStyle ? 8.5 : 9,
          );
    final backgroundColor = selected
        ? theme.colorScheme.primary.withValues(
            alpha: isCupertinoStyle ? 0.05 : 0.06,
          )
        : Colors.transparent;
    final accentColor = hasDraft
        ? theme.colorScheme.error
        : hasMentions
        ? theme.colorScheme.secondary
        : conversation.isPinned || hasUnread
        ? theme.colorScheme.primary
        : theme.textTheme.bodySmall?.color?.withValues(alpha: 0.58) ??
              theme.colorScheme.onSurface.withValues(alpha: 0.58);
    final titleWeight = hasUnread || hasDraft || hasMentions || selected
        ? FontWeight.w700
        : FontWeight.w600;
    final previewColor = hasDraft
        ? theme.colorScheme.error
        : conversation.isTyping
        ? theme.colorScheme.secondary
        : isMediaPreview
        ? theme.textTheme.bodySmall?.color?.withValues(
                alpha: hasUnread ? 0.58 : 0.50,
              ) ??
              theme.colorScheme.onSurface.withValues(
                alpha: hasUnread ? 0.58 : 0.50,
              )
        : hasUnread
        ? theme.colorScheme.onSurface.withValues(alpha: 0.8)
        : theme.textTheme.bodySmall?.color?.withValues(alpha: 0.8) ??
              theme.colorScheme.onSurface.withValues(alpha: 0.72);
    final timeSlotWidth = dense ? 40.0 : 46.0;
    final stateSlotWidth = dense ? 28.0 : 34.0;

    return InkWell(
      borderRadius: BorderRadius.circular(
        isCupertinoStyle ? (dense ? 9.0 : 14.0) : (dense ? 12.0 : 16.0),
      ),
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 180),
        curve: Curves.easeOut,
        padding: contentPadding,
        decoration: BoxDecoration(
          color: backgroundColor,
          borderRadius: BorderRadius.circular(
            isCupertinoStyle ? (dense ? 9.0 : 14.0) : (dense ? 12.0 : 16.0),
          ),
        ),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.center,
          children: <Widget>[
            CircleAvatar(
              radius: avatarRadius,
              backgroundColor: _leadingTint(
                theme: theme,
                palette: palette,
                conversation: conversation,
                selected: selected,
              ),
              child: _ConversationLeading(
                conversation: conversation,
                selected: selected,
              ),
            ),
            SizedBox(width: dense ? 9 : 10),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                mainAxisSize: MainAxisSize.min,
                children: <Widget>[
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.center,
                    children: <Widget>[
                      Expanded(
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: <Widget>[
                            Flexible(
                              child: Text(
                                conversation.title,
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                                style: theme.textTheme.titleMedium?.copyWith(
                                  fontWeight: titleWeight,
                                  fontSize: dense ? 15.0 : 15.6,
                                  letterSpacing: dense ? -0.14 : -0.1,
                                ),
                              ),
                            ),
                            if (showPinnedIndicator) ...<Widget>[
                              SizedBox(width: dense ? 4 : 5),
                              AppIcon(
                                AppSemanticIcon.pinned,
                                size: dense ? 11 : 12,
                                color: theme.colorScheme.primary.withValues(
                                  alpha: 0.42,
                                ),
                              ),
                            ],
                            if (showMutedIndicator) ...<Widget>[
                              SizedBox(width: dense ? 4 : 5),
                              AppIcon(
                                AppSemanticIcon.muted,
                                size: dense ? 11 : 12,
                                color: theme.textTheme.bodySmall?.color
                                    ?.withValues(alpha: 0.48),
                              ),
                            ],
                          ],
                        ),
                      ),
                      SizedBox(
                        width: timeSlotWidth,
                        child: Align(
                          alignment: Alignment.centerRight,
                          child: Text(
                            _formatTime(conversation.lastUpdated),
                            maxLines: 1,
                            style: theme.textTheme.labelSmall?.copyWith(
                              color: theme.textTheme.bodySmall?.color
                                  ?.withValues(alpha: 0.62),
                              fontSize: dense ? 9.7 : 10.4,
                              fontWeight: FontWeight.w500,
                              height: 1.0,
                              fontFeatures: const <FontFeature>[
                                FontFeature.tabularFigures(),
                              ],
                            ),
                          ),
                        ),
                      ),
                    ],
                  ),
                  SizedBox(height: dense ? 1.5 : 2.5),
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.center,
                    children: <Widget>[
                      Expanded(
                        child: Text.rich(
                          TextSpan(
                            style: theme.textTheme.bodySmall?.copyWith(
                              color: previewColor,
                              fontWeight: hasDraft || conversation.isTyping
                                  ? FontWeight.w600
                                  : isMediaPreview
                                  ? FontWeight.w400
                                  : hasUnread
                                  ? FontWeight.w500
                                  : FontWeight.w400,
                              fontSize: dense ? 11.5 : 12.2,
                              height: 1.1,
                            ),
                            children: <InlineSpan>[
                              if (previewPrefix case final prefix?)
                                TextSpan(
                                  text: '$prefix · ',
                                  style: theme.textTheme.bodySmall?.copyWith(
                                    color: accentColor.withValues(alpha: 0.76),
                                    fontWeight: FontWeight.w600,
                                    fontSize: dense ? 10.9 : 11.5,
                                    height: 1.1,
                                  ),
                                ),
                              TextSpan(text: preview),
                            ],
                          ),
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                        ),
                      ),
                      SizedBox(width: dense ? 8 : 10),
                      SizedBox(
                        width: stateSlotWidth,
                        child: Align(
                          alignment: Alignment.centerRight,
                          child: primaryState,
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  String _formatTime(DateTime time) {
    final hour = time.hour.toString().padLeft(2, '0');
    final minute = time.minute.toString().padLeft(2, '0');
    return '$hour:$minute';
  }

  Color _leadingTint({
    required ThemeData theme,
    required AppShellColors palette,
    required ConversationSummary conversation,
    required bool selected,
  }) {
    if (selected) {
      return theme.colorScheme.primary.withValues(alpha: 0.18);
    }
    if (conversation.kind == ConversationKind.system ||
        conversation.previewBadge == '系统消息' ||
        conversation.pillLabel == '系统' ||
        conversation.pillLabel == '服务') {
      return AppPalette.warning.withValues(alpha: 0.12);
    }
    if (conversation.kind == ConversationKind.fileAssistant ||
        conversation.pillLabel == '文件') {
      return theme.colorScheme.primary.withValues(alpha: 0.11);
    }
    if (conversation.kind == ConversationKind.channel) {
      return AppPalette.channelCyan.withValues(alpha: 0.14);
    }
    if (conversation.isGroupConversation) {
      return palette.surfaceVariant.withValues(alpha: 0.9);
    }
    return palette.surfaceVariant;
  }

  String? _conversationPreviewPrefix(ConversationSummary conversation) {
    if (conversation.kind == ConversationKind.system ||
        conversation.previewBadge == '系统消息' ||
        conversation.pillLabel == '系统' ||
        conversation.pillLabel == '服务') {
      return '服务';
    }
    if (conversation.previewBadge == '待处理 2') {
      return '待处理 2';
    }
    if (conversation.pillLabel == '文件') {
      return '文件';
    }
    return null;
  }
}

bool _isConversationMediaPreview(String preview) {
  final value = preview.trim();
  return value == '[图片]' ||
      value == '[视频]' ||
      value.endsWith(': [图片]') ||
      value.endsWith(': [视频]');
}

Widget? _buildConversationPrimaryState({
  required BuildContext context,
  required ConversationSummary conversation,
  required bool hasDraft,
  required bool hasUnread,
  required bool hasMentions,
  required bool dense,
  required bool selected,
}) {
  final theme = Theme.of(context);
  final compactLabelStyle = theme.textTheme.labelSmall?.copyWith(
    fontWeight: FontWeight.w700,
    fontSize: dense ? 8.8 : 9.3,
    height: 1,
  );

  if (hasDraft) {
    return _ConversationInlineStatePill(
      label: '草稿',
      backgroundColor: theme.colorScheme.error.withValues(alpha: 0.1),
      textColor: theme.colorScheme.error,
      textStyle: compactLabelStyle,
    );
  }

  if (hasMentions) {
    return _ConversationInlineStatePill(
      label: conversation.mentionCount > 1
          ? '@${conversation.mentionCount}'
          : '@',
      backgroundColor: theme.colorScheme.secondary.withValues(alpha: 0.11),
      textColor: theme.colorScheme.secondary,
      textStyle: compactLabelStyle,
    );
  }

  if (hasUnread) {
    final unreadLabel = conversation.unreadCount > 99
        ? '99+'
        : '${conversation.unreadCount}';
    return _ConversationInlineStatePill(
      label: unreadLabel,
      backgroundColor: conversation.isMuted
          ? theme.colorScheme.onSurface.withValues(alpha: 0.12)
          : theme.colorScheme.primary.withValues(alpha: 0.12),
      textColor: conversation.isMuted
          ? theme.colorScheme.onSurface.withValues(alpha: 0.72)
          : theme.colorScheme.primary,
      textStyle: compactLabelStyle,
      minWidth: dense ? 18 : 20,
    );
  }

  if (conversation.isPinned) {
    return AppIcon(
      AppSemanticIcon.pinned,
      size: dense ? 11 : 12,
      color: theme.colorScheme.primary.withValues(
        alpha: selected ? 0.68 : 0.56,
      ),
    );
  }

  if (conversation.isMuted) {
    return AppIcon(
      AppSemanticIcon.muted,
      size: dense ? 11 : 12,
      color: theme.textTheme.bodySmall?.color?.withValues(alpha: 0.52),
    );
  }

  if (conversation.isOnline) {
    return AppIcon(
      AppIcons.presence(conversation.isOnline),
      size: dense ? 7 : 8,
      color: theme.colorScheme.secondary.withValues(alpha: 0.62),
    );
  }

  return null;
}

class _ConversationInlineStatePill extends StatelessWidget {
  const _ConversationInlineStatePill({
    required this.label,
    required this.backgroundColor,
    required this.textColor,
    required this.textStyle,
    this.minWidth,
  });

  final String label;
  final Color backgroundColor;
  final Color textColor;
  final TextStyle? textStyle;
  final double? minWidth;

  @override
  Widget build(BuildContext context) {
    return Container(
      constraints: minWidth == null
          ? null
          : BoxConstraints(minWidth: minWidth!),
      padding: const EdgeInsets.symmetric(horizontal: 4.5, vertical: 1.5),
      decoration: BoxDecoration(
        color: backgroundColor,
        borderRadius: BorderRadius.circular(999),
      ),
      child: Text(
        label,
        textAlign: TextAlign.center,
        style: textStyle?.copyWith(color: textColor),
      ),
    );
  }
}

class _ConversationLeading extends StatelessWidget {
  const _ConversationLeading({
    required this.conversation,
    required this.selected,
  });

  final ConversationSummary conversation;
  final bool selected;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final iconColor = selected
        ? theme.colorScheme.primary
        : theme.colorScheme.onSurface;
    final semanticIcon = AppIcons.conversationLead(conversation);
    if (semanticIcon != null) {
      return AppIcon(semanticIcon, size: 18, color: iconColor);
    }
    return Text(
      conversation.title.characters.first.toUpperCase(),
      style: theme.textTheme.titleMedium?.copyWith(
        color: selected
            ? theme.colorScheme.primary
            : theme.colorScheme.onSurface,
      ),
    );
  }
}

class MessageBubble extends StatelessWidget {
  const MessageBubble({
    super.key,
    required this.message,
    this.showAvatar = false,
    this.showSenderLabel = true,
  });

  final ChatMessage message;
  final bool showAvatar;
  final bool showSenderLabel;

  @override
  Widget build(BuildContext context) {
    final isOutgoing = message.direction == MessageDirection.outgoing;
    final theme = Theme.of(context);
    final palette = Theme.of(context).extension<AppShellColors>()!;
    final alignment = isOutgoing
        ? CrossAxisAlignment.end
        : CrossAxisAlignment.start;
    final bubbleColor = isOutgoing
        ? palette.bubbleOutgoing
        : palette.bubbleIncoming;
    final isMobile = MediaQuery.sizeOf(context).width < 760;
    final maxBubbleWidth = isMobile
        ? MediaQuery.sizeOf(context).width * 0.73
        : 472.0;
    final desktopRadius = const Radius.circular(14);
    final borderRadius = BorderRadius.only(
      topLeft: isMobile
          ? const Radius.circular(AppTokens.radiusBubble)
          : desktopRadius,
      topRight: isMobile
          ? const Radius.circular(AppTokens.radiusBubble)
          : desktopRadius,
      bottomLeft: Radius.circular(
        isOutgoing
            ? (isMobile ? AppTokens.radiusBubble : 12)
            : (isMobile ? 6 : 4),
      ),
      bottomRight: Radius.circular(
        isOutgoing
            ? (isMobile ? 6 : 4)
            : (isMobile ? AppTokens.radiusBubble : 12),
      ),
    );
    final hasAttachment = message.attachment != null;
    final hasText = message.hasText;
    final useDesktopAttachmentShell = hasAttachment && !isMobile;
    final attachment = message.attachment;
    final hasVisualAttachment = switch (attachment) {
      final ChatAttachment value =>
        _resolvedAttachmentKind(value) != ChatAttachmentKind.file,
      null => false,
    };
    final useDesktopInlineMediaMeta =
        !isMobile && hasVisualAttachment && !hasText;

    final bubble = ConstrainedBox(
      constraints: BoxConstraints(maxWidth: maxBubbleWidth),
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: useDesktopAttachmentShell ? Colors.transparent : bubbleColor,
          borderRadius: borderRadius,
        ),
        child: Padding(
          padding: useDesktopAttachmentShell
              ? EdgeInsets.zero
              : EdgeInsets.symmetric(
                  horizontal: hasAttachment
                      ? (isMobile ? AppTokens.spacingSm + 3 : 10)
                      : (isMobile ? AppTokens.spacingLg : 14),
                  vertical: hasAttachment
                      ? (isMobile ? AppTokens.spacingSm : 6.5)
                      : (isMobile ? AppTokens.spacingMd : 8.0),
                ),
          child: Column(
            crossAxisAlignment: alignment,
            children: <Widget>[
              if (attachment case final attachment?)
                if (useDesktopInlineMediaMeta)
                  Stack(
                    clipBehavior: Clip.none,
                    children: <Widget>[
                      AttachmentMessageCard(
                        attachment: attachment,
                        isOutgoing: isOutgoing,
                      ),
                      Positioned(
                        right: 8,
                        bottom: attachment.state.hasProgress ? 7 : 6,
                        child: _DesktopMediaMetaPill(message: message),
                      ),
                    ],
                  )
                else
                  AttachmentMessageCard(
                    attachment: attachment,
                    isOutgoing: isOutgoing,
                  ),
              if (hasAttachment && hasText) const SizedBox(height: 7),
              if (hasText)
                Text(
                  message.text,
                  style: theme.textTheme.bodyMedium?.copyWith(
                    fontSize: isMobile ? null : 13.2,
                    height: isMobile ? 1.35 : 1.32,
                  ),
                ),
              if (!useDesktopInlineMediaMeta) ...<Widget>[
                SizedBox(height: hasAttachment ? 4.5 : 6),
                Row(
                  mainAxisSize: MainAxisSize.min,
                  children: <Widget>[
                    Text(
                      '${message.timestamp.hour.toString().padLeft(2, '0')}:${message.timestamp.minute.toString().padLeft(2, '0')}',
                      style: Theme.of(context).textTheme.labelSmall?.copyWith(
                        fontSize: isMobile ? 11 : 9.8,
                        height: 1.0,
                      ),
                    ),
                    if (isOutgoing) ...<Widget>[
                      const SizedBox(width: 4),
                      AppIcon(
                        AppIcons.messageStatus(message.status),
                        size: isMobile ? 12 : 10.8,
                        color: message.status == MessageDeliveryStatus.read
                            ? Theme.of(context).colorScheme.primary
                            : null,
                      ),
                    ],
                  ],
                ),
              ],
            ],
          ),
        ),
      ),
    );

    final incomingContent = Row(
      crossAxisAlignment: CrossAxisAlignment.end,
      children: <Widget>[
        if (showAvatar) ...<Widget>[
          CircleAvatar(
            radius: 14,
            backgroundColor: palette.surfaceVariant,
            child: Text(
              message.senderLabel.characters.first,
              style: Theme.of(context).textTheme.labelMedium,
            ),
          ),
          const SizedBox(width: AppTokens.spacingSm),
        ],
        Flexible(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              if (showSenderLabel)
                Padding(
                  padding: const EdgeInsets.only(left: 4, bottom: 4),
                  child: Text(
                    message.senderLabel,
                    style: Theme.of(context).textTheme.labelMedium,
                  ),
                ),
              bubble,
            ],
          ),
        ),
      ],
    );

    return Column(
      crossAxisAlignment: alignment,
      children: <Widget>[if (isOutgoing) bubble else incomingContent],
    );
  }
}

class _DesktopMediaMetaPill extends StatelessWidget {
  const _DesktopMediaMetaPill({required this.message});

  final ChatMessage message;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isOutgoing = message.direction == MessageDirection.outgoing;

    return ClipRRect(
      borderRadius: BorderRadius.circular(999),
      child: BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 9, sigmaY: 9),
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 4),
          decoration: BoxDecoration(
            color: Colors.black.withValues(alpha: 0.16),
            borderRadius: BorderRadius.circular(999),
            border: Border.all(color: Colors.white.withValues(alpha: 0.08)),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              Text(
                '${message.timestamp.hour.toString().padLeft(2, '0')}:${message.timestamp.minute.toString().padLeft(2, '0')}',
                style: theme.textTheme.labelSmall?.copyWith(
                  color: Colors.white.withValues(alpha: 0.9),
                  fontSize: 9.4,
                  height: 1.0,
                ),
              ),
              if (isOutgoing) ...<Widget>[
                const SizedBox(width: 4),
                AppIcon(
                  AppIcons.messageStatus(message.status),
                  size: 10.2,
                  color: message.status == MessageDeliveryStatus.read
                      ? theme.colorScheme.primary.withValues(alpha: 0.96)
                      : Colors.white.withValues(alpha: 0.74),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

class MediaSpoilerMask extends StatefulWidget {
  const MediaSpoilerMask({
    super.key,
    required this.label,
    required this.icon,
    required this.grainKey,
    required this.chipKey,
    this.compact = false,
    this.glowColor,
    this.showLabel = true,
  });

  final String label;
  final AppSemanticIcon icon;
  final Key grainKey;
  final Key chipKey;
  final bool compact;
  final Color? glowColor;
  final bool showLabel;

  @override
  State<MediaSpoilerMask> createState() => _MediaSpoilerMaskState();
}

class _MediaSpoilerMaskState extends State<MediaSpoilerMask>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller = AnimationController(
    vsync: this,
    duration: const Duration(milliseconds: 1150),
  )..forward();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final resolvedGlow =
        widget.glowColor ?? theme.colorScheme.primary.withValues(alpha: 0.18);
    final chipRadius = BorderRadius.circular(999);

    return AnimatedBuilder(
      animation: _controller,
      builder: (BuildContext context, Widget? child) {
        final phase = _controller.value;
        final shimmerAlignment = Alignment(-1.25 + phase * 2.5, -0.14);
        final shimmerOpacity = 0.014 + (0.006 * math.sin(phase * math.pi * 2));

        return ClipRect(
          child: Stack(
            children: <Widget>[
              Positioned.fill(
                child: BackdropFilter(
                  filter: ImageFilter.blur(
                    sigmaX: widget.compact ? 8 : 7,
                    sigmaY: widget.compact ? 8 : 7,
                  ),
                  child: ColoredBox(
                    color: Colors.black.withValues(
                      alpha: widget.compact ? 0.038 : 0.034,
                    ),
                  ),
                ),
              ),
              Positioned.fill(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                      colors: <Color>[
                        Colors.white.withValues(
                          alpha: widget.compact ? 0.014 : 0.012,
                        ),
                        Colors.transparent,
                        Colors.black.withValues(
                          alpha: widget.compact ? 0.022 : 0.020,
                        ),
                      ],
                      stops: const <double>[0, 0.42, 1],
                    ),
                  ),
                ),
              ),
              Positioned.fill(
                child: Align(
                  alignment: shimmerAlignment,
                  child: IgnorePointer(
                    child: Transform.rotate(
                      angle: -0.28,
                      child: FractionallySizedBox(
                        widthFactor: widget.compact ? 0.13 : 0.09,
                        heightFactor: 1.24,
                        child: DecoratedBox(
                          decoration: BoxDecoration(
                            gradient: LinearGradient(
                              begin: Alignment.centerLeft,
                              end: Alignment.centerRight,
                              colors: <Color>[
                                Colors.transparent,
                                Colors.white.withValues(alpha: shimmerOpacity),
                                resolvedGlow.withValues(
                                  alpha: widget.compact ? 0.022 : 0.020,
                                ),
                                Colors.transparent,
                              ],
                              stops: const <double>[0, 0.28, 0.64, 1],
                            ),
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
              ),
              Positioned.fill(
                child: IgnorePointer(
                  child: _MediaSpoilerGrain(
                    key: widget.grainKey,
                    compact: widget.compact,
                    glowColor: resolvedGlow,
                    phase: phase,
                  ),
                ),
              ),
              Align(
                alignment: widget.showLabel
                    ? Alignment.center
                    : Alignment.bottomRight,
                child: Padding(
                  padding: widget.showLabel
                      ? EdgeInsets.zero
                      : EdgeInsets.only(
                          right: widget.compact ? 10 : 8,
                          bottom: widget.compact ? 10 : 8,
                        ),
                  child: ClipRRect(
                    borderRadius: chipRadius,
                    child: BackdropFilter(
                      filter: ImageFilter.blur(
                        sigmaX: widget.compact ? 7 : 6,
                        sigmaY: widget.compact ? 7 : 6,
                      ),
                      child: Container(
                        key: widget.chipKey,
                        padding: EdgeInsets.symmetric(
                          horizontal: widget.showLabel
                              ? (widget.compact ? 10 : 8)
                              : (widget.compact ? 6 : 4.5),
                          vertical: widget.showLabel
                              ? (widget.compact ? 6 : 4)
                              : (widget.compact ? 6 : 4.5),
                        ),
                        decoration: BoxDecoration(
                          borderRadius: chipRadius,
                          color: Colors.black.withValues(
                            alpha: widget.showLabel
                                ? (widget.compact ? 0.16 : 0.14)
                                : (widget.compact ? 0.18 : 0.15),
                          ),
                          border: Border.all(
                            color: Colors.white.withValues(
                              alpha: widget.showLabel
                                  ? (widget.compact ? 0.14 : 0.12)
                                  : (widget.compact ? 0.08 : 0.06),
                            ),
                          ),
                          boxShadow: widget.showLabel
                              ? <BoxShadow>[
                                  BoxShadow(
                                    color: Colors.black.withValues(
                                      alpha: widget.compact ? 0.04 : 0.034,
                                    ),
                                    blurRadius: widget.compact ? 14 : 10,
                                    offset: Offset(0, widget.compact ? 6 : 4),
                                  ),
                                ]
                              : const <BoxShadow>[],
                        ),
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: <Widget>[
                            AppIcon(
                              widget.icon,
                              size: widget.showLabel
                                  ? (widget.compact ? 12 : 10)
                                  : (widget.compact ? 9.4 : 8.0),
                              color: Colors.white.withValues(
                                alpha: widget.showLabel ? 0.92 : 0.78,
                              ),
                            ),
                            if (widget.showLabel) ...<Widget>[
                              SizedBox(width: widget.compact ? 4.5 : 4),
                              Text(
                                widget.label,
                                style: theme.textTheme.labelSmall?.copyWith(
                                  color: Colors.white.withValues(alpha: 0.92),
                                  fontSize: widget.compact ? 8.3 : 6.8,
                                  fontWeight: FontWeight.w700,
                                  letterSpacing: -0.02,
                                  height: 1.0,
                                ),
                              ),
                            ],
                          ],
                        ),
                      ),
                    ),
                  ),
                ),
              ),
            ],
          ),
        );
      },
    );
  }
}

class _MediaSpoilerGrain extends StatelessWidget {
  const _MediaSpoilerGrain({
    super.key,
    required this.compact,
    required this.glowColor,
    required this.phase,
  });

  final bool compact;
  final Color glowColor;
  final double phase;

  static const List<_MediaSpoilerGrainNode> _nodes = <_MediaSpoilerGrainNode>[
    _MediaSpoilerGrainNode(0.14, 0.24, 0.08, 0.028, 0.08, -0.16),
    _MediaSpoilerGrainNode(0.25, 0.66, 0.1, 0.03, 0.06, 0.14),
    _MediaSpoilerGrainNode(0.38, 0.46, 0.03, 0.03, 0.05, 0),
    _MediaSpoilerGrainNode(0.51, 0.32, 0.09, 0.028, 0.07, 0.22),
    _MediaSpoilerGrainNode(0.63, 0.72, 0.11, 0.03, 0.05, -0.18),
    _MediaSpoilerGrainNode(0.74, 0.24, 0.08, 0.026, 0.07, 0.28),
    _MediaSpoilerGrainNode(0.84, 0.56, 0.09, 0.03, 0.05, -0.24),
    _MediaSpoilerGrainNode(0.18, 0.5, 0.028, 0.028, 0.04, 0),
    _MediaSpoilerGrainNode(0.58, 0.56, 0.026, 0.026, 0.04, 0),
    _MediaSpoilerGrainNode(0.88, 0.22, 0.03, 0.03, 0.04, 0),
  ];

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (BuildContext context, BoxConstraints constraints) {
        final width = constraints.maxWidth;
        final height = constraints.maxHeight;
        if (width <= 0 || height <= 0) {
          return const SizedBox.shrink();
        }
        return Stack(
          children: <Widget>[
            for (var index = 0; index < _nodes.length; index++)
              Positioned(
                left:
                    width * _animatedNodeX(_nodes[index], index) -
                    (width * _nodes[index].widthFactor) / 2,
                top:
                    height * _animatedNodeY(_nodes[index], index) -
                    (height * _nodes[index].heightFactor) / 2,
                child: Transform.rotate(
                  angle:
                      _nodes[index].rotation +
                      (math.sin((phase * math.pi * 2) + index) * 0.025),
                  child: Container(
                    width:
                        width * _nodes[index].widthFactor * (compact ? 1 : 0.9),
                    height:
                        height *
                        _nodes[index].heightFactor *
                        (compact ? 1 : 0.92),
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(
                        compact ? height * 0.08 : height * 0.1,
                      ),
                      color:
                          (_nodes[index].rotation == 0
                                  ? Colors.white
                                  : glowColor)
                              .withValues(
                                alpha: _animatedNodeAlpha(_nodes[index], index),
                              ),
                    ),
                  ),
                ),
              ),
          ],
        );
      },
    );
  }

  double _animatedNodeX(_MediaSpoilerGrainNode node, int index) {
    final wave = math.sin((phase * math.pi * 2) + (index * 0.82));
    return (node.x + wave * 0.012).clamp(0.02, 0.98);
  }

  double _animatedNodeY(_MediaSpoilerGrainNode node, int index) {
    final wave = math.cos((phase * math.pi * 2) + (index * 0.56));
    return (node.y + wave * 0.01).clamp(0.02, 0.98);
  }

  double _animatedNodeAlpha(_MediaSpoilerGrainNode node, int index) {
    final pulse = 0.84 + (0.22 * math.sin((phase * math.pi * 2) + index));
    final baseAlpha = compact ? node.alpha * 0.38 : node.alpha * 0.30;
    return (baseAlpha * pulse).clamp(0.006, 0.055);
  }
}

class _MediaSpoilerGrainNode {
  const _MediaSpoilerGrainNode(
    this.x,
    this.y,
    this.widthFactor,
    this.heightFactor,
    this.alpha,
    this.rotation,
  );

  final double x;
  final double y;
  final double widthFactor;
  final double heightFactor;
  final double alpha;
  final double rotation;
}

const Duration _kSpoilerRevealDuration = Duration(milliseconds: 240);
const Duration _kSpoilerHideDuration = Duration(milliseconds: 180);

Widget _buildAnimatedSpoilerOverlay({
  required bool visible,
  required Key overlayKey,
  required String transitionKey,
  required Widget child,
}) {
  return Positioned.fill(
    child: IgnorePointer(
      ignoring: !visible,
      child: AnimatedSwitcher(
        duration: _kSpoilerRevealDuration,
        reverseDuration: _kSpoilerHideDuration,
        switchInCurve: Curves.easeOutCubic,
        switchOutCurve: Curves.easeInCubic,
        layoutBuilder: (Widget? currentChild, List<Widget> previousChildren) {
          final currentChildren = currentChild == null
              ? const <Widget>[]
              : <Widget>[currentChild];
          return Stack(
            fit: StackFit.expand,
            children: <Widget>[...previousChildren, ...currentChildren],
          );
        },
        transitionBuilder: (Widget child, Animation<double> animation) {
          final curved = CurvedAnimation(
            parent: animation,
            curve: Curves.easeOutCubic,
            reverseCurve: Curves.easeInCubic,
          );
          return FadeTransition(opacity: curved, child: child);
        },
        child: visible
            ? KeyedSubtree(
                key: ValueKey<String>(transitionKey),
                child: KeyedSubtree(key: overlayKey, child: child),
              )
            : const SizedBox.expand(
                key: ValueKey<String>('spoiler-mask-hidden'),
              ),
      ),
    ),
  );
}

class AttachmentMessageCard extends StatelessWidget {
  const AttachmentMessageCard({
    super.key,
    required this.attachment,
    required this.isOutgoing,
  });

  final ChatAttachment attachment;
  final bool isOutgoing;

  @override
  Widget build(BuildContext context) {
    final resolvedKind = _resolvedAttachmentKind(attachment);
    if (resolvedKind != ChatAttachmentKind.file) {
      return _MediaAttachmentBubble(
        attachment: attachment,
        isOutgoing: isOutgoing,
        resolvedKind: resolvedKind,
      );
    }
    return _DocumentAttachmentCard(
      attachment: attachment,
      isOutgoing: isOutgoing,
    );
  }
}

class _DocumentAttachmentCard extends StatelessWidget {
  const _DocumentAttachmentCard({
    required this.attachment,
    required this.isOutgoing,
  });

  final ChatAttachment attachment;
  final bool isOutgoing;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final isCompactMobile = MediaQuery.sizeOf(context).width < 760;
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final extensionLabel = _attachmentExtensionLabelFor(attachment);
    final cardRadius = isCompactMobile ? (isCupertinoStyle ? 12.0 : 12.5) : 9.5;
    final outerPadding = isCompactMobile
        ? (isCupertinoStyle ? 5.0 : 5.5)
        : 3.25;
    final surfaceColor = isOutgoing
        ? Colors.white.withValues(alpha: isCompactMobile ? 0.04 : 0.034)
        : isCompactMobile
        ? palette.surfaceVariant.withValues(alpha: 0.36)
        : palette.surfaceVariant.withValues(alpha: 0.118);
    final borderColor = isOutgoing
        ? Colors.white.withValues(alpha: isCompactMobile ? 0.05 : 0.026)
        : palette.divider.withValues(alpha: isCompactMobile ? 0.12 : 0.038);
    final accentColor = isOutgoing ? Colors.white : theme.colorScheme.primary;
    final documentLeadColor = isOutgoing
        ? Colors.white.withValues(alpha: 0.82)
        : isCompactMobile
        ? accentColor
        : theme.textTheme.bodySmall?.color?.withValues(alpha: 0.72) ??
              theme.colorScheme.onSurface.withValues(alpha: 0.72);
    final stateColor = _attachmentStateColor(
      context,
      attachment.state,
      isOutgoing,
    );
    final progress = switch (attachment.progress) {
      final value? => value.clamp(0.0, 1.0).toDouble(),
      null => null,
    };
    final showProgress = progress != null && attachment.state.hasProgress;

    return Container(
      key: const Key('attachment-document-card'),
      padding: EdgeInsets.all(outerPadding),
      decoration: BoxDecoration(
        color: surfaceColor,
        borderRadius: BorderRadius.circular(cardRadius),
        border: Border.all(
          color: borderColor.withValues(alpha: isCompactMobile ? 0.72 : 0.56),
        ),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Container(
            key: const Key('attachment-document-icon-surface'),
            width: isCompactMobile ? 32 : 32,
            height: isCompactMobile ? 36 : 34,
            decoration: BoxDecoration(
              color: documentLeadColor.withValues(
                alpha: isOutgoing ? 0.12 : (isCompactMobile ? 0.06 : 0.05),
              ),
              borderRadius: BorderRadius.circular(isCompactMobile ? 9 : 10),
              border: isCompactMobile
                  ? null
                  : Border.all(color: borderColor.withValues(alpha: 0.5)),
            ),
            child: Stack(
              children: <Widget>[
                Positioned(
                  top: isCompactMobile ? 5 : 4,
                  left: isCompactMobile ? 6 : 4,
                  right: isCompactMobile ? 6 : 4,
                  child: IconTheme(
                    data: IconThemeData(
                      color: documentLeadColor,
                      size: isCompactMobile ? 13 : 12,
                    ),
                    child: const AppIcon(AppSemanticIcon.file),
                  ),
                ),
                Positioned(
                  left: 0,
                  right: 0,
                  bottom: isCompactMobile ? 6 : 5,
                  child: Text(
                    extensionLabel,
                    textAlign: TextAlign.center,
                    style: theme.textTheme.labelSmall?.copyWith(
                      color: documentLeadColor.withValues(
                        alpha: isCompactMobile ? 0.78 : 0.62,
                      ),
                      fontWeight: FontWeight.w800,
                      fontSize: isCompactMobile ? 7.0 : 6.7,
                      height: 1.0,
                    ),
                  ),
                ),
              ],
            ),
          ),
          SizedBox(width: isCompactMobile ? 6 : 5.5),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: <Widget>[
                Row(
                  children: <Widget>[
                    Expanded(
                      child: Text(
                        attachment.title,
                        maxLines: 2,
                        overflow: TextOverflow.ellipsis,
                        style: theme.textTheme.titleMedium?.copyWith(
                          fontSize: isCompactMobile ? 11.2 : 11.4,
                          fontWeight: FontWeight.w600,
                          height: 1.1,
                        ),
                      ),
                    ),
                    SizedBox(width: isCompactMobile ? 5 : 4.5),
                    Text(
                      attachment.state.label,
                      key: const Key('attachment-state-pill'),
                      style: theme.textTheme.labelSmall?.copyWith(
                        color: stateColor,
                        fontWeight: FontWeight.w500,
                        fontSize: isCompactMobile ? 8.0 : 7.0,
                        letterSpacing: isCompactMobile ? null : -0.05,
                        height: 1.0,
                      ),
                    ),
                  ],
                ),
                SizedBox(height: isCompactMobile ? 2 : 2),
                Row(
                  key: const Key('attachment-file-meta-row'),
                  children: <Widget>[
                    Expanded(
                      child: Text(
                        attachment.detail,
                        key: const Key('attachment-file-secondary-hint'),
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: theme.textTheme.bodySmall?.copyWith(
                          fontSize: isCompactMobile ? 8.8 : 8.7,
                          height: 1.15,
                          color: theme.textTheme.bodySmall?.color?.withValues(
                            alpha: isCompactMobile ? 1 : 0.82,
                          ),
                        ),
                      ),
                    ),
                    SizedBox(width: isCompactMobile ? 4 : 4),
                    Container(
                      key: const Key('attachment-extension-pill'),
                      child: Text(
                        extensionLabel,
                        style: theme.textTheme.labelSmall?.copyWith(
                          color: documentLeadColor.withValues(
                            alpha: isCompactMobile ? 0.78 : 0.62,
                          ),
                          fontWeight: FontWeight.w700,
                          fontSize: isCompactMobile ? 7.3 : 6.8,
                          height: 1.0,
                        ),
                      ),
                    ),
                  ],
                ),
                SizedBox(height: isCompactMobile ? 4 : 3.5),
                Row(
                  children: <Widget>[
                    Container(
                      key: const Key('attachment-file-primary-action'),
                      child: _AttachmentActionLabel(
                        key: const Key('attachment-action-chip'),
                        label: attachment.actionLabel,
                        tint: accentColor,
                        background: accentColor.withValues(alpha: 0.1),
                        borderColor: accentColor.withValues(alpha: 0.06),
                        compact: isCompactMobile,
                      ),
                    ),
                    if (showProgress) ...<Widget>[
                      SizedBox(width: isCompactMobile ? 5 : 4.5),
                      Expanded(
                        child: ClipRRect(
                          borderRadius: BorderRadius.circular(999),
                          child: LinearProgressIndicator(
                            minHeight: isCompactMobile ? 1.8 : 1.4,
                            value: progress,
                            backgroundColor: stateColor.withValues(
                              alpha: isCompactMobile ? 0.14 : 0.1,
                            ),
                            valueColor: AlwaysStoppedAnimation<Color>(
                              stateColor,
                            ),
                          ),
                        ),
                      ),
                    ],
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _MediaAttachmentBubble extends StatelessWidget {
  const _MediaAttachmentBubble({
    required this.attachment,
    required this.isOutgoing,
    required this.resolvedKind,
  });

  final ChatAttachment attachment;
  final bool isOutgoing;
  final ChatAttachmentKind resolvedKind;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final isCompactMobile = MediaQuery.sizeOf(context).width < 760;
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final accentColor = isOutgoing ? Colors.white : theme.colorScheme.primary;
    final borderColor = isOutgoing
        ? Colors.white.withValues(alpha: isCompactMobile ? 0.08 : 0.06)
        : palette.divider.withValues(alpha: isCompactMobile ? 0.14 : 0.08);
    final stateColor = _attachmentStateColor(
      context,
      attachment.state,
      isOutgoing,
    );
    final badgeLabel = _mediaBadgeLabel(attachment, resolvedKind);
    final titleLabel = _mediaPrimaryLabel(attachment, resolvedKind);

    return LayoutBuilder(
      builder: (BuildContext context, BoxConstraints constraints) {
        final availableWidth = constraints.maxWidth.isFinite
            ? constraints.maxWidth
            : (isCompactMobile ? 248.0 : 304.0);
        final targetWidth = isCompactMobile
            ? availableWidth
                  .clamp(
                    isCupertinoStyle ? 188.0 : 204.0,
                    isCupertinoStyle ? 234.0 : 252.0,
                  )
                  .toDouble()
            : availableWidth.clamp(236.0, 304.0).toDouble();
        final previewHeight = isCompactMobile
            ? targetWidth * (isCupertinoStyle ? 0.78 : 0.74)
            : targetWidth * 0.66;
        final cornerRadius = isCompactMobile
            ? (isCupertinoStyle ? 18.0 : 17.0)
            : 16.0;

        return SizedBox(
          key: const Key('attachment-image-card'),
          width: targetWidth,
          child: _MediaPreviewSurface(
            previewKey: Key(
              isCompactMobile
                  ? 'attachment-image-preview'
                  : 'attachment-desktop-image-thumb',
            ),
            overlayKey: Key(
              isCompactMobile
                  ? 'attachment-image-mask-overlay'
                  : 'attachment-desktop-image-mask',
            ),
            grainKey: Key(
              isCompactMobile
                  ? 'attachment-image-mask-grain'
                  : 'attachment-desktop-image-mask-grain',
            ),
            chipKey: Key(
              isCompactMobile
                  ? 'attachment-image-mask-chip'
                  : 'attachment-desktop-image-mask-chip',
            ),
            attachment: attachment,
            isOutgoing: isOutgoing,
            resolvedKind: resolvedKind,
            titleLabel: titleLabel,
            badgeLabel: badgeLabel,
            stateColor: stateColor,
            accentColor: accentColor,
            borderColor: borderColor,
            previewHeight: previewHeight,
            cornerRadius: cornerRadius,
            compact: isCompactMobile,
          ),
        );
      },
    );
  }
}

class _MediaPreviewSurface extends StatefulWidget {
  const _MediaPreviewSurface({
    required this.previewKey,
    required this.overlayKey,
    required this.grainKey,
    required this.chipKey,
    required this.attachment,
    required this.isOutgoing,
    required this.resolvedKind,
    required this.titleLabel,
    required this.badgeLabel,
    required this.stateColor,
    required this.accentColor,
    required this.borderColor,
    required this.previewHeight,
    required this.cornerRadius,
    required this.compact,
  });

  final Key previewKey;
  final Key overlayKey;
  final Key grainKey;
  final Key chipKey;
  final ChatAttachment attachment;
  final bool isOutgoing;
  final ChatAttachmentKind resolvedKind;
  final String titleLabel;
  final String badgeLabel;
  final Color stateColor;
  final Color accentColor;
  final Color borderColor;
  final double previewHeight;
  final double cornerRadius;
  final bool compact;

  @override
  State<_MediaPreviewSurface> createState() => _MediaPreviewSurfaceState();
}

class _MediaPreviewSurfaceState extends State<_MediaPreviewSurface> {
  bool _revealed = false;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final shouldMask =
        widget.attachment.sensitivity != ChatAttachmentSensitivity.none &&
        !_revealed;
    final actionTint = Colors.white.withValues(alpha: 0.94);
    final metaColor = Colors.white.withValues(alpha: 0.74);

    return Material(
      color: Colors.transparent,
      child: InkWell(
        borderRadius: BorderRadius.circular(widget.cornerRadius),
        onTap: widget.attachment.sensitivity == ChatAttachmentSensitivity.none
            ? null
            : () => setState(() => _revealed = !_revealed),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(widget.cornerRadius),
          child: SizedBox(
            height: widget.previewHeight,
            child: Stack(
              children: <Widget>[
                AnimatedScale(
                  duration: _kSpoilerRevealDuration,
                  curve: Curves.easeOutCubic,
                  scale: shouldMask ? 1 : 1.018,
                  child: _MediaArtwork(
                    key: widget.previewKey,
                    isOutgoing: widget.isOutgoing,
                    resolvedKind: widget.resolvedKind,
                    accentColor: widget.accentColor,
                    borderColor: widget.borderColor,
                    thumbnailSeed: widget.attachment.thumbnailSeed,
                  ),
                ),
                if (!shouldMask)
                  Positioned(
                    top: widget.compact ? 10 : 11,
                    left: widget.compact ? 10 : 11,
                    child: KeyedSubtree(
                      key: const Key('attachment-file-primary-action'),
                      child: _AttachmentActionLabel(
                        key: const Key('attachment-action-chip'),
                        label: widget.attachment.actionLabel,
                        tint: actionTint,
                        background: Colors.black.withValues(alpha: 0.18),
                        borderColor: Colors.white.withValues(alpha: 0.14),
                        compact: widget.compact,
                      ),
                    ),
                  ),
                if (!shouldMask)
                  Positioned(
                    top: widget.compact ? 12 : 11,
                    right: widget.compact ? 12 : 11,
                    child: Text(
                      widget.attachment.state.label,
                      key: const Key('attachment-state-pill'),
                      style: theme.textTheme.labelSmall?.copyWith(
                        color: widget.stateColor.withValues(alpha: 0.96),
                        fontWeight: FontWeight.w600,
                        fontSize: widget.compact ? 8.0 : 7.4,
                        height: 1.0,
                      ),
                    ),
                  ),
                if (!shouldMask)
                  Positioned(
                    left: 0,
                    right: 0,
                    bottom: 0,
                    child: Container(
                      padding: EdgeInsets.fromLTRB(
                        widget.compact ? 12 : 13,
                        widget.compact ? 26 : 28,
                        widget.compact ? 12 : 13,
                        widget.compact ? 11 : 10,
                      ),
                      decoration: BoxDecoration(
                        gradient: LinearGradient(
                          begin: Alignment.topCenter,
                          end: Alignment.bottomCenter,
                          colors: <Color>[
                            Colors.transparent,
                            Colors.black.withValues(alpha: 0.18),
                            Colors.black.withValues(alpha: 0.42),
                          ],
                          stops: const <double>[0, 0.36, 1],
                        ),
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        mainAxisSize: MainAxisSize.min,
                        children: <Widget>[
                          Text(
                            widget.titleLabel,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: theme.textTheme.titleMedium?.copyWith(
                              color: Colors.white.withValues(alpha: 0.96),
                              fontSize: widget.compact ? 12.0 : 11.4,
                              fontWeight: FontWeight.w600,
                              height: 1.0,
                            ),
                          ),
                          SizedBox(height: widget.compact ? 4 : 3),
                          Row(
                            key: const Key('attachment-file-meta-row'),
                            children: <Widget>[
                              Expanded(
                                child: Text(
                                  widget.attachment.detail,
                                  key: const Key(
                                    'attachment-file-secondary-hint',
                                  ),
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                  style: theme.textTheme.bodySmall?.copyWith(
                                    color: metaColor,
                                    fontSize: widget.compact ? 8.4 : 8.0,
                                    height: 1.0,
                                  ),
                                ),
                              ),
                              SizedBox(width: widget.compact ? 5 : 4),
                              Container(
                                key: const Key('attachment-extension-pill'),
                                child: Text(
                                  widget.badgeLabel,
                                  style: theme.textTheme.labelSmall?.copyWith(
                                    color: Colors.white.withValues(alpha: 0.9),
                                    fontWeight: FontWeight.w700,
                                    fontSize: widget.compact ? 7.4 : 6.9,
                                    height: 1.0,
                                  ),
                                ),
                              ),
                            ],
                          ),
                        ],
                      ),
                    ),
                  ),
                if (widget.attachment.sensitivity ==
                        ChatAttachmentSensitivity.none &&
                    widget.resolvedKind == ChatAttachmentKind.video)
                  Positioned.fill(
                    child: Center(
                      child: Container(
                        width: widget.compact ? 34 : 28,
                        height: widget.compact ? 34 : 28,
                        decoration: BoxDecoration(
                          color: Colors.black.withValues(alpha: 0.18),
                          shape: BoxShape.circle,
                          border: Border.all(
                            color: Colors.white.withValues(alpha: 0.16),
                          ),
                        ),
                        child: Center(
                          child: AppIcon(
                            AppSemanticIcon.video,
                            size: widget.compact ? 15 : 13,
                            color: Colors.white.withValues(alpha: 0.94),
                          ),
                        ),
                      ),
                    ),
                  ),
                if (widget.attachment.progress case final value?)
                  if (widget.attachment.state.hasProgress)
                    Positioned(
                      left: 0,
                      right: 0,
                      bottom: 0,
                      child: LinearProgressIndicator(
                        minHeight: widget.compact ? 2.4 : 1.8,
                        value: value,
                        backgroundColor: Colors.white.withValues(alpha: 0.16),
                        valueColor: AlwaysStoppedAnimation<Color>(
                          Colors.white.withValues(alpha: 0.88),
                        ),
                      ),
                    ),
                _buildAnimatedSpoilerOverlay(
                  visible: shouldMask,
                  overlayKey: widget.overlayKey,
                  transitionKey:
                      '${widget.compact ? 'mobile' : 'desktop'}-media-mask',
                  child: MediaSpoilerMask(
                    label: _sensitivityOverlayLabel(
                      widget.attachment.sensitivity,
                    ),
                    icon: widget.resolvedKind == ChatAttachmentKind.video
                        ? AppSemanticIcon.video
                        : AppSemanticIcon.media,
                    grainKey: widget.grainKey,
                    chipKey: widget.chipKey,
                    compact: widget.compact,
                    glowColor: widget.accentColor.withValues(alpha: 0.16),
                    showLabel:
                        widget.attachment.sensitivity ==
                        ChatAttachmentSensitivity.viewOnce,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _MediaArtwork extends StatelessWidget {
  const _MediaArtwork({
    super.key,
    required this.isOutgoing,
    required this.resolvedKind,
    required this.accentColor,
    required this.borderColor,
    this.thumbnailSeed,
  });

  final bool isOutgoing;
  final ChatAttachmentKind resolvedKind;
  final Color accentColor;
  final Color borderColor;
  final String? thumbnailSeed;

  @override
  Widget build(BuildContext context) {
    final palette = Theme.of(context).extension<AppShellColors>()!;
    final seedAccent = _mediaSeedAccent(thumbnailSeed);

    return Container(
      decoration: BoxDecoration(
        color: isOutgoing
            ? const Color(0xFF263D46)
            : palette.bg1.withValues(alpha: 0.98),
        border: Border.all(color: borderColor),
      ),
      child: Stack(
        fit: StackFit.expand,
        children: <Widget>[
          Positioned.fill(
            child: CustomPaint(
              painter: _MediaThumbnailPainter(
                isOutgoing: isOutgoing,
                accentColor: accentColor,
                seedAccent: seedAccent,
                resolvedKind: resolvedKind,
                surfaceColor: palette.bg1,
              ),
            ),
          ),
          Positioned.fill(
            child: DecoratedBox(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                  colors: <Color>[
                    Colors.white.withValues(alpha: isOutgoing ? 0.024 : 0.012),
                    Colors.transparent,
                    Colors.black.withValues(alpha: isOutgoing ? 0.18 : 0.16),
                  ],
                  stops: const <double>[0, 0.48, 1],
                ),
              ),
            ),
          ),
          if (resolvedKind == ChatAttachmentKind.video)
            Positioned.fill(
              child: Center(
                child: Container(
                  width: 44,
                  height: 44,
                  decoration: BoxDecoration(
                    color: Colors.black.withValues(alpha: 0.16),
                    shape: BoxShape.circle,
                    border: Border.all(
                      color: Colors.white.withValues(alpha: 0.14),
                    ),
                  ),
                  child: Center(
                    child: AppIcon(
                      AppSemanticIcon.video,
                      size: 18,
                      color: Colors.white.withValues(alpha: 0.94),
                    ),
                  ),
                ),
              ),
            ),
        ],
      ),
    );
  }
}

class _MediaThumbnailPainter extends CustomPainter {
  const _MediaThumbnailPainter({
    required this.isOutgoing,
    required this.accentColor,
    required this.seedAccent,
    required this.resolvedKind,
    required this.surfaceColor,
  });

  final bool isOutgoing;
  final Color accentColor;
  final Color seedAccent;
  final ChatAttachmentKind resolvedKind;
  final Color surfaceColor;

  @override
  void paint(Canvas canvas, Size size) {
    final rect = Offset.zero & size;
    final baseTop = isOutgoing
        ? const Color(0xFF425F68)
        : Color.alphaBlend(accentColor.withValues(alpha: 0.18), surfaceColor);
    final baseBottom = isOutgoing
        ? const Color(0xFF15252C)
        : const Color(0xFF121D27);
    canvas.drawRect(
      rect,
      Paint()
        ..shader = LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: <Color>[baseTop, baseBottom],
        ).createShader(rect),
    );

    final glowPaint = Paint()
      ..shader = RadialGradient(
        center: const Alignment(0.78, -0.72),
        radius: 0.95,
        colors: <Color>[
          seedAccent.withValues(alpha: isOutgoing ? 0.34 : 0.26),
          Colors.transparent,
        ],
      ).createShader(rect);
    canvas.drawRect(rect, glowPaint);

    final chrome = Paint()..color = Colors.white.withValues(alpha: 0.08);
    final shadow = Paint()..color = Colors.black.withValues(alpha: 0.20);
    final paper = Paint()..color = Colors.white.withValues(alpha: 0.15);
    final bubble = Paint()
      ..color = const Color(0xFF1C3342).withValues(alpha: 0.86);
    final outgoingBubble = Paint()
      ..color = accentColor.withValues(alpha: isOutgoing ? 0.38 : 0.30);

    final phone = RRect.fromRectAndRadius(
      Rect.fromLTWH(
        size.width * 0.09,
        size.height * 0.08,
        size.width * 0.56,
        size.height * 0.74,
      ),
      Radius.circular(size.width * 0.06),
    );
    canvas.drawRRect(
      phone.shift(Offset(size.width * 0.018, size.height * 0.025)),
      shadow,
    );
    canvas.drawRRect(
      phone,
      Paint()..color = const Color(0xFF0F1B24).withValues(alpha: 0.92),
    );
    canvas.drawRRect(
      RRect.fromRectAndRadius(
        Rect.fromLTWH(
          phone.left + 8,
          phone.top + 8,
          phone.width - 16,
          phone.height - 16,
        ),
        Radius.circular(size.width * 0.045),
      ),
      Paint()..color = const Color(0xFF142532).withValues(alpha: 0.98),
    );

    canvas.drawCircle(
      Offset(phone.left + 22, phone.top + 24),
      8,
      Paint()..color = seedAccent.withValues(alpha: 0.55),
    );
    _line(canvas, phone.left + 38, phone.top + 18, 70, chrome, height: 4);
    _line(canvas, phone.left + 38, phone.top + 28, 46, chrome, height: 3);

    _messageBubble(
      canvas,
      Rect.fromLTWH(phone.left + 18, phone.top + 50, phone.width * 0.62, 22),
      bubble,
    );
    _messageBubble(
      canvas,
      Rect.fromLTWH(phone.left + 18, phone.top + 78, phone.width * 0.72, 26),
      bubble,
    );
    _documentCard(
      canvas,
      Rect.fromLTWH(phone.left + 22, phone.top + 112, phone.width * 0.68, 42),
      paper,
    );
    _messageBubble(
      canvas,
      Rect.fromLTWH(
        phone.right - phone.width * 0.55 - 18,
        phone.top + 164,
        phone.width * 0.55,
        30,
      ),
      outgoingBubble,
    );

    final albumRect = Rect.fromLTWH(
      size.width * 0.49,
      size.height * 0.26,
      size.width * 0.40,
      size.height * 0.48,
    );
    canvas.drawRRect(
      RRect.fromRectAndRadius(
        albumRect.shift(const Offset(5, 8)),
        Radius.circular(size.width * 0.04),
      ),
      shadow,
    );
    canvas.drawRRect(
      RRect.fromRectAndRadius(albumRect, Radius.circular(size.width * 0.04)),
      Paint()..color = const Color(0xFF243C48).withValues(alpha: 0.96),
    );
    final tileGap = size.width * 0.012;
    final tileWidth = (albumRect.width - tileGap) / 2;
    final tileHeight = (albumRect.height - tileGap) / 2;
    for (var row = 0; row < 2; row += 1) {
      for (var col = 0; col < 2; col += 1) {
        final tile = Rect.fromLTWH(
          albumRect.left + col * (tileWidth + tileGap),
          albumRect.top + row * (tileHeight + tileGap),
          tileWidth,
          tileHeight,
        );
        canvas.drawRRect(
          RRect.fromRectAndRadius(tile, Radius.circular(size.width * 0.022)),
          Paint()
            ..shader = LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: <Color>[
                Color.alphaBlend(
                  seedAccent.withValues(alpha: 0.34),
                  const Color(0xFF5E7E85),
                ),
                Color.alphaBlend(
                  accentColor.withValues(alpha: 0.20),
                  const Color(0xFF1F323B),
                ),
              ],
            ).createShader(tile),
        );
        canvas.drawCircle(
          Offset(tile.right - tileWidth * 0.22, tile.top + tileHeight * 0.24),
          math.max(3, tileWidth * 0.11),
          Paint()..color = Colors.white.withValues(alpha: 0.20),
        );
      }
    }

    if (resolvedKind == ChatAttachmentKind.video) {
      final center = albumRect.center;
      final play = Path()
        ..moveTo(center.dx - 6, center.dy - 9)
        ..lineTo(center.dx - 6, center.dy + 9)
        ..lineTo(center.dx + 10, center.dy)
        ..close();
      canvas.drawCircle(
        center,
        21,
        Paint()..color = Colors.black.withValues(alpha: 0.24),
      );
      canvas.drawPath(
        play,
        Paint()..color = Colors.white.withValues(alpha: 0.86),
      );
    }
  }

  void _line(
    Canvas canvas,
    double x,
    double y,
    double width,
    Paint paint, {
    double height = 3,
  }) {
    canvas.drawRRect(
      RRect.fromRectAndRadius(
        Rect.fromLTWH(x, y, width, height),
        Radius.circular(height / 2),
      ),
      paint,
    );
  }

  void _messageBubble(Canvas canvas, Rect rect, Paint paint) {
    canvas.drawRRect(
      RRect.fromRectAndRadius(rect, const Radius.circular(10)),
      paint,
    );
    _line(
      canvas,
      rect.left + 10,
      rect.top + 8,
      rect.width * 0.58,
      Paint()..color = Colors.white.withValues(alpha: 0.16),
    );
    _line(
      canvas,
      rect.left + 10,
      rect.top + 16,
      rect.width * 0.42,
      Paint()..color = Colors.white.withValues(alpha: 0.10),
      height: 2.5,
    );
  }

  void _documentCard(Canvas canvas, Rect rect, Paint paint) {
    canvas.drawRRect(
      RRect.fromRectAndRadius(rect, const Radius.circular(10)),
      Paint()..color = Colors.white.withValues(alpha: 0.10),
    );
    final iconRect = Rect.fromLTWH(rect.left + 10, rect.top + 9, 20, 24);
    canvas.drawRRect(
      RRect.fromRectAndRadius(iconRect, const Radius.circular(5)),
      paint,
    );
    _line(
      canvas,
      rect.left + 38,
      rect.top + 11,
      rect.width * 0.44,
      Paint()..color = Colors.white.withValues(alpha: 0.20),
      height: 4,
    );
    _line(
      canvas,
      rect.left + 38,
      rect.top + 22,
      rect.width * 0.34,
      Paint()..color = Colors.white.withValues(alpha: 0.12),
      height: 3,
    );
  }

  @override
  bool shouldRepaint(covariant _MediaThumbnailPainter oldDelegate) {
    return oldDelegate.isOutgoing != isOutgoing ||
        oldDelegate.accentColor != accentColor ||
        oldDelegate.seedAccent != seedAccent ||
        oldDelegate.resolvedKind != resolvedKind ||
        oldDelegate.surfaceColor != surfaceColor;
  }
}

Color _attachmentStateColor(
  BuildContext context,
  ChatAttachmentState state,
  bool isOutgoing,
) {
  final theme = Theme.of(context);
  final neutralColor =
      theme.textTheme.bodySmall?.color?.withValues(alpha: 0.74) ??
      theme.colorScheme.onSurface.withValues(alpha: 0.74);
  return switch (state) {
    ChatAttachmentState.pendingSend => neutralColor,
    ChatAttachmentState.uploading => theme.colorScheme.primary,
    ChatAttachmentState.downloading => theme.colorScheme.primary,
    ChatAttachmentState.completed => theme.colorScheme.primary,
    ChatAttachmentState.failed => AppPalette.danger,
    ChatAttachmentState.expired =>
      theme.textTheme.bodySmall?.color ??
          theme.colorScheme.onSurface.withValues(alpha: 0.75),
    ChatAttachmentState.offlineQueued => neutralColor,
    ChatAttachmentState.retryReady => AppPalette.danger,
  };
}

ChatAttachmentKind _resolvedAttachmentKind(ChatAttachment attachment) {
  if (attachment.kind != ChatAttachmentKind.file) {
    return attachment.kind;
  }
  final extension = _attachmentExtensionLabelFor(attachment).toLowerCase();
  if (_imageExtensions.contains(extension)) {
    return ChatAttachmentKind.image;
  }
  if (_videoExtensions.contains(extension)) {
    return ChatAttachmentKind.video;
  }
  return ChatAttachmentKind.file;
}

String _attachmentExtensionLabelFor(ChatAttachment attachment) {
  final override = attachment.fileExtension?.trim();
  if (override != null && override.isNotEmpty) {
    return override.toUpperCase();
  }
  return _attachmentExtensionLabel(attachment.title);
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

String _mediaPrimaryLabel(
  ChatAttachment attachment,
  ChatAttachmentKind resolvedKind,
) {
  final caption = attachment.caption?.trim();
  if (caption != null && caption.isNotEmpty) {
    return caption;
  }
  if (resolvedKind == ChatAttachmentKind.image && attachment.albumCount > 1) {
    return '${attachment.albumCount} 张照片';
  }
  return resolvedKind == ChatAttachmentKind.video ? '视频' : '图片';
}

String _mediaBadgeLabel(
  ChatAttachment attachment,
  ChatAttachmentKind resolvedKind,
) {
  if (resolvedKind == ChatAttachmentKind.video &&
      attachment.durationLabel?.trim().isNotEmpty == true) {
    return attachment.durationLabel!.trim();
  }
  if (resolvedKind == ChatAttachmentKind.image && attachment.albumCount > 1) {
    return '${attachment.albumCount} 张';
  }
  return resolvedKind == ChatAttachmentKind.video ? '视频' : '图片';
}

Color _mediaSeedAccent(String? seed) {
  final normalized = seed?.trim();
  if (normalized == null || normalized.isEmpty) {
    return AppPalette.brandBlue;
  }
  var hash = 0;
  for (final codeUnit in normalized.codeUnits) {
    hash = ((hash << 5) - hash + codeUnit) & 0x7fffffff;
  }
  const accents = <Color>[
    AppPalette.brandBlue,
    AppPalette.channelCyan,
    AppPalette.socialGreen,
    AppPalette.qqYellow,
    AppPalette.mentionRed,
  ];
  return accents[hash % accents.length];
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

String _sensitivityOverlayLabel(ChatAttachmentSensitivity sensitivity) {
  return switch (sensitivity) {
    ChatAttachmentSensitivity.none => '查看',
    ChatAttachmentSensitivity.spoiler => '查看',
    ChatAttachmentSensitivity.viewOnce => '查看一次',
  };
}

List<ConversationSummary> prioritizeConversations(
  Iterable<ConversationSummary> conversations,
) {
  final items = conversations.toList();
  items.sort(compareConversationPriority);
  return items;
}

int conversationPriorityBucket(ConversationSummary conversation) {
  return _conversationPriorityBucket(conversation);
}

int compareConversationPriority(
  ConversationSummary left,
  ConversationSummary right,
) {
  final bucketDelta =
      _conversationPriorityBucket(left) - _conversationPriorityBucket(right);
  if (bucketDelta != 0) {
    return bucketDelta;
  }

  final updatedDelta = right.lastUpdated.compareTo(left.lastUpdated);
  if (updatedDelta != 0) {
    return updatedDelta;
  }

  if (left.unreadCount != right.unreadCount) {
    return right.unreadCount.compareTo(left.unreadCount);
  }

  return left.title.compareTo(right.title);
}

bool isHighValueConversation(ConversationSummary conversation) {
  final labels = <String?>[conversation.pillLabel, conversation.previewBadge];
  return conversation.mentionCount > 0 ||
      conversation.kind == ConversationKind.fileAssistant ||
      conversation.kind == ConversationKind.channel ||
      conversation.kind == ConversationKind.system ||
      labels.any(
        (label) =>
            label == '文件' ||
            label == '文件状态' ||
            label == '@你' ||
            label == '有人@你' ||
            label == '服务' ||
            label == '服务通知' ||
            label == '系统' ||
            label == '系统消息',
      );
}

int _conversationPriorityBucket(ConversationSummary conversation) {
  if (conversation.isPinned) {
    return 0;
  }
  if (conversation.isMuted) {
    return 5;
  }
  if (conversation.mentionCount > 0 ||
      (conversation.unreadCount > 0 && isHighValueConversation(conversation))) {
    return 1;
  }
  if (conversation.unreadCount > 0) {
    return 2;
  }
  if (conversation.hasDraft) {
    return 3;
  }
  return 4;
}

class _AttachmentActionLabel extends StatelessWidget {
  const _AttachmentActionLabel({
    super.key,
    required this.label,
    required this.tint,
    required this.background,
    required this.borderColor,
    this.compact = false,
  });

  final String label;
  final Color tint;
  final Color background;
  final Color borderColor;
  final bool compact;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: EdgeInsets.symmetric(
        horizontal: compact ? 1.4 : 1.8,
        vertical: 0,
      ),
      decoration: BoxDecoration(
        color: background.withValues(alpha: compact ? 0.18 : 1),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(
          color: borderColor.withValues(alpha: compact ? 0.0 : 1),
        ),
      ),
      child: Text(
        label,
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
        style: Theme.of(context).textTheme.labelMedium?.copyWith(
          color: tint,
          fontWeight: FontWeight.w600,
          fontSize: compact ? 6.9 : 7.4,
          height: 1.0,
        ),
      ),
    );
  }
}

class ComposerBar extends StatefulWidget {
  const ComposerBar({super.key, required this.onSend});

  final ValueChanged<String> onSend;

  @override
  State<ComposerBar> createState() => _ComposerBarState();
}

class _ComposerBarState extends State<ComposerBar> {
  final TextEditingController _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Row(
      children: <Widget>[
        Expanded(
          child: TextField(
            controller: _controller,
            decoration: const InputDecoration(
              hintText: '输入消息或附件说明…',
              prefixIcon: AppIcon(AppSemanticIcon.attachment),
            ),
          ),
        ),
        const SizedBox(width: AppTokens.spacingSm),
        FilledButton.tonalIcon(
          onPressed: () {
            final text = _controller.text;
            _controller.clear();
            widget.onSend(text);
          },
          icon: const AppIcon(AppSemanticIcon.send),
          label: const Text('发送'),
        ),
      ],
    );
  }
}

class DetailMetricTile extends StatelessWidget {
  const DetailMetricTile({
    super.key,
    required this.label,
    required this.value,
    required this.icon,
  });

  final String label;
  final String value;
  final IconData icon;

  @override
  Widget build(BuildContext context) {
    final palette = Theme.of(context).extension<AppShellColors>()!;
    return Container(
      padding: const EdgeInsets.all(AppTokens.spacingLg),
      decoration: BoxDecoration(
        color: palette.surfaceVariant,
        borderRadius: BorderRadius.circular(20),
      ),
      child: Row(
        children: <Widget>[
          Icon(icon, size: 18),
          const SizedBox(width: AppTokens.spacingSm),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(label, style: Theme.of(context).textTheme.labelMedium),
                const SizedBox(height: AppTokens.spacingXs),
                Text(value, style: Theme.of(context).textTheme.titleMedium),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
