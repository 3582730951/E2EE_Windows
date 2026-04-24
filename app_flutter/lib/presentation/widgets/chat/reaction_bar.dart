import 'package:flutter/material.dart';

import '../../../domain/entities/models.dart';
import '../../theme/app_theme.dart';
import '../../theme/app_tokens.dart';

class ReactionBar extends StatelessWidget {
  const ReactionBar({
    super.key,
    required this.reactions,
    required this.isOutgoing,
    this.compact = false,
  });

  final List<MessageReactionSummary> reactions;
  final bool isOutgoing;
  final bool compact;

  @override
  Widget build(BuildContext context) {
    if (reactions.isEmpty) {
      return const SizedBox.shrink();
    }
    final theme = Theme.of(context);
    final palette = theme.extension<AppShellColors>()!;
    return Wrap(
      key: const Key('message-reaction-bar'),
      spacing: 4,
      runSpacing: 4,
      alignment: isOutgoing ? WrapAlignment.end : WrapAlignment.start,
      children: <Widget>[
        for (final reaction in reactions)
          Container(
            padding: EdgeInsets.symmetric(
              horizontal: compact ? 5.5 : 6.5,
              vertical: compact ? 2.0 : 2.4,
            ),
            decoration: BoxDecoration(
              color: reaction.selected
                  ? theme.colorScheme.primary.withValues(alpha: 0.13)
                  : palette.surfaceVariant.withValues(alpha: 0.22),
              borderRadius: BorderRadius.circular(AppTokens.radiusFull),
              border: Border.all(
                color: reaction.selected
                    ? theme.colorScheme.primary.withValues(alpha: 0.18)
                    : palette.divider.withValues(alpha: 0.10),
              ),
            ),
            child: Text(
              '${reaction.emoji} ${reaction.count}',
              style: theme.textTheme.labelSmall?.copyWith(
                fontSize: compact ? 9.2 : 10.2,
                height: 1.0,
                fontWeight: FontWeight.w700,
                color: reaction.selected
                    ? theme.colorScheme.primary
                    : palette.textSecondary,
              ),
            ),
          ),
      ],
    );
  }
}
