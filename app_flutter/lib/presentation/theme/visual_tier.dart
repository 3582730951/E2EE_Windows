import 'package:flutter/foundation.dart';

enum VisualTier { auto, standard, enhancedGlass }

enum VisualPreference { auto, forceStandard, forceEnhanced }

class DeviceCapabilityProfile {
  const DeviceCapabilityProfile({
    required this.platform,
    required this.deviceLabel,
    required this.osVersion,
    required this.logicalWidth,
    required this.logicalHeight,
    required this.devicePixelRatio,
    required this.disableAnimations,
    this.androidIsLowRamDevice,
    this.androidPhysicalRamSizeMb,
    this.iosPhysicalRamSizeMb,
    this.desktopSystemMemoryInMegabytes,
    this.webDeviceMemoryGb,
  });

  final TargetPlatform platform;
  final String deviceLabel;
  final String osVersion;
  final double logicalWidth;
  final double logicalHeight;
  final double devicePixelRatio;
  final bool disableAnimations;
  final bool? androidIsLowRamDevice;
  final int? androidPhysicalRamSizeMb;
  final int? iosPhysicalRamSizeMb;
  final int? desktopSystemMemoryInMegabytes;
  final double? webDeviceMemoryGb;

  bool get hasCoreMetrics =>
      deviceLabel.isNotEmpty &&
      osVersion.isNotEmpty &&
      logicalWidth > 0 &&
      logicalHeight > 0 &&
      devicePixelRatio > 0;

  bool get hasPlatformMemoryMetrics {
    return switch (platform) {
      TargetPlatform.android =>
        androidIsLowRamDevice != null && androidPhysicalRamSizeMb != null,
      TargetPlatform.iOS => iosPhysicalRamSizeMb != null,
      TargetPlatform.macOS ||
      TargetPlatform.windows ||
      TargetPlatform.linux => desktopSystemMemoryInMegabytes != null,
      TargetPlatform.fuchsia => false,
    };
  }

  bool get hasCriticalMetrics => hasCoreMetrics && hasPlatformMemoryMetrics;

  bool get canUseEnhancedGlass {
    if (disableAnimations || !hasCriticalMetrics) {
      return false;
    }

    return switch (platform) {
      TargetPlatform.android =>
        androidIsLowRamDevice == false &&
            (androidPhysicalRamSizeMb ?? 0) >= 6144,
      TargetPlatform.iOS => (iosPhysicalRamSizeMb ?? 0) >= 6144,
      TargetPlatform.macOS || TargetPlatform.windows || TargetPlatform.linux =>
        (desktopSystemMemoryInMegabytes ?? 0) >= 8192 && logicalWidth >= 1100,
      TargetPlatform.fuchsia => false,
    };
  }

  String get summary {
    return '$deviceLabel · $osVersion';
  }
}
