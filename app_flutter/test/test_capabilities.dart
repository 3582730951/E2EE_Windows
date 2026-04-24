import 'package:flutter/material.dart';
import 'package:flutter_riverpod/misc.dart' show Override;

import 'package:mi_e2ee_im_app/bootstrap/device_capability_providers.dart';
import 'package:mi_e2ee_im_app/presentation/theme/visual_tier.dart';

DeviceCapabilityProfile buildTestCapabilityProfile({
  required TargetPlatform platform,
  Size logicalSize = const Size(1440, 960),
  double devicePixelRatio = 1.0,
  bool disableAnimations = true,
  bool? androidIsLowRamDevice,
  int? androidPhysicalRamSizeMb,
  int? iosPhysicalRamSizeMb,
  int? desktopSystemMemoryInMegabytes,
  double? webDeviceMemoryGb,
}) {
  final deviceLabel = switch (platform) {
    TargetPlatform.android => 'Pixel 8',
    TargetPlatform.iOS => 'iPhone 15',
    TargetPlatform.macOS => 'MacBook Pro',
    TargetPlatform.windows => 'Surface Laptop',
    TargetPlatform.linux => 'Linux Desktop',
    TargetPlatform.fuchsia => 'Fuchsia Device',
  };
  final osVersion = switch (platform) {
    TargetPlatform.android => '14',
    TargetPlatform.iOS => '17.0',
    TargetPlatform.macOS => '14.5',
    TargetPlatform.windows => '11.0.22631',
    TargetPlatform.linux => '6.8',
    TargetPlatform.fuchsia => '1.0',
  };

  return DeviceCapabilityProfile(
    platform: platform,
    deviceLabel: deviceLabel,
    osVersion: osVersion,
    logicalWidth: logicalSize.width,
    logicalHeight: logicalSize.height,
    devicePixelRatio: devicePixelRatio,
    disableAnimations: disableAnimations,
    androidIsLowRamDevice: androidIsLowRamDevice,
    androidPhysicalRamSizeMb: androidPhysicalRamSizeMb,
    iosPhysicalRamSizeMb: iosPhysicalRamSizeMb,
    desktopSystemMemoryInMegabytes: desktopSystemMemoryInMegabytes,
    webDeviceMemoryGb: webDeviceMemoryGb,
  );
}

List<Override> buildTestCapabilityOverrides({
  required TargetPlatform platform,
  Size logicalSize = const Size(1440, 960),
  double devicePixelRatio = 1.0,
  bool disableAnimations = true,
  bool? androidIsLowRamDevice,
  int? androidPhysicalRamSizeMb,
  int? iosPhysicalRamSizeMb,
  int? desktopSystemMemoryInMegabytes,
  double? webDeviceMemoryGb,
}) {
  final profile = buildTestCapabilityProfile(
    platform: platform,
    logicalSize: logicalSize,
    devicePixelRatio: devicePixelRatio,
    disableAnimations: disableAnimations,
    androidIsLowRamDevice: androidIsLowRamDevice,
    androidPhysicalRamSizeMb: androidPhysicalRamSizeMb,
    iosPhysicalRamSizeMb: iosPhysicalRamSizeMb,
    desktopSystemMemoryInMegabytes: desktopSystemMemoryInMegabytes,
    webDeviceMemoryGb: webDeviceMemoryGb,
  );
  return <Override>[
    deviceCapabilityProfileProvider.overrideWith((ref) async => profile),
  ];
}
