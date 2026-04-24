import 'dart:ui';

import 'package:device_info_plus/device_info_plus.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../presentation/theme/visual_tier.dart';
import 'linux_memory_reader.dart';

final deviceInfoPluginProvider = Provider<DeviceInfoPlugin>((ref) {
  return DeviceInfoPlugin();
});

final visualPreferenceProvider =
    NotifierProvider<VisualPreferenceController, VisualPreference>(
      VisualPreferenceController.new,
    );

final deviceCapabilityProfileProvider =
    FutureProvider<DeviceCapabilityProfile?>((ref) async {
      try {
        final plugin = ref.watch(deviceInfoPluginProvider);
        final view = _primaryView();
        if (view == null) {
          return null;
        }

        final disableAnimations = WidgetsBinding
            .instance
            .platformDispatcher
            .accessibilityFeatures
            .disableAnimations;
        final logicalSize = Size(
          view.physicalSize.width / view.devicePixelRatio,
          view.physicalSize.height / view.devicePixelRatio,
        );

        if (kIsWeb) {
          final browserInfo = await plugin.webBrowserInfo;
          final deviceLabel = browserInfo.browserName.name.isNotEmpty
              ? browserInfo.browserName.name
              : browserInfo.appName ?? browserInfo.vendor ?? '';
          final osVersion =
              browserInfo.userAgent ?? browserInfo.appVersion ?? '';
          return DeviceCapabilityProfile(
            platform: defaultTargetPlatform,
            deviceLabel: deviceLabel,
            osVersion: osVersion,
            logicalWidth: logicalSize.width,
            logicalHeight: logicalSize.height,
            devicePixelRatio: view.devicePixelRatio,
            disableAnimations: disableAnimations,
            webDeviceMemoryGb: browserInfo.deviceMemory,
          );
        }

        switch (defaultTargetPlatform) {
          case TargetPlatform.android:
            final androidInfo = await plugin.androidInfo;
            final deviceLabel = androidInfo.model;
            final osVersion = androidInfo.version.release.isNotEmpty
                ? androidInfo.version.release
                : androidInfo.version.sdkInt.toString();
            if (deviceLabel.isEmpty || osVersion.isEmpty) {
              return null;
            }
            return DeviceCapabilityProfile(
              platform: TargetPlatform.android,
              deviceLabel: deviceLabel,
              osVersion: osVersion,
              logicalWidth: logicalSize.width,
              logicalHeight: logicalSize.height,
              devicePixelRatio: view.devicePixelRatio,
              disableAnimations: disableAnimations,
              androidIsLowRamDevice: androidInfo.isLowRamDevice,
              androidPhysicalRamSizeMb: androidInfo.physicalRamSize,
            );
          case TargetPlatform.iOS:
            final iosInfo = await plugin.iosInfo;
            if (iosInfo.model.isEmpty || iosInfo.systemVersion.isEmpty) {
              return null;
            }
            return DeviceCapabilityProfile(
              platform: TargetPlatform.iOS,
              deviceLabel: iosInfo.model,
              osVersion: iosInfo.systemVersion,
              logicalWidth: logicalSize.width,
              logicalHeight: logicalSize.height,
              devicePixelRatio: view.devicePixelRatio,
              disableAnimations: disableAnimations,
              iosPhysicalRamSizeMb: iosInfo.physicalRamSize,
            );
          case TargetPlatform.macOS:
            final macInfo = await plugin.macOsInfo;
            final deviceLabel = macInfo.model.isNotEmpty
                ? macInfo.model
                : macInfo.modelName.isNotEmpty
                ? macInfo.modelName
                : macInfo.computerName;
            final osVersion = macInfo.osRelease;
            if (deviceLabel.isEmpty || osVersion.isEmpty) {
              return null;
            }
            return DeviceCapabilityProfile(
              platform: TargetPlatform.macOS,
              deviceLabel: deviceLabel,
              osVersion: osVersion,
              logicalWidth: logicalSize.width,
              logicalHeight: logicalSize.height,
              devicePixelRatio: view.devicePixelRatio,
              disableAnimations: disableAnimations,
              desktopSystemMemoryInMegabytes: _megabytesFromBytes(
                macInfo.memorySize,
              ),
            );
          case TargetPlatform.windows:
            final windowsInfo = await plugin.windowsInfo;
            final deviceLabel = windowsInfo.computerName;
            final osVersion = [
              windowsInfo.majorVersion,
              windowsInfo.minorVersion,
              windowsInfo.buildNumber,
            ].join('.');
            if (deviceLabel.isEmpty || osVersion == '..') {
              return null;
            }
            return DeviceCapabilityProfile(
              platform: TargetPlatform.windows,
              deviceLabel: deviceLabel,
              osVersion: osVersion,
              logicalWidth: logicalSize.width,
              logicalHeight: logicalSize.height,
              devicePixelRatio: view.devicePixelRatio,
              disableAnimations: disableAnimations,
              desktopSystemMemoryInMegabytes:
                  windowsInfo.systemMemoryInMegabytes,
            );
          case TargetPlatform.linux:
            final linuxInfo = await plugin.linuxInfo;
            final deviceLabel = linuxInfo.prettyName.isNotEmpty
                ? linuxInfo.prettyName
                : linuxInfo.name;
            final osVersion = linuxInfo.version ?? linuxInfo.versionId ?? '';
            if (deviceLabel.isEmpty || osVersion.isEmpty) {
              return null;
            }
            return DeviceCapabilityProfile(
              platform: TargetPlatform.linux,
              deviceLabel: deviceLabel,
              osVersion: osVersion,
              logicalWidth: logicalSize.width,
              logicalHeight: logicalSize.height,
              devicePixelRatio: view.devicePixelRatio,
              disableAnimations: disableAnimations,
              desktopSystemMemoryInMegabytes:
                  await readLinuxSystemMemoryInMegabytes(),
            );
          case TargetPlatform.fuchsia:
            return null;
        }
      } catch (_) {
        return null;
      }
    });

final resolvedVisualTierProvider = Provider<VisualTier>((ref) {
  final preference = ref.watch(visualPreferenceProvider);
  final profile = ref
      .watch(deviceCapabilityProfileProvider)
      .maybeWhen(data: (value) => value, orElse: () => null);
  if (profile == null) {
    return VisualTier.standard;
  }

  if (profile.disableAnimations) {
    return VisualTier.standard;
  }

  if (!profile.hasCriticalMetrics) {
    return VisualTier.standard;
  }

  if (preference == VisualPreference.forceStandard) {
    return VisualTier.standard;
  }

  if (preference == VisualPreference.forceEnhanced) {
    return _supportsEnhancedGlass(profile)
        ? VisualTier.enhancedGlass
        : VisualTier.standard;
  }

  if (!_supportsEnhancedGlass(profile)) {
    return VisualTier.standard;
  }

  return VisualTier.enhancedGlass;
});

class VisualPreferenceController extends Notifier<VisualPreference> {
  @override
  VisualPreference build() => VisualPreference.auto;

  void setPreference(VisualPreference value) {
    state = value;
  }
}

FlutterView? _primaryView() {
  final views = WidgetsBinding.instance.platformDispatcher.views;
  if (views.isEmpty) {
    return null;
  }
  return views.first;
}

bool _supportsEnhancedGlass(DeviceCapabilityProfile profile) {
  if (kIsWeb) {
    return false;
  }

  return profile.canUseEnhancedGlass;
}

int? _megabytesFromBytes(int bytes) {
  if (bytes <= 0) {
    return null;
  }
  return (bytes / 1024 / 1024).round();
}
