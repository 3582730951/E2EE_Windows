import 'package:flutter/material.dart';

import '../../application/chat_providers.dart';
import 'desktop_shell_screen.dart';
import 'mobile_shell_screen.dart';

class ShellScreen extends StatelessWidget {
  const ShellScreen({super.key, required this.section});

  final AppSection section;

  @override
  Widget build(BuildContext context) {
    final width = MediaQuery.sizeOf(context).width;
    if (width < 760) {
      return MobileShellScreen(section: section);
    }
    return DesktopShellScreen(section: section);
  }
}
