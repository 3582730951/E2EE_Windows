import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'fakes/fake_sdk_client.dart';
import 'package:mi_e2ee_im_app/presentation/screens/mobile_contacts_screen.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('contacts page shows social shortcuts and mainstream groups', (
    WidgetTester tester,
  ) async {
    await _pumpContactsScreen(tester, TargetPlatform.iOS);

    expect(find.byKey(const Key('mobile-contacts-add-friend')), findsOneWidget);
    expect(
      find.byKey(const Key('mobile-contacts-create-group')),
      findsOneWidget,
    );
    expect(find.byKey(const Key('contacts-search-field')), findsOneWidget);
    expect(
      find.byKey(const Key('mobile-contacts-quick-entries')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-contacts-section-new-friends')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-contacts-section-groups')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-contacts-section-friends')),
      findsOneWidget,
    );
    expect(
      find.byKey(const Key('mobile-contacts-section-services')),
      findsOneWidget,
    );
    expect(find.text('Aurora 设计群'), findsOneWidget);
    expect(find.text('Bob Chen'), findsOneWidget);
    expect(find.text('MI Chat 服务号'), findsOneWidget);
    expect(find.byKey(const ValueKey('social-avatar-bob')), findsOneWidget);
    expect(
      find.byKey(const ValueKey('social-avatar-group-design')),
      findsOneWidget,
    );
    expect(
      find.byKey(const ValueKey('social-avatar-service-notice')),
      findsOneWidget,
    );
    expect(find.textContaining('风控'), findsNothing);
  });
}

Future<void> _pumpContactsScreen(
  WidgetTester tester,
  TargetPlatform platform,
) async {
  final previousPlatform = debugDefaultTargetPlatformOverride;
  debugDefaultTargetPlatformOverride = platform;
  try {
    final fakeClient = FakeSdkClient();
    await fakeClient.initialize();
    addTearDown(fakeClient.dispose);

    tester.view.physicalSize = platform == TargetPlatform.iOS
        ? const Size(393, 852)
        : const Size(412, 915);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    await tester.pumpWidget(
      ProviderScope(
        overrides: [sdkClientProvider.overrideWith((_) => fakeClient)],
        child: MaterialApp(
          theme: AppTheme.light(),
          home: const Scaffold(body: MobileContactsScreen()),
        ),
      ),
    );
    await tester.pumpAndSettle();
  } finally {
    debugDefaultTargetPlatformOverride = previousPlatform;
  }
}
