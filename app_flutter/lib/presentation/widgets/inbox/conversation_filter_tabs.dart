import 'package:flutter/material.dart';

import '../../../domain/entities/models.dart';
import '../../theme/app_theme.dart';
import '../../theme/app_tokens.dart';

class ConversationFilterTabs extends StatelessWidget {
  const ConversationFilterTabs({
    super.key,
    required this.value,
    required this.onChanged,
    this.presets = const <ConversationFilterPreset>[
      ConversationFilterPreset.all,
      ConversationFilterPreset.unread,
      ConversationFilterPreset.mentions,
      ConversationFilterPreset.groups,
      ConversationFilterPreset.files,
    ],
    this.desktop = false,
  });

  final ConversationFilterPreset value;
  final ValueChanged<ConversationFilterPreset> onChanged;
  final List<ConversationFilterPreset> presets;
  final bool desktop;

  @override
  Widget build(BuildContext context) {
    final isCupertinoStyle = Theme.of(context).platform == TargetPlatform.iOS;
    final palette = Theme.of(context).extension<AppShellColors>()!;
    final height = desktop ? 32.0 : (isCupertinoStyle ? 30.0 : 34.0);

    return SizedBox(
      key: desktop
          ? const ValueKey('desktop-conversation-filter-tabs')
          : const ValueKey('mobile-chats-filter-tabs'),
      height: height,
      child: ListView.separated(
        scrollDirection: Axis.horizontal,
        physics: const BouncingScrollPhysics(),
        padding: EdgeInsets.symmetric(horizontal: desktop ? 0 : 2),
        itemCount: presets.length,
        separatorBuilder: (_, _) => SizedBox(width: desktop ? 5 : 7),
        itemBuilder: (context, index) {
          final preset = presets[index];
          final selected = preset == value;
          return _ConversationFilterTab(
            key: ValueKey<String>('chat-home-filter-${preset.name}'),
            label: preset.label,
            selected: selected,
            desktop: desktop,
            surfaceColor: selected
                ? Theme.of(context).colorScheme.primary.withValues(alpha: 0.12)
                : palette.surfaceVariant.withValues(
                    alpha: desktop ? 0.12 : 0.2,
                  ),
            borderColor: selected
                ? Theme.of(context).colorScheme.primary.withValues(alpha: 0.18)
                : palette.divider.withValues(alpha: desktop ? 0.08 : 0.12),
            onTap: () => onChanged(preset),
          );
        },
      ),
    );
  }
}

class _ConversationFilterTab extends StatelessWidget {
  const _ConversationFilterTab({
    super.key,
    required this.label,
    required this.selected,
    required this.desktop,
    required this.surfaceColor,
    required this.borderColor,
    required this.onTap,
  });

  final String label;
  final bool selected;
  final bool desktop;
  final Color surfaceColor;
  final Color borderColor;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Material(
      color: Colors.transparent,
      child: InkWell(
        borderRadius: BorderRadius.circular(AppTokens.radiusFull),
        onTap: onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 160),
          curve: Curves.easeOutCubic,
          constraints: BoxConstraints(minWidth: desktop ? 48 : 52),
          alignment: Alignment.center,
          padding: EdgeInsets.symmetric(horizontal: desktop ? 10 : 12),
          decoration: BoxDecoration(
            color: surfaceColor,
            borderRadius: BorderRadius.circular(AppTokens.radiusFull),
            border: Border.all(color: borderColor),
          ),
          child: Text(
            label,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: theme.textTheme.labelLarge?.copyWith(
              fontSize: desktop ? 11.2 : 12.0,
              fontWeight: selected ? FontWeight.w700 : FontWeight.w600,
              color: selected
                  ? theme.colorScheme.primary
                  : theme.textTheme.bodySmall?.color?.withValues(alpha: 0.86),
            ),
          ),
        ),
      ),
    );
  }
}

extension ConversationFilterPresetLabel on ConversationFilterPreset {
  String get label => switch (this) {
    ConversationFilterPreset.all => '全部',
    ConversationFilterPreset.unread => '未读',
    ConversationFilterPreset.mentions => '@我',
    ConversationFilterPreset.groups => '群聊',
    ConversationFilterPreset.channels => '频道',
    ConversationFilterPreset.files => '文件',
  };
}
