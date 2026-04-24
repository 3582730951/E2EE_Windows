import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

import '../../domain/entities/models.dart';

enum AppIconDisplayPreference { primaryGlyph, safeFallbackGlyph, monogram }

enum AppSemanticIcon {
  sessions,
  groupChat,
  contacts,
  settings,
  security,
  file,
  device,
  search,
  more,
  back,
  close,
  send,
  emoji,
  voice,
  attachment,
  sent,
  delivered,
  read,
  failed,
  pinned,
  muted,
  systemConversation,
  online,
  offline,
  account,
  lock,
  sync,
  alert,
  verified,
  refresh,
  visualMode,
  conversationDirect,
  chevronRight,
  notifications,
  contactAdd,
  privacy,
  key,
  folder,
  download,
  upload,
  image,
  video,
  media,
  deviceMobile,
  deviceTablet,
  deviceDesktop,
  deviceLinked,
  system,
  help,
}

sealed class AppIconVisual {
  const AppIconVisual();
}

class AppGlyphVisual extends AppIconVisual {
  const AppGlyphVisual(this.iconData);

  final IconData iconData;
}

class AppMonogramVisual extends AppIconVisual {
  const AppMonogramVisual(this.text);

  final String text;
}

class AppIconSpec {
  const AppIconSpec({
    required this.primaryGlyph,
    required this.safeFallbackGlyph,
    required this.monogram,
    this.applePreference = AppIconDisplayPreference.primaryGlyph,
  });

  final IconData primaryGlyph;
  final IconData safeFallbackGlyph;
  final String monogram;
  final AppIconDisplayPreference applePreference;
}

class AppIcons {
  const AppIcons._();

  // Some screenshot/golden environments render Cupertino glyphs as tofu.
  // Prefer the stable fallback glyph chain until dedicated bundled icons exist.
  static const bool _preferReliableAppleFallbackGlyphs = true;

  static AppSemanticIcon? conversationLead(ConversationSummary conversation) {
    if (conversation.kind == ConversationKind.system ||
        conversation.isServiceAccount ||
        conversation.previewBadge == '系统消息' ||
        conversation.pillLabel == '系统' ||
        conversation.pillLabel == '服务') {
      return AppSemanticIcon.systemConversation;
    }
    if (conversation.kind == ConversationKind.fileAssistant ||
        conversation.pillLabel == '文件') {
      return AppSemanticIcon.file;
    }
    if (conversation.kind == ConversationKind.channel) {
      return AppSemanticIcon.notifications;
    }
    if (conversation.kind == ConversationKind.bot) {
      return AppSemanticIcon.help;
    }
    if (conversation.isGroupConversation) {
      return AppSemanticIcon.groupChat;
    }
    return null;
  }

  static AppSemanticIcon messageStatus(MessageDeliveryStatus status) {
    return switch (status) {
      MessageDeliveryStatus.sending => AppSemanticIcon.sync,
      MessageDeliveryStatus.sent => AppSemanticIcon.sent,
      MessageDeliveryStatus.delivered => AppSemanticIcon.delivered,
      MessageDeliveryStatus.read => AppSemanticIcon.read,
      MessageDeliveryStatus.failed => AppSemanticIcon.failed,
    };
  }

  static AppSemanticIcon presence(bool online) {
    return online ? AppSemanticIcon.online : AppSemanticIcon.offline;
  }

  static AppIconSpec specFor(AppSemanticIcon icon) {
    return _specs[icon]!;
  }

  static Iterable<AppIconSpec> get allSpecs => _specs.values;

  static IconData resolveGlyph(TargetPlatform platform, AppSemanticIcon icon) {
    final spec = specFor(icon);
    final useAppleGlyph =
        platform == TargetPlatform.iOS || platform == TargetPlatform.macOS;
    if (!useAppleGlyph) {
      return spec.safeFallbackGlyph;
    }
    if (_preferReliableAppleFallbackGlyphs) {
      return spec.safeFallbackGlyph;
    }
    return switch (spec.applePreference) {
      AppIconDisplayPreference.primaryGlyph => spec.primaryGlyph,
      AppIconDisplayPreference.safeFallbackGlyph => spec.safeFallbackGlyph,
      AppIconDisplayPreference.monogram => spec.safeFallbackGlyph,
    };
  }

  static AppIconVisual resolveVisual(
    TargetPlatform platform,
    AppSemanticIcon icon,
  ) {
    final spec = specFor(icon);
    final useAppleGlyph =
        platform == TargetPlatform.iOS || platform == TargetPlatform.macOS;
    if (!useAppleGlyph) {
      return AppGlyphVisual(spec.safeFallbackGlyph);
    }
    if (_preferReliableAppleFallbackGlyphs) {
      return AppGlyphVisual(spec.safeFallbackGlyph);
    }
    return switch (spec.applePreference) {
      AppIconDisplayPreference.primaryGlyph => AppGlyphVisual(
        spec.primaryGlyph,
      ),
      AppIconDisplayPreference.safeFallbackGlyph => AppGlyphVisual(
        spec.safeFallbackGlyph,
      ),
      AppIconDisplayPreference.monogram => AppMonogramVisual(spec.monogram),
    };
  }

  static const Map<AppSemanticIcon, AppIconSpec> _specs =
      <AppSemanticIcon, AppIconSpec>{
        AppSemanticIcon.sessions: AppIconSpec(
          primaryGlyph: CupertinoIcons.chat_bubble_2,
          safeFallbackGlyph: Icons.forum_outlined,
          monogram: '会',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.groupChat: AppIconSpec(
          primaryGlyph: CupertinoIcons.person_2_fill,
          safeFallbackGlyph: Icons.groups_2_outlined,
          monogram: '群',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.contacts: AppIconSpec(
          primaryGlyph: CupertinoIcons.person_2,
          safeFallbackGlyph: Icons.people_outline_rounded,
          monogram: '联',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.settings: AppIconSpec(
          primaryGlyph: CupertinoIcons.settings,
          safeFallbackGlyph: Icons.settings_outlined,
          monogram: '设',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.security: AppIconSpec(
          primaryGlyph: CupertinoIcons.lock_shield_fill,
          safeFallbackGlyph: Icons.shield_outlined,
          monogram: '盾',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.file: AppIconSpec(
          primaryGlyph: CupertinoIcons.doc,
          safeFallbackGlyph: Icons.insert_drive_file_outlined,
          monogram: '文',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.device: AppIconSpec(
          primaryGlyph: CupertinoIcons.desktopcomputer,
          safeFallbackGlyph: Icons.devices_outlined,
          monogram: '机',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.search: AppIconSpec(
          primaryGlyph: CupertinoIcons.search,
          safeFallbackGlyph: Icons.search,
          monogram: '搜',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.more: AppIconSpec(
          primaryGlyph: CupertinoIcons.ellipsis,
          safeFallbackGlyph: Icons.more_horiz,
          monogram: '⋯',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.back: AppIconSpec(
          primaryGlyph: CupertinoIcons.chevron_left,
          safeFallbackGlyph: Icons.arrow_back_ios_new_rounded,
          monogram: '返',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.close: AppIconSpec(
          primaryGlyph: CupertinoIcons.xmark,
          safeFallbackGlyph: Icons.close_rounded,
          monogram: '关',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.send: AppIconSpec(
          primaryGlyph: CupertinoIcons.paperplane_fill,
          safeFallbackGlyph: Icons.send_rounded,
          monogram: '发',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.emoji: AppIconSpec(
          primaryGlyph: CupertinoIcons.smiley,
          safeFallbackGlyph: Icons.emoji_emotions_outlined,
          monogram: '☺',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.voice: AppIconSpec(
          primaryGlyph: CupertinoIcons.mic_circle_fill,
          safeFallbackGlyph: Icons.mic_rounded,
          monogram: '声',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.attachment: AppIconSpec(
          primaryGlyph: CupertinoIcons.paperclip,
          safeFallbackGlyph: Icons.attach_file_rounded,
          monogram: '附',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.sent: AppIconSpec(
          primaryGlyph: CupertinoIcons.checkmark,
          safeFallbackGlyph: Icons.done,
          monogram: '✓',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.delivered: AppIconSpec(
          primaryGlyph: CupertinoIcons.checkmark_circle,
          safeFallbackGlyph: Icons.done_all_outlined,
          monogram: '✓✓',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.read: AppIconSpec(
          primaryGlyph: CupertinoIcons.checkmark_circle_fill,
          safeFallbackGlyph: Icons.done_all,
          monogram: '✓✓',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.failed: AppIconSpec(
          primaryGlyph: CupertinoIcons.exclamationmark_circle_fill,
          safeFallbackGlyph: Icons.error_outline,
          monogram: '!',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.pinned: AppIconSpec(
          primaryGlyph: CupertinoIcons.pin_fill,
          safeFallbackGlyph: Icons.push_pin_outlined,
          monogram: '顶',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.muted: AppIconSpec(
          primaryGlyph: CupertinoIcons.bell_slash_fill,
          safeFallbackGlyph: Icons.notifications_off_outlined,
          monogram: '静',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.systemConversation: AppIconSpec(
          primaryGlyph: CupertinoIcons.settings_solid,
          safeFallbackGlyph: Icons.admin_panel_settings_outlined,
          monogram: '系',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.online: AppIconSpec(
          primaryGlyph: CupertinoIcons.circle_fill,
          safeFallbackGlyph: Icons.circle,
          monogram: '●',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.offline: AppIconSpec(
          primaryGlyph: CupertinoIcons.circle,
          safeFallbackGlyph: Icons.circle_outlined,
          monogram: '○',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.account: AppIconSpec(
          primaryGlyph: CupertinoIcons.person_crop_circle_fill,
          safeFallbackGlyph: Icons.person_outline_rounded,
          monogram: '账',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.lock: AppIconSpec(
          primaryGlyph: CupertinoIcons.lock,
          safeFallbackGlyph: Icons.lock_outline_rounded,
          monogram: '锁',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.sync: AppIconSpec(
          primaryGlyph: CupertinoIcons.arrow_clockwise_circle_fill,
          safeFallbackGlyph: Icons.sync_rounded,
          monogram: '↻',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.alert: AppIconSpec(
          primaryGlyph: CupertinoIcons.exclamationmark_circle_fill,
          safeFallbackGlyph: Icons.error_outline,
          monogram: '!',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.verified: AppIconSpec(
          primaryGlyph: CupertinoIcons.checkmark_circle_fill,
          safeFallbackGlyph: Icons.verified_rounded,
          monogram: '✓',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.refresh: AppIconSpec(
          primaryGlyph: CupertinoIcons.arrow_clockwise,
          safeFallbackGlyph: Icons.refresh_rounded,
          monogram: '↻',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.visualMode: AppIconSpec(
          primaryGlyph: CupertinoIcons.viewfinder_circle,
          safeFallbackGlyph: Icons.display_settings_outlined,
          monogram: '视',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.conversationDirect: AppIconSpec(
          primaryGlyph: CupertinoIcons.person,
          safeFallbackGlyph: Icons.person_outline_rounded,
          monogram: '人',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.chevronRight: AppIconSpec(
          primaryGlyph: CupertinoIcons.chevron_forward,
          safeFallbackGlyph: Icons.chevron_right_rounded,
          monogram: '›',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.notifications: AppIconSpec(
          primaryGlyph: CupertinoIcons.bell,
          safeFallbackGlyph: Icons.notifications_none_rounded,
          monogram: '铃',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.contactAdd: AppIconSpec(
          primaryGlyph: CupertinoIcons.person_add,
          safeFallbackGlyph: Icons.person_add_alt_1_rounded,
          monogram: '添',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.privacy: AppIconSpec(
          primaryGlyph: CupertinoIcons.hand_raised,
          safeFallbackGlyph: Icons.privacy_tip_outlined,
          monogram: '隐',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.key: AppIconSpec(
          primaryGlyph: CupertinoIcons.lock_fill,
          safeFallbackGlyph: Icons.key_outlined,
          monogram: '钥',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.folder: AppIconSpec(
          primaryGlyph: CupertinoIcons.folder,
          safeFallbackGlyph: Icons.folder_outlined,
          monogram: '夹',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.download: AppIconSpec(
          primaryGlyph: CupertinoIcons.download_circle,
          safeFallbackGlyph: Icons.download_rounded,
          monogram: '下',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.upload: AppIconSpec(
          primaryGlyph: CupertinoIcons.upload_circle,
          safeFallbackGlyph: Icons.upload_rounded,
          monogram: '上',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.image: AppIconSpec(
          primaryGlyph: CupertinoIcons.photo,
          safeFallbackGlyph: Icons.image_outlined,
          monogram: '图',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.video: AppIconSpec(
          primaryGlyph: CupertinoIcons.video_camera,
          safeFallbackGlyph: Icons.videocam_outlined,
          monogram: '影',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.media: AppIconSpec(
          primaryGlyph: CupertinoIcons.play_rectangle_fill,
          safeFallbackGlyph: Icons.perm_media_outlined,
          monogram: '媒',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.deviceMobile: AppIconSpec(
          primaryGlyph: CupertinoIcons.device_phone_portrait,
          safeFallbackGlyph: Icons.smartphone_rounded,
          monogram: '手',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.deviceTablet: AppIconSpec(
          primaryGlyph: CupertinoIcons.device_laptop,
          safeFallbackGlyph: Icons.tablet_mac_outlined,
          monogram: '板',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.deviceDesktop: AppIconSpec(
          primaryGlyph: CupertinoIcons.desktopcomputer,
          safeFallbackGlyph: Icons.desktop_windows_outlined,
          monogram: '桌',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.deviceLinked: AppIconSpec(
          primaryGlyph: CupertinoIcons.link,
          safeFallbackGlyph: Icons.phonelink_ring_outlined,
          monogram: '连',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.system: AppIconSpec(
          primaryGlyph: CupertinoIcons.settings_solid,
          safeFallbackGlyph: Icons.tune_rounded,
          monogram: '统',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
        AppSemanticIcon.help: AppIconSpec(
          primaryGlyph: CupertinoIcons.question_circle,
          safeFallbackGlyph: Icons.help_outline_rounded,
          monogram: '问',
          applePreference: AppIconDisplayPreference.primaryGlyph,
        ),
      };
}

class AppIcon extends StatelessWidget {
  const AppIcon(
    this.icon, {
    super.key,
    this.size = 18,
    this.color,
    this.semanticLabel,
  });

  final AppSemanticIcon icon;
  final double size;
  final Color? color;
  final String? semanticLabel;

  @override
  Widget build(BuildContext context) {
    final visual = AppIcons.resolveVisual(Theme.of(context).platform, icon);
    return switch (visual) {
      AppGlyphVisual(:final iconData) => Icon(
        iconData,
        size: size,
        color: color,
        semanticLabel: semanticLabel,
      ),
      AppMonogramVisual(:final text) => SizedBox.square(
        dimension: size,
        child: Semantics(
          label: semanticLabel ?? text,
          child: Center(
            child: ExcludeSemantics(
              child: Text(
                text,
                textAlign: TextAlign.center,
                style: TextStyle(
                  color:
                      color ??
                      IconTheme.of(context).color ??
                      Theme.of(context).colorScheme.onSurface,
                  fontFamily: 'SourceHanSansSC',
                  fontSize: size * 0.88,
                  fontWeight: FontWeight.w700,
                  height: 1.0,
                ),
              ),
            ),
          ),
        ),
      ),
    };
  }
}
