import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../application/session_controller.dart';
import '../native_sdk/sdk_client.dart';

final themeModeProvider = NotifierProvider<ThemeModeController, ThemeMode>(
  ThemeModeController.new,
);

final sdkClientProvider = Provider<NativeSdkClient>((ref) {
  final client = SdkClientFactory.create();
  ref.onDispose(client.dispose);
  return client;
});

final appBootstrapProvider = FutureProvider<void>((ref) async {
  await ref.read(sdkClientProvider).initialize();
});

final sessionControllerProvider =
    NotifierProvider<SessionController, SessionState>(SessionController.new);

final selectedConversationIdProvider =
    NotifierProvider<SelectedConversationController, String?>(
      SelectedConversationController.new,
    );

class ThemeModeController extends Notifier<ThemeMode> {
  @override
  ThemeMode build() => ThemeMode.dark;

  void setThemeMode(ThemeMode value) {
    state = value;
  }
}

class SelectedConversationController extends Notifier<String?> {
  @override
  String? build() => null;

  void select(String? conversationId) {
    state = conversationId;
  }
}
