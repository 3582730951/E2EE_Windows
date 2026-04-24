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
  const MobileChatsScreen({super.key});

  @override
  ConsumerState<MobileChatsScreen> createState() => _MobileChatsScreenState();
}

class _MobileChatsScreenState extends ConsumerState<MobileChatsScreen> {
  final ScrollController _scrollController = ScrollController();
  ConversationFilterPreset _filter = ConversationFilterPreset.all;
  bool _filtersVisible = false;
  double _topPullDistance = 0;

  @override
  void dispose() {
    _scrollController.dispose();
    super.dispose();
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
    final conversations = filterConversationsByPreset(
      rawConversations,
      _filtersVisible ? _filter : ConversationFilterPreset.all,
    );
    final topHeight = _mobileChatsTopHeight(
      cupertinoStyle: isCupertinoStyle,
      filtersVisible: _filtersVisible,
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
        Positioned(
          top: 0,
          left: 0,
          right: 0,
          child: _ChatsTopChrome(
            cupertinoStyle: isCupertinoStyle,
            filter: _filter,
            filtersVisible: _filtersVisible,
            onFilterChanged: (value) => setState(() => _filter = value),
          ),
        ),
        if (!isCupertinoStyle)
          Positioned(
            key: const Key('mobile-chats-fab-compose'),
            right: 18,
            bottom: 20,
            child: FloatingActionButton(
              onPressed: () {},
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
}

double _mobileChatsTopHeight({
  required bool cupertinoStyle,
  required bool filtersVisible,
}) {
  if (cupertinoStyle) {
    return filtersVisible
        ? ImUiMetrics.iosChatListTopWithFilters
        : ImUiMetrics.iosChatListTop;
  }
  return filtersVisible
      ? ImUiMetrics.androidChatListTopWithFilters
      : ImUiMetrics.androidChatListTop;
}

class _ChatsTopChrome extends StatelessWidget {
  const _ChatsTopChrome({
    required this.cupertinoStyle,
    required this.filter,
    required this.filtersVisible,
    required this.onFilterChanged,
  });

  final bool cupertinoStyle;
  final ConversationFilterPreset filter;
  final bool filtersVisible;
  final ValueChanged<ConversationFilterPreset> onFilterChanged;

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
                    onPressed: () => _showComposeMenu(context),
                  )
                else
                  _IconChromeButton(
                    key: const Key('mobile-chats-nav-search'),
                    icon: AppSemanticIcon.search,
                    cupertinoStyle: cupertinoStyle,
                    tooltip: '搜索',
                    onPressed: () {},
                  ),
              ],
            ),
          ),
          if (cupertinoStyle) ...<Widget>[
            const SizedBox(height: 6),
            _SearchShell(cupertinoStyle: cupertinoStyle),
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

  void _showComposeMenu(BuildContext context) {
    final actions = const <String>['发起聊天', '创建群聊', '加好友', '扫一扫'];
    if (cupertinoStyle) {
      showCupertinoModalPopup<void>(
        context: context,
        builder: (context) => CupertinoActionSheet(
          actions: <Widget>[
            for (final action in actions)
              CupertinoActionSheetAction(onPressed: () {}, child: Text(action)),
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
              ListTile(title: Text(action), onTap: () {}),
          ],
        ),
      ),
    );
  }
}

class _SearchShell extends StatelessWidget {
  const _SearchShell({required this.cupertinoStyle});

  final bool cupertinoStyle;

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
            child: Text(
              '搜索聊天、群、频道和文件',
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: theme.textTheme.bodyMedium?.copyWith(
                color: palette.textTertiary,
                fontSize: cupertinoStyle ? 12.8 : 13.2,
              ),
            ),
          ),
        ],
      ),
    );

    return content;
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
