import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../application/chat_providers.dart';
import '../../bootstrap/app_providers.dart';
import '../screens/login_screen.dart';
import '../screens/shell_screen.dart';

final appRouterProvider = Provider<GoRouter>((ref) {
  final bootstrap = ref.watch(appBootstrapProvider);
  final session = ref.watch(sessionControllerProvider);

  return GoRouter(
    initialLocation: '/auth',
    redirect: (context, state) {
      if (bootstrap.isLoading) {
        return '/auth';
      }
      final isAuth = state.uri.path == '/auth';
      if (!session.isAuthenticated) {
        return isAuth ? null : '/auth';
      }
      if (isAuth) {
        return '/app/chats';
      }
      return null;
    },
    routes: <GoRoute>[
      GoRoute(path: '/auth', builder: (context, state) => const LoginScreen()),
      GoRoute(
        path: '/app/:section',
        builder: (context, state) => ShellScreen(
          section: _parseSection(state.pathParameters['section']),
        ),
      ),
    ],
  );
});

AppSection _parseSection(String? raw) {
  return switch (raw) {
    'contacts' => AppSection.contacts,
    'settings' => AppSection.settings,
    _ => AppSection.chats,
  };
}
