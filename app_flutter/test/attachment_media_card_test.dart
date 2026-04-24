import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/domain/entities/models.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';
import 'package:mi_e2ee_im_app/presentation/widgets/components.dart';

void main() {
  testWidgets('desktop spoiler images stay media-first and hide file names', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(1366, 768);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(
      MaterialApp(
        theme: AppTheme.light().copyWith(platform: TargetPlatform.windows),
        home: Scaffold(
          body: Center(
            child: SizedBox(
              width: 320,
              child: AttachmentMessageCard(
                attachment: const ChatAttachment(
                  kind: ChatAttachmentKind.image,
                  title: 'hero-cover-preview.png',
                  detail: '2.4 MB · 图片',
                  actionLabel: '查看原图',
                  state: ChatAttachmentState.completed,
                  sensitivity: ChatAttachmentSensitivity.spoiler,
                ),
                isOutgoing: false,
              ),
            ),
          ),
        ),
      ),
    );

    await tester.pumpAndSettle();

    expect(find.byKey(const Key('attachment-image-card')), findsOneWidget);
    expect(
      find.byKey(const Key('attachment-desktop-image-thumb')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('attachment-desktop-image-mask')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('attachment-desktop-image-mask-grain')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('attachment-desktop-image-mask-chip')),
      findsOneWidget,
    );
    expect(find.byType(AnimatedSwitcher), findsWidgets);
    expect(find.text('hero-cover-preview.png'), findsNothing);

    await tester.tap(find.byKey(const Key('attachment-desktop-image-mask')));
    await tester.pumpAndSettle();

    expect(
      find.byKey(const Key('attachment-desktop-image-mask')),
      findsNothing,
    );
  });

  testWidgets('mobile spoiler images reveal from a protected preview', (
    WidgetTester tester,
  ) async {
    tester.view.physicalSize = const Size(393, 852);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(
      MaterialApp(
        theme: AppTheme.light().copyWith(platform: TargetPlatform.android),
        home: Scaffold(
          body: Center(
            child: SizedBox(
              width: 280,
              child: AttachmentMessageCard(
                attachment: const ChatAttachment(
                  kind: ChatAttachmentKind.image,
                  title: 'hero-cover-preview.png',
                  detail: '2.4 MB · 上传到会话',
                  actionLabel: '上传中',
                  state: ChatAttachmentState.uploading,
                  progress: 0.58,
                  sensitivity: ChatAttachmentSensitivity.spoiler,
                ),
                isOutgoing: true,
              ),
            ),
          ),
        ),
      ),
    );

    await tester.pumpAndSettle();

    expect(find.byKey(const Key('attachment-image-card')), findsOneWidget);
    expect(find.byKey(const Key('attachment-image-preview')), findsOneWidget);
    expect(
      find.byKey(const Key('attachment-image-mask-overlay')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('attachment-image-mask-grain')),
      findsOneWidget,
    );
    expect(find.byKey(const Key('attachment-image-mask-chip')), findsOneWidget);
    expect(find.byType(AnimatedSwitcher), findsWidgets);
    expect(find.text('hero-cover-preview.png'), findsNothing);

    await tester.tapAt(
      tester.getCenter(find.byKey(const Key('attachment-image-card'))),
    );
    await tester.pumpAndSettle();

    expect(
      find.byKey(const Key('attachment-image-mask-overlay')),
      findsNothing,
    );
  });
}
