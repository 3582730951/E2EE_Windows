import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_riverpod/misc.dart' show Override;

import 'package:mi_e2ee_im_app/bootstrap/app_providers.dart';
import 'package:mi_e2ee_im_app/bootstrap/device_capability_providers.dart';
import 'package:mi_e2ee_im_app/native_sdk/fake_sdk_client.dart';
import 'package:mi_e2ee_im_app/presentation/screens/login_screen.dart';
import 'package:mi_e2ee_im_app/presentation/theme/app_theme.dart';

import 'test_capabilities.dart';

void main() {
  testWidgets('keeps a centered and taller IM entry shell on Windows', (
    WidgetTester tester,
  ) async {
    await _pumpLogin(
      tester,
      platform: TargetPlatform.windows,
      size: const Size(1280, 900),
    );

    final panelRect = tester.getRect(_panelShell());
    final stageRect = tester.getRect(_stageGlow());
    final entryRect = tester.getRect(_entryStack());
    final heroRect = tester.getRect(_heroStack());
    final titleRect = tester.getRect(_heroTitle());
    final avatarRect = tester.getRect(_heroAvatar());
    final formRect = tester.getRect(_formColumn());
    final formSurfaceRect = tester.getRect(_formSurface());
    final continueRect = tester.getRect(_continueButton());

    expect((panelRect.center.dx - 640).abs(), lessThanOrEqualTo(16));
    expect(panelRect.width, inInclusiveRange(438, 452));
    expect(panelRect.height, inInclusiveRange(452, 480));
    expect(entryRect.height, lessThanOrEqualTo(panelRect.height - 12));
    expect(
      (avatarRect.center.dx - titleRect.center.dx).abs(),
      lessThanOrEqualTo(4),
    );
    expect(avatarRect.width, lessThanOrEqualTo(45));
    expect(titleRect.height, lessThanOrEqualTo(20));
    expect(heroRect.bottom, lessThan(formRect.top));
    expect(heroRect.bottom, lessThan(formSurfaceRect.top));
    expect(formRect.width, lessThanOrEqualTo(320));
    expect(formSurfaceRect.width, lessThanOrEqualTo(332));
    expect(continueRect.height, inInclusiveRange(47, 49));
    expect(stageRect.width, greaterThan(panelRect.width * 1.6));
    expect(stageRect.height, greaterThan(panelRect.height * 1.05));
    expect(_windowsFormShell(), findsOneWidget);
    expect(find.byKey(const ValueKey('login-mode-tabs')), findsOneWidget);
    expect(find.byKey(const ValueKey('login-qr-preview')), findsOneWidget);
    expect(find.byKey(const ValueKey('login-qr-code-surface')), findsOneWidget);
    expect(find.byKey(const ValueKey('login-help-links')), findsOneWidget);
    expect(find.text('扫码登录'), findsAtLeastNWidgets(1));
    expect(find.text('账号登录'), findsOneWidget);
    expect(find.text('注册账号'), findsOneWidget);
    expect(find.text('忘记密码'), findsOneWidget);
    expect(find.text('代理设置'), findsOneWidget);
  });

  testWidgets('stacks avatar, title and fields on Android login', (
    WidgetTester tester,
  ) async {
    await _pumpLogin(
      tester,
      platform: TargetPlatform.android,
      size: const Size(412, 915),
    );

    _expectAndroidEntryAxis(tester, viewportHeight: 915);
    expect(_androidFormShell(), findsOneWidget);
    expect(_iosFrostLayer(), findsNothing);
  });

  testWidgets('stacks avatar, title and fields on iOS login', (
    WidgetTester tester,
  ) async {
    await _pumpLogin(
      tester,
      platform: TargetPlatform.iOS,
      size: const Size(393, 852),
    );

    _expectIosEntryAxis(tester, viewportHeight: 852);
    expect(_iosFrostLayer(), findsOneWidget);
    expect(_iosFormShell(), findsOneWidget);
    expect(_androidFormShell(), findsNothing);
  });

  testWidgets('uses distinct platform login surfaces on iOS and Windows', (
    WidgetTester tester,
  ) async {
    await _pumpLogin(
      tester,
      platform: TargetPlatform.windows,
      size: const Size(1280, 900),
    );

    expect(_windowsFormShell(), findsOneWidget);
    expect(_iosFrostLayer(), findsNothing);

    await _pumpLogin(
      tester,
      platform: TargetPlatform.iOS,
      size: const Size(393, 852),
    );

    expect(_iosFrostLayer(), findsOneWidget);
    expect(_windowsFormShell(), findsNothing);
  });
}

Future<void> _pumpLogin(
  WidgetTester tester, {
  required TargetPlatform platform,
  required Size size,
}) async {
  tester.view.physicalSize = size;
  tester.view.devicePixelRatio = 1.0;

  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);
  final fakeClient = FakeSdkClient();
  await fakeClient.initialize();
  addTearDown(fakeClient.dispose);

  await tester.pumpWidget(
    ProviderScope(
      overrides: <Override>[
        sdkClientProvider.overrideWith((_) => fakeClient),
        deviceCapabilityProfileProvider.overrideWith(
          (ref) => Future.value(
            buildTestCapabilityProfile(
              platform: platform,
              logicalSize: size,
              devicePixelRatio: 1.0,
            ),
          ),
        ),
      ],
      child: MaterialApp(
        theme: AppTheme.dark().copyWith(platform: platform),
        home: const LoginScreen(),
      ),
    ),
  );
  await tester.pumpAndSettle();
}

void _expectIosEntryAxis(
  WidgetTester tester, {
  required double viewportHeight,
}) {
  final panelRect = tester.getRect(_panelShell());
  final stageRect = tester.getRect(_stageGlow());
  final entryRect = tester.getRect(_entryStack());
  final heroRect = tester.getRect(_heroStack());
  final titleRect = tester.getRect(_heroTitle());
  final avatarRect = tester.getRect(_heroAvatar());
  final formSurfaceRect = tester.getRect(_formSurface());
  final formRect = tester.getRect(_formColumn());
  final continueRect = tester.getRect(_continueButton());
  final identityRect = tester.getRect(_identityZone());

  expect(panelRect.width, greaterThanOrEqualTo(300));
  expect(panelRect.top, inInclusiveRange(188, 300));
  expect(
    (panelRect.center.dy - viewportHeight / 2).abs(),
    lessThanOrEqualTo(72),
  );
  expect(
    (stageRect.center.dy - panelRect.center.dy).abs(),
    lessThanOrEqualTo(88),
  );
  expect(identityRect.height, inInclusiveRange(76, 88));
  expect(entryRect.height, inInclusiveRange(360, 430));
  expect(
    (avatarRect.center.dx - titleRect.center.dx).abs(),
    lessThanOrEqualTo(4),
  );
  expect(avatarRect.bottom, lessThan(titleRect.top));
  expect(titleRect.height, lessThanOrEqualTo(20));
  expect(heroRect.bottom, lessThan(formRect.top));
  expect(heroRect.bottom, lessThan(formSurfaceRect.top));
  expect(formRect.top, lessThan(continueRect.top));
  expect(continueRect.height, inInclusiveRange(47.5, 50));
  expect(continueRect.bottom, lessThanOrEqualTo(viewportHeight));
}

void _expectAndroidEntryAxis(
  WidgetTester tester, {
  required double viewportHeight,
}) {
  final panelRect = tester.getRect(_panelShell());
  final stageRect = tester.getRect(_stageGlow());
  final entryRect = tester.getRect(_entryStack());
  final heroRect = tester.getRect(_heroStack());
  final titleRect = tester.getRect(_heroTitle());
  final avatarRect = tester.getRect(_heroAvatar());
  final formSurfaceRect = tester.getRect(_formSurface());
  final continueRect = tester.getRect(_continueButton());
  final identityRect = tester.getRect(_identityZone());

  expect(panelRect.width, greaterThanOrEqualTo(320));
  expect(panelRect.top, inInclusiveRange(190, 320));
  expect(
    (panelRect.center.dy - viewportHeight / 2).abs(),
    lessThanOrEqualTo(84),
  );
  expect(
    (stageRect.center.dy - panelRect.center.dy).abs(),
    lessThanOrEqualTo(96),
  );
  expect(panelRect.height, greaterThanOrEqualTo(320));
  expect(entryRect.height, lessThan(panelRect.height));
  expect(identityRect.height, inInclusiveRange(78, 92));
  expect(find.byKey(const ValueKey('login-qr-preview')), findsOneWidget);
  expect(
    (avatarRect.center.dx - titleRect.center.dx).abs(),
    lessThanOrEqualTo(4),
  );
  expect(avatarRect.bottom, lessThan(titleRect.top));
  expect(heroRect.bottom, lessThan(formSurfaceRect.top));
  expect(formSurfaceRect.top, lessThan(continueRect.top));
  expect(continueRect.height, inInclusiveRange(48, 50));
  expect(continueRect.bottom, lessThanOrEqualTo(viewportHeight));
  expect(continueRect.bottom, lessThan(panelRect.bottom - 12));
}

Finder _panelShell() => find.byKey(const ValueKey('login-panel-shell'));

Finder _stageGlow() => find.byKey(const ValueKey('login-stage-glow'));

Finder _heroStack() => find.byKey(const ValueKey('login-hero-stack'));

Finder _heroTitle() => find.byKey(const ValueKey('login-entry-title'));

Finder _heroAvatar() => find.byKey(const ValueKey('login-identity-header'));

Finder _identityZone() => find.byKey(const ValueKey('login-identity-zone'));

Finder _formSurface() => find.byKey(const ValueKey('login-form-surface'));

Finder _iosFrostLayer() => find.byKey(const ValueKey('login-ios-frost-layer'));

Finder _iosFormShell() => find.byKey(const ValueKey('login-ios-form-shell'));

Finder _androidFormShell() =>
    find.byKey(const ValueKey('login-android-form-shell'));

Finder _windowsFormShell() =>
    find.byKey(const ValueKey('login-windows-form-shell'));

Finder _formColumn() => find.byKey(const ValueKey('login-form-column'));

Finder _continueButton() => find.byKey(const ValueKey('login-continue-button'));

Finder _entryStack() => find.byKey(const ValueKey('login-entry-stack'));
