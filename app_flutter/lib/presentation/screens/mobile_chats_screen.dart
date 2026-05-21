import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../application/chat_providers.dart';
import '../../domain/entities/models.dart';
import '../theme/app_icons.dart';
import '../theme/app_theme.dart';
import '../theme/app_tokens.dart';
import '../widgets/inbox/conversation_filter_tabs.dart';
import '../widgets/inbox/conversation_row.dart';

class MobileChatsScreen extends ConsumerStatefulWidget {
  const MobileChatsScreen({
    super.key,
    this.onOpenContacts,
    this.onOpenSettings,
  });

  final VoidCallback? onOpenContacts;
  final VoidCallback? onOpenSettings;

  @override
  ConsumerState<MobileChatsScreen> createState() => _MobileChatsScreenState();
}

class _MobileChatsScreenState extends ConsumerState<MobileChatsScreen> {
  final ScrollController _scrollController = ScrollController();
  final TextEditingController _searchController = TextEditingController();
  final FocusNode _searchFocusNode = FocusNode();
  ConversationFilterPreset _filter = ConversationFilterPreset.all;
  bool _filtersVisible = false;
  bool _searchVisible = false;
  String _searchQuery = '';
  double _topPullDistance = 0;

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
    _scrollController.dispose();
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

  void _openSearch() {
    if (!_searchVisible) {
      setState(() {
        _searchVisible = true;
      });
    }
    _searchFocusNode.requestFocus();
  }

  void _clearSearch() {
    if (_searchController.text.isNotEmpty) {
      _searchController.clear();
    }
    if (Theme.of(context).platform != TargetPlatform.iOS) {
      setState(() {
        _searchVisible = false;
      });
    }
  }

  bool _handleScrollNotification(ScrollNotification notification) {
    final atTop =
        notification.metrics.pixels <= notification.metrics.minScrollExtent + 1;
    if (!_filtersVisible && atTop) {
      if (notification is OverscrollNotification &&
          notification.overscroll < 0) {
        _topPullDistance += -notification.overscroll;
      } else if (notification is ScrollUpdateNotification) {
        final dragDelta = notification.dragDetails?.delta.dy ?? 0;
        if (dragDelta > 0) {
          _topPullDistance += dragDelta;
        }
      }

      if (_topPullDistance >= 38) {
        setState(() {
          _filtersVisible = true;
        });
        _topPullDistance = 0;
      }
    }

    if (notification is ScrollEndNotification || !atTop) {
      _topPullDistance = 0;
    }
    return false;
  }

  void _hideFilters() {
    if (!_filtersVisible && _filter == ConversationFilterPreset.all) {
      return;
    }
    setState(() {
      _filtersVisible = false;
      _filter = ConversationFilterPreset.all;
    });
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isCupertinoStyle = theme.platform == TargetPlatform.iOS;
    final rawConversations = ref
        .watch(conversationsProvider)
        .maybeWhen(
          data: (value) => value,
          orElse: () => const <ConversationSummary>[],
        );
    final searchVisible = isCupertinoStyle || _searchVisible;
    final conversations = filterConversationsBySearch(
      filterConversationsByPreset(
        rawConversations,
        _filtersVisible ? _filter : ConversationFilterPreset.all,
      ),
      _searchQuery,
    );
    final topHeight = _mobileChatsTopHeight(
      cupertinoStyle: isCupertinoStyle,
      filtersVisible: _filtersVisible,
      searchVisible: searchVisible,
    );
    final fileAssistantId = _firstConversationIdOfKind(
      rawConversations,
      ConversationKind.fileAssistant,
    );

    return Stack(
      children: <Widget>[
        Positioned.fill(
          child: NotificationListener<ScrollNotification>(
            onNotification: _handleScrollNotification,
            child: ListView.separated(
              controller: _scrollController,
              key: const Key('mobile-chats-list'),
              padding: EdgeInsets.fromLTRB(
                0,
                topHeight,
                0,
                isCupertinoStyle ? 18 : 82,
              ),
              physics: const AlwaysScrollableScrollPhysics(
                parent: BouncingScrollPhysics(),
              ),
              itemCount: conversations.length,
              separatorBuilder: (_, _) => Divider(
                height: 1,
                indent: isCupertinoStyle ? 76 : 78,
                endIndent: 14,
                color: theme.extension<AppShellColors>()!.divider.withValues(
                  alpha: isCupertinoStyle ? 0.14 : 0.20,
                ),
              ),
              itemBuilder: (context, index) {
                final conversation = conversations[index];
                return InboxConversationRow(
                  conversation: conversation,
                  selected: false,
                  compact: isCupertinoStyle,
                  onTap: () {
                    _hideFilters();
                    ref
                        .read(chatActionsProvider)
                        .selectConversation(conversation.id);
                  },
                );
              },
            ),
          ),
        ),
        if (conversations.isEmpty && _searchQuery.trim().isNotEmpty)
          Positioned.fill(
            top: topHeight,
            child: _EmptySearchState(query: _searchQuery),
          ),
        Positioned(
          top: 0,
          left: 0,
          right: 0,
          child: _ChatsTopChrome(
            cupertinoStyle: isCupertinoStyle,
            filter: _filter,
            filtersVisible: _filtersVisible,
            searchVisible: searchVisible,
            searchController: _searchController,
            searchFocusNode: _searchFocusNode,
            searchQuery: _searchQuery,
            onFilterChanged: (value) => setState(() => _filter = value),
            onSearchPressed: _openSearch,
            onClearSearch: _clearSearch,
            onCreateGroup: _createGroup,
            onAddFriend: _showAddFriendDialog,
            onOpenContacts: _openContacts,
            onOpenSettings: _openSettings,
            onOpenFileAssistant: fileAssistantId == null
                ? null
                : () => _selectConversation(fileAssistantId),
          ),
        ),
        if (!isCupertinoStyle)
          Positioned(
            key: const Key('mobile-chats-fab-compose'),
            right: 18,
            bottom: 20,
            child: FloatingActionButton(
              onPressed: () => _showMobileComposeMenu(
                context,
                cupertinoStyle: isCupertinoStyle,
                onCreateGroup: _createGroup,
                onAddFriend: _showAddFriendDialog,
                onOpenContacts: _openContacts,
                onOpenSettings: _openSettings,
                onOpenFileAssistant: fileAssistantId == null
                    ? null
                    : () => _selectConversation(fileAssistantId),
              ),
              tooltip: '新建聊天',
              elevation: 0,
              focusElevation: 0,
              hoverElevation: 0,
              highlightElevation: 0,
              backgroundColor: theme.colorScheme.primary,
              foregroundColor: Colors.white,
              shape: CircleBorder(
                side: BorderSide(
                  color: Colors.white.withValues(alpha: 0.10),
                  width: 0.8,
                ),
              ),
              child: const AppIcon(AppSemanticIcon.contactAdd, size: 21),
            ),
          ),
      ],
    );
  }

  Future<void> _createGroup() async {
    final conversationId = await ref.read(chatActionsProvider).createGroup();
    if (!mounted) {
      return;
    }
    if (conversationId == null) {
      _showStatus('暂时无法创建群聊');
      return;
    }
    _hideFilters();
  }

  void _selectConversation(String conversationId) {
    _hideFilters();
    ref.read(chatActionsProvider).selectConversation(conversationId);
  }

  void _openContacts() {
    final handler = widget.onOpenContacts;
    if (handler != null) {
      handler();
      return;
    }
    _showStatus('请打开联系人页选择联系人');
  }

  void _openSettings() {
    final handler = widget.onOpenSettings;
    if (handler != null) {
      handler();
      return;
    }
    _showStatus('请打开设置页使用扫码和设备同步');
  }

  Future<void> _showAddFriendDialog() async {
    final request = await showDialog<_FriendRequestDraft>(
      context: context,
      builder: (context) => const _AddFriendDialog(),
    );
    if (request == null) {
      return;
    }
    if (!mounted) {
      return;
    }
    await ref
        .read(chatActionsProvider)
        .sendFriendRequest(
          accountId: request.accountId,
          remark: request.remark,
        );
    if (!mounted) {
      return;
    }
    _showStatus('好友请求已发送');
  }

  void _showStatus(String message) {
    ScaffoldMessenger.maybeOf(
      context,
    )?.showSnackBar(SnackBar(content: Text(message)));
  }
}

double _mobileChatsTopHeight({
  required bool cupertinoStyle,
  required bool filtersVisible,
  required bool searchVisible,
}) {
  if (cupertinoStyle) {
    return filtersVisible
        ? ImUiMetrics.iosChatListTopWithFilters
        : ImUiMetrics.iosChatListTop;
  }
  if (searchVisible && filtersVisible) {
    return ImUiMetrics.androidChatListTopWithSearchAndFilters;
  }
  if (searchVisible) {
    return ImUiMetrics.androidChatListTopWithSearch;
  }
  if (filtersVisible) {
    return ImUiMetrics.androidChatListTopWithFilters;
  }
  return ImUiMetrics.androidChatListTop;
}

class _ChatsTopChrome extends StatelessWidget {
  const _ChatsTopChrome({
    required this.cupertinoStyle,
    required this.filter,
    required this.filtersVisible,
    required this.searchVisible,
    required this.searchController,
    required this.searchFocusNode,
    required this.searchQuery,
    required this.onFilterChanged,
    required this.onSearchPressed,
    required this.onClearSearch,
    required this.onCreateGroup,
    required this.onAddFriend,
    required this.onOpenContacts,
    required this.onOpenSettings,
    required this.onOpenFileAssistant,
  });

  final bool cupertinoStyle;
  final ConversationFilterPreset filter;
  final bool filtersVisible;
  final bool searchVisible;
  final TextEditingController searchController;
  final FocusNode searchFocusNode;
  final String searchQuery;
  final ValueChanged<ConversationFilterPreset> onFilterChanged;
  final VoidCallback onSearchPressed;
  final VoidCallback onClearSearch;
  final Future<void> Function() onCreateGroup;
  final Future<void> Function() onAddFriend;
  final VoidCallback onOpenContacts;
  final VoidCallback onOpenSettings;
  final VoidCallback? onOpenFileAssistant;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    const horizontal = 12.0;
    final child = Padding(
      padding: EdgeInsets.fromLTRB(
        horizontal,
        cupertinoStyle ? 5 : 4,
        horizontal,
        cupertinoStyle ? 9 : 4,
      ),
      child: Column(
        key: Key(
          cupertinoStyle
              ? 'mobile-chats-top-chrome-cupertino'
              : 'mobile-chats-top-chrome-material',
        ),
        crossAxisAlignment: CrossAxisAlignment.stretch,
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          SizedBox(
            height: cupertinoStyle ? 40 : 40,
            child: Row(
              key: Key(
                cupertinoStyle
                    ? 'mobile-chats-header-cupertino'
                    : 'mobile-chats-header-material',
              ),
              children: <Widget>[
                Text(
                  '聊天',
                  key: Key(
                    cupertinoStyle
                        ? 'mobile-chats-large-title-cupertino'
                        : 'mobile-chats-title-material',
                  ),
                  style:
                      (cupertinoStyle
                              ? theme.textTheme.headlineMedium
                              : theme.textTheme.titleLarge)
                          ?.copyWith(
                            fontSize: cupertinoStyle ? 31 : 20,
                            fontWeight: FontWeight.w700,
                            letterSpacing: 0,
                            height: cupertinoStyle ? 1.0 : 1.05,
                          ),
                ),
                const Spacer(),
                if (cupertinoStyle)
                  _IconChromeButton(
                    key: const Key('mobile-chats-nav-compose'),
                    icon: AppSemanticIcon.contactAdd,
                    cupertinoStyle: cupertinoStyle,
                    tooltip: '新聊天',
                    onPressed: () => _showMobileComposeMenu(
                      context,
                      cupertinoStyle: cupertinoStyle,
                      onCreateGroup: onCreateGroup,
                      onAddFriend: onAddFriend,
                      onOpenContacts: onOpenContacts,
                      onOpenSettings: onOpenSettings,
                      onOpenFileAssistant: onOpenFileAssistant,
                    ),
                  )
                else
                  _IconChromeButton(
                    key: const Key('mobile-chats-nav-search'),
                    icon: AppSemanticIcon.search,
                    cupertinoStyle: cupertinoStyle,
                    tooltip: '搜索',
                    onPressed: onSearchPressed,
                  ),
              ],
            ),
          ),
          if (cupertinoStyle || searchVisible) ...<Widget>[
            const SizedBox(height: 6),
            _SearchShell(
              cupertinoStyle: cupertinoStyle,
              controller: searchController,
              focusNode: searchFocusNode,
              query: searchQuery,
              onClear: onClearSearch,
            ),
          ],
          if (filtersVisible) ...<Widget>[
            SizedBox(height: cupertinoStyle ? 8 : 6),
            KeyedSubtree(
              key: const ValueKey('mobile-chats-filter-tabs'),
              child: ConversationFilterTabs(
                value: filter,
                onChanged: onFilterChanged,
              ),
            ),
          ],
        ],
      ),
    );

    if (cupertinoStyle) {
      return DecoratedBox(
        key: const Key('mobile-chats-top-matte-shell-cupertino'),
        decoration: BoxDecoration(
          color: theme.colorScheme.surface.withValues(alpha: 0.985),
          border: Border(
            bottom: BorderSide(color: palette.divider.withValues(alpha: 0.08)),
          ),
        ),
        child: child,
      );
    }

    return DecoratedBox(
      decoration: BoxDecoration(
        color: theme.colorScheme.surface.withValues(alpha: 0.98),
        border: Border(
          bottom: BorderSide(color: palette.divider.withValues(alpha: 0.12)),
        ),
      ),
      child: child,
    );
  }
}

void _showMobileComposeMenu(
  BuildContext context, {
  required bool cupertinoStyle,
  required Future<void> Function() onCreateGroup,
  required Future<void> Function() onAddFriend,
  required VoidCallback onOpenContacts,
  required VoidCallback onOpenSettings,
  required VoidCallback? onOpenFileAssistant,
}) {
  final actions = <_MobileComposeAction>[
    _MobileComposeAction('发起聊天', () async => onOpenContacts()),
    _MobileComposeAction('创建群聊', onCreateGroup),
    _MobileComposeAction('加好友', onAddFriend),
    if (onOpenFileAssistant != null)
      _MobileComposeAction('文件助手', () async => onOpenFileAssistant()),
    _MobileComposeAction('扫一扫', () async => onOpenSettings()),
  ];
  if (cupertinoStyle) {
    showCupertinoModalPopup<void>(
      context: context,
      builder: (context) => CupertinoActionSheet(
        actions: <Widget>[
          for (final action in actions)
            CupertinoActionSheetAction(
              onPressed: () => _runMobileComposeAction(context, action),
              child: Text(action.label),
            ),
        ],
        cancelButton: CupertinoActionSheetAction(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('取消'),
        ),
      ),
    );
    return;
  }
  showModalBottomSheet<void>(
    context: context,
    builder: (context) => SafeArea(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          for (final action in actions)
            ListTile(
              title: Text(action.label),
              onTap: () => _runMobileComposeAction(context, action),
            ),
        ],
      ),
    ),
  );
}

class _MobileComposeAction {
  const _MobileComposeAction(this.label, this.run);

  final String label;
  final Future<void> Function() run;
}

void _runMobileComposeAction(
  BuildContext context,
  _MobileComposeAction action,
) {
  Navigator.of(context).pop();
  action.run();
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

class _FriendRequestDraft {
  const _FriendRequestDraft({required this.accountId, required this.remark});

  final String accountId;
  final String remark;
}

class _AddFriendDialog extends StatefulWidget {
  const _AddFriendDialog();

  @override
  State<_AddFriendDialog> createState() => _AddFriendDialogState();
}

class _AddFriendDialogState extends State<_AddFriendDialog> {
  final TextEditingController _accountController = TextEditingController();
  final TextEditingController _remarkController = TextEditingController();

  @override
  void dispose() {
    _accountController.dispose();
    _remarkController.dispose();
    super.dispose();
  }

  void _submit() {
    final accountId = _accountController.text.trim();
    if (accountId.isEmpty) {
      return;
    }
    Navigator.of(context).pop(
      _FriendRequestDraft(
        accountId: accountId,
        remark: _remarkController.text.trim(),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('加好友'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          TextField(
            key: const Key('mobile-add-friend-username'),
            controller: _accountController,
            textInputAction: TextInputAction.next,
            decoration: const InputDecoration(labelText: '账号'),
          ),
          TextField(
            key: const Key('mobile-add-friend-remark'),
            controller: _remarkController,
            textInputAction: TextInputAction.done,
            onSubmitted: (_) => _submit(),
            decoration: const InputDecoration(labelText: '备注'),
          ),
        ],
      ),
      actions: <Widget>[
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('取消'),
        ),
        FilledButton(
          key: const Key('mobile-add-friend-submit'),
          onPressed: _submit,
          child: const Text('发送请求'),
        ),
      ],
    );
  }
}

class _SearchShell extends StatelessWidget {
  const _SearchShell({
    required this.cupertinoStyle,
    required this.controller,
    required this.focusNode,
    required this.query,
    required this.onClear,
  });

  final bool cupertinoStyle;
  final TextEditingController controller;
  final FocusNode focusNode;
  final String query;
  final VoidCallback onClear;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final searchSurface = cupertinoStyle
        ? Color.alphaBlend(
            palette.surfaceVariant.withValues(alpha: 0.22),
            theme.colorScheme.surface.withValues(alpha: 0.84),
          )
        : palette.surfaceVariant.withValues(alpha: 0.22);
    final searchBorder = palette.divider.withValues(
      alpha: cupertinoStyle ? 0.10 : 0.12,
    );
    final content = Container(
      key: Key(
        cupertinoStyle
            ? 'mobile-chats-search-cupertino'
            : 'mobile-chats-search-material',
      ),
      height: cupertinoStyle ? 33 : 34,
      padding: const EdgeInsets.symmetric(horizontal: 12),
      decoration: BoxDecoration(
        color: searchSurface,
        borderRadius: BorderRadius.circular(cupertinoStyle ? 12 : 14),
        border: Border.all(color: searchBorder),
      ),
      child: Row(
        children: <Widget>[
          AppIcon(
            AppSemanticIcon.search,
            size: 15,
            color: palette.textSecondary.withValues(alpha: 0.72),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: TextField(
              controller: controller,
              focusNode: focusNode,
              key: Key(
                cupertinoStyle
                    ? 'mobile-chats-search-input-cupertino'
                    : 'mobile-chats-search-input-material',
              ),
              textInputAction: TextInputAction.search,
              enableSuggestions: false,
              autocorrect: false,
              smartDashesType: SmartDashesType.disabled,
              smartQuotesType: SmartQuotesType.disabled,
              decoration: InputDecoration(
                hintText: '搜索聊天、群、频道和文件',
                border: InputBorder.none,
                isDense: true,
                contentPadding: const EdgeInsets.symmetric(vertical: 6),
                hintStyle: theme.textTheme.bodyMedium?.copyWith(
                  color: palette.textTertiary,
                  fontSize: cupertinoStyle ? 12.8 : 13.2,
                ),
              ),
              style: theme.textTheme.bodyMedium?.copyWith(
                color: theme.colorScheme.onSurface,
                fontSize: cupertinoStyle ? 12.8 : 13.2,
              ),
            ),
          ),
          if (query.trim().isNotEmpty)
            _SearchClearButton(
              cupertinoStyle: cupertinoStyle,
              onPressed: onClear,
            ),
        ],
      ),
    );

    return content;
  }
}

class _SearchClearButton extends StatelessWidget {
  const _SearchClearButton({
    required this.cupertinoStyle,
    required this.onPressed,
  });

  final bool cupertinoStyle;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    final icon = AppIcon(
      AppSemanticIcon.close,
      size: 12,
      color: Theme.of(
        context,
      ).textTheme.bodySmall?.color?.withValues(alpha: 0.62),
    );
    if (cupertinoStyle) {
      return CupertinoButton(
        key: const Key('mobile-chats-search-clear'),
        padding: EdgeInsets.zero,
        minimumSize: const Size(28, 28),
        onPressed: onPressed,
        child: icon,
      );
    }
    return IconButton(
      key: const Key('mobile-chats-search-clear'),
      onPressed: onPressed,
      icon: icon,
      constraints: const BoxConstraints.tightFor(width: 28, height: 28),
      padding: EdgeInsets.zero,
      style: IconButton.styleFrom(
        tapTargetSize: MaterialTapTargetSize.shrinkWrap,
      ),
    );
  }
}

class _EmptySearchState extends StatelessWidget {
  const _EmptySearchState({required this.query});

  final String query;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return IgnorePointer(
      child: Center(
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 32),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              AppIcon(
                AppSemanticIcon.search,
                size: 31,
                color: palette.textTertiary.withValues(alpha: 0.72),
              ),
              const SizedBox(height: 12),
              Text(
                '没有找到相关会话',
                key: const Key('mobile-chats-search-empty-title'),
                style: theme.textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
              ),
              const SizedBox(height: 5),
              Text(
                query.trim(),
                key: const Key('mobile-chats-search-empty-query'),
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
                textAlign: TextAlign.center,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: palette.textSecondary,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _IconChromeButton extends StatelessWidget {
  const _IconChromeButton({
    super.key,
    required this.icon,
    required this.cupertinoStyle,
    required this.tooltip,
    required this.onPressed,
  });

  final AppSemanticIcon icon;
  final bool cupertinoStyle;
  final String tooltip;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    if (cupertinoStyle) {
      return CupertinoButton(
        onPressed: onPressed,
        padding: EdgeInsets.zero,
        minimumSize: const Size(34, 34),
        child: AppIcon(
          icon,
          size: 19,
          color: Theme.of(context).colorScheme.primary,
        ),
      );
    }
    return Tooltip(
      message: tooltip,
      child: IconButton(
        onPressed: onPressed,
        icon: AppIcon(icon, size: 19),
        constraints: const BoxConstraints.tightFor(width: 38, height: 38),
        style: IconButton.styleFrom(
          tapTargetSize: MaterialTapTargetSize.shrinkWrap,
          padding: EdgeInsets.zero,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(AppTokens.radiusSm),
          ),
        ),
      ),
    );
  }
}
