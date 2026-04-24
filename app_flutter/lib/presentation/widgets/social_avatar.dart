import 'package:flutter/material.dart';

import '../../domain/entities/models.dart';
import '../theme/app_icons.dart';
import '../theme/app_tokens.dart';

class SocialAvatar extends StatelessWidget {
  const SocialAvatar({
    super.key,
    required this.id,
    required this.title,
    this.kind = ConversationKind.direct,
    this.size = AppTokens.avatarLg,
    this.avatarSeed,
    this.initials,
    this.online = false,
    this.verified = false,
    this.official = false,
    this.selected = false,
  });

  factory SocialAvatar.conversation(
    ConversationSummary conversation, {
    Key? key,
    double size = AppTokens.avatarLg,
    bool selected = false,
  }) {
    return SocialAvatar(
      key: key,
      id: conversation.id,
      title: conversation.title,
      kind: conversation.kind,
      size: size,
      avatarSeed: conversation.avatarSeed,
      initials: conversation.avatarInitials,
      online: conversation.isOnline,
      verified: conversation.isVerified,
      official: conversation.isOfficial || conversation.isServiceAccount,
      selected: selected,
    );
  }

  factory SocialAvatar.contact(
    ContactProfile contact, {
    Key? key,
    double size = AppTokens.avatarLg,
  }) {
    return SocialAvatar(
      key: key,
      id: contact.id,
      title: contact.displayName,
      kind: switch (contact.groupLabel) {
        '群聊' => ConversationKind.group,
        '公众号或服务号' => ConversationKind.bot,
        _ => ConversationKind.direct,
      },
      size: size,
      avatarSeed: contact.avatarSeed,
      initials: contact.displayName.characters.take(2).join(),
      online: contact.online,
      official: contact.groupLabel == '公众号或服务号',
    );
  }

  final String id;
  final String title;
  final ConversationKind kind;
  final double size;
  final String? avatarSeed;
  final String? initials;
  final bool online;
  final bool verified;
  final bool official;
  final bool selected;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final seed = avatarSeed?.trim().isNotEmpty == true ? avatarSeed! : id;
    final colors = _paletteFor(seed, theme.brightness);
    final foreground = selected
        ? theme.colorScheme.primary
        : theme.colorScheme.onSurface.withValues(alpha: 0.9);
    final child = switch (kind) {
      ConversationKind.group => _GroupAvatarArtwork(
        size: size,
        seed: seed,
        selected: selected,
      ),
      ConversationKind.channel => _IconAvatarArtwork(
        size: size,
        icon: AppSemanticIcon.notifications,
        foreground: foreground,
        colors: colors,
      ),
      ConversationKind.bot || ConversationKind.system => _IconAvatarArtwork(
        size: size,
        icon: kind == ConversationKind.system
            ? AppSemanticIcon.systemConversation
            : AppSemanticIcon.help,
        foreground: foreground,
        colors: colors,
      ),
      ConversationKind.fileAssistant => _IconAvatarArtwork(
        size: size,
        icon: AppSemanticIcon.file,
        foreground: foreground,
        colors: colors,
      ),
      ConversationKind.direct => _InitialAvatarArtwork(
        size: size,
        label: initials?.trim().isNotEmpty == true
            ? initials!.trim()
            : _initialsForTitle(title),
        colors: colors,
        selected: selected,
      ),
    };

    return SizedBox.square(
      key: ValueKey<String>('social-avatar-$id'),
      dimension: size,
      child: Stack(
        clipBehavior: Clip.none,
        children: <Widget>[
          Positioned.fill(child: child),
          if (official || verified)
            Positioned(
              right: -1,
              top: -1,
              child: _AvatarDot(
                size: (size * 0.22).clamp(8.0, 12.0).toDouble(),
                color: official ? AppPalette.channelCyan : AppPalette.qqYellow,
                borderColor: theme.colorScheme.surface,
                child: AppIcon(
                  official
                      ? AppSemanticIcon.verified
                      : AppSemanticIcon.security,
                  size: (size * 0.12).clamp(5.0, 7.0).toDouble(),
                  color: Colors.white,
                ),
              ),
            ),
          if (online && kind == ConversationKind.direct)
            Positioned(
              right: 0,
              bottom: 0,
              child: _AvatarDot(
                size: (size * 0.22).clamp(8.0, 11.0).toDouble(),
                color: AppPalette.socialGreen,
                borderColor: theme.colorScheme.surface,
              ),
            ),
        ],
      ),
    );
  }
}

class _InitialAvatarArtwork extends StatelessWidget {
  const _InitialAvatarArtwork({
    required this.size,
    required this.label,
    required this.colors,
    required this.selected,
  });

  final double size;
  final String label;
  final List<Color> colors;
  final bool selected;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return ClipOval(
      child: DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: selected
                ? <Color>[
                    theme.colorScheme.primary.withValues(alpha: 0.18),
                    colors.last.withValues(alpha: 0.72),
                  ]
                : colors,
          ),
        ),
        child: Center(
          child: Text(
            label,
            maxLines: 1,
            overflow: TextOverflow.clip,
            style: theme.textTheme.titleMedium?.copyWith(
              color: theme.brightness == Brightness.dark
                  ? Colors.white.withValues(alpha: 0.92)
                  : Colors.white,
              fontSize: (size * 0.34).clamp(12.0, 18.0).toDouble(),
              fontWeight: FontWeight.w800,
              height: 1.0,
              letterSpacing: 0,
            ),
          ),
        ),
      ),
    );
  }
}

class _IconAvatarArtwork extends StatelessWidget {
  const _IconAvatarArtwork({
    required this.size,
    required this.icon,
    required this.foreground,
    required this.colors,
  });

  final double size;
  final AppSemanticIcon icon;
  final Color foreground;
  final List<Color> colors;

  @override
  Widget build(BuildContext context) {
    return ClipOval(
      child: DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: <Color>[
              colors.first.withValues(alpha: 0.82),
              colors.last.withValues(alpha: 0.92),
            ],
          ),
        ),
        child: Center(
          child: AppIcon(
            icon,
            size: (size * 0.38).clamp(14.0, 22.0).toDouble(),
            color: foreground,
          ),
        ),
      ),
    );
  }
}

class _GroupAvatarArtwork extends StatelessWidget {
  const _GroupAvatarArtwork({
    required this.size,
    required this.seed,
    required this.selected,
  });

  final double size;
  final String seed;
  final bool selected;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final baseColors = _paletteFor(seed, theme.brightness);
    return ClipOval(
      child: DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: <Color>[
              baseColors.first.withValues(alpha: selected ? 0.42 : 0.36),
              baseColors.last.withValues(alpha: selected ? 0.72 : 0.62),
            ],
          ),
        ),
        child: Stack(
          children: <Widget>[
            _GroupMemberDisc(
              size: size * 0.47,
              alignment: const Alignment(-0.43, -0.34),
              colors: _paletteFor('$seed-a', theme.brightness),
            ),
            _GroupMemberDisc(
              size: size * 0.52,
              alignment: const Alignment(0.34, -0.12),
              colors: _paletteFor('$seed-b', theme.brightness),
            ),
            _GroupMemberDisc(
              size: size * 0.45,
              alignment: const Alignment(-0.06, 0.48),
              colors: _paletteFor('$seed-c', theme.brightness),
            ),
          ],
        ),
      ),
    );
  }
}

class _GroupMemberDisc extends StatelessWidget {
  const _GroupMemberDisc({
    required this.size,
    required this.alignment,
    required this.colors,
  });

  final double size;
  final Alignment alignment;
  final List<Color> colors;

  @override
  Widget build(BuildContext context) {
    return Align(
      alignment: alignment,
      child: Container(
        width: size,
        height: size,
        decoration: BoxDecoration(
          gradient: LinearGradient(colors: colors),
          shape: BoxShape.circle,
          border: Border.all(
            color: Theme.of(context).colorScheme.surface.withValues(alpha: 0.7),
            width: 1.2,
          ),
        ),
      ),
    );
  }
}

class _AvatarDot extends StatelessWidget {
  const _AvatarDot({
    required this.size,
    required this.color,
    required this.borderColor,
    this.child,
  });

  final double size;
  final Color color;
  final Color borderColor;
  final Widget? child;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: size,
      height: size,
      decoration: BoxDecoration(
        color: color,
        shape: BoxShape.circle,
        border: Border.all(color: borderColor, width: 1.5),
      ),
      alignment: Alignment.center,
      child: child,
    );
  }
}

List<Color> _paletteFor(String seed, Brightness brightness) {
  const lightPalettes = <List<Color>>[
    <Color>[Color(0xFF5EA8C8), Color(0xFF2C6D8D)],
    <Color>[Color(0xFF6EBB8D), Color(0xFF2F7B5B)],
    <Color>[Color(0xFFD09B54), Color(0xFF8D6230)],
    <Color>[Color(0xFFB7788C), Color(0xFF7A4559)],
    <Color>[Color(0xFF7D96C7), Color(0xFF465E93)],
    <Color>[Color(0xFF73AFA4), Color(0xFF3E756C)],
  ];
  const darkPalettes = <List<Color>>[
    <Color>[Color(0xFF2D596D), Color(0xFF183849)],
    <Color>[Color(0xFF2E654B), Color(0xFF183B2C)],
    <Color>[Color(0xFF72532D), Color(0xFF432F1B)],
    <Color>[Color(0xFF704354), Color(0xFF3F2531)],
    <Color>[Color(0xFF425377), Color(0xFF252F47)],
    <Color>[Color(0xFF35655E), Color(0xFF203E3A)],
  ];
  final palettes = brightness == Brightness.dark ? darkPalettes : lightPalettes;
  return palettes[_stableHash(seed).abs() % palettes.length];
}

int _stableHash(String value) {
  var hash = 0;
  for (final codeUnit in value.codeUnits) {
    hash = 0x1fffffff & (hash + codeUnit);
    hash = 0x1fffffff & (hash + ((0x0007ffff & hash) << 10));
    hash ^= hash >> 6;
  }
  hash = 0x1fffffff & (hash + ((0x03ffffff & hash) << 3));
  hash ^= hash >> 11;
  hash = 0x1fffffff & (hash + ((0x00003fff & hash) << 15));
  return hash;
}

String _initialsForTitle(String title) {
  final trimmed = title.trim();
  if (trimmed.isEmpty) {
    return 'IM';
  }
  final words = trimmed
      .split(RegExp(r'\s+'))
      .where((word) => word.trim().isNotEmpty)
      .toList(growable: false);
  if (words.length >= 2) {
    return '${words.first.characters.first}${words[1].characters.first}'
        .toUpperCase();
  }
  return trimmed.characters.take(2).join().toUpperCase();
}
