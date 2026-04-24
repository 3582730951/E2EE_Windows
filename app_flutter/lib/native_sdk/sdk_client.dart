import 'dart:async';
import 'dart:io';

import '../domain/entities/models.dart';
import 'fake_sdk_client.dart';
import 'ffi_sdk_client.dart';

abstract class NativeSdkClient {
  Future<void> initialize();
  Future<SessionProfile?> login({
    required String username,
    required String password,
  });
  Future<void> logout();
  Stream<List<ConversationSummary>> watchConversations();
  Stream<List<ChatMessage>> watchMessages(String conversationId);
  Stream<List<ContactProfile>> watchContacts();
  Stream<List<DeviceTrustInfo>> watchDevices();
  Future<void> markConversationRead({required String conversationId});
  Future<void> sendMessage({
    required String conversationId,
    required String text,
  });
  void dispose();
}

class SdkClientFactory {
  const SdkClientFactory._();

  static NativeSdkClient create() {
    final mode = Platform.environment['MI_E2EE_SDK_MODE']?.toLowerCase();
    if (mode == 'fake') {
      return FakeSdkClient();
    }
    return FfiSdkClient(_defaultLibraryPath());
  }

  static String _defaultLibraryPath() {
    final explicitPath =
        Platform.environment['MI_E2EE_SDK_LIBRARY'] ??
        Platform.environment['MI_E2EE_SDK_DLL'];
    if (explicitPath != null && explicitPath.isNotEmpty) {
      return explicitPath;
    }
    if (Platform.isWindows) {
      return 'mi_e2ee_client_sdk.dll';
    }
    if (Platform.isAndroid || Platform.isLinux) {
      return 'libmi_e2ee_client_sdk.so';
    }
    if (Platform.isMacOS) {
      return 'libmi_e2ee_client_sdk.dylib';
    }
    if (Platform.isIOS) {
      return '';
    }
    throw UnsupportedError('当前平台没有可加载的 MI E2EE 原生 SDK。');
  }
}
