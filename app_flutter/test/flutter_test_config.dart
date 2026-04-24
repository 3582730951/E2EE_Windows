import 'dart:async';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

Future<void> testExecutable(FutureOr<void> Function() testMain) async {
  TestWidgetsFlutterBinding.ensureInitialized();
  await _loadAppFonts();
  await testMain();
}

Future<void> _loadAppFonts() async {
  final fontAssets = <String, List<String>>{
    'MaterialIcons': <String>['fonts/MaterialIcons-Regular.otf'],
    'CupertinoIcons': <String>[
      'packages/cupertino_icons/assets/CupertinoIcons.ttf',
    ],
    'SourceHanSansSC': <String>[
      'assets/fonts/source_han_sans_sc_regular.otf',
      'assets/fonts/source_han_sans_sc_medium.otf',
      'assets/fonts/source_han_sans_sc_semibold.otf',
    ],
    'SourceSans3': <String>[
      'assets/fonts/source_sans_3_regular.ttf',
      'assets/fonts/source_sans_3_medium.ttf',
      'assets/fonts/source_sans_3_semibold.ttf',
    ],
    'JetBrainsMono': <String>[
      'assets/fonts/jetbrains_mono_regular.ttf',
      'assets/fonts/jetbrains_mono_medium.ttf',
    ],
  };

  for (final entry in fontAssets.entries) {
    final loader = FontLoader(entry.key);
    for (final asset in entry.value) {
      loader.addFont(rootBundle.load(asset));
    }
    await loader.load();
  }
}
