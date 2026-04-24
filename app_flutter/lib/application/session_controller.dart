import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../bootstrap/app_providers.dart';
import '../domain/entities/models.dart';

class SessionState {
  const SessionState({
    required this.isAuthenticated,
    this.isBusy = false,
    this.errorMessage,
    this.profile,
  });

  const SessionState.signedOut()
    : isAuthenticated = false,
      isBusy = false,
      errorMessage = null,
      profile = null;

  final bool isAuthenticated;
  final bool isBusy;
  final String? errorMessage;
  final SessionProfile? profile;

  SessionState copyWith({
    bool? isAuthenticated,
    bool? isBusy,
    String? errorMessage,
    SessionProfile? profile,
    bool clearError = false,
  }) {
    return SessionState(
      isAuthenticated: isAuthenticated ?? this.isAuthenticated,
      isBusy: isBusy ?? this.isBusy,
      errorMessage: clearError ? null : errorMessage ?? this.errorMessage,
      profile: profile ?? this.profile,
    );
  }
}

class SessionController extends Notifier<SessionState> {
  @override
  SessionState build() => const SessionState.signedOut();

  Future<bool> signIn({
    required String username,
    required String password,
  }) async {
    state = state.copyWith(isBusy: true, clearError: true);
    SessionProfile? profile;
    try {
      profile = await ref
          .read(sdkClientProvider)
          .login(username: username, password: password);
    } catch (error) {
      state = state.copyWith(
        isBusy: false,
        errorMessage: _formatLoginError(error),
      );
      return false;
    }
    if (profile == null) {
      state = state.copyWith(isBusy: false, errorMessage: '请输入有效的账号和密码。');
      return false;
    }
    state = SessionState(
      isAuthenticated: true,
      isBusy: false,
      profile: profile,
    );
    return true;
  }

  Future<void> signOut() async {
    await ref.read(sdkClientProvider).logout();
    state = const SessionState.signedOut();
  }

  String _formatLoginError(Object error) {
    final text = error.toString();
    if (text.startsWith('Bad state: ')) {
      return text.substring('Bad state: '.length);
    }
    return text;
  }
}
