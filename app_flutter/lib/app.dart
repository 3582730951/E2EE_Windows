import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'bootstrap/device_capability_providers.dart';
import 'bootstrap/app_providers.dart';
import 'presentation/routes/app_router.dart';
import 'presentation/theme/app_theme.dart';

class MyApp extends ConsumerWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final router = ref.watch(appRouterProvider);
    final themeMode = ref.watch(themeModeProvider);
    final resolvedVisualTier = ref.watch(resolvedVisualTierProvider);

    return MaterialApp.router(
      title: 'MI E2EE IM',
      debugShowCheckedModeBanner: false,
      routerConfig: router,
      themeMode: themeMode,
      theme: AppTheme.light(visualTier: resolvedVisualTier),
      darkTheme: AppTheme.dark(visualTier: resolvedVisualTier),
    );
  }
}
