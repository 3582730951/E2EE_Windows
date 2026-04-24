import 'dart:io';

Future<int?> readLinuxSystemMemoryInMegabytes() async {
  try {
    final memInfo = await File('/proc/meminfo').readAsLines();
    for (final line in memInfo) {
      if (!line.startsWith('MemTotal:')) {
        continue;
      }
      final match = RegExp(r'^MemTotal:\s+(\d+)\s+kB$').firstMatch(line);
      if (match == null) {
        return null;
      }
      final kib = int.tryParse(match.group(1)!);
      if (kib == null || kib <= 0) {
        return null;
      }
      return (kib / 1024).round();
    }
    return null;
  } catch (_) {
    return null;
  }
}
