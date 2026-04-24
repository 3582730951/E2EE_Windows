import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

class MessageActionSheet extends StatelessWidget {
  const MessageActionSheet({super.key, required this.cupertinoStyle});

  final bool cupertinoStyle;

  static const List<String> actions = <String>[
    '回复',
    '转发',
    '复制',
    '收藏',
    '删除',
    '更多表情',
  ];

  @override
  Widget build(BuildContext context) {
    if (cupertinoStyle) {
      return CupertinoActionSheet(
        actions: <Widget>[
          for (final action in actions)
            CupertinoActionSheetAction(
              onPressed: () => Navigator.of(context).pop(),
              isDestructiveAction: action == '删除',
              child: Text(action),
            ),
        ],
        cancelButton: CupertinoActionSheetAction(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('取消'),
        ),
      );
    }

    return SafeArea(
      child: Column(
        key: const Key('mobile-chat-message-action-sheet'),
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          for (final action in actions)
            ListTile(
              dense: true,
              title: Text(action),
              textColor: action == '删除'
                  ? Theme.of(context).colorScheme.error
                  : null,
              onTap: () => Navigator.of(context).pop(),
            ),
        ],
      ),
    );
  }
}
