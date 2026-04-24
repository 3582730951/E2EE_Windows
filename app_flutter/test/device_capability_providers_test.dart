import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'package:mi_e2ee_im_app/bootstrap/device_capability_providers.dart';
import 'package:mi_e2ee_im_app/presentation/theme/visual_tier.dart';

import 'test_capabilities.dart';

void main() {
  test('Android low-RAM device falls back to standard', () async {
    final container = _containerWithProfile(
      buildTestCapabilityProfile(
        platform: TargetPlatform.android,
        disableAnimations: false,
        androidIsLowRamDevice: true,
        androidPhysicalRamSizeMb: 8192,
      ),
    );
    addTearDown(container.dispose);

    await container.read(deviceCapabilityProfileProvider.future);

    expect(container.read(resolvedVisualTierProvider), VisualTier.standard);
  });

  test('Android 8GB non-low-RAM device enables enhancedGlass', () async {
    final container = _containerWithProfile(
      buildTestCapabilityProfile(
        platform: TargetPlatform.android,
        disableAnimations: false,
        androidIsLowRamDevice: false,
        androidPhysicalRamSizeMb: 8192,
      ),
    );
    addTearDown(container.dispose);

    await container.read(deviceCapabilityProfileProvider.future);

    expect(
      container.read(resolvedVisualTierProvider),
      VisualTier.enhancedGlass,
    );
  });

  test('iOS 4GB device falls back to standard', () async {
    final container = _containerWithProfile(
      buildTestCapabilityProfile(
        platform: TargetPlatform.iOS,
        disableAnimations: false,
        iosPhysicalRamSizeMb: 4096,
      ),
    );
    addTearDown(container.dispose);

    await container.read(deviceCapabilityProfileProvider.future);

    expect(container.read(resolvedVisualTierProvider), VisualTier.standard);
  });

  test('Windows 16GB wide desktop enables enhancedGlass', () async {
    final container = _containerWithProfile(
      buildTestCapabilityProfile(
        platform: TargetPlatform.windows,
        disableAnimations: false,
        logicalSize: const Size(1600, 960),
        desktopSystemMemoryInMegabytes: 16384,
      ),
    );
    addTearDown(container.dispose);

    await container.read(deviceCapabilityProfileProvider.future);

    expect(
      container.read(resolvedVisualTierProvider),
      VisualTier.enhancedGlass,
    );
  });

  test(
    'wide-memory desktop still stays standard when viewport is narrow',
    () async {
      final container = _containerWithProfile(
        buildTestCapabilityProfile(
          platform: TargetPlatform.windows,
          disableAnimations: false,
          logicalSize: const Size(1024, 960),
          desktopSystemMemoryInMegabytes: 16384,
        ),
      );
      addTearDown(container.dispose);

      await container.read(deviceCapabilityProfileProvider.future);

      expect(container.read(resolvedVisualTierProvider), VisualTier.standard);
    },
  );

  test('missing metrics fall back to standard', () async {
    final container = _containerWithProfile(
      DeviceCapabilityProfile(
        platform: TargetPlatform.windows,
        deviceLabel: 'Surface Laptop',
        osVersion: '11.0.22631',
        logicalWidth: 1600,
        logicalHeight: 960,
        devicePixelRatio: 1.0,
        disableAnimations: false,
      ),
    );
    addTearDown(container.dispose);

    await container.read(deviceCapabilityProfileProvider.future);

    expect(container.read(resolvedVisualTierProvider), VisualTier.standard);
  });

  test('forceEnhanced still respects disableAnimations', () async {
    final container = _containerWithProfile(
      buildTestCapabilityProfile(
        platform: TargetPlatform.android,
        disableAnimations: true,
        androidIsLowRamDevice: false,
        androidPhysicalRamSizeMb: 8192,
      ),
    );
    addTearDown(container.dispose);

    await container.read(deviceCapabilityProfileProvider.future);
    container
        .read(visualPreferenceProvider.notifier)
        .setPreference(VisualPreference.forceEnhanced);

    expect(container.read(resolvedVisualTierProvider), VisualTier.standard);
  });

  test('forces standard when preference is set explicitly', () async {
    final container = _containerWithProfile(
      buildTestCapabilityProfile(
        platform: TargetPlatform.macOS,
        disableAnimations: false,
        desktopSystemMemoryInMegabytes: 16384,
      ),
    );
    addTearDown(container.dispose);

    await container.read(deviceCapabilityProfileProvider.future);
    container
        .read(visualPreferenceProvider.notifier)
        .setPreference(VisualPreference.forceStandard);

    expect(container.read(resolvedVisualTierProvider), VisualTier.standard);
  });

  test(
    'forceEnhanced remains standard when profile does not support glass',
    () async {
      final container = _containerWithProfile(
        buildTestCapabilityProfile(
          platform: TargetPlatform.iOS,
          disableAnimations: false,
          iosPhysicalRamSizeMb: 4096,
        ),
      );
      addTearDown(container.dispose);

      await container.read(deviceCapabilityProfileProvider.future);
      container
          .read(visualPreferenceProvider.notifier)
          .setPreference(VisualPreference.forceEnhanced);

      expect(container.read(resolvedVisualTierProvider), VisualTier.standard);
    },
  );
}

ProviderContainer _containerWithProfile(DeviceCapabilityProfile profile) {
  return ProviderContainer(
    overrides: [
      deviceCapabilityProfileProvider.overrideWith((ref) async => profile),
    ],
  );
}
