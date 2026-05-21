import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../application/chat_providers.dart';
import '../../domain/entities/models.dart';
import '../theme/app_icons.dart';
import '../theme/app_theme.dart';
import '../theme/app_tokens.dart';
import '../widgets/social_avatar.dart';

class MobileContactsScreen extends ConsumerStatefulWidget {
  const MobileContactsScreen({super.key});

  @override
  ConsumerState<MobileContactsScreen> createState() =>
      _MobileContactsScreenState();
}

class _MobileContactsScreenState extends ConsumerState<MobileContactsScreen> {
  final TextEditingController _searchController = TextEditingController();
  String _query = '';

  @override
  void dispose() {
    _searchController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final cupertinoStyle = theme.platform == TargetPlatform.iOS;
    final contacts = ref
        .watch(contactsProvider)
        .maybeWhen(
          data: (value) => value,
          orElse: () => const <ContactProfile>[],
        );
    final conversations = ref
        .watch(conversationsProvider)
        .maybeWhen(
          data: (value) => value,
          orElse: () => const <ConversationSummary>[],
        );
    final query = _query.trim().toLowerCase();
    final searchedContacts = query.isEmpty
        ? const <ContactProfile>[]
        : contacts.where((contact) => _matchesQuery(contact, query)).toList();
    final groupContacts = contacts
        .where((contact) => contact.groupLabel == '群聊')
        .toList(growable: false);
    final serviceContacts = contacts
        .where((contact) => contact.groupLabel == '公众号或服务号')
        .toList(growable: false);
    final newFriendContacts = contacts
        .where((contact) => contact.groupLabel == '新的朋友')
        .toList(growable: false);
    final friendContacts =
        contacts
            .where(
              (contact) =>
                  contact.groupLabel != '群聊' &&
                  contact.groupLabel != '公众号或服务号' &&
                  contact.groupLabel != '新的朋友',
            )
            .toList(growable: false)
          ..sort(_contactCompare);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        _ContactsTopChrome(
          controller: _searchController,
          cupertinoStyle: cupertinoStyle,
          onChanged: (value) => setState(() => _query = value),
          onAddFriend: _showAddFriendDialog,
          onCreateGroup: _createGroup,
        ),
        Expanded(
          child: ListView(
            key: const Key('mobile-contacts-list'),
            padding: EdgeInsets.fromLTRB(
              cupertinoStyle ? 4 : 2,
              12,
              cupertinoStyle ? 4 : 2,
              AppTokens.spacingXl,
            ),
            children: query.isNotEmpty
                ? <Widget>[
                    _ContactsSectionHeader(
                      key: const Key('mobile-contacts-section-search'),
                      title: '搜索结果',
                      caption: '${searchedContacts.length} 个匹配',
                    ),
                    const SizedBox(height: 8),
                    _ContactsGroup(
                      children: searchedContacts.isEmpty
                          ? const <Widget>[_ContactsEmptyState(text: '没有找到联系人')]
                          : <Widget>[
                              for (
                                var index = 0;
                                index < searchedContacts.length;
                                index++
                              )
                                _GroupedContactRow(
                                  contact: searchedContacts[index],
                                  conversation: _matchedConversation(
                                    searchedContacts[index],
                                    conversations,
                                  ),
                                  isLast: index == searchedContacts.length - 1,
                                  onTap: () => _openContactChat(
                                    searchedContacts[index],
                                    conversations,
                                  ),
                                ),
                            ],
                    ),
                  ]
                : <Widget>[
                    _QuickEntryGrid(
                      cupertinoStyle: cupertinoStyle,
                      onAddFriend: _showAddFriendDialog,
                      onCreateGroup: _createGroup,
                    ),
                    SizedBox(height: cupertinoStyle ? 12 : 16),
                    const _ContactsSectionHeader(
                      key: Key('mobile-contacts-section-new-friends'),
                      title: '新的朋友',
                      caption: '好友申请',
                    ),
                    const SizedBox(height: 8),
                    _ContactsGroup(
                      children: <Widget>[
                        _QuickContactRow(
                          key: const Key('mobile-contacts-new-friends-row'),
                          icon: AppSemanticIcon.contactAdd,
                          title: '新的朋友',
                          subtitle: '查看好友申请和通讯录推荐',
                          onTap: () => _showNewFriends(newFriendContacts),
                        ),
                      ],
                    ),
                    SizedBox(height: cupertinoStyle ? 12 : 16),
                    _ContactsSectionHeader(
                      key: const Key('mobile-contacts-section-groups'),
                      title: '群聊',
                      caption: '${groupContacts.length} 个群',
                    ),
                    const SizedBox(height: 8),
                    _ContactsGroup(
                      children: groupContacts.isEmpty
                          ? const <Widget>[_ContactsEmptyState(text: '还没有加入群聊')]
                          : <Widget>[
                              for (
                                var index = 0;
                                index < groupContacts.length;
                                index++
                              )
                                _GroupedContactRow(
                                  contact: groupContacts[index],
                                  conversation: _matchedConversation(
                                    groupContacts[index],
                                    conversations,
                                  ),
                                  isLast: index == groupContacts.length - 1,
                                  onTap: () => _openContactChat(
                                    groupContacts[index],
                                    conversations,
                                  ),
                                ),
                            ],
                    ),
                    SizedBox(height: cupertinoStyle ? 12 : 16),
                    _ContactsSectionHeader(
                      key: const Key('mobile-contacts-section-friends'),
                      title: '我的好友',
                      caption: '${friendContacts.length} 位好友',
                    ),
                    const SizedBox(height: 8),
                    _ContactsGroup(
                      children: friendContacts.isEmpty
                          ? const <Widget>[_ContactsEmptyState(text: '还没有好友')]
                          : <Widget>[
                              for (
                                var index = 0;
                                index < friendContacts.length;
                                index++
                              )
                                _GroupedContactRow(
                                  contact: friendContacts[index],
                                  conversation: _matchedConversation(
                                    friendContacts[index],
                                    conversations,
                                  ),
                                  isLast: index == friendContacts.length - 1,
                                  onTap: () => _openContactChat(
                                    friendContacts[index],
                                    conversations,
                                  ),
                                ),
                            ],
                    ),
                    SizedBox(height: cupertinoStyle ? 12 : 16),
                    _ContactsSectionHeader(
                      key: const Key('mobile-contacts-section-services'),
                      title: '公众号或服务号',
                      caption: '${serviceContacts.length} 个服务',
                    ),
                    const SizedBox(height: 8),
                    _ContactsGroup(
                      children: serviceContacts.isEmpty
                          ? const <Widget>[_ContactsEmptyState(text: '暂无服务号')]
                          : <Widget>[
                              for (
                                var index = 0;
                                index < serviceContacts.length;
                                index++
                              )
                                _GroupedContactRow(
                                  contact: serviceContacts[index],
                                  conversation: _matchedConversation(
                                    serviceContacts[index],
                                    conversations,
                                  ),
                                  isLast: index == serviceContacts.length - 1,
                                  onTap: () => _openContactChat(
                                    serviceContacts[index],
                                    conversations,
                                  ),
                                ),
                            ],
                    ),
                  ],
          ),
        ),
      ],
    );
  }

  void _openContactChat(
    ContactProfile contact,
    List<ConversationSummary> conversations,
  ) {
    final conversation = _matchedConversation(contact, conversations);
    if (conversation == null) {
      return;
    }
    ref.read(chatActionsProvider).selectConversation(conversation.id);
    context.go('/app/chats');
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
    context.go('/app/chats');
  }

  Future<void> _showAddFriendDialog() async {
    final request = await showDialog<_ContactFriendRequestDraft>(
      context: context,
      builder: (context) => const _ContactAddFriendDialog(),
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

  void _showNewFriends(List<ContactProfile> contacts) {
    showModalBottomSheet<void>(
      context: context,
      builder: (context) => SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(18, 14, 18, 18),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                '新的朋友',
                style: Theme.of(
                  context,
                ).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w700),
              ),
              const SizedBox(height: 10),
              if (contacts.isEmpty)
                const Text('暂无新的好友申请')
              else
                for (final contact in contacts.take(6))
                  ListTile(
                    dense: true,
                    contentPadding: EdgeInsets.zero,
                    leading: SocialAvatar(
                      id: contact.id,
                      title: contact.displayName,
                      size: 36,
                    ),
                    title: Text(contact.displayName),
                    subtitle: Text(contact.statusLabel),
                  ),
              const SizedBox(height: 10),
              Align(
                alignment: Alignment.centerRight,
                child: FilledButton(
                  key: const Key('mobile-contacts-new-friends-add'),
                  onPressed: () {
                    Navigator.of(context).pop();
                    _showAddFriendDialog();
                  },
                  child: const Text('添加好友'),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  void _showStatus(String message) {
    ScaffoldMessenger.maybeOf(
      context,
    )?.showSnackBar(SnackBar(content: Text(message)));
  }
}

class _ContactsTopChrome extends StatelessWidget {
  const _ContactsTopChrome({
    required this.controller,
    required this.cupertinoStyle,
    required this.onChanged,
    required this.onAddFriend,
    required this.onCreateGroup,
  });

  final TextEditingController controller;
  final bool cupertinoStyle;
  final ValueChanged<String> onChanged;
  final Future<void> Function() onAddFriend;
  final Future<void> Function() onCreateGroup;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      key: Key(
        cupertinoStyle
            ? 'mobile-contacts-top-chrome-cupertino'
            : 'mobile-contacts-top-chrome-material',
      ),
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        SizedBox(
          height: cupertinoStyle ? 34 : 42,
          child: Row(
            children: <Widget>[
              Expanded(
                child: Text(
                  '联系人',
                  key: Key(
                    cupertinoStyle
                        ? 'mobile-contacts-large-title-cupertino'
                        : 'mobile-contacts-large-title-material',
                  ),
                  style: theme.textTheme.headlineMedium?.copyWith(
                    fontSize: cupertinoStyle ? 29 : 26,
                    fontWeight: FontWeight.w700,
                    letterSpacing: 0,
                  ),
                ),
              ),
              _TopIconButton(
                key: const Key('mobile-contacts-add-friend'),
                icon: AppSemanticIcon.contactAdd,
                tooltip: '添加好友',
                cupertinoStyle: cupertinoStyle,
                onPressed: onAddFriend,
              ),
              const SizedBox(width: 4),
              _TopIconButton(
                key: const Key('mobile-contacts-create-group'),
                icon: AppSemanticIcon.groupChat,
                tooltip: '创建群聊',
                cupertinoStyle: cupertinoStyle,
                onPressed: onCreateGroup,
              ),
            ],
          ),
        ),
        const SizedBox(height: 9),
        _ContactsSearchField(
          controller: controller,
          cupertinoStyle: cupertinoStyle,
          onChanged: onChanged,
        ),
      ],
    );
  }
}

class _ContactsSearchField extends StatelessWidget {
  const _ContactsSearchField({
    required this.controller,
    required this.cupertinoStyle,
    required this.onChanged,
  });

  final TextEditingController controller;
  final bool cupertinoStyle;
  final ValueChanged<String> onChanged;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Container(
      key: const ValueKey('contacts-search-field'),
      height: cupertinoStyle ? 34 : 40,
      padding: const EdgeInsets.symmetric(horizontal: 12),
      decoration: BoxDecoration(
        color: palette.surfaceVariant.withValues(alpha: 0.18),
        borderRadius: BorderRadius.circular(cupertinoStyle ? 12 : 16),
        border: Border.all(color: palette.divider.withValues(alpha: 0.12)),
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
              onChanged: onChanged,
              enableSuggestions: false,
              enableIMEPersonalizedLearning: false,
              autocorrect: false,
              smartDashesType: SmartDashesType.disabled,
              smartQuotesType: SmartQuotesType.disabled,
              decoration: InputDecoration(
                hintText: '搜索好友、群聊和服务号',
                border: InputBorder.none,
                isDense: true,
                contentPadding: EdgeInsets.zero,
                hintStyle: theme.textTheme.bodyMedium?.copyWith(
                  color: palette.textTertiary,
                  fontSize: 13,
                ),
              ),
              style: theme.textTheme.bodyMedium?.copyWith(fontSize: 13),
            ),
          ),
        ],
      ),
    );
  }
}

class _QuickEntryGrid extends StatelessWidget {
  const _QuickEntryGrid({
    required this.cupertinoStyle,
    required this.onAddFriend,
    required this.onCreateGroup,
  });

  final bool cupertinoStyle;
  final Future<void> Function() onAddFriend;
  final Future<void> Function() onCreateGroup;

  @override
  Widget build(BuildContext context) {
    return Row(
      key: const Key('mobile-contacts-quick-entries'),
      children: <Widget>[
        Expanded(
          child: _QuickEntryCard(
            key: const Key('mobile-contacts-quick-add-friend'),
            icon: AppSemanticIcon.contactAdd,
            title: '添加好友',
            onPressed: onAddFriend,
          ),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: _QuickEntryCard(
            key: const Key('mobile-contacts-quick-create-group'),
            icon: AppSemanticIcon.groupChat,
            title: '创建群聊',
            onPressed: onCreateGroup,
          ),
        ),
      ],
    );
  }
}

class _QuickEntryCard extends StatelessWidget {
  const _QuickEntryCard({
    super.key,
    required this.icon,
    required this.title,
    required this.onPressed,
  });

  final AppSemanticIcon icon;
  final String title;
  final Future<void> Function() onPressed;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final compact = theme.platform == TargetPlatform.iOS;
    final radius = BorderRadius.circular(14);
    return Material(
      color: Colors.transparent,
      child: InkWell(
        borderRadius: radius,
        onTap: onPressed,
        child: Ink(
          padding: EdgeInsets.symmetric(
            horizontal: 13,
            vertical: compact ? 9 : 12,
          ),
          decoration: BoxDecoration(
            color: theme.colorScheme.surface.withValues(alpha: 0.94),
            borderRadius: radius,
            border: Border.all(color: palette.divider.withValues(alpha: 0.12)),
          ),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: <Widget>[
              AppIcon(icon, size: 17, color: theme.colorScheme.primary),
              const SizedBox(width: 8),
              Text(
                title,
                style: theme.textTheme.labelLarge?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _ContactsSectionHeader extends StatelessWidget {
  const _ContactsSectionHeader({
    super.key,
    required this.title,
    required this.caption,
  });

  final String title;
  final String caption;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 4),
      child: Row(
        children: <Widget>[
          Expanded(
            child: Text(
              title,
              style: theme.textTheme.labelLarge?.copyWith(
                fontWeight: FontWeight.w800,
                color: palette.textSecondary,
              ),
            ),
          ),
          Text(caption, style: theme.textTheme.labelSmall),
        ],
      ),
    );
  }
}

class _ContactsGroup extends StatelessWidget {
  const _ContactsGroup({required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Container(
      decoration: BoxDecoration(
        color: theme.colorScheme.surface.withValues(alpha: 0.94),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: palette.divider.withValues(alpha: 0.12)),
      ),
      child: Column(children: children),
    );
  }
}

class _GroupedContactRow extends StatelessWidget {
  const _GroupedContactRow({
    required this.contact,
    required this.conversation,
    required this.isLast,
    required this.onTap,
  });

  final ContactProfile contact;
  final ConversationSummary? conversation;
  final bool isLast;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Column(
      children: <Widget>[
        _ContactRow(contact: contact, conversation: conversation, onTap: onTap),
        if (!isLast)
          Divider(
            height: 1,
            indent: 68,
            endIndent: 12,
            color: Theme.of(
              context,
            ).extension<AppShellColors>()!.divider.withValues(alpha: 0.18),
          ),
      ],
    );
  }
}

class _QuickContactRow extends StatelessWidget {
  const _QuickContactRow({
    super.key,
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  final AppSemanticIcon icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final compact = theme.platform == TargetPlatform.iOS;
    return Material(
      color: Colors.transparent,
      child: ListTile(
        dense: compact,
        minLeadingWidth: compact ? 38 : null,
        minVerticalPadding: compact ? 7 : null,
        contentPadding: const EdgeInsets.symmetric(horizontal: 13),
        leading: CircleAvatar(
          radius: compact ? 20 : 22,
          backgroundColor: theme.colorScheme.primary.withValues(alpha: 0.14),
          child: AppIcon(icon, color: theme.colorScheme.primary),
        ),
        title: Text(title),
        subtitle: Text(subtitle),
        trailing: const AppIcon(AppSemanticIcon.chevronRight),
        onTap: onTap,
      ),
    );
  }
}

class _ContactRow extends StatelessWidget {
  const _ContactRow({
    required this.contact,
    required this.conversation,
    required this.onTap,
  });

  final ContactProfile contact;
  final ConversationSummary? conversation;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    final compact = theme.platform == TargetPlatform.iOS;
    final primaryStatus = contact.signature?.trim().isNotEmpty == true
        ? contact.signature!.trim()
        : contact.statusLabel;
    final subtitleParts = <String>[
      primaryStatus,
      if (contact.lastSeenLabel != null) contact.lastSeenLabel!,
      if (contact.relationLabel != null) contact.relationLabel!,
      if (contact.requestStatus != null) contact.requestStatus!,
      if (contact.mutualGroupCount > 0) '${contact.mutualGroupCount} 个共同群',
    ];
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(16),
      child: Padding(
        padding: EdgeInsets.symmetric(
          horizontal: 13,
          vertical: compact ? 8 : 10,
        ),
        child: Row(
          children: <Widget>[
            SocialAvatar.contact(contact, size: compact ? 40 : 44),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Row(
                    children: <Widget>[
                      Expanded(
                        child: Text(
                          contact.displayName,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: theme.textTheme.titleMedium?.copyWith(
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                      ),
                      if (conversation?.unreadCount case final unread?
                          when unread > 0)
                        _UnreadBadge(count: unread),
                    ],
                  ),
                  const SizedBox(height: 3),
                  Text(
                    subtitleParts.join(' · '),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: palette.textSecondary.withValues(alpha: 0.76),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(width: 8),
            AppIcon(
              AppSemanticIcon.chevronRight,
              size: 18,
              color: palette.textTertiary,
            ),
          ],
        ),
      ),
    );
  }
}

class _UnreadBadge extends StatelessWidget {
  const _UnreadBadge({required this.count});

  final int count;

  @override
  Widget build(BuildContext context) {
    return Container(
      constraints: const BoxConstraints(minWidth: 20),
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.primary,
        borderRadius: BorderRadius.circular(AppTokens.radiusFull),
      ),
      child: Text(
        count > 99 ? '99+' : '$count',
        textAlign: TextAlign.center,
        style: Theme.of(context).textTheme.labelSmall?.copyWith(
          color: Colors.white,
          fontWeight: FontWeight.w800,
          height: 1.0,
        ),
      ),
    );
  }
}

class _TopIconButton extends StatelessWidget {
  const _TopIconButton({
    super.key,
    required this.icon,
    required this.tooltip,
    required this.cupertinoStyle,
    required this.onPressed,
  });

  final AppSemanticIcon icon;
  final String tooltip;
  final bool cupertinoStyle;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    if (cupertinoStyle) {
      return CupertinoButton(
        padding: EdgeInsets.zero,
        minimumSize: const Size(34, 34),
        onPressed: onPressed,
        child: AppIcon(
          icon,
          size: 18,
          color: Theme.of(context).colorScheme.primary,
        ),
      );
    }
    return IconButton(
      tooltip: tooltip,
      onPressed: onPressed,
      icon: AppIcon(icon, size: 20),
    );
  }
}

class _ContactFriendRequestDraft {
  const _ContactFriendRequestDraft({
    required this.accountId,
    required this.remark,
  });

  final String accountId;
  final String remark;
}

class _ContactAddFriendDialog extends StatefulWidget {
  const _ContactAddFriendDialog();

  @override
  State<_ContactAddFriendDialog> createState() =>
      _ContactAddFriendDialogState();
}

class _ContactAddFriendDialogState extends State<_ContactAddFriendDialog> {
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
      _ContactFriendRequestDraft(
        accountId: accountId,
        remark: _remarkController.text.trim(),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('添加好友'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          TextField(
            key: const Key('mobile-contacts-add-friend-username'),
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
            key: const Key('mobile-contacts-add-friend-remark'),
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
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('取消'),
        ),
        FilledButton(
          key: const Key('mobile-contacts-add-friend-submit'),
          onPressed: _submit,
          child: const Text('发送请求'),
        ),
      ],
    );
  }
}

class _ContactsEmptyState extends StatelessWidget {
  const _ContactsEmptyState({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(16),
      child: Text(text, style: Theme.of(context).textTheme.bodySmall),
    );
  }
}

bool _matchesQuery(ContactProfile contact, String query) {
  return contact.displayName.toLowerCase().contains(query) ||
      contact.handle.toLowerCase().contains(query) ||
      contact.statusLabel.toLowerCase().contains(query) ||
      contact.groupLabel.toLowerCase().contains(query);
}

int _contactCompare(ContactProfile left, ContactProfile right) {
  if (left.online != right.online) {
    return left.online ? -1 : 1;
  }
  return left.displayName.toLowerCase().compareTo(
    right.displayName.toLowerCase(),
  );
}

ConversationSummary? _matchedConversation(
  ContactProfile contact,
  List<ConversationSummary> conversations,
) {
  for (final conversation in conversations) {
    if (conversation.id == contact.id ||
        conversation.id.contains(contact.id) ||
        conversation.title == contact.displayName) {
      return conversation;
    }
  }
  return null;
}
