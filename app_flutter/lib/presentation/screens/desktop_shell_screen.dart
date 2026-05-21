import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter/services.dart';

import '../../application/chat_providers.dart';
import '../../bootstrap/app_providers.dart';
import '../../domain/entities/models.dart';
import '../theme/app_icons.dart';
import '../theme/app_theme.dart';
import '../theme/app_tokens.dart';
import '../widgets/components.dart';
import '../widgets/inbox/conversation_filter_tabs.dart';
import '../widgets/social_avatar.dart';
import '../widgets/shell_chrome.dart';
import 'shell_screen_support.dart';

class DesktopShellScreen extends ConsumerStatefulWidget {
  const DesktopShellScreen({super.key, required this.section});

  // ignore: unused_field
  final AppSection section;

  @override
  ConsumerState<DesktopShellScreen> createState() => _DesktopShellScreenState();
}

class _DesktopShellScreenState extends ConsumerState<DesktopShellScreen> {
  final TextEditingController _searchController = TextEditingController();
  final FocusNode _searchFocusNode = FocusNode();
  bool _contextPanelOpen = false;
  ConversationFilterPreset _filter = ConversationFilterPreset.all;
  String _searchQuery = '';

  @override
  void initState() {
    super.initState();
    _searchController.addListener(_syncSearchQuery);
  }

  @override
  void dispose() {
    _searchController.removeListener(_syncSearchQuery);
    _searchController.dispose();
    _searchFocusNode.dispose();
    super.dispose();
  }

  void _syncSearchQuery() {
    final query = _searchController.text;
    if (query == _searchQuery) {
      return;
    }
    setState(() {
      _searchQuery = query;
    });
  }

  void _clearSearch() {
    _searchController.clear();
  }

  void _focusSearch() {
    _searchFocusNode.requestFocus();
  }

  void _closeContextPanel() {
    if (!_contextPanelOpen) {
      return;
    }
    setState(() {
      _contextPanelOpen = false;
    });
  }

  void _openContextPanel() {
    if (_contextPanelOpen) {
      return;
    }
    setState(() {
      _contextPanelOpen = true;
    });
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(sessionControllerProvider);
    final activeConversation = ref.watch(activeConversationProvider);
    final openedUnreadSnapshots = ref.watch(
      openedConversationUnreadSnapshotProvider,
    );
    final rawConversations = ref
        .watch(conversationsProvider)
        .maybeWhen(
          data: (value) => value,
          orElse: () => const <ConversationSummary>[],
        )
        .map((conversation) {
          final snapshotUnread = openedUnreadSnapshots[conversation.id];
          if (snapshotUnread == null ||
              snapshotUnread <= conversation.unreadCount) {
            return conversation;
          }
          return conversation.copyWith(unreadCount: snapshotUnread);
        })
        .toList(growable: false);
    final conversations = filterConversationsBySearch(
      filterConversationsByPreset(rawConversations, _filter),
      _searchQuery,
    );
    if (activeConversation != null) {
      final activeIndex = conversations.indexWhere(
        (conversation) => conversation.id == activeConversation.id,
      );
      if (activeIndex != -1) {
        final activeItem = conversations.removeAt(activeIndex);
        if (conversationPriorityBucket(activeItem) >= 3) {
          var insertIndex = conversations.indexWhere(
            (conversation) => conversationPriorityBucket(conversation) >= 3,
          );
          if (insertIndex == -1) {
            insertIndex = conversations.length;
          }
          conversations.insert(insertIndex, activeItem);
        } else {
          conversations.insert(activeIndex, activeItem);
        }
      }
    }
    final messages = ref
        .watch(activeMessagesProvider)
        .maybeWhen(data: (value) => value, orElse: () => const <ChatMessage>[]);
    final devices = ref
        .watch(devicesProvider)
        .maybeWhen(
          data: (value) => value,
          orElse: () => const <DeviceTrustInfo>[],
        );
    final contacts = ref
        .watch(contactsProvider)
        .maybeWhen(
          data: (value) => value,
          orElse: () => const <ContactProfile>[],
        );
    final attachments = messages
        .where((message) => message.attachment != null)
        .map((message) => message.attachment!)
        .toList(growable: false);
    final isFileConversationActive = isFileConversation(activeConversation);
    final fileAssistantId = _firstConversationIdOfKind(
      rawConversations,
      ConversationKind.fileAssistant,
    );

    return CallbackShortcuts(
      bindings: <ShortcutActivator, VoidCallback>{
        const SingleActivator(LogicalKeyboardKey.keyK, control: true):
            _focusSearch,
      },
      child: Focus(
        autofocus: true,
        child: Scaffold(
          body: DecoratedBox(
            decoration: BoxDecoration(
              color: Theme.of(context).scaffoldBackgroundColor,
            ),
            child: SafeArea(
              child: Padding(
                padding: const EdgeInsets.fromLTRB(
                  AppTokens.spacingSm,
                  AppTokens.spacingXs + 2,
                  0,
                  AppTokens.spacingXs + 2,
                ),
                child: LayoutBuilder(
                  builder: (context, constraints) {
                    final canShowAuxiliaryPanel = constraints.maxWidth >= 1200;
                    final showAuxiliaryPanel =
                        canShowAuxiliaryPanel && _contextPanelOpen;
                    final leftRailWidth = constraints.maxWidth >= 1440
                        ? ImUiMetrics.desktopLeftRailWide
                        : ImUiMetrics.desktopLeftRailCompact;
                    final rightRailWidth = isFileConversationActive
                        ? ImUiMetrics.desktopFilePanel
                        : ImUiMetrics.desktopRightPanel;
                    return _DesktopWorkbenchFrame(
                      child: Row(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: <Widget>[
                          SizedBox(
                            width: leftRailWidth,
                            child: _DesktopConversationRail(
                              displayName: session.profile?.displayName ?? '消息',
                              sessionSubtitle:
                                  session.profile?.subtitle ?? '消息、群聊与文件同步',
                              totalUnreadCount: rawConversations.fold<int>(
                                0,
                                (sum, conversation) =>
                                    sum + conversation.unreadCount,
                              ),
                              conversations: conversations,
                              activeConversation: activeConversation,
                              filter: _filter,
                              searchController: _searchController,
                              searchQuery: _searchQuery,
                              searchFocusNode: _searchFocusNode,
                              onFilterChanged: (filter) =>
                                  setState(() => _filter = filter),
                              onClearSearch: _clearSearch,
                              onSelectConversation: (conversationId) => ref
                                  .read(chatActionsProvider)
                                  .selectConversation(conversationId),
                            ),
                          ),
                          const _DesktopWorkbenchDivider(),
                          Expanded(
                            child: _DesktopConversationStage(
                              activeConversation: activeConversation,
                              openingUnreadCount: activeConversation == null
                                  ? 0
                                  : openedUnreadSnapshots[activeConversation
                                            .id] ??
                                        activeConversation.unreadCount,
                              messages: messages,
                              attachments: attachments,
                              contextPanelVisible: showAuxiliaryPanel,
                              canToggleContextPanel: canShowAuxiliaryPanel,
                              onOpenContextPanel: _openContextPanel,
                              onFocusConversationSearch: _focusSearch,
                              onCreateGroup: () async {
                                final conversationId = await ref
                                    .read(chatActionsProvider)
                                    .createGroup();
                                if (conversationId == null && context.mounted) {
                                  _showDesktopStatus(context, '暂时无法创建群聊');
                                }
                              },
                              onAddContact: () => _showDesktopAddFriendDialog(
                                context,
                                onSubmit: (request) async {
                                  await ref
                                      .read(chatActionsProvider)
                                      .sendFriendRequest(
                                        accountId: request.accountId,
                                        remark: request.remark,
                                      );
                                  if (context.mounted) {
                                    _showDesktopStatus(context, '好友请求已发送');
                                  }
                                },
                              ),
                              onOpenFileAssistant: fileAssistantId == null
                                  ? null
                                  : () => ref
                                        .read(chatActionsProvider)
                                        .selectConversation(fileAssistantId),
                              onSendFile: activeConversation == null
                                  ? null
                                  : (path) => ref
                                        .read(chatActionsProvider)
                                        .sendConversationFile(
                                          conversationId: activeConversation.id,
                                          path: path,
                                        ),
                              onClearHistory: activeConversation == null
                                  ? null
                                  : () => ref
                                        .read(chatActionsProvider)
                                        .clearConversationHistory(
                                          conversationId: activeConversation.id,
                                        ),
                              onSend: (text) => ref
                                  .read(chatActionsProvider)
                                  .sendCurrentConversationMessage(text),
                            ),
                          ),
                          if (showAuxiliaryPanel) ...<Widget>[
                            const _DesktopWorkbenchDivider(),
                            SizedBox(
                              width: rightRailWidth,
                              child: isFileConversationActive
                                  ? _DesktopFileDetailPanel(
                                      attachments: attachments,
                                      totalMessages: messages.length,
                                      onClose: _closeContextPanel,
                                    )
                                  : _DesktopConversationContextPanel(
                                      activeConversation: activeConversation,
                                      messages: messages,
                                      attachments: attachments,
                                      contacts: contacts,
                                      devices: devices,
                                      onClose: _closeContextPanel,
                                    ),
                            ),
                          ],
                        ],
                      ),
                    );
                  },
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _DesktopWorkbenchFrame extends StatelessWidget {
  const _DesktopWorkbenchFrame({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final radius = BorderRadius.circular(10);
    return ClipRRect(
      key: const ValueKey('desktop-workbench-frame'),
      borderRadius: radius,
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: theme.colorScheme.surface.withValues(alpha: 0.992),
          borderRadius: radius,
          border: Border.all(color: palette.divider.withValues(alpha: 0.12)),
        ),
        child: child,
      ),
    );
  }
}

class _DesktopWorkbenchDivider extends StatelessWidget {
  const _DesktopWorkbenchDivider();

  @override
  Widget build(BuildContext context) {
    final palette = Theme.of(context).extension<AppShellColors>()!;
    return SizedBox(
      width: 1,
      child: DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: <Color>[
              palette.divider.withValues(alpha: 0.08),
              palette.divider.withValues(alpha: 0.18),
              palette.divider.withValues(alpha: 0.1),
            ],
          ),
        ),
      ),
    );
  }
}

class _DesktopSectionDivider extends StatelessWidget {
  const _DesktopSectionDivider();

  @override
  Widget build(BuildContext context) {
    final palette = Theme.of(context).extension<AppShellColors>()!;
    return Divider(
      height: 1,
      thickness: 1,
      color: palette.divider.withValues(alpha: 0.1),
    );
  }
}

enum _DesktopWorkbenchTone { rail, stage, detail }

class _DesktopWorkbenchSection extends StatelessWidget {
  const _DesktopWorkbenchSection({required this.child, required this.tone});

  final Widget child;
  final _DesktopWorkbenchTone tone;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final material = theme.extension<AppShellMaterial>()!;
    final palette = theme.extension<AppShellColors>()!;
    final Color nonGlassColor;

    switch (tone) {
      case _DesktopWorkbenchTone.rail:
        nonGlassColor = palette.surfaceVariant.withValues(alpha: 0.024);
        break;
      case _DesktopWorkbenchTone.stage:
        nonGlassColor = theme.colorScheme.surface.withValues(alpha: 0.992);
        break;
      case _DesktopWorkbenchTone.detail:
        nonGlassColor = palette.surfaceVariant.withValues(alpha: 0.024);
        break;
    }

    final section = DecoratedBox(
      decoration: BoxDecoration(color: nonGlassColor),
      child: child,
    );
    if (!material.isGlass || tone == _DesktopWorkbenchTone.stage) {
      return section;
    }
    return LiquidGlassPanel(
      enabled: true,
      role: ShellChromeRole.desktopPanel,
      radius: 0,
      blurSigma: material.blurSigma * 0.46,
      shadowOpacity: 0.018,
      backgroundColor: material.sideBarColor,
      borderColor: palette.divider.withValues(alpha: 0.08),
      child: section,
    );
  }
}

class _DesktopRailSearchBar extends StatelessWidget {
  const _DesktopRailSearchBar({
    required this.controller,
    required this.focusNode,
    required this.query,
    required this.onClear,
  });

  final TextEditingController controller;
  final FocusNode focusNode;
  final String query;
  final VoidCallback onClear;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return SizedBox(
      key: const ValueKey('desktop-inbox-search-field'),
      height: 31,
      child: Container(
        key: const ValueKey('desktop-inbox-search-surface'),
        padding: const EdgeInsets.symmetric(horizontal: 8),
        decoration: BoxDecoration(
          color: palette.surfaceVariant.withValues(alpha: 0.16),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: palette.divider.withValues(alpha: 0.14)),
        ),
        child: Row(
          children: <Widget>[
            AppIcon(
              AppSemanticIcon.search,
              size: 12.4,
              color: theme.textTheme.bodySmall?.color?.withValues(alpha: 0.8),
            ),
            const SizedBox(width: 6),
            Expanded(
              child: TextField(
                controller: controller,
                focusNode: focusNode,
                key: const ValueKey('desktop-inbox-search-input'),
                textInputAction: TextInputAction.search,
                enableSuggestions: false,
                enableIMEPersonalizedLearning: false,
                autocorrect: false,
                smartDashesType: SmartDashesType.disabled,
                smartQuotesType: SmartQuotesType.disabled,
                decoration: InputDecoration(
                  hintText: '搜索会话',
                  border: InputBorder.none,
                  isDense: true,
                  contentPadding: const EdgeInsets.symmetric(vertical: 5),
                  hintStyle: theme.textTheme.bodySmall?.copyWith(
                    fontSize: 12.1,
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: 0.82,
                    ),
                    height: 1.0,
                  ),
                ),
                style: theme.textTheme.bodySmall?.copyWith(
                  fontSize: 12.1,
                  color: theme.colorScheme.onSurface,
                  height: 1.0,
                ),
              ),
            ),
            if (query.trim().isEmpty)
              Container(
                key: const ValueKey('desktop-inbox-search-shortcut'),
                padding: const EdgeInsets.symmetric(
                  horizontal: 4.5,
                  vertical: 1.25,
                ),
                decoration: BoxDecoration(
                  color: palette.surfaceVariant.withValues(alpha: 0.22),
                  borderRadius: BorderRadius.circular(6),
                ),
                child: Text(
                  'Ctrl K',
                  style: theme.textTheme.labelSmall?.copyWith(
                    fontSize: 8.8,
                    color: theme.textTheme.labelSmall?.color?.withValues(
                      alpha: 0.5,
                    ),
                    fontWeight: FontWeight.w600,
                    height: 1.0,
                  ),
                ),
              )
            else
              IconButton(
                key: const ValueKey('desktop-inbox-search-clear'),
                onPressed: onClear,
                icon: AppIcon(
                  AppSemanticIcon.close,
                  size: 11,
                  color: theme.textTheme.bodySmall?.color?.withValues(
                    alpha: 0.64,
                  ),
                ),
                constraints: const BoxConstraints.tightFor(
                  width: 24,
                  height: 24,
                ),
                padding: EdgeInsets.zero,
                style: IconButton.styleFrom(
                  tapTargetSize: MaterialTapTargetSize.shrinkWrap,
                ),
              ),
          ],
        ),
      ),
    );
  }
}

class _DesktopInboxProfileHeader extends StatelessWidget {
  const _DesktopInboxProfileHeader({
    required this.displayName,
    required this.summary,
  });

  final String displayName;
  final String summary;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.center,
      children: <Widget>[
        Container(
          width: 30,
          height: 30,
          decoration: BoxDecoration(
            color: theme.colorScheme.primary.withValues(alpha: 0.09),
            shape: BoxShape.circle,
            border: Border.all(
              color: theme.colorScheme.primary.withValues(alpha: 0.05),
            ),
          ),
          alignment: Alignment.center,
          child: Text(
            displayName.characters.first.toUpperCase(),
            style: theme.textTheme.titleSmall?.copyWith(
              fontWeight: FontWeight.w700,
              fontSize: 12.2,
            ),
          ),
        ),
        const SizedBox(width: AppTokens.spacingSm + 1),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                displayName,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: theme.textTheme.titleSmall?.copyWith(
                  fontWeight: FontWeight.w600,
                  fontSize: 13.4,
                ),
              ),
              const SizedBox(height: 2),
              Row(
                children: <Widget>[
                  Container(
                    width: 5,
                    height: 5,
                    decoration: BoxDecoration(
                      color: theme.colorScheme.primary.withValues(alpha: 0.68),
                      shape: BoxShape.circle,
                    ),
                  ),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      summary,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: theme.textTheme.bodySmall?.copyWith(
                        fontSize: 11.0,
                        color: theme.textTheme.bodySmall?.color?.withValues(
                          alpha: 0.74,
                        ),
                        height: 1.0,
                      ),
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
        Container(
          width: 6,
          height: 6,
          decoration: BoxDecoration(
            color: const Color(0xFF56C16A).withValues(alpha: 0.8),
            shape: BoxShape.circle,
            border: Border.all(
              color: palette.surfaceVariant.withValues(alpha: 0.12),
            ),
          ),
        ),
      ],
    );
  }
}

class _DesktopInboxConversationTile extends StatefulWidget {
  const _DesktopInboxConversationTile({
    required this.conversation,
    required this.selected,
    required this.onTap,
  });

  final ConversationSummary conversation;
  final bool selected;
  final VoidCallback onTap;

  @override
  State<_DesktopInboxConversationTile> createState() =>
      _DesktopInboxConversationTileState();
}

class _DesktopInboxConversationTileState
    extends State<_DesktopInboxConversationTile> {
  bool _hovered = false;

  void _setHovered(bool value) {
    if (_hovered == value) {
      return;
    }
    setState(() {
      _hovered = value;
    });
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final conversation = widget.conversation;
    final selected = widget.selected;
    final hovered = _hovered && !selected;
    final hasUnread = conversation.unreadCount > 0;
    final hasDraft = conversation.hasDraft;
    final isHighValue = isHighValueConversation(conversation);
    final rawPreview = hasDraft
        ? conversation.draftText!.trim()
        : conversation.isTyping
        ? '正在输入…'
        : conversation.preview;
    final previewPrefix = _desktopConversationPreviewPrefix(conversation);
    final preview = _desktopConversationPreviewText(conversation, rawPreview);
    final tileColor = selected
        ? theme.colorScheme.primary.withValues(alpha: 0.04)
        : hovered
        ? palette.surfaceVariant.withValues(alpha: 0.046)
        : hasDraft
        ? theme.colorScheme.error.withValues(alpha: 0.024)
        : hasUnread
        ? theme.colorScheme.surface.withValues(alpha: 0.044)
        : Colors.transparent;
    final tileBorderColor = selected
        ? theme.colorScheme.primary.withValues(alpha: 0.014)
        : hovered
        ? palette.divider.withValues(alpha: 0.07)
        : hasDraft
        ? theme.colorScheme.error.withValues(alpha: 0.034)
        : Colors.transparent;
    final titleStyle = theme.textTheme.titleMedium?.copyWith(
      fontWeight: hasUnread || selected || hasDraft
          ? FontWeight.w700
          : FontWeight.w600,
      fontSize: 14.1,
      letterSpacing: -0.16,
    );
    final timeStyle = theme.textTheme.labelSmall?.copyWith(
      fontSize: 9.1,
      color: theme.textTheme.bodySmall?.color?.withValues(
        alpha: selected ? 0.74 : 0.66,
      ),
      fontWeight: hasUnread ? FontWeight.w700 : FontWeight.w500,
      height: 1.0,
    );
    final tileShadow = selected
        ? <BoxShadow>[
            BoxShadow(
              color: theme.colorScheme.primary.withValues(alpha: 0.004),
              blurRadius: 1.5,
              offset: const Offset(0, 1),
            ),
          ]
        : const <BoxShadow>[];
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => _setHovered(true),
      onExit: (_) => _setHovered(false),
      child: InkWell(
        borderRadius: BorderRadius.circular(10),
        onTap: widget.onTap,
        hoverColor: Colors.transparent,
        splashColor: Colors.transparent,
        highlightColor: Colors.transparent,
        child: AnimatedContainer(
          key: ValueKey<String>(
            selected
                ? 'desktop-inbox-tile-selected-${conversation.id}'
                : 'desktop-inbox-tile-${conversation.id}',
          ),
          duration: const Duration(milliseconds: 180),
          curve: Curves.easeOut,
          padding: const EdgeInsets.symmetric(horizontal: 7.5, vertical: 5.75),
          decoration: BoxDecoration(
            color: tileColor,
            borderRadius: BorderRadius.circular(10),
            border: Border.all(color: tileBorderColor),
            boxShadow: tileShadow,
          ),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              AnimatedContainer(
                duration: const Duration(milliseconds: 180),
                curve: Curves.easeOut,
                width: 2.5,
                height: 38,
                decoration: BoxDecoration(
                  color: selected
                      ? theme.colorScheme.primary.withValues(alpha: 0.46)
                      : Colors.transparent,
                  borderRadius: BorderRadius.circular(999),
                ),
              ),
              const SizedBox(width: 6.5),
              SocialAvatar.conversation(
                conversation,
                key: ValueKey<String>(
                  'desktop-inbox-avatar-${conversation.id}',
                ),
                size: 35,
                selected: selected,
              ),
              const SizedBox(width: 7),
              Expanded(
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: <Widget>[
                          Row(
                            children: <Widget>[
                              Expanded(
                                child: Text(
                                  conversation.title,
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                  style: titleStyle,
                                ),
                              ),
                              if (conversation.isPinned) ...<Widget>[
                                const SizedBox(width: 2),
                                AppIcon(
                                  AppSemanticIcon.pinned,
                                  key: ValueKey<String>(
                                    'desktop-inbox-pin-${conversation.id}',
                                  ),
                                  size: 9.5,
                                  color: theme.textTheme.bodySmall?.color
                                      ?.withValues(
                                        alpha: hovered ? 0.54 : 0.46,
                                      ),
                                ),
                              ],
                            ],
                          ),
                          const SizedBox(height: 2.75),
                          Text.rich(
                            TextSpan(
                              style: theme.textTheme.bodySmall?.copyWith(
                                fontSize: 11.4,
                                color: conversation.hasDraft
                                    ? theme.colorScheme.error
                                    : hasUnread
                                    ? theme.colorScheme.onSurface.withValues(
                                        alpha: hovered ? 0.96 : 0.92,
                                      )
                                    : theme.textTheme.bodySmall?.color
                                          ?.withValues(
                                            alpha: hovered ? 0.84 : 0.78,
                                          ),
                                fontWeight: conversation.hasDraft || hasUnread
                                    ? FontWeight.w500
                                    : FontWeight.w400,
                                height: 1.12,
                              ),
                              children: <InlineSpan>[
                                if (previewPrefix case final prefix?)
                                  TextSpan(
                                    text: '$prefix ',
                                    style: theme.textTheme.bodySmall?.copyWith(
                                      fontSize: 11.0,
                                      color: conversation.hasDraft
                                          ? theme.colorScheme.error
                                          : theme.textTheme.bodySmall?.color
                                                ?.withValues(
                                                  alpha: hovered ? 0.68 : 0.62,
                                                ),
                                      fontWeight: FontWeight.w600,
                                    ),
                                  ),
                                TextSpan(text: preview),
                              ],
                            ),
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(width: 6),
                    SizedBox(
                      width: 35,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.end,
                        children: <Widget>[
                          Text(
                            _formatCompactConversationTime(
                              conversation.lastUpdated,
                            ),
                            style: hovered && !selected
                                ? timeStyle?.copyWith(
                                    color: theme.textTheme.bodySmall?.color
                                        ?.withValues(alpha: 0.64),
                                  )
                                : timeStyle,
                          ),
                          const SizedBox(height: 2),
                          _DesktopInboxConversationBadge(
                            conversation: conversation,
                            selected: selected,
                            hasUnread: hasUnread,
                            hasDraft: hasDraft,
                            isHighValue: isHighValue,
                          ),
                        ],
                      ),
                    ),
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

class _DesktopInboxConversationBadge extends StatelessWidget {
  const _DesktopInboxConversationBadge({
    required this.conversation,
    required this.selected,
    required this.hasUnread,
    required this.hasDraft,
    required this.isHighValue,
  });

  final ConversationSummary conversation;
  final bool selected;
  final bool hasUnread;
  final bool hasDraft;
  final bool isHighValue;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    if (conversation.mentionCount > 0 || conversation.pillLabel == '@你') {
      return Container(
        constraints: const BoxConstraints(minWidth: 12),
        padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 1),
        decoration: BoxDecoration(
          color: theme.colorScheme.secondary.withValues(alpha: 0.05),
          borderRadius: BorderRadius.circular(999),
        ),
        alignment: Alignment.center,
        child: Text(
          '@',
          style: theme.textTheme.labelSmall?.copyWith(
            color: theme.colorScheme.secondary,
            fontWeight: FontWeight.w700,
            fontSize: 8.4,
            height: 1.0,
          ),
        ),
      );
    }

    if (hasUnread) {
      return Container(
        key: selected
            ? null
            : ValueKey<String>(
                'desktop-inbox-unread-accent-${conversation.id}',
              ),
        constraints: const BoxConstraints(minWidth: 14),
        padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 1.5),
        decoration: BoxDecoration(
          color: conversation.isMuted
              ? theme.colorScheme.onSurface.withValues(alpha: 0.08)
              : theme.colorScheme.primary.withValues(alpha: 0.8),
          borderRadius: BorderRadius.circular(999),
        ),
        alignment: Alignment.center,
        child: Text(
          conversation.unreadCount > 99 ? '99+' : '${conversation.unreadCount}',
          style: theme.textTheme.labelSmall?.copyWith(
            fontSize: 8.2,
            color: Colors.white,
            fontWeight: FontWeight.w700,
            height: 1.0,
          ),
        ),
      );
    }

    if (conversation.isPinned) {
      return Align(
        alignment: Alignment.centerRight,
        child: AppIcon(
          AppSemanticIcon.pinned,
          key: ValueKey<String>('desktop-inbox-pin-${conversation.id}'),
          size: 9,
          color: theme.textTheme.bodySmall?.color?.withValues(alpha: 0.44),
        ),
      );
    }

    if (conversation.isMuted) {
      return Align(
        alignment: Alignment.centerRight,
        child: AppIcon(
          AppSemanticIcon.muted,
          size: 9,
          color: theme.textTheme.bodySmall?.color?.withValues(alpha: 0.42),
        ),
      );
    }

    if (!conversation.isGroup && conversation.isOnline && !hasDraft) {
      return Align(
        alignment: Alignment.centerRight,
        child: Container(
          width: 6,
          height: 6,
          decoration: BoxDecoration(
            color: const Color(
              0xFF56C16A,
            ).withValues(alpha: selected || isHighValue ? 0.76 : 0.56),
            shape: BoxShape.circle,
          ),
        ),
      );
    }

    return const SizedBox(height: 12);
  }
}

class _DesktopConversationRail extends StatelessWidget {
  const _DesktopConversationRail({
    required this.displayName,
    required this.sessionSubtitle,
    required this.totalUnreadCount,
    required this.conversations,
    required this.activeConversation,
    required this.filter,
    required this.searchController,
    required this.searchQuery,
    required this.searchFocusNode,
    required this.onFilterChanged,
    required this.onClearSearch,
    required this.onSelectConversation,
  });

  final String displayName;
  final String sessionSubtitle;
  final int totalUnreadCount;
  final List<ConversationSummary> conversations;
  final ConversationSummary? activeConversation;
  final ConversationFilterPreset filter;
  final TextEditingController searchController;
  final String searchQuery;
  final FocusNode searchFocusNode;
  final ValueChanged<ConversationFilterPreset> onFilterChanged;
  final VoidCallback onClearSearch;
  final ValueChanged<String> onSelectConversation;

  @override
  Widget build(BuildContext context) {
    return _DesktopWorkbenchSection(
      tone: _DesktopWorkbenchTone.rail,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Padding(
            padding: const EdgeInsets.fromLTRB(
              AppTokens.spacingSm + 1,
              AppTokens.spacingSm + 1,
              AppTokens.spacingSm + 1,
              AppTokens.spacingSm - 1,
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                _DesktopInboxProfileHeader(
                  displayName: displayName,
                  summary: _desktopInboxSummary(
                    sessionSubtitle: sessionSubtitle,
                    unreadCount: totalUnreadCount,
                  ),
                ),
                const SizedBox(height: AppTokens.spacingXs + 1),
                _DesktopRailSearchBar(
                  controller: searchController,
                  focusNode: searchFocusNode,
                  query: searchQuery,
                  onClear: onClearSearch,
                ),
                const SizedBox(height: AppTokens.spacingXs + 1),
                _DesktopFolderStrip(value: filter, onChanged: onFilterChanged),
              ],
            ),
          ),
          const _DesktopSectionDivider(),
          Expanded(
            child: conversations.isEmpty
                ? _DesktopInboxEmptyState(searchQuery: searchQuery)
                : ListView.separated(
                    padding: const EdgeInsets.fromLTRB(
                      AppTokens.spacingSm - 1,
                      AppTokens.spacingSm - 2,
                      AppTokens.spacingSm - 1,
                      AppTokens.spacingSm,
                    ),
                    itemCount: conversations.length,
                    separatorBuilder: (_, index) => const SizedBox(height: 3),
                    itemBuilder: (context, index) {
                      final conversation = conversations[index];
                      final selected =
                          activeConversation?.id == conversation.id;
                      return _DesktopInboxConversationTile(
                        conversation: conversation,
                        selected: selected,
                        onTap: () => onSelectConversation(conversation.id),
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

class _DesktopFolderStrip extends StatelessWidget {
  const _DesktopFolderStrip({required this.value, required this.onChanged});

  final ConversationFilterPreset value;
  final ValueChanged<ConversationFilterPreset> onChanged;

  @override
  Widget build(BuildContext context) {
    return ConversationFilterTabs(
      key: const ValueKey('desktop-folder-strip'),
      desktop: true,
      value: value,
      presets: const <ConversationFilterPreset>[
        ConversationFilterPreset.all,
        ConversationFilterPreset.unread,
        ConversationFilterPreset.groups,
        ConversationFilterPreset.channels,
        ConversationFilterPreset.files,
      ],
      onChanged: onChanged,
    );
  }
}

class _DesktopInboxEmptyState extends StatelessWidget {
  const _DesktopInboxEmptyState({required this.searchQuery});

  final String searchQuery;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final query = searchQuery.trim();
    return Center(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 26),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            AppIcon(
              AppSemanticIcon.search,
              size: 28,
              color: palette.textTertiary.withValues(alpha: 0.70),
            ),
            const SizedBox(height: 11),
            Text(
              query.isEmpty ? '当前分类暂无会话' : '没有找到相关会话',
              key: const ValueKey('desktop-inbox-empty-title'),
              textAlign: TextAlign.center,
              style: theme.textTheme.titleSmall?.copyWith(
                fontWeight: FontWeight.w700,
              ),
            ),
            if (query.isNotEmpty) ...<Widget>[
              const SizedBox(height: 5),
              Text(
                query,
                key: const ValueKey('desktop-inbox-empty-query'),
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
                textAlign: TextAlign.center,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: palette.textSecondary,
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _DesktopConversationStage extends StatelessWidget {
  const _DesktopConversationStage({
    required this.activeConversation,
    required this.openingUnreadCount,
    required this.messages,
    required this.attachments,
    required this.contextPanelVisible,
    required this.canToggleContextPanel,
    required this.onOpenContextPanel,
    required this.onFocusConversationSearch,
    required this.onCreateGroup,
    required this.onAddContact,
    required this.onOpenFileAssistant,
    required this.onSendFile,
    required this.onClearHistory,
    required this.onSend,
  });

  final ConversationSummary? activeConversation;
  final int openingUnreadCount;
  final List<ChatMessage> messages;
  final List<ChatAttachment> attachments;
  final bool contextPanelVisible;
  final bool canToggleContextPanel;
  final VoidCallback onOpenContextPanel;
  final VoidCallback onFocusConversationSearch;
  final Future<void> Function() onCreateGroup;
  final VoidCallback onAddContact;
  final VoidCallback? onOpenFileAssistant;
  final ValueChanged<String>? onSendFile;
  final Future<void> Function()? onClearHistory;
  final ValueChanged<String> onSend;

  static const double _kStageColumnInsetStart =
      ImUiMetrics.desktopStageInsetStart;
  static const double _kStageColumnInsetEnd = ImUiMetrics.desktopStageInsetEnd;
  static const double _kStageReadableWidth =
      ImUiMetrics.desktopStageReadableWidth;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final conversation = activeConversation;
    final title = conversation?.title ?? '消息';
    final subtitle = conversation == null
        ? ''
        : _conversationStageSubtitle(
            conversation: conversation,
            messages: messages,
            attachments: attachments,
          );
    final badgeLabel = conversation == null
        ? null
        : isFileConversation(conversation)
        ? '文件'
        : conversation.isGroup
        ? '群组'
        : '联系人';
    final detailLabel = conversation == null
        ? null
        : _conversationStageDetail(
            conversation: conversation,
            messages: messages,
            attachments: attachments,
          );
    final timelineMaxContentWidth = contextPanelVisible
        ? _kStageReadableWidth
        : double.infinity;
    return KeyedSubtree(
      key: const ValueKey('desktop-conversation-stage'),
      child: _DesktopWorkbenchSection(
        tone: _DesktopWorkbenchTone.stage,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Padding(
              padding: const EdgeInsets.fromLTRB(
                _kStageColumnInsetStart,
                AppTokens.spacingSm + 1,
                _kStageColumnInsetEnd,
                AppTokens.spacingSm,
              ),
              child: SizedBox(
                width: double.infinity,
                child: _DesktopStageHeader(
                  leading: conversation == null
                      ? null
                      : _DesktopStageIdentity(conversation: conversation),
                  title: title,
                  subtitle: subtitle,
                  badgeLabel: badgeLabel,
                  detailLabel: detailLabel,
                  action: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: <Widget>[
                      IconButton(
                        key: const ValueKey('desktop-stage-search-action'),
                        onPressed: conversation == null
                            ? null
                            : () => _showDesktopMessageSearch(
                                context,
                                conversation,
                                messages,
                              ),
                        icon: const AppIcon(AppSemanticIcon.search),
                        tooltip: '搜索消息',
                        iconSize: 13,
                        visualDensity: VisualDensity.compact,
                        padding: EdgeInsets.zero,
                        constraints: const BoxConstraints.tightFor(
                          width: 25,
                          height: 25,
                        ),
                        style: IconButton.styleFrom(
                          tapTargetSize: MaterialTapTargetSize.shrinkWrap,
                          minimumSize: const Size(25, 25),
                          maximumSize: const Size(25, 25),
                          backgroundColor: Colors.transparent,
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(7),
                          ),
                          padding: EdgeInsets.zero,
                        ),
                      ),
                      _DesktopStageMoreMenu(
                        enabled: canToggleContextPanel && conversation != null,
                        onOpenContextPanel: onOpenContextPanel,
                        onSearchMessages: conversation == null
                            ? null
                            : () => _showDesktopMessageSearch(
                                context,
                                conversation,
                                messages,
                              ),
                        onClearHistory:
                            conversation == null || onClearHistory == null
                            ? null
                            : () async {
                                final confirmed =
                                    await _confirmDesktopClearHistory(
                                      context,
                                      conversation.title,
                                    );
                                if (!context.mounted) {
                                  return;
                                }
                                if (confirmed) {
                                  await onClearHistory!();
                                }
                              },
                      ),
                    ],
                  ),
                ),
              ),
            ),
            const _DesktopSectionDivider(),
            Expanded(
              child: Padding(
                padding: EdgeInsets.zero,
                child: SizedBox.expand(
                  child: _DesktopStageCanvas(
                    child: conversation == null
                        ? _DesktopEmptyConversationState(
                            onFocusConversationSearch:
                                onFocusConversationSearch,
                            onCreateGroup: onCreateGroup,
                            onAddContact: onAddContact,
                            onOpenFileAssistant: onOpenFileAssistant,
                          )
                        : _DesktopChatTimeline(
                            conversation: conversation,
                            initialUnreadCount: openingUnreadCount,
                            messages: messages,
                            maxContentWidth: timelineMaxContentWidth,
                          ),
                  ),
                ),
              ),
            ),
            if (conversation != null)
              Container(
                padding: const EdgeInsets.fromLTRB(
                  _kStageColumnInsetStart,
                  AppTokens.spacingSm,
                  _kStageColumnInsetEnd,
                  AppTokens.spacingSm + 1,
                ),
                decoration: BoxDecoration(
                  color: theme.colorScheme.surface.withValues(alpha: 0.94),
                  border: Border(
                    top: BorderSide(
                      color: Theme.of(context)
                          .extension<AppShellColors>()!
                          .divider
                          .withValues(alpha: 0.12),
                    ),
                  ),
                ),
                child: _DesktopChatComposer(
                  enabled: true,
                  onSend: onSend,
                  onSendFile: onSendFile,
                ),
              ),
          ],
        ),
      ),
    );
  }
}

enum _DesktopStageMenuAction { profile, search, media, clear }

class _DesktopStageMoreMenu extends StatelessWidget {
  const _DesktopStageMoreMenu({
    required this.enabled,
    required this.onOpenContextPanel,
    required this.onSearchMessages,
    required this.onClearHistory,
  });

  final bool enabled;
  final VoidCallback onOpenContextPanel;
  final VoidCallback? onSearchMessages;
  final Future<void> Function()? onClearHistory;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final iconColor = enabled
        ? theme.colorScheme.onSurface.withValues(alpha: 0.78)
        : theme.disabledColor.withValues(alpha: 0.5);
    return PopupMenuButton<_DesktopStageMenuAction>(
      key: const ValueKey('desktop-stage-more-action'),
      enabled: enabled,
      tooltip: '更多',
      padding: EdgeInsets.zero,
      position: PopupMenuPosition.under,
      constraints: const BoxConstraints(minWidth: 172),
      onSelected: (value) {
        switch (value) {
          case _DesktopStageMenuAction.profile:
          case _DesktopStageMenuAction.media:
            onOpenContextPanel();
            return;
          case _DesktopStageMenuAction.search:
            onSearchMessages?.call();
            return;
          case _DesktopStageMenuAction.clear:
            onClearHistory?.call();
            return;
        }
      },
      itemBuilder: (context) => <PopupMenuEntry<_DesktopStageMenuAction>>[
        const PopupMenuItem<_DesktopStageMenuAction>(
          key: ValueKey('desktop-stage-menu-profile'),
          value: _DesktopStageMenuAction.profile,
          child: Text('会话资料'),
        ),
        if (onSearchMessages != null)
          const PopupMenuItem<_DesktopStageMenuAction>(
            key: ValueKey('desktop-stage-menu-search'),
            value: _DesktopStageMenuAction.search,
            child: Text('搜索消息'),
          ),
        const PopupMenuItem<_DesktopStageMenuAction>(
          key: ValueKey('desktop-stage-menu-media'),
          value: _DesktopStageMenuAction.media,
          child: Text('媒体与文件'),
        ),
        if (onClearHistory != null)
          const PopupMenuItem<_DesktopStageMenuAction>(
            key: ValueKey('desktop-stage-menu-clear'),
            value: _DesktopStageMenuAction.clear,
            child: Text('清空记录'),
          ),
      ],
      child: SizedBox(
        width: 25,
        height: 25,
        child: Center(
          child: AppIcon(AppSemanticIcon.more, size: 13, color: iconColor),
        ),
      ),
    );
  }
}

class _DesktopConversationBackdrop extends StatelessWidget {
  const _DesktopConversationBackdrop();

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return IgnorePointer(
      child: LayoutBuilder(
        builder: (context, constraints) {
          final stageBandWidth = constraints.maxWidth;
          return Stack(
            fit: StackFit.expand,
            children: <Widget>[
              Align(
                alignment: Alignment.center,
                child: Container(
                  key: const ValueKey('desktop-stage-backdrop-band'),
                  width: stageBandWidth,
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.topLeft,
                      end: Alignment.bottomRight,
                      colors: <Color>[
                        Colors.white.withValues(alpha: 0.012),
                        palette.surfaceVariant.withValues(alpha: 0.018),
                        theme.colorScheme.primary.withValues(alpha: 0.008),
                      ],
                      stops: const <double>[0, 0.44, 1],
                    ),
                    borderRadius: BorderRadius.circular(24),
                  ),
                ),
              ),
              Positioned(
                top: 88,
                right: 34,
                child: Container(
                  width: 360,
                  height: 260,
                  decoration: BoxDecoration(
                    gradient: RadialGradient(
                      center: const Alignment(0.38, -0.62),
                      radius: 1.04,
                      colors: <Color>[
                        Colors.white.withValues(alpha: 0.010),
                        theme.colorScheme.primary.withValues(alpha: 0.006),
                        Colors.transparent,
                      ],
                      stops: const <double>[0, 0.36, 1],
                    ),
                  ),
                ),
              ),
              Positioned(
                left: 96,
                right: 96,
                bottom: 62,
                child: Container(
                  height: 1,
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      colors: <Color>[
                        Colors.transparent,
                        Colors.white.withValues(alpha: 0.014),
                        Colors.transparent,
                      ],
                    ),
                  ),
                ),
              ),
              Align(
                alignment: Alignment.topCenter,
                child: Container(
                  width: stageBandWidth - 72,
                  height: 228,
                  margin: const EdgeInsets.only(top: 6),
                  decoration: BoxDecoration(
                    gradient: RadialGradient(
                      center: const Alignment(0, -0.9),
                      radius: 1.08,
                      colors: <Color>[
                        Colors.white.withValues(alpha: 0.020),
                        theme.colorScheme.primary.withValues(alpha: 0.010),
                        Colors.transparent,
                      ],
                      stops: const <double>[0, 0.32, 1],
                    ),
                  ),
                ),
              ),
              Align(
                alignment: Alignment.centerLeft,
                child: Container(
                  width: 28,
                  margin: const EdgeInsets.symmetric(vertical: 28),
                  decoration: const BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.centerLeft,
                      end: Alignment.centerRight,
                      colors: <Color>[Colors.transparent, Colors.transparent],
                    ),
                  ),
                ),
              ),
              Align(
                alignment: Alignment.bottomCenter,
                child: Container(
                  width: stageBandWidth - 32,
                  height: 168,
                  margin: const EdgeInsets.only(bottom: 6),
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.bottomCenter,
                      end: Alignment.topCenter,
                      colors: <Color>[
                        theme.colorScheme.surface.withValues(alpha: 0.040),
                        Colors.transparent,
                      ],
                    ),
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}

class _DesktopStageCanvas extends StatelessWidget {
  const _DesktopStageCanvas({required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final radius = BorderRadius.circular(24);
    return Container(
      key: const ValueKey('desktop-stage-canvas'),
      decoration: BoxDecoration(
        borderRadius: radius,
        gradient: LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: <Color>[
            Colors.white.withValues(alpha: 0.014),
            theme.colorScheme.surface.withValues(alpha: 0.988),
            theme.colorScheme.surface.withValues(alpha: 0.984),
          ],
          stops: const <double>[0, 0.18, 1],
        ),
        border: Border.all(color: palette.divider.withValues(alpha: 0.034)),
        boxShadow: <BoxShadow>[
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.018),
            blurRadius: 20,
            offset: const Offset(0, 8),
          ),
        ],
      ),
      child: ClipRRect(
        borderRadius: radius,
        child: Stack(
          fit: StackFit.expand,
          children: <Widget>[
            DecoratedBox(
              decoration: BoxDecoration(
                color: theme.colorScheme.surface.withValues(alpha: 0.996),
              ),
            ),
            const _DesktopConversationBackdrop(),
            Positioned(
              top: 108,
              right: 0,
              bottom: 0,
              child: IgnorePointer(
                child: Container(
                  key: const ValueKey('desktop-stage-right-edge-mask'),
                  width: 3,
                  decoration: BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                      colors: <Color>[
                        theme.colorScheme.surface.withValues(alpha: 0),
                        theme.colorScheme.surface.withValues(alpha: 0.98),
                        theme.colorScheme.surface.withValues(alpha: 1),
                      ],
                      stops: const <double>[0, 0.08, 1],
                    ),
                  ),
                ),
              ),
            ),
            child,
          ],
        ),
      ),
    );
  }
}

enum _DesktopContextTab { profile, members, media, files, links }

class _DesktopConversationContextPanel extends StatefulWidget {
  const _DesktopConversationContextPanel({
    required this.activeConversation,
    required this.messages,
    required this.attachments,
    required this.contacts,
    required this.devices,
    required this.onClose,
  });

  final ConversationSummary? activeConversation;
  final List<ChatMessage> messages;
  final List<ChatAttachment> attachments;
  final List<ContactProfile> contacts;
  final List<DeviceTrustInfo> devices;
  final VoidCallback onClose;

  @override
  State<_DesktopConversationContextPanel> createState() =>
      _DesktopConversationContextPanelState();
}

class _DesktopConversationContextPanelState
    extends State<_DesktopConversationContextPanel> {
  _DesktopContextTab _activeTab = _DesktopContextTab.profile;

  @override
  Widget build(BuildContext context) {
    final conversation = widget.activeConversation;
    if (conversation == null) {
      return _DesktopWorkbenchSection(
        tone: _DesktopWorkbenchTone.detail,
        child: _DesktopEmptyContextState(onClose: widget.onClose),
      );
    }

    final trailingLabel = conversation.isGroup
        ? conversation.memberCount > 0
              ? '${conversation.memberCount} 人'
              : '群成员'
        : conversation.isOnline
        ? '在线'
        : '离线';
    final visualAttachments = widget.attachments
        .where(_isDesktopVisualAttachment)
        .toList(growable: false);
    final fileAttachments = widget.attachments
        .where((attachment) => !_isDesktopVisualAttachment(attachment))
        .toList(growable: false);
    final links = _desktopLinksFromMessages(widget.messages);

    return _DesktopWorkbenchSection(
      tone: _DesktopWorkbenchTone.detail,
      child: Column(
        key: const ValueKey('desktop-conversation-sidebar'),
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Padding(
            padding: const EdgeInsets.fromLTRB(
              AppTokens.spacingSm - 1,
              AppTokens.spacingXs - 1,
              AppTokens.spacingSm - 1,
              AppTokens.spacingXs - 1,
            ),
            child: _DesktopContextHeader(
              title: conversation.title,
              subtitle: conversation.isGroup ? '群组资料' : '联系人资料',
              trailing: trailingLabel,
              onClose: widget.onClose,
            ),
          ),
          const _DesktopSectionDivider(),
          Expanded(
            child: ListView(
              padding: const EdgeInsets.fromLTRB(
                AppTokens.spacingXs - 1,
                AppTokens.spacingXs - 1,
                AppTokens.spacingXs - 1,
                AppTokens.spacingXs + 1,
              ),
              children: <Widget>[
                _DesktopContextProfileHero(conversation: conversation),
                const SizedBox(height: AppTokens.spacingXs + 1),
                _DesktopContextTabs(
                  activeTab: _activeTab,
                  onChanged: (tab) => setState(() => _activeTab = tab),
                ),
                const SizedBox(height: AppTokens.spacingXs + 1),
                ..._contextTabContent(
                  conversation: conversation,
                  trailingLabel: trailingLabel,
                  messages: widget.messages,
                  contacts: widget.contacts,
                  visualAttachments: visualAttachments,
                  fileAttachments: fileAttachments,
                  links: links,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  List<Widget> _contextTabContent({
    required ConversationSummary conversation,
    required String trailingLabel,
    required List<ChatMessage> messages,
    required List<ContactProfile> contacts,
    required List<ChatAttachment> visualAttachments,
    required List<ChatAttachment> fileAttachments,
    required List<_DesktopSharedLink> links,
  }) {
    return switch (_activeTab) {
      _DesktopContextTab.profile => <Widget>[
        _DesktopContextOverviewCard(
          containerKey: const ValueKey('desktop-context-summary-card'),
          icon: conversation.isGroup
              ? AppSemanticIcon.sessions
              : AppSemanticIcon.account,
          title: conversation.title,
          contextLabel: conversation.isGroup ? '当前群组' : '当前联系人',
          primaryLine: _desktopSidebarPrimaryLine(
            conversation: conversation,
            messages: widget.messages,
            attachments: widget.attachments,
          ),
          secondaryLine: _desktopSidebarSecondaryLine(
            conversation: conversation,
          ),
          trailingLabel: conversation.isMuted ? '已静音' : trailingLabel,
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        _DesktopContextSectionHeader(
          title: '成员',
          trailing: conversation.isGroupConversation
              ? '${conversation.onlineCount} 在线'
              : trailingLabel,
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        _DesktopContextIdleRow(
          title: conversation.isGroupConversation ? '群成员' : '联系人状态',
          subtitle: conversation.isGroupConversation
              ? '查看成员、群公告和群资料'
              : '查看资料、共同群和最近在线',
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        _DesktopContextSectionHeader(
          title: '媒体',
          trailing: '${visualAttachments.length} 项',
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        if (visualAttachments.isNotEmpty) ...<Widget>[
          _DesktopContextMediaGrid(attachments: visualAttachments),
          const SizedBox(height: AppTokens.spacingXs + 1),
        ],
        _DesktopContextSectionHeader(
          title: '文件',
          trailing: '${fileAttachments.length}',
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        _DesktopContextIdleRow(
          title: fileAttachments.isEmpty ? '最近文件' : fileAttachments.first.title,
          subtitle: fileAttachments.isEmpty
              ? '共享文档和语音文件会集中在这里'
              : fileAttachments.first.detail,
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        _DesktopContextSectionHeader(title: '链接', trailing: '${links.length}'),
        const SizedBox(height: AppTokens.spacingXs + 1),
        _DesktopContextIdleRow(
          title: links.isEmpty ? '共享链接' : links.first.url,
          subtitle: links.isEmpty
              ? '聊天中的链接会自动归档'
              : '${links.first.senderLabel} · ${_formatClock(links.first.timestamp)}',
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        const _DesktopContextSectionHeader(title: '通知设置', trailing: '默认'),
        const SizedBox(height: AppTokens.spacingXs + 1),
        _DesktopContextIdleRow(
          title: conversation.isMuted ? '已静音' : '接收提醒',
          subtitle: conversation.isMuted ? '当前会话不会打扰你' : '新消息、@我和文件完成时提醒',
        ),
      ],
      _DesktopContextTab.members => <Widget>[
        _DesktopContextSectionHeader(
          title: '成员',
          trailing: conversation.isGroupConversation
              ? '${conversation.onlineCount} 在线'
              : trailingLabel,
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        _DesktopContextMemberList(
          members: _desktopContextMembers(
            conversation: conversation,
            messages: messages,
            contacts: contacts,
          ),
        ),
      ],
      _DesktopContextTab.media => <Widget>[
        _DesktopContextSectionHeader(
          title: '媒体',
          trailing: '${visualAttachments.length} 项',
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        if (visualAttachments.isEmpty)
          const _DesktopContextEmptyHint(text: '暂无共享媒体。')
        else
          _DesktopContextMediaGrid(attachments: visualAttachments),
      ],
      _DesktopContextTab.files => <Widget>[
        _DesktopContextSectionHeader(
          title: '文件',
          trailing: '${fileAttachments.length}',
        ),
        const SizedBox(height: AppTokens.spacingXs + 1),
        if (fileAttachments.isEmpty)
          const _DesktopContextEmptyHint(text: '暂无共享文件。')
        else
          for (var index = 0; index < fileAttachments.length; index++) ...[
            _DesktopAttachmentCard(
              containerKey: ValueKey('desktop-context-file-row-$index'),
              attachment: fileAttachments[index],
            ),
            const SizedBox(height: AppTokens.spacingXs + 1),
          ],
      ],
      _DesktopContextTab.links => <Widget>[
        _DesktopContextSectionHeader(title: '链接', trailing: '${links.length}'),
        const SizedBox(height: AppTokens.spacingXs + 1),
        if (links.isEmpty)
          const _DesktopContextEmptyHint(text: '暂无共享链接。')
        else
          for (final link in links.take(8)) ...[
            _DesktopContextIdleRow(
              title: link.url,
              subtitle: '${link.senderLabel} · ${_formatClock(link.timestamp)}',
            ),
            const SizedBox(height: AppTokens.spacingXs + 1),
          ],
      ],
    };
  }
}

class _DesktopContextProfileHero extends StatelessWidget {
  const _DesktopContextProfileHero({required this.conversation});

  final ConversationSummary conversation;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final subtitle = conversation.isGroupConversation
        ? '${conversation.memberCount} 位成员 · ${conversation.onlineCount} 在线'
        : conversation.presenceLabel ??
              (conversation.isOnline ? '在线' : '最近有消息');
    return Container(
      key: const ValueKey('desktop-context-profile-hero'),
      padding: const EdgeInsets.fromLTRB(8, 10, 8, 10),
      decoration: BoxDecoration(
        color: palette.surfaceVariant.withValues(alpha: 0.038),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: palette.divider.withValues(alpha: 0.08)),
      ),
      child: Row(
        children: <Widget>[
          SocialAvatar.conversation(conversation, size: 62),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  conversation.title,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w700,
                    fontSize: 15.2,
                  ),
                ),
                const SizedBox(height: 3),
                Text(
                  subtitle,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: palette.textSecondary.withValues(alpha: 0.82),
                    fontSize: 11.2,
                  ),
                ),
                if (conversation.membersPreview?.trim().isNotEmpty ==
                    true) ...<Widget>[
                  const SizedBox(height: 3),
                  Text(
                    conversation.membersPreview!.trim(),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: palette.textTertiary,
                      fontSize: 10.6,
                    ),
                  ),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _DesktopContextTabs extends StatelessWidget {
  const _DesktopContextTabs({required this.activeTab, required this.onChanged});

  final _DesktopContextTab activeTab;
  final ValueChanged<_DesktopContextTab> onChanged;

  static const tabs = <_DesktopContextTab, String>{
    _DesktopContextTab.profile: '资料',
    _DesktopContextTab.members: '成员',
    _DesktopContextTab.media: '媒体',
    _DesktopContextTab.files: '文件',
    _DesktopContextTab.links: '链接',
  };

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Row(
      key: const ValueKey('desktop-context-tabs'),
      children: <Widget>[
        for (final entry in tabs.entries) ...<Widget>[
          Expanded(
            child: InkWell(
              key: ValueKey('desktop-context-tab-${entry.value}'),
              borderRadius: BorderRadius.circular(7),
              onTap: () => onChanged(entry.key),
              child: Container(
                height: 25,
                alignment: Alignment.center,
                decoration: BoxDecoration(
                  color: entry.key == activeTab
                      ? theme.colorScheme.primary.withValues(alpha: 0.08)
                      : palette.surfaceVariant.withValues(alpha: 0.05),
                  borderRadius: BorderRadius.circular(7),
                ),
                child: Text(
                  entry.value,
                  style: theme.textTheme.labelSmall?.copyWith(
                    fontWeight: FontWeight.w700,
                    color: entry.key == activeTab
                        ? theme.colorScheme.primary
                        : palette.textSecondary,
                  ),
                ),
              ),
            ),
          ),
          if (entry.key != tabs.keys.last) const SizedBox(width: 4),
        ],
      ],
    );
  }
}

class _DesktopContextMember {
  const _DesktopContextMember({
    required this.id,
    required this.name,
    required this.subtitle,
    required this.online,
    required this.kind,
    this.avatarSeed,
    this.initials,
    this.trailing,
  });

  final String id;
  final String name;
  final String subtitle;
  final bool online;
  final ConversationKind kind;
  final String? avatarSeed;
  final String? initials;
  final String? trailing;
}

class _DesktopContextMemberList extends StatelessWidget {
  const _DesktopContextMemberList({required this.members});

  final List<_DesktopContextMember> members;

  @override
  Widget build(BuildContext context) {
    if (members.isEmpty) {
      return const _DesktopContextEmptyHint(text: '暂无成员信息。');
    }
    return Column(
      key: const ValueKey('desktop-context-member-list'),
      children: <Widget>[
        for (var index = 0; index < members.length; index++) ...<Widget>[
          _DesktopContextMemberTile(
            key: ValueKey('desktop-context-member-row-$index'),
            member: members[index],
          ),
          if (index != members.length - 1)
            const SizedBox(height: AppTokens.spacingXs + 1),
        ],
      ],
    );
  }
}

class _DesktopContextMemberTile extends StatelessWidget {
  const _DesktopContextMemberTile({super.key, required this.member});

  final _DesktopContextMember member;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 8),
      decoration: BoxDecoration(
        color: palette.surfaceVariant.withValues(alpha: 0.045),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: palette.divider.withValues(alpha: 0.055)),
      ),
      child: Row(
        children: <Widget>[
          SocialAvatar(
            id: member.id,
            title: member.name,
            kind: member.kind,
            size: 32,
            avatarSeed: member.avatarSeed,
            initials: member.initials,
            online: member.online,
          ),
          const SizedBox(width: 9),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  member.name,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodyMedium?.copyWith(
                    fontWeight: FontWeight.w700,
                    fontSize: 12.8,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  member.subtitle,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: palette.textSecondary,
                    fontSize: 10.8,
                  ),
                ),
              ],
            ),
          ),
          if (member.trailing != null) ...<Widget>[
            const SizedBox(width: 8),
            Text(
              member.trailing!,
              style: theme.textTheme.labelSmall?.copyWith(
                color: member.online
                    ? theme.colorScheme.primary
                    : palette.textTertiary,
                fontWeight: FontWeight.w700,
              ),
            ),
          ],
        ],
      ),
    );
  }
}

List<_DesktopContextMember> _desktopContextMembers({
  required ConversationSummary conversation,
  required List<ChatMessage> messages,
  required List<ContactProfile> contacts,
}) {
  final members = <String, _DesktopContextMember>{};

  void addMember({
    required String rawLabel,
    String? subtitle,
    ContactProfile? contact,
    DateTime? timestamp,
    bool self = false,
  }) {
    final label = rawLabel.trim();
    if (label.isEmpty || label == '系统') {
      return;
    }
    final matchedContact = contact ?? _contactForLabel(label, contacts);
    final name = self ? '你' : matchedContact?.displayName ?? label;
    final key = _desktopMemberLookupKey(name);
    if (members.containsKey(key)) {
      return;
    }
    final online = self || matchedContact?.online == true;
    members[key] = _DesktopContextMember(
      id: matchedContact?.id ?? key,
      name: name,
      subtitle:
          subtitle ??
          matchedContact?.statusLabel ??
          (timestamp == null ? '成员资料' : '${_formatClock(timestamp)} 发言'),
      online: online,
      kind: matchedContact?.groupLabel == '群聊'
          ? ConversationKind.group
          : ConversationKind.direct,
      avatarSeed: matchedContact?.avatarSeed,
      initials: name.characters.take(2).join(),
      trailing: self
          ? '我'
          : online
          ? '在线'
          : matchedContact?.lastSeenLabel,
    );
  }

  if (!conversation.isGroupConversation) {
    final contact = _contactForConversation(conversation, contacts);
    addMember(
      rawLabel: contact?.displayName ?? conversation.title,
      subtitle:
          contact?.statusLabel ??
          conversation.presenceLabel ??
          _trailingLabelForConversation(conversation),
      contact: contact,
    );
    return members.values.toList(growable: false);
  }

  for (final label in _memberLabelsFromPreview(conversation.membersPreview)) {
    addMember(rawLabel: label);
  }
  for (final message in messages.reversed) {
    addMember(
      rawLabel: message.senderLabel,
      timestamp: message.timestamp,
      self: message.direction == MessageDirection.outgoing,
    );
  }
  if (members.isEmpty) {
    addMember(
      rawLabel: conversation.title,
      subtitle: conversation.presenceLabel ?? '${conversation.memberCount} 位成员',
    );
  }
  return members.values.take(12).toList(growable: false);
}

String _trailingLabelForConversation(ConversationSummary conversation) {
  if (conversation.isOnline) {
    return '在线';
  }
  return conversation.presenceLabel ?? '离线';
}

Iterable<String> _memberLabelsFromPreview(String? preview) {
  final value = preview?.trim();
  if (value == null || value.isEmpty) {
    return const <String>[];
  }
  final withoutCount = value.replaceAll(RegExp(r'等\s*\d+\s*人'), '');
  return withoutCount
      .split(RegExp(r'[、,，;；]+'))
      .map((label) => label.trim())
      .where((label) => label.isNotEmpty);
}

ContactProfile? _contactForConversation(
  ConversationSummary conversation,
  List<ContactProfile> contacts,
) {
  final conversationKey = _desktopMemberLookupKey(conversation.id);
  final titleKey = _desktopMemberLookupKey(conversation.title);
  for (final contact in contacts) {
    final contactId = _desktopMemberLookupKey(contact.id);
    final displayName = _desktopMemberLookupKey(contact.displayName);
    final handle = _desktopMemberLookupKey(contact.handle);
    if (contactId == conversationKey ||
        displayName == titleKey ||
        handle == conversationKey) {
      return contact;
    }
  }
  return null;
}

ContactProfile? _contactForLabel(String label, List<ContactProfile> contacts) {
  final labelKey = _desktopMemberLookupKey(label);
  for (final contact in contacts) {
    final displayName = _desktopMemberLookupKey(contact.displayName);
    final id = _desktopMemberLookupKey(contact.id);
    final handle = _desktopMemberLookupKey(contact.handle);
    if (displayName == labelKey ||
        id == labelKey ||
        handle == labelKey ||
        displayName.contains(labelKey)) {
      return contact;
    }
  }
  return null;
}

String _desktopMemberLookupKey(String value) {
  return value.toLowerCase().replaceAll(RegExp(r'[^a-z0-9\u4e00-\u9fff]+'), '');
}

class _DesktopSharedLink {
  const _DesktopSharedLink({
    required this.url,
    required this.senderLabel,
    required this.timestamp,
  });

  final String url;
  final String senderLabel;
  final DateTime timestamp;
}

List<_DesktopSharedLink> _desktopLinksFromMessages(List<ChatMessage> messages) {
  final pattern = RegExp(r'https?:\/\/[^\s]+', caseSensitive: false);
  final links = <_DesktopSharedLink>[];
  for (final message in messages) {
    for (final match in pattern.allMatches(message.text)) {
      links.add(
        _DesktopSharedLink(
          url: match.group(0)!,
          senderLabel: message.senderLabel,
          timestamp: message.timestamp,
        ),
      );
    }
  }
  return links;
}

class _DesktopContextMediaGrid extends StatelessWidget {
  const _DesktopContextMediaGrid({required this.attachments});

  final List<ChatAttachment> attachments;

  @override
  Widget build(BuildContext context) {
    final visible = attachments.take(6).toList(growable: false);
    return GridView.builder(
      key: const ValueKey('desktop-context-media-grid'),
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      itemCount: visible.length,
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: 3,
        crossAxisSpacing: 5,
        mainAxisSpacing: 5,
        childAspectRatio: 1.04,
      ),
      itemBuilder: (context, index) {
        return _DesktopContextMediaTile(
          attachment: visible[index],
          index: index,
        );
      },
    );
  }
}

class _DesktopContextMediaTile extends StatelessWidget {
  const _DesktopContextMediaTile({
    required this.attachment,
    required this.index,
  });

  final ChatAttachment attachment;
  final int index;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isVideo =
        attachment.kind == ChatAttachmentKind.video ||
        _desktopAttachmentExtension(attachment) == 'mp4' ||
        _desktopAttachmentExtension(attachment) == 'mov';
    final isAlbum = attachment.albumCount > 1;
    return ClipRRect(
      borderRadius: BorderRadius.circular(7),
      child: Stack(
        fit: StackFit.expand,
        children: <Widget>[
          CustomPaint(
            painter: _DesktopMediaThumbPainter(
              seed: attachment.thumbnailSeed?.isNotEmpty == true
                  ? attachment.thumbnailSeed!
                  : attachment.title,
              index: index,
            ),
          ),
          DecoratedBox(
            decoration: BoxDecoration(
              gradient: LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: <Color>[
                  Colors.black.withValues(alpha: 0.0),
                  Colors.black.withValues(alpha: 0.34),
                ],
                stops: const <double>[0.52, 1],
              ),
            ),
          ),
          if (isVideo)
            Center(
              child: Container(
                width: 24,
                height: 24,
                decoration: BoxDecoration(
                  color: Colors.black.withValues(alpha: 0.42),
                  shape: BoxShape.circle,
                ),
                alignment: Alignment.center,
                child: const AppIcon(
                  AppSemanticIcon.media,
                  size: 12,
                  color: Colors.white,
                ),
              ),
            ),
          Positioned(
            left: 5,
            right: 5,
            bottom: 5,
            child: Row(
              children: <Widget>[
                Expanded(
                  child: Text(
                    isAlbum
                        ? '${attachment.albumCount} 张'
                        : isVideo
                        ? (attachment.durationLabel ?? '视频')
                        : _desktopAttachmentExtension(attachment).toUpperCase(),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: theme.textTheme.labelSmall?.copyWith(
                      color: Colors.white.withValues(alpha: 0.92),
                      fontSize: 8.8,
                      height: 1.0,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ),
                if (attachment.state == ChatAttachmentState.uploading)
                  Container(
                    width: 5,
                    height: 5,
                    decoration: BoxDecoration(
                      color: theme.colorScheme.primary,
                      shape: BoxShape.circle,
                    ),
                  ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _DesktopMediaThumbPainter extends CustomPainter {
  const _DesktopMediaThumbPainter({required this.seed, required this.index});

  final String seed;
  final int index;

  @override
  void paint(Canvas canvas, Size size) {
    final hash = seed.codeUnits.fold<int>(
      17 + index * 37,
      (value, unit) => (value * 31 + unit) & 0x7fffffff,
    );
    final top = Color.lerp(
      const Color(0xFF223744),
      const Color(0xFF6B7A56),
      ((hash % 7) + 1) / 9,
    )!;
    final bottom = Color.lerp(
      const Color(0xFF132636),
      const Color(0xFF485C68),
      (((hash >> 3) % 9) + 1) / 11,
    )!;
    final bg = Paint()
      ..shader = LinearGradient(
        begin: Alignment.topLeft,
        end: Alignment.bottomRight,
        colors: <Color>[top, bottom],
      ).createShader(Offset.zero & size);
    canvas.drawRect(Offset.zero & size, bg);

    final panel = Paint()..color = Colors.black.withValues(alpha: 0.20);
    final line = Paint()
      ..color = Colors.white.withValues(alpha: 0.16)
      ..strokeWidth = 1.2
      ..strokeCap = StrokeCap.round;
    final accent = Paint()
      ..color = AppPalette.brandBlue.withValues(alpha: 0.62);

    final frame = RRect.fromRectAndRadius(
      Rect.fromLTWH(
        size.width * 0.16,
        size.height * 0.15,
        size.width * 0.52,
        size.height * 0.56,
      ),
      const Radius.circular(7),
    );
    canvas.drawRRect(frame, panel);

    for (var row = 0; row < 3; row++) {
      final y = size.height * (0.28 + row * 0.15);
      canvas.drawLine(
        Offset(size.width * 0.25, y),
        Offset(size.width * (0.48 + row * 0.05), y),
        line,
      );
      canvas.drawLine(
        Offset(size.width * 0.25, y + 6),
        Offset(size.width * (0.42 + row * 0.04), y + 6),
        line..color = Colors.white.withValues(alpha: 0.09),
      );
    }

    final tilePaint = Paint()..color = Colors.white.withValues(alpha: 0.24);
    for (var tile = 0; tile < 4; tile++) {
      final left = size.width * (0.56 + (tile.isOdd ? 0.20 : 0));
      final top = size.height * (0.30 + (tile >= 2 ? 0.22 : 0));
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromLTWH(left, top, size.width * 0.17, size.height * 0.16),
          const Radius.circular(4),
        ),
        tilePaint,
      );
    }

    canvas.drawCircle(
      Offset(size.width * 0.26, size.height * 0.22),
      size.shortestSide * 0.055,
      accent,
    );
  }

  @override
  bool shouldRepaint(covariant _DesktopMediaThumbPainter oldDelegate) =>
      oldDelegate.seed != seed || oldDelegate.index != index;
}

class _DesktopContextHeader extends StatelessWidget {
  const _DesktopContextHeader({
    required this.title,
    required this.subtitle,
    required this.trailing,
    this.onClose,
  });

  final String title;
  final String subtitle;
  final String trailing;
  final VoidCallback? onClose;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                title,
                style: theme.textTheme.labelLarge?.copyWith(
                  fontWeight: FontWeight.w600,
                  fontSize: 12.6,
                ),
              ),
              const SizedBox(height: 1),
              Text(
                subtitle,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: theme.textTheme.bodySmall?.copyWith(
                  fontSize: 11.0,
                  color: theme.textTheme.bodySmall?.color?.withValues(
                    alpha: 0.6,
                  ),
                  height: 1.0,
                ),
              ),
            ],
          ),
        ),
        const SizedBox(width: AppTokens.spacingXs),
        Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            Text(
              trailing,
              style: theme.textTheme.labelMedium?.copyWith(
                fontWeight: FontWeight.w600,
                fontSize: 10.8,
                color: theme.textTheme.labelMedium?.color?.withValues(
                  alpha: 0.64,
                ),
              ),
            ),
            if (onClose case final onClose?) ...<Widget>[
              const SizedBox(width: 2),
              IconButton(
                key: const ValueKey('desktop-context-panel-close'),
                onPressed: onClose,
                tooltip: '关闭会话信息',
                icon: const AppIcon(AppSemanticIcon.close),
                iconSize: 13,
                padding: EdgeInsets.zero,
                constraints: const BoxConstraints.tightFor(
                  width: 24,
                  height: 24,
                ),
                style: IconButton.styleFrom(
                  tapTargetSize: MaterialTapTargetSize.shrinkWrap,
                  minimumSize: const Size(24, 24),
                  maximumSize: const Size(24, 24),
                  backgroundColor: Colors.transparent,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(7),
                  ),
                  padding: EdgeInsets.zero,
                ),
              ),
            ],
          ],
        ),
      ],
    );
  }
}

void _showDesktopMessageSearch(
  BuildContext context,
  ConversationSummary conversation,
  List<ChatMessage> messages,
) {
  showDialog<void>(
    context: context,
    builder: (context) => _DesktopMessageSearchDialog(
      conversation: conversation,
      messages: messages,
    ),
  );
}

Future<String?> _pickDesktopFile() async {
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

Future<bool> _confirmDesktopClearHistory(
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
          key: const ValueKey('desktop-clear-history-confirm'),
          onPressed: () => Navigator.of(context).pop(true),
          child: const Text('清空'),
        ),
      ],
    ),
  );
  return result ?? false;
}

void _showDesktopAddFriendDialog(
  BuildContext context, {
  required Future<void> Function(_DesktopFriendRequestDraft request) onSubmit,
}) {
  showDialog<void>(
    context: context,
    builder: (context) => _DesktopAddFriendDialog(onSubmit: onSubmit),
  );
}

void _showDesktopStatus(BuildContext context, String message) {
  ScaffoldMessenger.maybeOf(
    context,
  )?.showSnackBar(SnackBar(content: Text(message)));
}

String? _firstConversationIdOfKind(
  Iterable<ConversationSummary> conversations,
  ConversationKind kind,
) {
  for (final conversation in conversations) {
    if (conversation.kind == kind) {
      return conversation.id;
    }
  }
  return null;
}

class _DesktopMessageSearchDialog extends StatefulWidget {
  const _DesktopMessageSearchDialog({
    required this.conversation,
    required this.messages,
  });

  final ConversationSummary conversation;
  final List<ChatMessage> messages;

  @override
  State<_DesktopMessageSearchDialog> createState() =>
      _DesktopMessageSearchDialogState();
}

class _DesktopMessageSearchDialogState
    extends State<_DesktopMessageSearchDialog> {
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
    final results = _query.isEmpty
        ? widget.messages
        : widget.messages
              .where(
                (message) => '${message.senderLabel} ${message.text}'
                    .toLowerCase()
                    .contains(_query.toLowerCase()),
              )
              .toList(growable: false);
    return AlertDialog(
      title: Text('搜索 ${widget.conversation.title}'),
      content: SizedBox(
        width: 420,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            TextField(
              key: const ValueKey('desktop-message-search-input'),
              controller: _controller,
              autofocus: true,
              textInputAction: TextInputAction.search,
              enableSuggestions: false,
              enableIMEPersonalizedLearning: false,
              autocorrect: false,
              smartDashesType: SmartDashesType.disabled,
              smartQuotesType: SmartQuotesType.disabled,
              decoration: const InputDecoration(
                prefixIcon: AppIcon(AppSemanticIcon.search),
                hintText: '搜索消息内容',
              ),
            ),
            const SizedBox(height: 12),
            ConstrainedBox(
              constraints: const BoxConstraints(maxHeight: 320),
              child: results.isEmpty
                  ? const Center(child: Text('没有找到相关消息'))
                  : ListView.builder(
                      shrinkWrap: true,
                      itemCount: results.length,
                      itemBuilder: (context, index) {
                        final message = results[index];
                        return ListTile(
                          key: ValueKey<String>(
                            'desktop-message-search-result-${message.id}',
                          ),
                          dense: true,
                          title: Text(message.text),
                          subtitle: Text(
                            '${message.senderLabel} · ${_formatClock(message.timestamp)}',
                          ),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
      actions: <Widget>[
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('关闭'),
        ),
      ],
    );
  }
}

class _DesktopFriendRequestDraft {
  const _DesktopFriendRequestDraft({
    required this.accountId,
    required this.remark,
  });

  final String accountId;
  final String remark;
}

class _DesktopAddFriendDialog extends StatefulWidget {
  const _DesktopAddFriendDialog({required this.onSubmit});

  final Future<void> Function(_DesktopFriendRequestDraft request) onSubmit;

  @override
  State<_DesktopAddFriendDialog> createState() =>
      _DesktopAddFriendDialogState();
}

class _DesktopAddFriendDialogState extends State<_DesktopAddFriendDialog> {
  final TextEditingController _accountController = TextEditingController();
  final TextEditingController _remarkController = TextEditingController();
  bool _submitting = false;

  @override
  void dispose() {
    _accountController.dispose();
    _remarkController.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    final accountId = _accountController.text.trim();
    if (accountId.isEmpty || _submitting) {
      return;
    }
    setState(() {
      _submitting = true;
    });
    await widget.onSubmit(
      _DesktopFriendRequestDraft(
        accountId: accountId,
        remark: _remarkController.text.trim(),
      ),
    );
    if (mounted) {
      Navigator.of(context).pop();
    }
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('导入联系人'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          TextField(
            key: const ValueKey('desktop-add-friend-username'),
            controller: _accountController,
            textInputAction: TextInputAction.next,
            enableSuggestions: false,
            enableIMEPersonalizedLearning: false,
            autocorrect: false,
            smartDashesType: SmartDashesType.disabled,
            smartQuotesType: SmartQuotesType.disabled,
            decoration: const InputDecoration(labelText: '账号'),
          ),
          TextField(
            key: const ValueKey('desktop-add-friend-remark'),
            controller: _remarkController,
            textInputAction: TextInputAction.done,
            enableSuggestions: false,
            enableIMEPersonalizedLearning: false,
            autocorrect: false,
            smartDashesType: SmartDashesType.disabled,
            smartQuotesType: SmartQuotesType.disabled,
            onSubmitted: (_) => _submit(),
            decoration: const InputDecoration(labelText: '备注'),
          ),
        ],
      ),
      actions: <Widget>[
        TextButton(
          onPressed: _submitting ? null : () => Navigator.of(context).pop(),
          child: const Text('取消'),
        ),
        FilledButton(
          key: const ValueKey('desktop-add-friend-submit'),
          onPressed: _submitting ? null : _submit,
          child: const Text('发送请求'),
        ),
      ],
    );
  }
}

class _DesktopStageHeader extends StatelessWidget {
  const _DesktopStageHeader({
    this.leading,
    required this.title,
    required this.subtitle,
    this.badgeLabel,
    this.detailLabel,
    required this.action,
  });

  final Widget? leading;
  final String title;
  final String subtitle;
  final String? badgeLabel;
  final String? detailLabel;
  final Widget action;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final hasMetadata =
        subtitle.isNotEmpty || badgeLabel != null || detailLabel != null;
    return Row(
      crossAxisAlignment: CrossAxisAlignment.center,
      children: <Widget>[
        if (leading case final leading?) ...<Widget>[
          leading,
          const SizedBox(width: AppTokens.spacingSm),
        ],
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                title,
                style: theme.textTheme.titleMedium?.copyWith(
                  fontSize: 14.5,
                  fontWeight: FontWeight.w600,
                  letterSpacing: -0.18,
                ),
              ),
              if (hasMetadata) ...<Widget>[
                const SizedBox(height: 2),
                Text.rich(
                  key: const ValueKey('desktop-stage-metadata'),
                  TextSpan(
                    children: <InlineSpan>[
                      if (badgeLabel case final badge?)
                        TextSpan(
                          text: badge,
                          style: theme.textTheme.labelSmall?.copyWith(
                            fontSize: 10.2,
                            fontWeight: FontWeight.w700,
                            color: theme.colorScheme.primary.withValues(
                              alpha: 0.76,
                            ),
                          ),
                        ),
                      if (badgeLabel != null)
                        TextSpan(
                          text: '  ·  ',
                          style: theme.textTheme.labelSmall?.copyWith(
                            color: theme.textTheme.labelSmall?.color
                                ?.withValues(alpha: 0.42),
                          ),
                        ),
                      if (subtitle.isNotEmpty)
                        TextSpan(
                          text: subtitle,
                          style: theme.textTheme.bodySmall?.copyWith(
                            fontSize: 11.1,
                            color: theme.textTheme.bodySmall?.color?.withValues(
                              alpha: 0.82,
                            ),
                            height: 1.0,
                          ),
                        ),
                      if (detailLabel case final detail?) ...<InlineSpan>[
                        if (subtitle.isNotEmpty || badgeLabel != null)
                          TextSpan(
                            text: '  ·  ',
                            style: theme.textTheme.labelSmall?.copyWith(
                              color: theme.textTheme.labelSmall?.color
                                  ?.withValues(alpha: 0.42),
                            ),
                          ),
                        TextSpan(
                          text: detail,
                          style: theme.textTheme.labelSmall?.copyWith(
                            fontSize: 10.7,
                            color: theme.textTheme.labelSmall?.color
                                ?.withValues(alpha: 0.68),
                            fontWeight: FontWeight.w500,
                          ),
                        ),
                      ],
                    ],
                  ),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
              ],
            ],
          ),
        ),
        const SizedBox(width: AppTokens.spacingXs + 2),
        Container(
          key: const ValueKey('desktop-stage-action-group'),
          padding: const EdgeInsets.all(0.5),
          decoration: BoxDecoration(
            color: theme.colorScheme.surface.withValues(alpha: 0.18),
            borderRadius: BorderRadius.circular(7.5),
            border: Border.all(
              color: theme.dividerColor.withValues(alpha: 0.024),
            ),
          ),
          child: action,
        ),
      ],
    );
  }
}

class _DesktopStageIdentity extends StatelessWidget {
  const _DesktopStageIdentity({required this.conversation});

  final ConversationSummary conversation;

  @override
  Widget build(BuildContext context) {
    return SocialAvatar.conversation(
      conversation,
      key: const ValueKey('desktop-stage-identity'),
      size: 34,
    );
  }
}

class _DesktopChatComposer extends StatefulWidget {
  const _DesktopChatComposer({
    required this.enabled,
    required this.onSend,
    required this.onSendFile,
  });

  final bool enabled;
  final ValueChanged<String> onSend;
  final ValueChanged<String>? onSendFile;

  @override
  State<_DesktopChatComposer> createState() => _DesktopChatComposerState();
}

class _DesktopChatComposerState extends State<_DesktopChatComposer> {
  final TextEditingController _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _send() {
    if (!widget.enabled) {
      return;
    }
    final text = _controller.text;
    _controller.clear();
    widget.onSend(text);
  }

  Future<void> _sendFile() async {
    final onSendFile = widget.onSendFile;
    if (onSendFile == null) {
      return;
    }
    final path = await _pickDesktopFile();
    if (!mounted) {
      return;
    }
    if (path == null || path.trim().isEmpty) {
      return;
    }
    onSendFile(path.trim());
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    if (!widget.enabled) {
      return Container(
        key: const ValueKey('desktop-composer-idle-state'),
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 9),
        decoration: BoxDecoration(
          color: palette.surfaceVariant.withValues(alpha: 0.055),
          borderRadius: BorderRadius.circular(11),
          border: Border.all(color: theme.dividerColor.withValues(alpha: 0.08)),
        ),
        child: Row(
          children: <Widget>[
            AppIcon(
              AppSemanticIcon.sessions,
              size: 15,
              color: theme.textTheme.bodySmall?.color?.withValues(alpha: 0.62),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                '选择会话后开始输入',
                style: theme.textTheme.bodySmall?.copyWith(
                  fontSize: 11.8,
                  color: theme.textTheme.bodySmall?.color?.withValues(
                    alpha: 0.6,
                  ),
                ),
              ),
            ),
          ],
        ),
      );
    }

    return Container(
      key: const ValueKey('desktop-composer-surface'),
      constraints: const BoxConstraints(minHeight: 50),
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 7),
      decoration: BoxDecoration(
        color: Color.alphaBlend(
          palette.surfaceVariant.withValues(alpha: 0.05),
          theme.colorScheme.surface.withValues(alpha: 0.96),
        ),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: theme.dividerColor.withValues(alpha: 0.095)),
        boxShadow: <BoxShadow>[
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.014),
            blurRadius: 7,
            offset: const Offset(0, 1),
          ),
        ],
      ),
      child: Row(
        children: <Widget>[
          IconButton(
            key: const ValueKey('desktop-composer-attach-button'),
            onPressed: widget.onSendFile == null ? null : _sendFile,
            icon: const AppIcon(AppSemanticIcon.attachment),
            tooltip: '附件',
            visualDensity: VisualDensity.standard,
            iconSize: 16,
            padding: EdgeInsets.zero,
            constraints: const BoxConstraints.tightFor(width: 34, height: 34),
            style: IconButton.styleFrom(
              tapTargetSize: MaterialTapTargetSize.shrinkWrap,
              minimumSize: const Size(34, 34),
              maximumSize: const Size(34, 34),
              backgroundColor: palette.surfaceVariant.withValues(alpha: 0.12),
              side: BorderSide(
                color: theme.dividerColor.withValues(alpha: 0.074),
              ),
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(17),
              ),
              padding: EdgeInsets.zero,
            ),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Container(
              key: const ValueKey('desktop-composer-input-surface'),
              constraints: const BoxConstraints(minHeight: 36),
              padding: const EdgeInsets.symmetric(horizontal: 13),
              decoration: BoxDecoration(
                color: palette.surfaceVariant.withValues(alpha: 0.105),
                borderRadius: BorderRadius.circular(18),
                border: Border.all(
                  color: theme.dividerColor.withValues(alpha: 0.072),
                ),
              ),
              child: TextField(
                controller: _controller,
                onSubmitted: (_) => _send(),
                textAlignVertical: TextAlignVertical.center,
                minLines: 1,
                maxLines: 3,
                enableSuggestions: false,
                enableIMEPersonalizedLearning: false,
                autocorrect: false,
                smartDashesType: SmartDashesType.disabled,
                smartQuotesType: SmartQuotesType.disabled,
                style: theme.textTheme.bodyMedium?.copyWith(
                  fontSize: 13.6,
                  height: 1.22,
                ),
                decoration: InputDecoration(
                  isDense: true,
                  border: InputBorder.none,
                  hintText: '写一条消息',
                  hintStyle: theme.textTheme.bodyMedium?.copyWith(
                    fontSize: 13.1,
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: 0.68,
                    ),
                  ),
                  contentPadding: const EdgeInsets.symmetric(vertical: 9),
                ),
              ),
            ),
          ),
          const SizedBox(width: 8),
          FilledButton(
            key: const ValueKey('desktop-composer-send-button'),
            onPressed: _send,
            style: FilledButton.styleFrom(
              fixedSize: const Size(36, 34),
              minimumSize: const Size(36, 34),
              padding: EdgeInsets.zero,
              backgroundColor: theme.colorScheme.primary.withValues(
                alpha: 0.82,
              ),
              foregroundColor: Colors.white,
              side: BorderSide(
                color: theme.colorScheme.primary.withValues(alpha: 0.16),
              ),
              elevation: 0,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(17),
              ),
              tapTargetSize: MaterialTapTargetSize.shrinkWrap,
            ),
            child: const AppIcon(AppSemanticIcon.send, size: 16.5),
          ),
        ],
      ),
    );
  }
}

class _DesktopChatTimeline extends StatefulWidget {
  const _DesktopChatTimeline({
    required this.conversation,
    required this.initialUnreadCount,
    required this.messages,
    required this.maxContentWidth,
  });

  final ConversationSummary conversation;
  final int initialUnreadCount;
  final List<ChatMessage> messages;
  final double maxContentWidth;

  @override
  State<_DesktopChatTimeline> createState() => _DesktopChatTimelineState();
}

class _DesktopChatTimelineState extends State<_DesktopChatTimeline> {
  final ScrollController _scrollController = ScrollController();
  late int _openingUnreadCount;

  @override
  void initState() {
    super.initState();
    _openingUnreadCount = widget.initialUnreadCount;
    _scheduleJumpToLatest();
  }

  @override
  void didUpdateWidget(covariant _DesktopChatTimeline oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.conversation.id != widget.conversation.id ||
        oldWidget.initialUnreadCount != widget.initialUnreadCount) {
      _openingUnreadCount = widget.initialUnreadCount;
    }
    if (oldWidget.messages != widget.messages ||
        oldWidget.conversation.id != widget.conversation.id) {
      _scheduleJumpToLatest();
    }
  }

  void _scheduleJumpToLatest() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _jumpToLatestIfReady();
      WidgetsBinding.instance.addPostFrameCallback((_) {
        _jumpToLatestIfReady();
      });
    });
  }

  void _jumpToLatestIfReady() {
    if (!mounted || !_scrollController.hasClients) {
      return;
    }
    _scrollController.jumpTo(_scrollController.position.maxScrollExtent);
  }

  @override
  void dispose() {
    _scrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final orderedMessages = List<ChatMessage>.from(widget.messages)
      ..sort((left, right) => left.timestamp.compareTo(right.timestamp));
    final now = DateTime.now();
    final unreadCount = _openingUnreadCount > 0
        ? _openingUnreadCount
        : widget.conversation.unreadCount;
    final firstUnreadIndex = _firstUnreadTimelineIndex(
      unreadCount: unreadCount,
      messageCount: orderedMessages.length,
    );
    final children = <Widget>[];

    for (var index = 0; index < orderedMessages.length; index++) {
      final message = orderedMessages[index];
      final previous = index > 0 ? orderedMessages[index - 1] : null;

      final needsDayDivider =
          previous == null ||
          !DateUtils.isSameDay(previous.timestamp, message.timestamp);
      if (needsDayDivider) {
        if (children.isNotEmpty) {
          children.add(const SizedBox(height: AppTokens.spacingSm));
        }
        children.add(
          _DesktopTimelineDivider(
            label: _formatTimelineDayLabel(message.timestamp, now),
          ),
        );
        children.add(const SizedBox(height: AppTokens.spacingSm));
      }

      if (firstUnreadIndex == index) {
        children.add(
          _DesktopTimelineDivider(label: '$unreadCount 条新消息', accent: true),
        );
        children.add(const SizedBox(height: AppTokens.spacingSm));
      }

      if (message.isSystemNotice) {
        children.add(
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 6),
            child: _DesktopSystemNotice(text: message.text),
          ),
        );
        continue;
      }

      final clusterStart = !_sharesMessageCluster(previous, message);
      final incomingGroupMessage =
          widget.conversation.isGroup &&
          message.direction == MessageDirection.incoming &&
          !message.isSystemNotice;

      children.add(
        _DesktopTimelineMessageEntry(
          message: message,
          showIdentity: incomingGroupMessage && clusterStart,
          reserveIdentitySlot: incomingGroupMessage,
          topSpacing: clusterStart ? 6.5 : 1.5,
        ),
      );
    }

    final timeline = ScrollConfiguration(
      behavior: const MaterialScrollBehavior().copyWith(scrollbars: false),
      child: LayoutBuilder(
        builder: (context, constraints) {
          const padding = EdgeInsets.fromLTRB(
            3,
            AppTokens.spacingMd + 1,
            ImUiMetrics.desktopOutgoingRightInset,
            AppTokens.spacingLg,
          );
          if (constraints.maxHeight < 700) {
            return ListView(
              controller: _scrollController,
              key: const ValueKey('desktop-chat-timeline'),
              padding: padding,
              children: children,
            );
          }

          final minTimelineHeight = constraints.maxHeight > padding.vertical
              ? constraints.maxHeight - padding.vertical
              : 0.0;
          return SingleChildScrollView(
            controller: _scrollController,
            key: const ValueKey('desktop-chat-timeline'),
            padding: padding,
            child: ConstrainedBox(
              constraints: BoxConstraints(minHeight: minTimelineHeight),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.end,
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: children,
              ),
            ),
          );
        },
      ),
    );

    if (!widget.maxContentWidth.isFinite) {
      return Padding(padding: EdgeInsets.zero, child: timeline);
    }

    return Padding(
      padding: EdgeInsets.zero,
      child: Align(
        alignment: Alignment.topCenter,
        child: ConstrainedBox(
          constraints: BoxConstraints(maxWidth: widget.maxContentWidth),
          child: timeline,
        ),
      ),
    );
  }
}

class _DesktopTimelineMessageEntry extends StatelessWidget {
  const _DesktopTimelineMessageEntry({
    required this.message,
    required this.showIdentity,
    required this.reserveIdentitySlot,
    required this.topSpacing,
  });

  final ChatMessage message;
  final bool showIdentity;
  final bool reserveIdentitySlot;
  final double topSpacing;

  @override
  Widget build(BuildContext context) {
    final isOutgoing = message.direction == MessageDirection.outgoing;
    final bubble = _DesktopMessageBubbleFrame(message: message);

    if (isOutgoing) {
      return Padding(
        padding: EdgeInsets.only(top: topSpacing),
        child: Align(alignment: Alignment.centerRight, child: bubble),
      );
    }

    final avatarSlot = reserveIdentitySlot
        ? SizedBox(
            width: 32,
            child: showIdentity
                ? Align(
                    alignment: Alignment.topLeft,
                    child: SocialAvatar(
                      key: ValueKey('desktop-message-avatar-${message.id}'),
                      id: message.senderLabel,
                      title: message.senderLabel,
                      size: 31,
                    ),
                  )
                : const SizedBox.shrink(),
          )
        : const SizedBox.shrink();

    return Padding(
      padding: EdgeInsets.only(top: topSpacing),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          if (reserveIdentitySlot) ...<Widget>[
            avatarSlot,
            const SizedBox(width: 6),
          ],
          Flexible(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                if (showIdentity)
                  Padding(
                    padding: const EdgeInsets.only(left: 2, bottom: 2),
                    child: Text(
                      message.senderLabel,
                      style: Theme.of(context).textTheme.labelSmall?.copyWith(
                        fontSize: 10.2,
                        height: 1.0,
                      ),
                    ),
                  ),
                bubble,
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _DesktopMessageBubbleFrame extends StatelessWidget {
  const _DesktopMessageBubbleFrame({required this.message});

  final ChatMessage message;

  @override
  Widget build(BuildContext context) {
    final maxWidth = message.attachment != null
        ? 324.0
        : message.text.trim().length > 90
        ? 496.0
        : 448.0;

    return ConstrainedBox(
      key: ValueKey('desktop-message-bubble-${message.id}'),
      constraints: BoxConstraints(maxWidth: maxWidth),
      child: MessageBubble(
        message: message,
        showAvatar: false,
        showSenderLabel: false,
      ),
    );
  }
}

class _DesktopTimelineDivider extends StatelessWidget {
  const _DesktopTimelineDivider({required this.label, this.accent = false});

  final String label;
  final bool accent;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final lineColor = accent
        ? theme.colorScheme.primary.withValues(alpha: 0.16)
        : palette.divider.withValues(alpha: 0.18);
    final chipColor = accent
        ? theme.colorScheme.primary.withValues(alpha: 0.06)
        : theme.colorScheme.surface.withValues(alpha: 0.88);

    return Row(
      children: <Widget>[
        Expanded(child: Divider(color: lineColor, thickness: 1)),
        Container(
          margin: const EdgeInsets.symmetric(horizontal: 12),
          padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 3),
          decoration: BoxDecoration(
            color: chipColor,
            borderRadius: BorderRadius.circular(999),
            border: Border.all(color: lineColor.withValues(alpha: 0.7)),
          ),
          child: Text(
            label,
            style: theme.textTheme.labelSmall?.copyWith(
              fontWeight: FontWeight.w600,
              color: accent
                  ? theme.colorScheme.primary
                  : theme.textTheme.labelSmall?.color?.withValues(alpha: 0.82),
            ),
          ),
        ),
        Expanded(child: Divider(color: lineColor, thickness: 1)),
      ],
    );
  }
}

class _DesktopSystemNotice extends StatelessWidget {
  const _DesktopSystemNotice({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Center(
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
        decoration: BoxDecoration(
          color: palette.surfaceVariant.withValues(alpha: 0.1),
          borderRadius: BorderRadius.circular(999),
        ),
        child: Text(
          text,
          textAlign: TextAlign.center,
          style: theme.textTheme.labelSmall?.copyWith(
            color: theme.textTheme.bodySmall?.color?.withValues(alpha: 0.74),
          ),
        ),
      ),
    );
  }
}

class _DesktopContextOverviewCard extends StatelessWidget {
  const _DesktopContextOverviewCard({
    this.containerKey,
    required this.icon,
    required this.title,
    required this.contextLabel,
    required this.primaryLine,
    required this.secondaryLine,
    required this.trailingLabel,
  });

  final AppSemanticIcon icon;
  final Key? containerKey;
  final String title;
  final String contextLabel;
  final String primaryLine;
  final String secondaryLine;
  final String trailingLabel;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Container(
      key: containerKey,
      padding: const EdgeInsets.fromLTRB(
        AppTokens.spacingXs + 3,
        AppTokens.spacingXs + 1,
        AppTokens.spacingXs + 3,
        AppTokens.spacingXs + 1,
      ),
      decoration: BoxDecoration(
        color: palette.surfaceVariant.withValues(alpha: 0.03),
        borderRadius: BorderRadius.circular(6),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Container(
            width: 24,
            height: 24,
            decoration: BoxDecoration(
              color: theme.colorScheme.primary.withValues(alpha: 0.05),
              borderRadius: BorderRadius.circular(6),
            ),
            alignment: Alignment.center,
            child: AppIcon(icon, color: theme.colorScheme.primary, size: 12),
          ),
          const SizedBox(width: AppTokens.spacingXs + 3),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Row(
                  children: <Widget>[
                    Expanded(
                      child: Text(
                        title,
                        style: theme.textTheme.bodyMedium?.copyWith(
                          fontWeight: FontWeight.w600,
                          fontSize: 12.3,
                        ),
                      ),
                    ),
                    Text(
                      trailingLabel,
                      style: theme.textTheme.labelMedium?.copyWith(
                        fontWeight: FontWeight.w600,
                        fontSize: 10.6,
                        color: theme.textTheme.labelMedium?.color?.withValues(
                          alpha: 0.58,
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 1),
                Text(
                  contextLabel,
                  style: theme.textTheme.bodySmall?.copyWith(
                    fontWeight: FontWeight.w500,
                    fontSize: 10.8,
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: 0.62,
                    ),
                  ),
                ),
                const SizedBox(height: 3),
                Text(
                  primaryLine,
                  style: theme.textTheme.bodyMedium?.copyWith(fontSize: 12.1),
                ),
                const SizedBox(height: 1),
                Text(
                  secondaryLine,
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(
                    fontSize: 11.0,
                    color: theme.textTheme.bodySmall?.color?.withValues(
                      alpha: 0.68,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _DesktopContextSectionHeader extends StatelessWidget {
  const _DesktopContextSectionHeader({
    required this.title,
    required this.trailing,
  });

  final String title;
  final String trailing;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Row(
      children: <Widget>[
        Expanded(
          child: Text(
            title,
            style: theme.textTheme.labelMedium?.copyWith(
              fontWeight: FontWeight.w500,
              fontSize: 11.0,
              color: theme.textTheme.labelMedium?.color?.withValues(alpha: 0.7),
            ),
          ),
        ),
        Text(
          trailing,
          style: theme.textTheme.labelMedium?.copyWith(
            fontSize: 10.6,
            color: theme.textTheme.labelMedium?.color?.withValues(alpha: 0.56),
          ),
        ),
      ],
    );
  }
}

class _DesktopContextEmptyHint extends StatelessWidget {
  const _DesktopContextEmptyHint({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: AppTokens.spacingXs),
      child: Text(
        text,
        style: Theme.of(context).textTheme.bodySmall?.copyWith(
          color: Theme.of(
            context,
          ).textTheme.bodySmall?.color?.withValues(alpha: 0.68),
          fontSize: 10.8,
        ),
      ),
    );
  }
}

class _DesktopEmptyContextState extends StatelessWidget {
  const _DesktopEmptyContextState({required this.onClose});

  final VoidCallback onClose;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      key: const ValueKey('desktop-conversation-sidebar'),
      padding: const EdgeInsets.fromLTRB(
        AppTokens.spacingSm,
        AppTokens.spacingMd,
        AppTokens.spacingSm,
        AppTokens.spacingSm,
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Row(
            children: <Widget>[
              Expanded(
                child: Text(
                  '会话上下文',
                  style: theme.textTheme.labelLarge?.copyWith(
                    fontWeight: FontWeight.w600,
                    fontSize: 12.4,
                  ),
                ),
              ),
              IconButton(
                key: const ValueKey('desktop-context-panel-close'),
                onPressed: onClose,
                tooltip: '关闭会话信息',
                icon: const AppIcon(AppSemanticIcon.close),
                iconSize: 13,
                padding: EdgeInsets.zero,
                constraints: const BoxConstraints.tightFor(
                  width: 24,
                  height: 24,
                ),
                style: IconButton.styleFrom(
                  tapTargetSize: MaterialTapTargetSize.shrinkWrap,
                  minimumSize: const Size(24, 24),
                  maximumSize: const Size(24, 24),
                  backgroundColor: Colors.transparent,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(7),
                  ),
                  padding: EdgeInsets.zero,
                ),
              ),
            ],
          ),
          const SizedBox(height: 3),
          Text(
            '打开会话后，这里会显示成员、媒体、文件和通知设置。',
            style: theme.textTheme.bodySmall?.copyWith(
              fontSize: 10.8,
              color: theme.textTheme.bodySmall?.color?.withValues(alpha: 0.68),
            ),
          ),
          const SizedBox(height: AppTokens.spacingSm + 2),
          const _DesktopSectionDivider(),
          const SizedBox(height: AppTokens.spacingSm + 2),
          const _DesktopContextIdleRow(
            title: '共享媒体',
            subtitle: '最近文件、图片和可回溯附件',
          ),
          const SizedBox(height: AppTokens.spacingSm),
          const _DesktopContextIdleRow(title: '通知设置', subtitle: '免打扰、置顶和群提醒'),
        ],
      ),
    );
  }
}

class _DesktopEmptyConversationState extends StatelessWidget {
  const _DesktopEmptyConversationState({
    required this.onFocusConversationSearch,
    required this.onCreateGroup,
    required this.onAddContact,
    required this.onOpenFileAssistant,
  });

  final VoidCallback onFocusConversationSearch;
  final Future<void> Function() onCreateGroup;
  final VoidCallback onAddContact;
  final VoidCallback? onOpenFileAssistant;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Center(
      child: Padding(
        padding: const EdgeInsets.symmetric(
          horizontal: AppTokens.spacingXl + 8,
          vertical: AppTokens.spacingXl,
        ),
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 420),
          child: Container(
            key: const ValueKey('desktop-empty-state-surface'),
            decoration: BoxDecoration(
              color: theme.colorScheme.surface.withValues(alpha: 0.16),
              borderRadius: BorderRadius.circular(24),
              border: Border.all(
                color: theme.dividerColor.withValues(alpha: 0.052),
              ),
              boxShadow: <BoxShadow>[
                BoxShadow(
                  color: Colors.black.withValues(alpha: 0.012),
                  blurRadius: 14,
                  offset: const Offset(0, 6),
                ),
              ],
            ),
            child: Padding(
              padding: const EdgeInsets.symmetric(
                horizontal: AppTokens.spacingXl,
                vertical: AppTokens.spacingXl,
              ),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.center,
                children: <Widget>[
                  Container(
                    width: 58,
                    height: 58,
                    decoration: BoxDecoration(
                      color: theme.colorScheme.primary.withValues(alpha: 0.05),
                      shape: BoxShape.circle,
                      border: Border.all(
                        color: theme.colorScheme.primary.withValues(
                          alpha: 0.04,
                        ),
                      ),
                    ),
                    alignment: Alignment.center,
                    child: AppIcon(
                      AppSemanticIcon.sessions,
                      size: 24,
                      color: theme.colorScheme.primary.withValues(alpha: 0.76),
                    ),
                  ),
                  const SizedBox(height: AppTokens.spacingLg),
                  Text(
                    '从左侧打开一个聊天',
                    textAlign: TextAlign.center,
                    style: theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w600,
                      fontSize: 20,
                      letterSpacing: -0.28,
                    ),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    '最近消息、文件和群聊会在这里展开。\n左侧保持导航，中央专注当前会话。',
                    textAlign: TextAlign.center,
                    style: theme.textTheme.bodySmall?.copyWith(
                      fontSize: 12.8,
                      height: 1.32,
                      color: theme.textTheme.bodySmall?.color?.withValues(
                        alpha: 0.62,
                      ),
                    ),
                  ),
                  const SizedBox(height: AppTokens.spacingLg),
                  Wrap(
                    key: const ValueKey('desktop-empty-state-actions'),
                    alignment: WrapAlignment.center,
                    spacing: 8,
                    runSpacing: 8,
                    children: <Widget>[
                      _DesktopEmptyAction(
                        label: '选择一个聊天',
                        onPressed: onFocusConversationSearch,
                      ),
                      _DesktopEmptyAction(
                        label: '新建聊天',
                        onPressed: () {
                          onCreateGroup();
                        },
                      ),
                      _DesktopEmptyAction(
                        label: '导入联系人',
                        onPressed: onAddContact,
                      ),
                      if (onOpenFileAssistant != null)
                        _DesktopEmptyAction(
                          label: '打开文件助手',
                          onPressed: onOpenFileAssistant!,
                        ),
                    ],
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _DesktopEmptyAction extends StatelessWidget {
  const _DesktopEmptyAction({required this.label, required this.onPressed});

  final String label;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return OutlinedButton(
      onPressed: onPressed,
      style: OutlinedButton.styleFrom(
        visualDensity: VisualDensity.compact,
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 7),
        side: BorderSide(color: theme.dividerColor.withValues(alpha: 0.16)),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
      ),
      child: Text(
        label,
        style: theme.textTheme.labelMedium?.copyWith(
          color: theme.colorScheme.onSurface.withValues(alpha: 0.72),
          fontWeight: FontWeight.w600,
        ),
      ),
    );
  }
}

class _DesktopContextIdleRow extends StatelessWidget {
  const _DesktopContextIdleRow({required this.title, required this.subtitle});

  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Text(
          title,
          style: theme.textTheme.bodyMedium?.copyWith(
            fontWeight: FontWeight.w600,
            fontSize: 11.6,
          ),
        ),
        const SizedBox(height: 2),
        Text(
          subtitle,
          style: theme.textTheme.bodySmall?.copyWith(
            fontSize: 10.6,
            color: theme.textTheme.bodySmall?.color?.withValues(alpha: 0.66),
          ),
        ),
      ],
    );
  }
}

class _DesktopFileDetailPanel extends StatelessWidget {
  const _DesktopFileDetailPanel({
    required this.attachments,
    required this.totalMessages,
    required this.onClose,
  });

  final List<ChatAttachment> attachments;
  final int totalMessages;
  final VoidCallback onClose;

  @override
  Widget build(BuildContext context) {
    return _DesktopWorkbenchSection(
      tone: _DesktopWorkbenchTone.detail,
      child: Column(
        key: const Key('desktop-file-detail-panel'),
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Padding(
            padding: const EdgeInsets.fromLTRB(
              AppTokens.spacingSm - 1,
              AppTokens.spacingSm - 1,
              AppTokens.spacingSm - 1,
              AppTokens.spacingXs - 1,
            ),
            child: _DesktopContextHeader(
              title: '文件上下文',
              subtitle: '最近接收的共享文件',
              trailing: '${attachments.length} 个文件',
              onClose: onClose,
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(
              horizontal: AppTokens.spacingSm - 1,
            ),
            child: _DesktopContextOverviewCard(
              containerKey: const ValueKey('desktop-context-summary-card'),
              icon: AppSemanticIcon.file,
              title: attachments.isNotEmpty ? attachments.last.title : '暂无共享文件',
              contextLabel: '文件会话',
              primaryLine: '${attachments.length} 个共享文件 · $totalMessages 条消息',
              secondaryLine: attachments.isNotEmpty
                  ? '最近共享：${attachments.last.title}'
                  : '等待新的共享文件进入列表。',
              trailingLabel: '可回溯',
            ),
          ),
          const _DesktopSectionDivider(),
          Expanded(
            child: ListView.separated(
              padding: const EdgeInsets.fromLTRB(
                AppTokens.spacingXs - 1,
                AppTokens.spacingXs,
                AppTokens.spacingXs - 1,
                AppTokens.spacingXs + 1,
              ),
              itemCount: attachments.length,
              separatorBuilder: (_, index) =>
                  const SizedBox(height: AppTokens.spacingXs),
              itemBuilder: (context, index) {
                final attachment = attachments[index];
                return _DesktopAttachmentCard(
                  containerKey: ValueKey(
                    'desktop-context-attachment-row-$index',
                  ),
                  attachment: attachment,
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

class _DesktopAttachmentCard extends StatelessWidget {
  const _DesktopAttachmentCard({this.containerKey, required this.attachment});

  final Key? containerKey;
  final ChatAttachment attachment;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final metaColor =
        theme.textTheme.bodySmall?.color?.withValues(alpha: 0.76) ??
        theme.colorScheme.onSurface.withValues(alpha: 0.76);
    return Container(
      key: containerKey,
      padding: const EdgeInsets.fromLTRB(
        AppTokens.spacingXs + 1,
        AppTokens.spacingXs - 1,
        AppTokens.spacingXs + 1,
        AppTokens.spacingXs - 1,
      ),
      decoration: BoxDecoration(
        color: palette.surfaceVariant.withValues(alpha: 0.022),
        borderRadius: BorderRadius.circular(6),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.center,
        children: <Widget>[
          Container(
            width: 18,
            height: 18,
            decoration: BoxDecoration(
              color: theme.colorScheme.primary.withValues(alpha: 0.025),
              borderRadius: BorderRadius.circular(4),
            ),
            alignment: Alignment.center,
            child: AppIcon(
              AppSemanticIcon.file,
              color: theme.colorScheme.primary,
              size: 10,
            ),
          ),
          const SizedBox(width: AppTokens.spacingXs - 1),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  attachment.title,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(
                    fontWeight: FontWeight.w600,
                    fontSize: 11.4,
                  ),
                ),
                const SizedBox(height: 1),
                Row(
                  children: <Widget>[
                    Expanded(
                      child: Text(
                        attachment.detail,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: theme.textTheme.labelSmall?.copyWith(
                          color: metaColor.withValues(alpha: 0.86),
                          fontSize: 10.4,
                        ),
                      ),
                    ),
                    const SizedBox(width: 5),
                    Text(
                      attachment.actionLabel,
                      style: theme.textTheme.labelSmall?.copyWith(
                        color: metaColor.withValues(alpha: 0.78),
                        fontWeight: FontWeight.w500,
                        fontSize: 10.0,
                      ),
                    ),
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

bool _isDesktopVisualAttachment(ChatAttachment attachment) {
  if (attachment.isVisualMedia) {
    return true;
  }
  return switch (_desktopAttachmentExtension(attachment)) {
    'jpg' ||
    'jpeg' ||
    'png' ||
    'webp' ||
    'heic' ||
    'gif' ||
    'mp4' ||
    'mov' => true,
    _ => false,
  };
}

String _desktopAttachmentExtension(ChatAttachment attachment) {
  final explicit = attachment.fileExtension?.trim().toLowerCase();
  if (explicit != null && explicit.isNotEmpty) {
    return explicit.replaceFirst('.', '');
  }
  final title = attachment.title.trim();
  final dotIndex = title.lastIndexOf('.');
  if (dotIndex >= 0 && dotIndex < title.length - 1) {
    return title.substring(dotIndex + 1).toLowerCase();
  }
  return switch (attachment.kind) {
    ChatAttachmentKind.image => 'jpg',
    ChatAttachmentKind.video => 'mp4',
    ChatAttachmentKind.audio => 'm4a',
    ChatAttachmentKind.file => 'file',
  };
}

String _desktopInboxSummary({
  required String sessionSubtitle,
  required int unreadCount,
}) {
  if (unreadCount <= 0) {
    return sessionSubtitle;
  }
  return '$sessionSubtitle · $unreadCount 条未读';
}

String? _desktopConversationPreviewPrefix(ConversationSummary conversation) {
  if (conversation.hasDraft) {
    return '草稿:';
  }
  if (conversation.mentionCount > 0 || conversation.pillLabel == '@你') {
    return '@你';
  }
  if (conversation.pillLabel == '文件') {
    return '文件';
  }
  if (conversation.pillLabel == '安全') {
    return '服务';
  }
  if (conversation.isGroup && conversation.preview.contains(':')) {
    final sender = conversation.preview.split(':').first.trim();
    if (sender.isNotEmpty) {
      return '$sender:';
    }
  }
  return null;
}

String _desktopConversationPreviewText(
  ConversationSummary conversation,
  String preview,
) {
  if (conversation.isGroup && preview.contains(':')) {
    return preview.split(':').skip(1).join(':').trim();
  }
  return preview;
}

String _formatCompactConversationTime(DateTime time) {
  final hour = time.hour.toString().padLeft(2, '0');
  final minute = time.minute.toString().padLeft(2, '0');
  return '$hour:$minute';
}

String _conversationStageDetail({
  required ConversationSummary conversation,
  required List<ChatMessage> messages,
  required List<ChatAttachment> attachments,
}) {
  final parts = <String>[
    '${messages.length} 条消息',
    if (attachments.isNotEmpty) '${attachments.length} 个附件',
    if (!conversation.isGroup && conversation.isOnline) '在线',
  ];
  return parts.join(' · ');
}

String _conversationStageSubtitle({
  required ConversationSummary? conversation,
  required List<ChatMessage> messages,
  required List<ChatAttachment> attachments,
}) {
  if (conversation == null) {
    return '从左侧选择会话';
  }

  final now = DateTime.now();
  if (isFileConversation(conversation)) {
    if (attachments.isEmpty) {
      return '把文件临时发到这里，稍后在其他设备接收。';
    }
    return '最近共享 ${attachments.last.title}';
  }

  if (conversation.isGroup) {
    if (conversation.unreadCount > 0) {
      return '${conversation.unreadCount} 条新消息';
    }
    if (messages.isEmpty) {
      return '群聊刚建立，暂无消息。';
    }
    return '最后一条消息在 ${_formatRelativeConversationMoment(messages.last.timestamp, now)}';
  }

  if (conversation.isOnline) {
    return '在线';
  }
  if (messages.isEmpty) {
    return '暂无聊天记录';
  }
  return '上次出现于 ${_formatRelativeConversationMoment(messages.last.timestamp, now)}';
}

String _desktopSidebarPrimaryLine({
  required ConversationSummary conversation,
  required List<ChatMessage> messages,
  required List<ChatAttachment> attachments,
}) {
  final parts = <String>[
    if (conversation.isGroup) '群聊' else '私聊',
    if (conversation.unreadCount > 0) '${conversation.unreadCount} 条未读',
    '${messages.length} 条消息',
    if (attachments.isNotEmpty) '${attachments.length} 个文件',
  ];
  return parts.join(' · ');
}

String _desktopSidebarSecondaryLine({
  required ConversationSummary conversation,
}) {
  if (conversation.isTyping) {
    return '对方正在输入';
  }
  if (conversation.isOnline && !conversation.isGroup) {
    return '当前在线';
  }
  if (conversation.isMuted) {
    return '通知已静音';
  }
  if (conversation.isPinned) {
    return '会话已置顶';
  }
  return '共享媒体、文件和通知设置。';
}

int? _firstUnreadTimelineIndex({
  required int unreadCount,
  required int messageCount,
}) {
  if (messageCount == 0 || unreadCount <= 0) {
    return null;
  }
  final clampedUnreadCount = unreadCount > messageCount
      ? messageCount
      : unreadCount;
  return messageCount - clampedUnreadCount;
}

bool _sharesMessageCluster(ChatMessage? leading, ChatMessage? trailing) {
  if (leading == null || trailing == null) {
    return false;
  }
  if (leading.isSystemNotice || trailing.isSystemNotice) {
    return false;
  }
  if (leading.direction != trailing.direction) {
    return false;
  }
  if (leading.senderLabel != trailing.senderLabel) {
    return false;
  }
  if (!DateUtils.isSameDay(leading.timestamp, trailing.timestamp)) {
    return false;
  }
  return trailing.timestamp.difference(leading.timestamp).inMinutes.abs() <= 8;
}

String _formatTimelineDayLabel(DateTime timestamp, DateTime now) {
  if (DateUtils.isSameDay(timestamp, now)) {
    return '今天';
  }

  final yesterday = now.subtract(const Duration(days: 1));
  if (DateUtils.isSameDay(timestamp, yesterday)) {
    return '昨天';
  }

  const weekdays = <String>['周一', '周二', '周三', '周四', '周五', '周六', '周日'];
  return '${timestamp.month}月${timestamp.day}日 ${weekdays[timestamp.weekday - 1]}';
}

String _formatRelativeConversationMoment(DateTime timestamp, DateTime now) {
  if (DateUtils.isSameDay(timestamp, now)) {
    return _formatClock(timestamp);
  }

  final yesterday = now.subtract(const Duration(days: 1));
  if (DateUtils.isSameDay(timestamp, yesterday)) {
    return '昨天 ${_formatClock(timestamp)}';
  }

  return '${timestamp.month}月${timestamp.day}日 ${_formatClock(timestamp)}';
}

String _formatClock(DateTime timestamp) {
  final hour = timestamp.hour.toString().padLeft(2, '0');
  final minute = timestamp.minute.toString().padLeft(2, '0');
  return '$hour:$minute';
}
