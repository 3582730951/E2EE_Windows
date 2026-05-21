#!/usr/bin/env python3
from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
CONTRACT_DIR = ROOT / "ui_contract"
WINDOWS_OWNERSHIP_PATH = ROOT / "tools" / "agent_ownership_windows_ui.json"
ANDROID_OWNERSHIP_PATH = ROOT / "tools" / "agent_ownership_android_ui.json"
IOS_OWNERSHIP_PATH = ROOT / "tools" / "agent_ownership_ios_ui.json"
UI_TEXT_WHITELIST_PATH = Path(__file__).with_name("ui_text_whitelist.json")


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def expect(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


def verify_ui_text_whitelist(errors: list[str]) -> None:
    expect(UI_TEXT_WHITELIST_PATH.exists(), f"missing UI text whitelist: {UI_TEXT_WHITELIST_PATH}", errors)
    if not UI_TEXT_WHITELIST_PATH.exists():
        return
    payload = load_json(UI_TEXT_WHITELIST_PATH)
    expect(isinstance(payload, dict), "ui_text_whitelist.json must be a JSON object", errors)
    expect(
        isinstance(payload.get("zh_cn_allowed_latin_tokens"), list),
        "ui_text_whitelist.json.zh_cn_allowed_latin_tokens must be a list",
        errors,
    )
    expect(
        isinstance(payload.get("en_us_allowed_cjk_tokens"), list),
        "ui_text_whitelist.json.en_us_allowed_cjk_tokens must be a list",
        errors,
    )
    expect(
        isinstance(payload.get("notes"), str) and payload.get("notes"),
        "ui_text_whitelist.json.notes must be a non-empty string",
        errors,
    )


def verify_contract_files(errors: list[str]) -> dict[str, Any]:
    required = [
        "tokens.json",
        "routes.json",
        "components.json",
        "acceptance.json",
        "copy_keys.json",
    ]
    loaded: dict[str, Any] = {}
    for name in required:
        path = CONTRACT_DIR / name
        expect(path.exists(), f"missing contract file: {path}", errors)
        if path.exists():
            loaded[name] = load_json(path)
    return loaded


def verify_tokens(tokens: dict[str, Any], errors: list[str]) -> None:
    expect(tokens.get("style_anchor") == "mi-hybrid", "tokens.style_anchor must be mi-hybrid", errors)
    theme_policy = tokens.get("theme_policy", {})
    expect(theme_policy.get("default_mode") == "system", "theme_policy.default_mode must be system", errors)
    expect(sorted(theme_policy.get("supports", [])) == ["dark", "light"], "theme_policy.supports must contain light and dark", errors)
    for theme_name in ("light", "dark"):
        theme = tokens.get("semantic_colors", {}).get(theme_name, {})
        for key in (
            "background",
            "surface",
            "surface_variant",
            "text_primary",
            "text_secondary",
            "accent",
            "success",
            "warning",
            "danger",
        ):
            expect(key in theme, f"semantic_colors.{theme_name}.{key} missing", errors)
    layout = tokens.get("layout", {})
    expect(layout.get("conversation_row_height") == 64, "layout.conversation_row_height must be 64", errors)
    expect(layout.get("avatar_size") == 44, "layout.avatar_size must be 44", errors)
    breakpoints = tokens.get("desktop_breakpoints", {})
    expect(breakpoints.get("three_column_min_width") == 1360, "desktop three-column breakpoint must be 1360", errors)
    typography = tokens.get("typography", {})
    windows_font_stacks = typography.get("windows_font_stacks", {})
    expect(
        windows_font_stacks.get("sans_zh_cn") == [
            "Microsoft YaHei UI",
            "Segoe UI Variable",
            "Segoe UI",
        ],
        "typography.windows_font_stacks.sans_zh_cn mismatch",
        errors,
    )
    expect(
        windows_font_stacks.get("sans_en_us") == [
            "Segoe UI Variable",
            "Segoe UI",
            "Microsoft YaHei UI",
        ],
        "typography.windows_font_stacks.sans_en_us mismatch",
        errors,
    )
    expect(
        windows_font_stacks.get("mono") == [
            "JetBrains Mono",
            "Consolas",
            "Cascadia Mono",
        ],
        "typography.windows_font_stacks.mono mismatch",
        errors,
    )
    overflow_roles = typography.get("overflow_roles", {})
    for role in (
        "display",
        "title",
        "subtitle",
        "caption",
        "button_label",
        "code_inline",
        "value_single",
    ):
        expect(
            overflow_roles.get(role) == "singleLineElide",
            f"typography.overflow_roles.{role} must be singleLineElide",
            errors,
        )
    for role in ("detail", "supporting"):
        expect(
            overflow_roles.get(role) == "twoLineWrap",
            f"typography.overflow_roles.{role} must be twoLineWrap",
            errors,
        )
    message_body = overflow_roles.get("message_body", {})
    expect(message_body.get("mode") == "wrap", "typography.overflow_roles.message_body.mode must be wrap", errors)
    expect(
        message_body.get("metaInsetBottom") == 20,
        "typography.overflow_roles.message_body.metaInsetBottom must be 20",
        errors,
    )
    for forbidden in ("raw_id", "path"):
        expect(
            overflow_roles.get(forbidden) == "forbidden_in_overview",
            f"typography.overflow_roles.{forbidden} must be forbidden_in_overview",
            errors,
        )
    masking = typography.get("masking", {})
    expect(
        masking.get("gateway_display_states") == ["本地配置", "远程接入", "已固定"],
        "typography.masking.gateway_display_states mismatch",
        errors,
    )
    gateway_display_detail = masking.get("gateway_display_detail", {})
    expect(
        gateway_display_detail.get("mode") == "singleLineElide",
        "typography.masking.gateway_display_detail.mode must be singleLineElide",
        errors,
    )
    expect(
        gateway_display_detail.get("forbidden_fragments") == ["config:", "\\", "/"],
        "typography.masking.gateway_display_detail.forbidden_fragments mismatch",
        errors,
    )
    expect(
        masking.get("masked_current_device_id") == "first4_ellipsis_last4",
        "typography.masking.masked_current_device_id mismatch",
        errors,
    )
    expect(
        masking.get("full_sensitive_values") == "copy_only",
        "typography.masking.full_sensitive_values must be copy_only",
        errors,
    )
    expect(
        tokens.get("identity_system")
        == {
            "avatar_modes": ["person", "group-collage", "device", "system"],
            "avatar_rendering": "generated-not-remote",
            "presence_states": ["online", "typing", "muted", "secure", "busy"],
        },
        "tokens.identity_system mismatch",
        errors,
    )
    expect(
        tokens.get("media_system")
        == {
            "preview_kinds": ["photo", "file", "voice", "link"],
            "preview_density": "inline-card",
            "conversation_row_supports_media_hint": True,
        },
        "tokens.media_system mismatch",
        errors,
    )
    expect(
        tokens.get("security_state_tones") == ["healthy", "checking", "review", "blocked"],
        "tokens.security_state_tones mismatch",
        errors,
    )
    expect(
        tokens.get("empty_state") == {"illustration_required": True, "text_budget": "title+supporting-only"},
        "tokens.empty_state mismatch",
        errors,
    )


def verify_routes(routes: dict[str, Any], errors: list[str]) -> None:
    expect(routes.get("primary_domains") == ["chats", "contacts", "calls", "settings"], "primary_domains must be chats/contacts/calls/settings", errors)
    security_center = routes.get("security_center", {})
    expect(security_center.get("parent_domain") == "settings", "Security Center must live under settings", errors)
    for platform in ("ios", "android", "windows"):
        expect(platform in routes.get("platform_adaptations", {}), f"missing platform adaptation for {platform}", errors)
    ios = routes.get("platform_adaptations", {}).get("ios", {})
    android = routes.get("platform_adaptations", {}).get("android", {})
    expect(ios.get("detail_hides_root_tab") is True, "iOS detail_hides_root_tab must be true", errors)
    expect(android.get("detail_hides_root_tab") is True, "Android detail_hides_root_tab must be true", errors)


def verify_components(components: dict[str, Any], errors: list[str]) -> None:
    present = {component["id"] for component in components.get("required_components", [])}
    for component_id in (
        "UiText",
        "AppBarPrimary",
        "ConversationRow",
        "MessageBubble",
        "ComposerBar",
        "StatusBanner",
        "SecurityBadge",
        "IdentityAvatar",
        "MediaPreviewCard",
        "SecurityStateStrip",
        "EmptyStateIllustration",
        "RightDetailPane",
    ):
        expect(component_id in present, f"required component missing: {component_id}", errors)
    windows_fields = components.get("windows_display_fields", {})
    expect(
        windows_fields.get("gatewayDisplayState", {}).get("allowed_values") == ["本地配置", "远程接入", "已固定"],
        "components.windows_display_fields.gatewayDisplayState.allowed_values mismatch",
        errors,
    )
    detail_field = windows_fields.get("gatewayDisplayDetail", {})
    expect(
        detail_field.get("mode") == "singleLineElide",
        "components.windows_display_fields.gatewayDisplayDetail.mode must be singleLineElide",
        errors,
    )
    expect(
        detail_field.get("forbidden_fragments") == ["config:", "\\", "/"],
        "components.windows_display_fields.gatewayDisplayDetail.forbidden_fragments mismatch",
        errors,
    )
    expect(
        windows_fields.get("maskedCurrentDeviceId", {}).get("format") == "first4_ellipsis_last4",
        "components.windows_display_fields.maskedCurrentDeviceId.format mismatch",
        errors,
    )


def verify_acceptance(acceptance: dict[str, Any], errors: list[str]) -> None:
    scenes = acceptance.get("fixture_scenes", {})
    required_scenes = {
        "ios": {"detail", "calls", "security"},
        "android": {"calls", "security_center"},
        "windows": {"post_login_light", "security_center", "chat_detail", "calls_home", "settings_home"},
    }
    for platform, platform_scenes in required_scenes.items():
        for required_scene in platform_scenes:
            expect(required_scene in scenes.get(platform, []), f"fixture scene missing: {platform}.{required_scene}", errors)
    expect(
        acceptance.get("theme_matrix") == ["light", "dark"],
        "acceptance.theme_matrix must be [light, dark]",
        errors,
    )
    expect(
        acceptance.get("state_matrix") == ["healthy", "attention", "blocking"],
        "acceptance.state_matrix must be [healthy, attention, blocking]",
        errors,
    )
    expect(
        acceptance.get("required_evidence")
        == [
            "build",
            "navigation_flow",
            "golden_diff",
            "accessibility",
            "theme_parity",
            "packaging",
            "ownership_manifest",
            "text_policy",
            "literal_copy",
            "identity_visuals",
            "security_tone_matrix",
            "empty_state_art",
            "detail_tab_hidden_parity",
            "scene_matrix",
            "viewport_matrix",
            "tuple_matrix",
        ],
        "acceptance.required_evidence mismatch",
        errors,
    )
    metrics = acceptance.get("metrics", {})
    expect(metrics.get("minimum_hit_target_ios") == 44, "iOS minimum hit target must be 44", errors)
    expect(metrics.get("minimum_hit_target_android") == 48, "Android minimum hit target must be 48", errors)
    platform_requirements = acceptance.get("platform_requirements", {})
    ios_requirements = platform_requirements.get("ios", {})
    android_requirements = platform_requirements.get("android", {})
    windows_requirements = platform_requirements.get("windows", {})
    expect(
        ios_requirements.get("required_native_interactions") == ["swipe_actions", "context_menu", "detail_hides_root_tab"],
        "acceptance.platform_requirements.ios.required_native_interactions mismatch",
        errors,
    )
    expect(
        ios_requirements.get("required_fixture_pairs") == [["login", "detail"], ["chats", "calls"], ["settings", "security"]],
        "acceptance.platform_requirements.ios.required_fixture_pairs mismatch",
        errors,
    )
    expect(
        android_requirements.get("required_composer_surfaces") == ["collapsed", "quick_actions", "emoji", "voice"],
        "acceptance.platform_requirements.android.required_composer_surfaces mismatch",
        errors,
    )
    expect(
        android_requirements.get("required_fixture_pairs") == [["login", "detail"], ["chats", "calls"], ["settings", "security_center"]],
        "acceptance.platform_requirements.android.required_fixture_pairs mismatch",
        errors,
    )
    expect(
        windows_requirements.get("required_shell_features") == [
            "open_new_chat_shortcut",
            "compact_two_column",
            "drawer_two_column",
            "escape_close_priority",
        ],
        "acceptance.platform_requirements.windows.required_shell_features mismatch",
        errors,
    )
    expect(
        windows_requirements.get("required_shortcuts") == ["Ctrl+K", "Ctrl+F", "Ctrl+N", "Esc"],
        "acceptance.platform_requirements.windows.required_shortcuts mismatch",
        errors,
    )
    expected_mobile_viewports = {
        "login": [430, 932],
        "chats": [430, 932],
        "detail": [430, 932],
        "calls": [430, 932],
        "settings": [430, 932],
        "security": [430, 932],
    }
    expect(
        ios_requirements.get("fixture_viewports") == expected_mobile_viewports,
        "acceptance.platform_requirements.ios.fixture_viewports mismatch",
        errors,
    )
    expected_android_viewports = {
        "login": [412, 915],
        "chats": [412, 915],
        "detail": [412, 915],
        "calls": [412, 915],
        "settings": [412, 915],
        "security_center": [412, 915],
    }
    expect(
        android_requirements.get("fixture_viewports") == expected_android_viewports,
        "acceptance.platform_requirements.android.fixture_viewports mismatch",
        errors,
    )
    windows = acceptance.get("windows", {})
    expect(
        windows.get("dpi_matrix") == [1.0, 1.25, 1.5, 1.75, 2.0],
        "acceptance.windows.dpi_matrix mismatch",
        errors,
    )
    expect(
        windows.get("fixture_viewports", {}).get("login") == [840, 620],
        "acceptance.windows.fixture_viewports.login mismatch",
        errors,
    )
    for scene in ("post_login", "chat_detail", "calls_home", "settings_home", "post_login_light", "security_center"):
        expect(
            windows.get("fixture_viewports", {}).get(scene) == [900, 620],
            f"acceptance.windows.fixture_viewports.{scene} mismatch",
            errors,
        )
    expect(
        len(windows.get("acceptance_surfaces", [])) >= 10,
        "acceptance.windows.acceptance_surfaces must list the Windows acceptance perimeter",
        errors,
    )
    excluded = windows.get("excluded_surfaces", [])
    for surface in (
        "dialogs/NewChatDialog.qml",
        "dialogs/AddContactDialog.qml",
        "dialogs/CreateGroupWizard.qml",
        "dialogs/NotificationCenterDialog.qml",
        "shell/GroupCallWindow.qml",
        "widgets/**",
    ):
        expect(surface in excluded, f"acceptance.windows.excluded_surfaces missing {surface}", errors)
    expect(windows.get("legacy_scope") == "excluded", "acceptance.windows.legacy_scope must be excluded", errors)


def verify_runtime_artifact_contract(acceptance: dict[str, Any], errors: list[str]) -> None:
    runtime = acceptance.get("runtime_artifact_contract", {})
    manifests = runtime.get("evidence_manifests", {})
    static_manifest = manifests.get("static_contract", {})
    expect(
        static_manifest.get("artifact_name") == "mi_e2ee_ui_contract_evidence",
        "runtime_artifact_contract.evidence_manifests.static_contract.artifact_name mismatch",
        errors,
    )
    expect(
        static_manifest.get("manifest_file") == "ui-contract-static-evidence.json",
        "runtime_artifact_contract.evidence_manifests.static_contract.manifest_file mismatch",
        errors,
    )
    expect(
        static_manifest.get("required_evidence") == ["accessibility", "ownership_manifest", "text_policy", "literal_copy"],
        "runtime_artifact_contract.evidence_manifests.static_contract.required_evidence mismatch",
        errors,
    )
    expected_runtime_manifests = {
        "windows_runtime": (
            "mi_e2ee_windows_ui_runtime",
            "windows-runtime-evidence.json",
            "windows-theme-",
            "windows-state-",
            "windows-scene-",
            "windows-tuple-",
        ),
        "ios_runtime": (
            "mi_e2ee_ios_rootauth_privacy_evidence",
            "ios-runtime-evidence.json",
            "ios-theme-",
            "ios-state-",
            "ios-scene-",
            "ios-tuple-",
        ),
        "android_runtime": (
            "mi_e2ee_android_ui_smoke",
            "android-runtime-evidence.json",
            "android-theme-",
            "android-state-",
            "android-scene-",
            "android-tuple-",
        ),
    }
    for key, (artifact_name, manifest_file, theme_prefix, state_prefix, scene_prefix, tuple_prefix) in expected_runtime_manifests.items():
        manifest = manifests.get(key, {})
        expect(
            manifest.get("artifact_name") == artifact_name,
            f"runtime_artifact_contract.evidence_manifests.{key}.artifact_name mismatch",
            errors,
        )
        expect(
            manifest.get("manifest_file") == manifest_file,
            f"runtime_artifact_contract.evidence_manifests.{key}.manifest_file mismatch",
            errors,
        )
        expect(
            manifest.get("theme_marker_prefix") == theme_prefix,
            f"runtime_artifact_contract.evidence_manifests.{key}.theme_marker_prefix mismatch",
            errors,
        )
        expect(
            manifest.get("state_marker_prefix") == state_prefix,
            f"runtime_artifact_contract.evidence_manifests.{key}.state_marker_prefix mismatch",
            errors,
        )
        expect(
            manifest.get("scene_marker_prefix") == scene_prefix,
            f"runtime_artifact_contract.evidence_manifests.{key}.scene_marker_prefix mismatch",
            errors,
        )
        expect(
            manifest.get("tuple_marker_prefix") == tuple_prefix,
            f"runtime_artifact_contract.evidence_manifests.{key}.tuple_marker_prefix mismatch",
            errors,
        )
    expect(
        manifests.get("android_runtime", {}).get("required_runtime_markers")
        == ["scene", "theme", "state", "tuple"],
        "runtime_artifact_contract.evidence_manifests.android_runtime.required_runtime_markers mismatch",
        errors,
    )
    expect(
        manifests.get("android_runtime", {}).get("runtime_marker_source")
        == "android/scripts/ci_ui_smoke.sh",
        "runtime_artifact_contract.evidence_manifests.android_runtime.runtime_marker_source mismatch",
        errors,
    )
    artifact_expectations = runtime.get("artifact_expectations", {})
    expect(
        artifact_expectations.get("windows_packages") == ["mi_e2ee_client", "mi_e2ee_server"],
        "runtime_artifact_contract.artifact_expectations.windows_packages mismatch",
        errors,
    )
    expect(
        artifact_expectations.get("ios_bundle_glob") == "RootAuthApp-sim.zip",
        "runtime_artifact_contract.artifact_expectations.ios_bundle_glob mismatch",
        errors,
    )
    expect(
        artifact_expectations.get("android_packages") == [
            "mi_e2ee_android_debug",
            "mi_e2ee_android_rootauth_debug",
            "mi_e2ee_android_release",
            "mi_e2ee_android_rootauth_release",
        ],
        "runtime_artifact_contract.artifact_expectations.android_packages mismatch",
        errors,
    )
    expect(
        artifact_expectations.get("android_arm64_validation_manifest") == "android-arm64-validation.json",
        "runtime_artifact_contract.artifact_expectations.android_arm64_validation_manifest mismatch",
        errors,
    )
    expect(
        artifact_expectations.get("android_arm64_requirement")
        == {
            "mode": "required_or_waived",
            "allowed_waiver_reasons": ["android_tests_skipped", "missing_gcp_credentials"],
            "validated_evidence_source": "firebase_test_lab",
            "waived_evidence_source": "ci_waiver",
            "required_privacy_fields": [
                "skip_android_tests",
                "has_gcp_service_account_json",
                "has_gcp_project_id",
                "ftl_validated_reported",
                "ftl_requested_device_matrix_non_empty",
                "ftl_device_matrix_arm64_approved",
            ],
            "validated_requires_privacy_true": [
                "ftl_validated_reported",
                "ftl_requested_device_matrix_non_empty",
                "ftl_device_matrix_arm64_approved",
            ],
            "required_manifest_fields": [
                "platform",
                "requirement_mode",
                "validated",
                "waived",
                "waiver_reason",
                "reason_detail",
                "gate_passed",
                "evidence_source",
                "privacy_evidence",
                "requested_device_matrix",
            ],
        },
        "runtime_artifact_contract.artifact_expectations.android_arm64_requirement mismatch",
        errors,
    )
    screenshot_inventory = runtime.get("screenshot_inventory", {})
    expected_inventory = {
        "windows": {
            "login": "login-{locale}-{theme}-{scale}.png",
            "post_login": "post-login-{locale}-{theme}-{scale}.png",
            "chat_detail": "chat-detail-{locale}-{theme}-{scale}.png",
            "calls_home": "calls-home-{locale}-{theme}-{scale}.png",
            "settings_home": "settings-home-{locale}-{theme}-{scale}.png",
            "post_login_light": "post-login-light-{locale}-{theme}-{scale}.png",
            "security_center": "security-center-{locale}-{theme}-{scale}.png",
        },
        "ios": {
            "login": "rootauth-ios-login.png",
            "chats": "rootauth-ios-home.png",
            "chats_dark": "rootauth-ios-home-dark.png",
            "detail": "rootauth-ios-detail.png",
            "calls": "rootauth-ios-calls.png",
            "settings": "rootauth-ios-settings.png",
            "security": "rootauth-ios-security.png",
            "security_dark": "rootauth-ios-security-dark.png",
        },
        "android": {
            "login": "android-login.png",
            "chats": "android-chats.png",
            "chats_dark": "android-chats-dark.png",
            "detail": "android-detail.png",
            "calls": "android-calls.png",
            "settings": "android-settings.png",
            "security_center": "android-security.png",
            "security_center_dark": "android-security-dark.png",
        },
    }
    for platform, inventory in expected_inventory.items():
        expect(
            screenshot_inventory.get(platform) == inventory,
            f"runtime_artifact_contract.screenshot_inventory.{platform} mismatch",
            errors,
        )
    expected_tuple_inventory = {
        "windows": [
            {
                "scene": "login",
                "theme": "light",
                "state": "blocking",
                "evidence_files": [
                    "build/login-zh-CN-light-100.png",
                    "dist/login-zh-CN-light-100.png",
                ],
            },
            {
                "scene": "post_login",
                "theme": "light",
                "state": "healthy",
                "evidence_files": [
                    "build/post-login-zh-CN-light-100.png",
                    "dist/post-login-zh-CN-light-100.png",
                ],
            },
            {
                "scene": "post_login",
                "theme": "dark",
                "state": "healthy",
                "evidence_files": [
                    "build/post-login-zh-CN-dark-100.png",
                    "dist/post-login-zh-CN-dark-100.png",
                ],
            },
            {
                "scene": "chat_detail",
                "theme": "light",
                "state": "healthy",
                "evidence_files": [
                    "build/chat-detail-zh-CN-light-100.png",
                    "dist/chat-detail-zh-CN-light-100.png",
                ],
            },
            {
                "scene": "calls_home",
                "theme": "light",
                "state": "healthy",
                "evidence_files": [
                    "build/calls-home-zh-CN-light-100.png",
                    "dist/calls-home-zh-CN-light-100.png",
                ],
            },
            {
                "scene": "settings_home",
                "theme": "light",
                "state": "healthy",
                "evidence_files": [
                    "build/settings-home-zh-CN-light-100.png",
                    "dist/settings-home-zh-CN-light-100.png",
                ],
            },
            {
                "scene": "post_login_light",
                "theme": "light",
                "state": "healthy",
                "evidence_files": [
                    "build/post-login-light-zh-CN-light-100.png",
                    "dist/post-login-light-zh-CN-light-100.png",
                ],
            },
            {
                "scene": "security_center",
                "theme": "light",
                "state": "attention",
                "evidence_files": [
                    "build/security-center-zh-CN-light-100.png",
                    "dist/security-center-zh-CN-light-100.png",
                ],
            },
        ],
        "ios": [
            {
                "scene": "login",
                "theme": "light",
                "state": "blocking",
                "evidence_files": ["rootauth-ios-login.png"],
            },
            {
                "scene": "chats",
                "theme": "light",
                "state": "healthy",
                "evidence_files": ["rootauth-ios-home.png"],
            },
            {
                "scene": "chats",
                "theme": "dark",
                "state": "healthy",
                "evidence_files": ["rootauth-ios-home-dark.png"],
            },
            {
                "scene": "detail",
                "theme": "light",
                "state": "healthy",
                "evidence_files": ["rootauth-ios-detail.png"],
            },
            {
                "scene": "calls",
                "theme": "light",
                "state": "healthy",
                "evidence_files": ["rootauth-ios-calls.png"],
            },
            {
                "scene": "settings",
                "theme": "light",
                "state": "healthy",
                "evidence_files": ["rootauth-ios-settings.png"],
            },
            {
                "scene": "security",
                "theme": "light",
                "state": "attention",
                "evidence_files": ["rootauth-ios-security.png"],
            },
            {
                "scene": "security",
                "theme": "dark",
                "state": "attention",
                "evidence_files": ["rootauth-ios-security-dark.png"],
            },
        ],
        "android": [
            {
                "scene": "login",
                "theme": "light",
                "state": "blocking",
                "evidence_files": ["android-login.png"],
            },
            {
                "scene": "chats",
                "theme": "light",
                "state": "healthy",
                "evidence_files": ["android-chats.png"],
            },
            {
                "scene": "chats",
                "theme": "dark",
                "state": "healthy",
                "evidence_files": ["android-chats-dark.png"],
            },
            {
                "scene": "detail",
                "theme": "light",
                "state": "healthy",
                "evidence_files": ["android-detail.png"],
            },
            {
                "scene": "calls",
                "theme": "light",
                "state": "healthy",
                "evidence_files": ["android-calls.png"],
            },
            {
                "scene": "settings",
                "theme": "light",
                "state": "healthy",
                "evidence_files": ["android-settings.png"],
            },
            {
                "scene": "security_center",
                "theme": "light",
                "state": "attention",
                "evidence_files": ["android-security.png"],
            },
            {
                "scene": "security_center",
                "theme": "dark",
                "state": "attention",
                "evidence_files": ["android-security-dark.png"],
            },
        ],
    }
    expect(
        runtime.get("tuple_inventory") == expected_tuple_inventory,
        "runtime_artifact_contract.tuple_inventory mismatch",
        errors,
    )
    tuple_inventory = runtime.get("tuple_inventory", {})
    for platform, entries in tuple_inventory.items():
        expect(isinstance(entries, list) and len(entries) > 0, f"tuple_inventory.{platform} must be a non-empty list", errors)
        if not isinstance(entries, list):
            continue
        seen: set[str] = set()
        for index, entry in enumerate(entries):
            expect(isinstance(entry, dict), f"tuple_inventory.{platform}[{index}] must be an object", errors)
            if not isinstance(entry, dict):
                continue
            scene = entry.get("scene")
            theme = entry.get("theme")
            state = entry.get("state")
            evidence_files = entry.get("evidence_files")
            expect(scene in acceptance.get("fixture_scenes", {}).get(platform, []), f"tuple_inventory.{platform}[{index}].scene is invalid", errors)
            expect(theme in acceptance.get("theme_matrix", []), f"tuple_inventory.{platform}[{index}].theme is invalid", errors)
            expect(state in acceptance.get("state_matrix", []), f"tuple_inventory.{platform}[{index}].state is invalid", errors)
            expect(
                isinstance(evidence_files, list) and len(evidence_files) > 0,
                f"tuple_inventory.{platform}[{index}].evidence_files must be a non-empty list",
                errors,
            )
            if scene is None or theme is None or state is None:
                continue
            tuple_key = f"{scene}|{theme}|{state}"
            expect(tuple_key not in seen, f"tuple_inventory.{platform} duplicate tuple: {tuple_key}", errors)
            seen.add(tuple_key)


def verify_copy_keys(copy_keys: dict[str, Any], errors: list[str]) -> None:
    domains = copy_keys.get("canonical_domains", {})
    for domain in ("navigation", "auth", "chat", "empty_state", "approval", "security_state", "identity", "security_center"):
        expect(domain in domains, f"copy key domain missing: {domain}", errors)
    ios_hints = copy_keys.get("platform_key_hints", {}).get("ios", {})
    for key, value in ios_hints.items():
        expect(value != "hardcoded-currently", f"iOS platform_key_hints.{key} must not be hardcoded-currently", errors)


def verify_one_ownership_manifest(
    path: Path, agent_names: list[str], errors: list[str]
) -> None:
    expect(path.exists(), f"missing ownership manifest: {path}", errors)
    if not path.exists():
        return
    ownership = load_json(path)
    for key in (
        "allowed_paths",
        "forbidden_paths",
        "no_edit_surfaces",
        "legacy_excluded_surfaces",
        "single_writer_manifests",
    ):
        expect(key in ownership, f"ownership manifest missing key: {path.name}.{key}", errors)
    allowed = ownership.get("allowed_paths", {})
    for agent_name in agent_names:
        expect(agent_name in allowed, f"ownership manifest missing agent: {path.name}.{agent_name}", errors)
    claimed_paths: dict[str, list[str]] = {}
    for agent_name in agent_names:
        for allowed_path in allowed.get(agent_name, []):
            claimed_paths.setdefault(allowed_path, []).append(agent_name)
    for repo_path, owners in sorted(claimed_paths.items()):
        expect(
            len(owners) == 1,
            f"ownership path claimed by multiple agents: {path.name}:{repo_path}:{','.join(owners)}",
            errors,
        )
    for writer_path in ownership.get("single_writer_manifests", []):
        expect((ROOT / writer_path).exists(), f"single writer manifest path missing: {writer_path}", errors)
    for repo_path in ownership.get("forbidden_paths", []):
        expect((ROOT / repo_path).exists(), f"forbidden path missing from repo: {repo_path}", errors)
    for agent_name in agent_names:
        for allowed_path in allowed.get(agent_name, []):
            expect((ROOT / allowed_path).exists(), f"allowed path missing from repo: {path.name}:{agent_name}:{allowed_path}", errors)


def verify_ownership(errors: list[str]) -> None:
    verify_one_ownership_manifest(WINDOWS_OWNERSHIP_PATH, ["Agent1", "Agent2", "Agent3"], errors)
    verify_one_ownership_manifest(ANDROID_OWNERSHIP_PATH, ["AndroidAgent1", "AndroidAgent2", "ValidationAgent"], errors)
    verify_one_ownership_manifest(IOS_OWNERSHIP_PATH, ["iOSAgent1", "iOSAgent2", "iOSAgent3", "ValidationAgent"], errors)


def manifest_coverage_paths(ownership: dict[str, Any]) -> set[str]:
    covered: set[str] = set(ownership.get("no_edit_surfaces", []))
    covered.update(ownership.get("single_writer_manifests", []))
    for paths in ownership.get("allowed_paths", {}).values():
        covered.update(paths)
    return covered


def verify_windows_ownership_coverage(acceptance: dict[str, Any], errors: list[str]) -> None:
    ownership = load_json(WINDOWS_OWNERSHIP_PATH)
    covered = manifest_coverage_paths(ownership)
    phase_critical_paths = set(acceptance.get("windows", {}).get("acceptance_surfaces", []))
    phase_critical_paths.update(
        {
            "client/ui/qml/i18n/en-US.json",
            "client/ui/qml/i18n/zh-CN.json",
            "client/ui/qml_main.cpp",
            "client/ui/common/ui_resources.qrc",
            "client/ui/qml/qmldir",
            "client/ui/qml/stores/qmldir",
            "client/ui/CMakeLists.txt",
            ".github/workflows/ci.yml",
            "tools/verify_ui_contract.py",
            "tools/windows_qml_accessibility_policy_check.py",
            "tools/windows_qml_text_policy_check.py",
            "tools/windows_qml_literal_copy_policy_check.py",
            "tools/windows_runtime_matrix_check.py",
            "tools/windows_runtime_golden_diff.py",
            "tools/agent_ownership_windows_ui.json",
        }
    )
    for repo_path in sorted(phase_critical_paths):
        expect(
            repo_path in covered,
            f"windows ownership coverage missing phase-critical path: {repo_path}",
            errors,
        )


def verify_platform_hooks(errors: list[str]) -> None:
    ios_app_shell = (ROOT / "ios_root_app/RootAuthApp/AppShell.swift").read_text(encoding="utf-8")
    ios_theme = (ROOT / "ios_root_app/RootAuthApp/SecureChatTheme.swift").read_text(encoding="utf-8")
    ios_security = (ROOT / "ios_root_app/RootAuthApp/SecurityCenterView.swift").read_text(encoding="utf-8")
    ios_workspace = (ROOT / "ios_root_app/RootAuthApp/ClientWorkspaceViews.swift").read_text(encoding="utf-8")
    android_main = (ROOT / "android/app/src/main/java/mi/e2ee/android/MainActivity.kt").read_text(encoding="utf-8")
    android_smoke_test = (ROOT / "android/app/src/androidTest/java/mi/e2ee/android/ui/UiScreensSmokeTest.kt").read_text(encoding="utf-8")
    android_chat = (ROOT / "android/ui/src/main/java/mi/e2ee/android/ui/ChatUi.kt").read_text(encoding="utf-8")
    android_calls = (ROOT / "android/ui/src/main/java/mi/e2ee/android/ui/CallsHomeUi.kt").read_text(encoding="utf-8")
    android_conversations = (ROOT / "android/ui/src/main/java/mi/e2ee/android/ui/ConversationListUi.kt").read_text(encoding="utf-8")
    android_host = (ROOT / "android/ui/src/main/java/mi/e2ee/android/ui/UiHost.kt").read_text(encoding="utf-8")
    android_login = (ROOT / "android/ui/src/main/java/mi/e2ee/android/ui/LoginUi.kt").read_text(encoding="utf-8")
    android_group = (ROOT / "android/ui/src/main/java/mi/e2ee/android/ui/GroupChatUi.kt").read_text(encoding="utf-8")
    android_security = (ROOT / "android/ui/src/main/java/mi/e2ee/android/ui/SecurityCenterUi.kt").read_text(encoding="utf-8")
    android_settings = (ROOT / "android/ui/src/main/java/mi/e2ee/android/ui/SettingsUi.kt").read_text(encoding="utf-8")
    android_components = (ROOT / "android/ui/src/main/java/mi/e2ee/android/ui/UiComponents.kt").read_text(encoding="utf-8")
    windows_app_store = (ROOT / "client/ui/qml/AppStore.qml").read_text(encoding="utf-8")
    windows_chat_store = (ROOT / "client/ui/qml/stores/ChatDisplayStore.qml").read_text(encoding="utf-8")
    windows_qml = (ROOT / "client/ui/qml_main.cpp").read_text(encoding="utf-8")
    windows_main = (ROOT / "client/ui/qml/Main.qml").read_text(encoding="utf-8")
    windows_shell = (ROOT / "client/ui/qml/shell/AppShell.qml").read_text(encoding="utf-8")
    windows_left = (ROOT / "client/ui/qml/shell/LeftPane.qml").read_text(encoding="utf-8")
    windows_center = (
        (ROOT / "client/ui/qml/shell/CenterPane.qml").read_text(encoding="utf-8")
        + "\n"
        + (ROOT / "client/ui/qml/shell/UtilitySurface.qml").read_text(encoding="utf-8")
    )
    windows_right = (ROOT / "client/ui/qml/shell/RightPane.qml").read_text(encoding="utf-8")
    windows_auth = (ROOT / "client/ui/qml/auth/AuthFlow.qml").read_text(encoding="utf-8")
    windows_style = (ROOT / "client/ui/qml/Style.qml").read_text(encoding="utf-8")
    expect("MI_E2EE_IOS_SCREENSHOT_MODE" in ios_app_shell, "iOS screenshot hook missing", errors)
    expect(".swipeActions(edge: .leading" in ios_workspace, "iOS leading swipe actions missing", errors)
    expect(".swipeActions(edge: .trailing" in ios_workspace, "iOS trailing swipe actions missing", errors)
    expect(".contextMenu {" in ios_workspace, "iOS context menu missing", errors)
    expect(".toolbar(.hidden, for: .tabBar)" in ios_workspace, "iOS detail tab-bar hiding hook missing", errors)
    expect("SCREENSHOT_MODE" in android_main, "Android screenshot hook missing", errors)
    expect("enum class ComposerSurfaceState" in android_chat, "Android composer surface state missing", errors)
    expect("BackHandler(enabled = actionTarget != null ||" in android_chat, "Android chat back-priority handler missing", errors)
    expect("ComposerSurfacePanel(" in android_chat, "Android composer surface panel missing", errors)
    expect("BackHandler(enabled = canNavigateBack)" in android_host, "Android host back handler missing", errors)
    expect("Shell.AppShell" in windows_main, "Windows Main.qml must mount the real shell directly", errors)
    expect("Ui.SecurityDialogCoordinator" in windows_main, "Windows Main.qml security coordinator missing", errors)
    expect("Probe" + "Adapter" not in windows_main + windows_qml, "Windows production UI must not mount capture adapters", errors)
    expect("ui" + "Probe" not in windows_main + windows_qml, "Windows production UI must not expose capture globals", errors)
    for shortcut in ('sequence: "Ctrl+K"', 'sequence: "Ctrl+F"', 'sequence: "Ctrl+N"', 'sequence: "Esc"'):
        expect(shortcut in windows_main, f"Windows shortcut missing from Main.qml: {shortcut}", errors)
    expect("readonly property var shellLayoutContract" in windows_style, "Windows shellLayoutContract missing from Style.qml", errors)
    expect("compactTwoColumnMinWidth: shellLayoutContract.compactTwoColumnMinWidth" in windows_style, "Windows compactTwoColumnMinWidth must read from shellLayoutContract", errors)
    expect("twoColumnDrawerMinWidth: shellLayoutContract.twoColumnDrawerMinWidth" in windows_style, "Windows twoColumnDrawerMinWidth must read from shellLayoutContract", errors)
    expect("threeColumnMinWidth: shellLayoutContract.threeColumnMinWidth" in windows_style, "Windows threeColumnMinWidth must read from shellLayoutContract", errors)
    expect("? Number(uiCompactTwoColumnMinWidth)" in windows_style, "Windows compactTwoColumnMinWidth default injection missing", errors)
    expect("? Number(uiTwoColumnDrawerMinWidth)" in windows_style, "Windows twoColumnDrawerMinWidth default injection missing", errors)
    expect("? Number(uiThreeColumnMinWidth)" in windows_style, "Windows threeColumnMinWidth default injection missing", errors)
    expect("canUseCompactTwoColumn" in windows_shell, "Windows compact breakpoint hook missing in AppShell.qml", errors)
    expect("canUseDrawerTwoColumn" in windows_shell, "Windows drawer breakpoint hook missing in AppShell.qml", errors)
    expect("id: shellIdentityCard" in windows_left, "Windows shell identity card missing in LeftPane.qml", errors)
    expect("visibleUtilityCount" in windows_left, "Windows utility visible-count guard missing in LeftPane.qml", errors)
    expect("showContactsList" in windows_left, "Windows contacts primary domain hook missing in LeftPane.qml", errors)
    expect('surface: "contacts"' in windows_left, "Windows contacts primary-tab surface missing in LeftPane.qml", errors)
    expect("id: contactsList" in windows_left, "Windows contacts list missing in LeftPane.qml", errors)
    expect("Ui.ChatDisplayStore.filteredContactsModel" in windows_left, "Windows contacts list must read filteredContactsModel", errors)
    expect("function conversationSectionLabel" in windows_left, "Windows conversation section labeling missing in LeftPane.qml", errors)
    expect("id: sectionHeader" in windows_left, "Windows conversation section header missing in LeftPane.qml", errors)
    expect("typingActive" in windows_left, "Windows conversation typing-state hook missing in LeftPane.qml", errors)
    expect("previewDisplayText" in windows_left, "Windows conversation preview synthesis missing in LeftPane.qml", errors)
    expect("id: postLoginEmptyStateCard" in windows_center, "Windows post-login empty-state card missing in CenterPane.qml", errors)
    expect("id: postLoginEmptyStateActions" in windows_center, "Windows post-login empty-state action row missing in CenterPane.qml", errors)
    expect("id: postLoginEmptyStateInsights" in windows_center, "Windows post-login empty-state insights missing in CenterPane.qml", errors)
    expect('placeholderText: Ui.I18n.t("chat.find")' in windows_center, "Windows in-chat search should keep a compact placeholder", errors)
    expect('placeholderText: Ui.I18n.t("chat.writeMessage")' in windows_center, "Windows composer should keep a compact message placeholder", errors)
    expect("id: chatMetaSummary" not in windows_center, "Windows chat header should not keep the extra meta summary line", errors)
    expect("Text.WrapAtWordBoundaryOrAnywhere" in windows_center, "Windows chat bubbles must wrap long links and tokens safely", errors)
    expect("property int drawerReserveWidth:" in windows_center, "Windows center pane must expose a drawer reserve width for narrow detail mode", errors)
    expect("readonly property real contentCenterOffset: drawerReserveWidth > 0" in windows_center, "Windows top bar must reserve drawer width when the detail drawer is open", errors)
    expect("parent.width - root.drawerReserveWidth - width" in windows_center, "Windows message list must reserve drawer width when the detail drawer is open", errors)
    expect("Layout.rightMargin: root.drawerReserveWidth" in windows_center, "Windows composer row should not collapse when the detail drawer is open", errors)
    expect("id: chatEmptyPrimaryAction" in windows_center, "Windows empty-state primary chat action missing in CenterPane.qml", errors)
    expect("id: contactsHub" in windows_center, "Windows contacts hub surface missing in CenterPane.qml", errors)
    expect("id: chatHeaderStateChips" in windows_center, "Windows chat header state chips missing in CenterPane.qml", errors)
    expect("id: utilityPageTitle" in windows_center, "Windows utility surfaces should keep a centered page title anchor", errors)
    expect("id: utilityPageHeader" in windows_center, "Windows utility surfaces should render a dedicated centered header with back navigation", errors)
    expect("id: settingsIdentityCard" not in windows_center, "Windows settings first screen should avoid a duplicate identity anchor card", errors)
    expect("id: settingsPrimaryList" in windows_center, "Windows settings should render a primary list instead of dashboard cards", errors)
    expect("id: settingsPreferencesList" not in windows_center, "Windows settings should merge the old second utility card into one lighter list", errors)
    expect("id: securitySummaryList" in windows_center, "Windows security should render a compact summary list", errors)
    expect("id: securityDevicesCard" in windows_center, "Windows security devices card missing in CenterPane.qml", errors)
    expect("id: callsCurrentCard" in windows_center, "Windows calls should render a compact current-call card", errors)
    expect("id: callsRecentCard" in windows_center, "Windows calls should render a recent-call list card", errors)
    expect("id: securityHeroBanner" not in windows_center, "Windows security should not keep the older hero banner", errors)
    expect("id: settingsSecurityShortcutCard" not in windows_center, "Windows settings should not keep the shortcut dashboard card", errors)
    expect("id: utilityOverviewGrid" not in windows_center, "Windows utility surfaces should not keep the older overview grid", errors)
    expect("id: utilityIntroColumn" not in windows_center, "Windows calls should not keep the older intro hero column", errors)
    expect("id: sharedMediaCard" in windows_right, "Windows shared-media card missing in RightPane.qml", errors)
    expect("id: sharedFilesCard" in windows_right, "Windows shared-files card missing in RightPane.qml", errors)
    expect("id: sharedLinksCard" in windows_right, "Windows shared-links card missing in RightPane.qml", errors)
    expect("id: detailTabsBar" in windows_right, "Windows right-pane detail tabs missing in RightPane.qml", errors)
    expect("id: detailPanels" in windows_right, "Windows right-pane stacked detail panels missing in RightPane.qml", errors)
    expect("Ui.ChatDisplayStore.sharedMediaModel" in windows_right, "Windows right-pane shared media must read runtime sharedMediaModel", errors)
    expect("Ui.ChatDisplayStore.sharedFilesModel" in windows_right, "Windows right-pane shared files must read runtime sharedFilesModel", errors)
    expect("Ui.ChatDisplayStore.sharedLinksModel" in windows_right, "Windows right-pane shared links must read runtime sharedLinksModel", errors)
    expect("id: authModeTabs" in windows_auth, "Windows auth mode tabs missing in AuthFlow.qml", errors)
    expect("id: advancedAccessButton" in windows_auth, "Windows auth advanced toggle missing in AuthFlow.qml", errors)
    expect("property int settingsRowMinHeight" in windows_style, "Windows settings row min-height token missing in Style.qml", errors)
    expect("property real badgeMaxWidthRatio" in windows_style, "Windows badge max-width token missing in Style.qml", errors)
    expect('surface === "contacts"' in windows_app_store, "Windows AppStore contacts surface normalization missing", errors)
    expect("property ListModel sharedMediaModel" in windows_app_store, "Windows AppStore sharedMediaModel missing", errors)
    expect("property ListModel sharedFilesModel" in windows_app_store, "Windows AppStore sharedFilesModel missing", errors)
    expect("property ListModel sharedLinksModel" in windows_app_store, "Windows AppStore sharedLinksModel missing", errors)
    expect("function rebuildSharedDetailModels" in windows_app_store, "Windows AppStore shared detail rebuild hook missing", errors)
    expect("readonly property var sharedMediaModel" in windows_chat_store, "Windows ChatDisplayStore sharedMediaModel exposure missing", errors)
    expect("readonly property var sharedFilesModel" in windows_chat_store, "Windows ChatDisplayStore sharedFilesModel exposure missing", errors)
    expect("readonly property var sharedLinksModel" in windows_chat_store, "Windows ChatDisplayStore sharedLinksModel exposure missing", errors)
    for forbidden in (
        "固定网关审查、手动审批码与 QR 设备登录共用入口，但仍保持分开的安全步骤。",
        "Pinned gateway review, manual approval codes, and QR device sign-in share one entry point while keeping separate security steps.",
        "已加密私聊",
        "共享文件与链接",
        "共享媒体与成员",
    ):
        expect(forbidden not in windows_auth + windows_right, f"Windows shell still carries verbose desktop copy: {forbidden}", errors)
    detail_copy_index = windows_center.rindex("text: modelData.detail")
    detail_copy_section = windows_center[max(0, detail_copy_index - 220):detail_copy_index + 40]
    detail_copy_match = "root.showingCallsSurface" in detail_copy_section
    expect(
        "root.showingCallsSurface || root.showingSecuritySurface" not in detail_copy_section,
        "Windows security cards should not keep the older secondary detail visibility rule",
        errors,
    )
    expect(detail_copy_match is not None, "Windows utility detail copy should only remain on calls surface", errors)
    expect("|| settingsCard" not in windows_center, "Windows settings rows should not keep a second block of explanatory detail copy", errors)
    expect('text: Ui.I18n.usesCjkLocale\n                                          ? "Transport"' not in windows_center, "Windows security hero should prefer compact badges over extra text labels", errors)
    expect("Switch {" not in windows_center.replace("InlineSwitch {", ""), "Windows settings surface should not use the legacy default Switch", errors)
    expect("Components.InlineSwitch" in windows_center, "Windows settings surface should use the modern InlineSwitch", errors)
    expect("headerSubtitle()" not in windows_left, "Windows left-rail header should not keep the extra subtitle line", errors)
    expect("shellIdentitySubtitle" not in windows_left, "Windows left-rail should not keep the verbose identity subtitle", errors)
    expect("function surfaceIconSource(surface)" in windows_left, "Windows left rail should use icon-driven primary navigation", errors)
    expect("source: root.surfaceIconSource(modelData.surface)" in windows_left, "Windows primary navigation should render icons instead of text-only pills", errors)
    utility_badge_section = windows_left.split("function utilityBadgeText(entry) {", 1)[1].split("function utilityItemActive(entry) {", 1)[0]
    expect("noLinkedDevices" not in utility_badge_section, "Windows left-rail utility badges should stay compact", errors)
    dialog_row_match = re.search(r"property int dialogRowHeight: (\d+)", windows_style)
    expect(dialog_row_match is not None and int(dialog_row_match.group(1)) >= 68, "Windows dialog rows should keep a taller list rhythm", errors)
    expect('Ui.I18n.usesCjkLocale ? "准备发起通话"' not in windows_center, "Windows calls should not rely on inline hero copy", errors)
    expect('Ui.I18n.usesCjkLocale ? "选择联系人发起通话"' not in windows_center, "Windows calls should not rely on inline intro copy", errors)
    expect('Ui.I18n.usesCjkLocale ? "当前设备"' not in windows_center, "Windows utility surfaces should not hardcode inline section labels", errors)
    expect('Ui.I18n.usesCjkLocale ? "呼叫状态"' not in windows_center, "Windows calls should not hardcode dashboard metric labels", errors)
    expect("function compactTrustSnapshotLabel()" in windows_right, "Windows right pane should compact trust snapshot labels", errors)
    expect("function compactDeviceSnapshotLabel()" in windows_right, "Windows right pane should compact device snapshot labels", errors)
    right_snapshot_section = windows_right.split("id: trustSnapshotRow", 1)[1].split("TabBar {", 1)[0]
    expect("devicesValue" not in right_snapshot_section, "Windows right-pane trust snapshot should not use verbose device labels", errors)
    security_summary_section = windows_center.split("id: securitySummaryList", 1)[1].split("id: securityDevicesCard", 1)[0]
    expect("gatewayDisplayDetail.length > 0" not in security_summary_section, "Windows security summary should not surface raw gateway endpoints", errors)
    expect('titleText: Ui.I18n.t("dialog.securityCenter.devicesTitle")' in security_summary_section, "Windows security summary should surface linked-device status inline on the first card", errors)
    expect("color: Ui.Style.panelBg" in security_summary_section, "Windows security summary should render as a grounded panel list", errors)
    expect("border.width: 1" in security_summary_section, "Windows security summary should keep a light outline for structure", errors)
    expect(security_summary_section.count("UtilityNavRow {") >= 7, "Windows security first screen should merge summary and device details into one denser list", errors)
    calls_current_section = windows_center.split("id: callsCurrentCard", 1)[1].split("id: callsRecentCard", 1)[0]
    expect("visible: root.showingCallsSurface &&" in calls_current_section, "Windows current-call card should hide when there is no ongoing call", errors)
    calls_recent_section = windows_center.split("id: callsRecentCard", 1)[1].split("Item {\n                            Layout.fillWidth: true", 1)[0]
    expect("color: Ui.Style.panelBg" in calls_recent_section, "Windows calls history should render as a grounded panel to avoid an airy layout", errors)
    expect("border.width: 1" in calls_recent_section, "Windows calls history should keep a light outline to anchor the list", errors)
    expect('text: Ui.I18n.usesCjkLocale ? "快速发起通话" : "Quick call"' in calls_recent_section, "Windows calls should fold quick actions into the main history surface", errors)
    expect("seedProbe" not in windows_app_store, "Windows AppStore must not keep seeded probe preview state", errors)
    expect("enterProbeShellPreview" not in windows_app_store, "Windows AppStore must not expose probe shell preview entrypoints", errors)
    expect("drawerReserveWidth: root.drawerTightChatColumns" in windows_shell, "Windows drawer reserve binding missing from AppShell.qml", errors)
    expect("property int drawerReserveWidth:" in windows_center, "Windows center pane drawer reserve property missing", errors)
    expect("readonly property real contentCenterOffset: drawerReserveWidth > 0" in windows_center, "Windows center pane must offset content away from the drawer", errors)
    expect("parent.width - root.drawerReserveWidth - width" in windows_center, "Windows message list must lay out inside the drawer-safe width", errors)
    expect("Layout.rightMargin: root.drawerReserveWidth" in windows_center, "Windows composer must reserve drawer width", errors)
    primary_nav_section = windows_left.split("model: [", 1)[1].split("ToolTip.visible: navMouse.containsMouse", 1)[0]
    expect("activeFocusOnTab: true" in primary_nav_section, "Windows primary nav must be keyboard focusable", errors)
    expect("Accessible.role: Accessible.Button" in primary_nav_section, "Windows primary nav must expose button accessibility role", errors)
    expect("Keys.onReturnPressed: activate()" in primary_nav_section, "Windows primary nav must support Return activation", errors)
    expect("Keys.onSpacePressed: activate()" in primary_nav_section, "Windows primary nav must support Space activation", errors)
    expect("Math.max(320, contactsHub.width - Ui.Style.paddingL * 2)" not in windows_center, "Windows contacts hub should not force a wider-than-viewport content width", errors)
    expect("Math.max(320, utilitySurface.width - Ui.Style.paddingL * 2)" not in windows_center, "Windows utility surface should not force a wider-than-viewport content width", errors)
    expect("id: utilityScroll" in windows_center, "Windows utility surface should name its scroll viewport", errors)
    expect("width: Math.max(0, utilityScroll.availableWidth)" in windows_center, "Windows utility surface should size to the scroll viewport width", errors)
    expect("readonly property int utilityCardWidth: Math.min(width, Ui.Style.utilitySurfaceMaxWidth)" in windows_center, "Windows utility pages should clamp list width to a centered max width", errors)
    expect("property int utilitySurfaceMaxWidth: 640" in windows_style, "Windows utility max-width token missing", errors)
    expect("property int rightPaneDrawerCompactWidth: 340" in windows_style, "Windows compact drawer width token missing", errors)
    expect("root.windowWidth < 1400" in windows_shell, "Windows chat-detail tightening threshold should expand to medium desktop widths", errors)
    expect("Math.min(10, Ui.ChatDisplayStore.filteredDialogsModel.count)" in windows_center, "Windows calls should fill the recent list with more history before leaving empty space", errors)
    expect("detailText: Ui.I18n.t(\"settings.privacy.clipboardIsolationHint\")" not in windows_center, "Windows settings rows should drop long clipboard helper copy on the first screen", errors)
    expect("detailText: Ui.I18n.t(\"settings.privacy.saveHistoryHint\")" not in windows_center, "Windows settings rows should drop long history helper copy on the first screen", errors)
    expect("detailText: Ui.I18n.t(\"settings.privacy.aiEnhanceHint\")" not in windows_center, "Windows settings rows should drop long AI helper copy on the first screen", errors)
    expect('trailingText: Ui.I18n.usesCjkLocale ? "账号" : "Account"' in windows_center, "Windows settings first screen should restore a compact account anchor", errors)
    settings_primary_section = windows_center.split("id: settingsPrimaryList", 1)[1].split("id: securitySummaryList", 1)[0]
    expect("UtilityToggleRow {" not in settings_primary_section, "Windows settings first screen should drop inline toggles and read like navigation", errors)
    expect(settings_primary_section.count("UtilityNavRow {") >= 7, "Windows settings first screen should render as a denser navigation list", errors)
    expect("color: Ui.Style.panelBg" in settings_primary_section, "Windows settings first screen should render as a grounded panel list", errors)
    expect("border.width: 1" in settings_primary_section, "Windows settings first screen should keep a light outline for structure", errors)
    expect("id: settingsSupportList" not in windows_center, "Windows settings should read as one primary navigation list instead of stacked shell-like cards", errors)
    expect('titleText: Ui.I18n.t("dialog.deviceManager.linkedDevices")' in settings_primary_section, "Windows settings first screen should surface linked devices as a compact navigation row", errors)
    expect('Label("Chats", systemImage: "message.fill")' not in ios_app_shell, "iOS tab bar should be icon-only for Chats", errors)
    expect('Label("Contacts", systemImage: "person.2.fill")' not in ios_app_shell, "iOS tab bar should be icon-only for Contacts", errors)
    expect('Label("Calls", systemImage: "phone.fill")' not in ios_app_shell, "iOS tab bar should be icon-only for Calls", errors)
    expect('Label("Settings", systemImage: "gearshape.fill")' not in ios_app_shell, "iOS tab bar should be icon-only for Settings", errors)
    expect('Image(systemName: "message.fill")' in ios_app_shell, "iOS Chats tab icon missing", errors)
    expect('Image(systemName: "person.2.fill")' in ios_app_shell, "iOS Contacts tab icon missing", errors)
    expect('Image(systemName: "phone.fill")' in ios_app_shell, "iOS Calls tab icon missing", errors)
    expect('Image(systemName: "gearshape.fill")' in ios_app_shell, "iOS Settings tab icon missing", errors)
    android_bottom_nav_section = android_conversations.split("private fun RowScope.ConversationBottomNavItem(", 1)[1].split("private fun CompactSearchField(", 1)[0]
    expect("Text(\n            text = label" in android_bottom_nav_section, "Android bottom navigation should restore short labels for clarity", errors)
    expect("text: root.headerTitle()" not in windows_left, "Windows left rail should not duplicate page titles", errors)
    expect("property int compactRailHeaderHeight: 112" in windows_style, "Windows left rail header height must tighten to 112", errors)
    expect("property int leftPaneWidthUtilityRail: 60" in windows_style, "Windows utility compact rail width token missing", errors)
    expect("property int leftPaneWidthDetailTight: 300" in windows_style, "Windows detail-tight left rail width token missing", errors)
    expect("property int rightPaneWidthTight: 280" in windows_style, "Windows detail-tight right pane width token missing", errors)
    expect("readonly property bool tightChatColumns:" in windows_shell, "Windows shell should tighten chat columns near the three-pane breakpoint", errors)
    expect("readonly property bool immersiveUtilitySurface:" in windows_shell, "Windows shell should expose immersive utility pages for settings/security/calls", errors)
    expect("readonly property bool canUseUtilityRail: root.windowWidth >= 820" in windows_shell, "Windows shell should expose a dedicated utility-rail threshold", errors)
    expect("!root.drawerAsSurface" in windows_shell, "Windows shell should hide the rail when narrow detail uses a focused surface", errors)
    expect("SplitView.preferredWidth: root.immersiveUtilitySurface" in windows_shell, "Windows utility pages should reserve a compact rail width in the split view", errors)
    expect("readonly property bool showUtilityCompactRail: showUtilityList" in windows_left, "Windows left rail should expose a compact utility rail mode", errors)
    expect('readonly property bool showUtilityList: shellSurface === "settings" || shellSurface === "security" || shellSurface === "calls"' in windows_left, "Windows left rail utility mode should cover calls as well as settings/security", errors)
    expect("visible: root.showUtilityCompactRail" in windows_left, "Windows left rail compact utility rail missing", errors)
    expect("visible: false" in windows_left.split("id: utilityList", 1)[1].split("model: root.utilitySectionsModel", 1)[0], "Windows legacy utility list should stay hidden in compact utility mode", errors)
    expect("anchors.topMargin: 18" in windows_left, "Windows utility compact rail should align near the top instead of floating mid-column", errors)
    expect('text: Ui.I18n.t("auth.login")' in windows_auth, "Windows auth shell should keep a centered login title", errors)
    expect('text: Ui.I18n.t("auth.login")' in windows_auth.split("id: loginButton", 1)[1], "Windows login CTA must render a text label", errors)
    expect('text: Ui.I18n.t("auth.qrLogin")' in windows_auth, "Windows login surface must render a labeled QR entry", errors)
    expect('text: Ui.I18n.t("auth.register")' in windows_auth, "Windows login surface must render a labeled register entry", errors)
    expect('text: advancedExpanded' in windows_auth or 'text: Ui.I18n.t("auth.advanced")' in windows_auth, "Windows login surface must render a labeled advanced entry", errors)
    expect('text: Ui.I18n.t("auth.mode.account")' not in windows_auth, "Windows auth mode switch should be icon-only", errors)
    expect('text: Ui.I18n.t("auth.mode.qr")' not in windows_auth, "Windows QR auth mode switch should be icon-only", errors)
    expect("text: modelData" not in windows_right.split("TabBar {", 1)[1].split("StackLayout {", 1)[0], "Windows right-pane detail tabs should be icon-only", errors)
    expect("Text(title.uppercased())" not in ios_theme, "iOS metric badges should not render uppercase title text", errors)
    expect("Text(label.uppercased())" not in ios_theme, "iOS metric tiles should not render uppercase label text", errors)
    expect("Text(title.uppercased())" not in ios_workspace, "iOS settings headers should not render uppercase group labels", errors)
    expect('TextField("Phone or email", text: $store.username)' in ios_workspace, "iOS login should keep a clear account placeholder", errors)
    expect('SecureField("Password", text: $store.password)' in ios_workspace, "iOS login should keep a clear password placeholder", errors)
    expect(
        'TextField("Search chats", text: $query)' in ios_workspace or
        '.searchable(text: $query, placement: .navigationBarDrawer(displayMode: .always), prompt: "Search chats")' in ios_workspace,
        "iOS conversations should keep a search placeholder",
        errors,
    )
    expect("Transport, device identity, and approval stay visible before you open any deeper tools." not in ios_security, "iOS security overview still uses verbose explanatory copy", errors)
    expect("Trust stays visible while chats stay primary." not in ios_workspace, "iOS workspace card still uses verbose subtitle copy", errors)
    expect("ClientOverviewHero(store: clientStore)" not in ios_workspace, "iOS settings should not reuse the large overview hero", errors)
    expect("SecureMetricTile(" not in ios_workspace, "iOS settings should not use metric tiles on the main settings screen", errors)
    expect("SecurityStateStrip(" not in android_settings, "Android settings should not show the large security state strip", errors)
    expect("SettingsHeader(" not in android_settings, "Android settings should not keep the large profile header", errors)
    expect('text = tr("conversations_title", "Chats")' in android_conversations and ".height(80.dp)" in android_conversations, "Android conversations should center the title inside a taller whitespace header band", errors)
    expect("Spacer(modifier = Modifier.height(10.dp))" in android_conversations, "Android conversations should leave a soft gap beneath the title band", errors)
    expect(".height(80.dp)" in android_chat and "Modifier.align(Alignment.Center)" in android_chat and "padding(horizontal = 76.dp)" in android_chat, "Android chat detail should use a centered title band instead of a cramped toolbar title", errors)
    expect("CenterAlignedTopAppBar(" in android_calls, "Android calls should use a centered title bar", errors)
    expect("CenterAlignedTopAppBar(" in android_settings, "Android settings should use a centered title bar", errors)
    expect("CenterAlignedTopAppBar(" in android_security, "Android security should use a centered title bar", errors)
    expect('text = tr("login_sign_in", "Sign in")' in android_login, "Android login should keep a centered sign-in title", errors)
    expect('tr("login_account_hint", "Use your account to continue")' in android_login, "Android login should keep a short structural helper line", errors)
    expect("Spacer(modifier = Modifier.weight(0.60f))" not in android_login, "Android login should no longer keep the oversized top spacer", errors)
    expect("Spacer(modifier = Modifier.weight(0.42f))" not in android_login, "Android login should move the form higher than the intermediate anchor", errors)
    expect("Spacer(modifier = Modifier.weight(0.38f))" in android_login, "Android login should sit closer to the visual center instead of floating too high", errors)
    expect('PrimaryButton(' in android_login and 'label = tr("login_sign_in", "Sign in")' in android_login, "Android login CTA must render a text label", errors)
    expect('tr("login_qr_show", "QR sign in")' in android_login, "Android login surface must render a labeled QR entry", errors)
    expect('tr("login_new_here", "Register")' in android_login, "Android login surface must render a labeled register entry", errors)
    expect('tr("login_show_advanced", "Advanced")' in android_login or 'tr("login_hide_advanced", "Hide advanced")' in android_login, "Android login surface must render a labeled advanced entry", errors)
    expect("text.uppercase()" not in android_components, "Android section headers should not force uppercase labels", errors)
    expect(".defaultMinSize(minHeight = 88.dp)" in android_conversations, "Android conversation rows should use a taller minimum height", errors)
    expect(
        """Column(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxHeight(),
                    verticalArrangement = Arrangement.Center""" in android_conversations,
        "Android conversation text column should be vertically centered",
        errors,
    )
    expect("calls_recent_room" not in android_calls, "Android calls should not use decorative Recent labels", errors)
    expect('tr("calls_open", "Open")' not in android_calls, "Android calls should not use Open as the primary CTA", errors)
    expect('MediaHintChip(' not in android_calls.split("private fun RecentCallRow", 1)[1].split("private fun CallEmptyState", 1)[0], "Android recent-call rows should avoid dashboard-style action chips", errors)
    expect('val activePeer = if (sceneId == "calls_home")' in android_main, "Android calls smoke scene should not keep an active call strip by default", errors)
    expect('waitForTag("calls-ongoing-entry")' not in android_smoke_test, "Android smoke test should not expect the removed calls ongoing strip", errors)
    expect('title = sdk.deviceDisplayId.ifBlank { tr("app_name", "MI E2EE") }' in android_settings, "Android settings must keep an identity entry on the first screen", errors)
    expect('subtitle = tr("settings_account_section", "Account")' in android_settings, "Android settings identity entry must keep the account subtitle", errors)
    expect("IdentityAvatar(" in android_settings, "Android settings should anchor the first screen with an account avatar instead of a generic settings icon", errors)
    expect("val accountEntry = SettingEntry(" in android_settings, "Android settings should isolate the account anchor from the rest of the list", errors)
    expect("val coreSettings = listOf(" in android_settings, "Android settings should keep a compact core settings block", errors)
    expect("val preferenceSettings = buildList {" in android_settings, "Android settings should keep a separate lightweight preferences block", errors)
    expect("items(coreSettings)" in android_settings, "Android settings should render the core settings block as lightweight rows", errors)
    expect("items(preferenceSettings)" in android_settings, "Android settings should render the preferences block as lightweight rows", errors)
    expect("Switch(" not in android_settings, "Android settings first screen should avoid inline switches", errors)
    expect("SettingsSection(entries = accountSettings)" not in android_settings, "Android settings should not keep two separate oversized groups", errors)
    expect("SettingsSection(entries = appSettings)" not in android_settings, "Android settings should not keep two separate oversized groups", errors)
    expect("SettingsSectionLabel(title = tr(\"settings_account_section\", \"Account\"))" not in android_settings, "Android settings should drop system-style grouped section labels on the first screen", errors)
    expect("SettingsSectionLabel(title = tr(\"settings_preferences_section\", \"Preferences\"))" not in android_settings, "Android settings should drop system-style grouped section labels on the first screen", errors)
    expect("private fun SettingsSection(" not in android_settings, "Android settings should no longer wrap the first screen in a grouped card helper", errors)
    expect("private fun GroupedSettingsContainer(" not in android_settings, "Android settings should no longer keep the grouped-card container helper", errors)
    expect("SecurityCompactActionsRow(" not in android_security, "Android security first screen should avoid action-cluster rows", errors)
    expect("SecurityOverviewSection(" in android_security, "Android security first screen should merge devices and security into one overview group", errors)
    expect('label = tr("settings_devices", "Devices")' in android_security, "Android security first screen should reduce linked devices to a simple Devices row", errors)
    security_overview_section = android_security.split("private fun SecurityOverviewSection(", 1)[1].split("private fun SecurityNavRow(", 1)[0]
    expect("SecuritySectionHeaderRow(" not in android_security, "Android security first screen should avoid dashboard-style section headers", errors)
    expect("SecurityDeviceRow(" not in security_overview_section, "Android security first screen should avoid a dedicated device hero row", errors)
    expect('detail = ""' in security_overview_section, "Android security first screen should keep overview rows lightweight without repeated summary text", errors)
    expect(security_overview_section.count("SecurityNavRow(") >= 3, "Android security first screen should reduce to simple navigation rows", errors)
    expect("item(key = \"transport-trust-card\")" not in android_security, "Android security should not keep a second dashboard card on the first screen", errors)
    expect("UiToolbarIconButton(" not in android_security.split("private fun SecurityDeviceRow(", 1)[1].split("private fun SecurityFactCard(", 1)[0], "Android security first screen should not expose inline device removal buttons", errors)
    expect("linkedDevices.forEachIndexed" not in android_security, "Android security first screen should avoid listing every linked device in the opening viewport", errors)
    expect("MI-A13F-7C91" not in android_security and "Pad-F2D0-11AA" not in android_security and "Desk-9CC0-219D" not in android_security, "Android security preview should not expose raw device IDs", errors)
    expect("284391" not in android_security and "8fdca349d2aa1bc5a1e84c6b8023d4d4afba1a6f0d81291c4b27fae6837ef2a0" not in android_security, "Android security preview should not expose approval codes or raw root keys", errors)
    expect("private fun SecurityPrimaryGroup(" not in android_security, "Android security overview should no longer keep the grouped panel helper", errors)
    expect("private fun SecurityCompactInfoRow(" not in android_security, "Android security overview should no longer keep the old fact-row helper", errors)
    expect("ComboBox {" not in windows_center, "Windows settings should avoid legacy ComboBox controls", errors)
    for forbidden in (
        "登录后继续",
        "安全桌面",
        "Secure desktop",
        "入口合并",
        "QR, approval, and device entry.",
        "Back to secure chats",
        "Trust stays visible.",
        "Private messaging, quieter UI.",
        "Secure Chat",
        "MI Secure",
        '"Welcome back"',
        '"Private access with pinned transport and approval fallback."',
        '"Sign in and get back to chats, calls, and linked devices."',
    ):
        expect(
            forbidden not in windows_auth + ios_workspace + android_login,
            f"Login surfaces still use non-IM explanatory copy: {forbidden}",
            errors,
        )
    for forbidden in (
        "传输与信任。",
        "安全入口集中在此。",
        "查看安全概览。",
        "传输、设备、审批。",
        "主题与语言",
        "Transport and trust.",
        "Security entry points, together.",
        "View the security overview.",
        "Transport, device, approval.",
        "Theme and language",
        "Linked device inventory",
        "Track current and linked identities with less copy and faster visual scanning.",
        'tr("settings_secure_title", "Secure session ready")',
        'tr("settings_secure_detail", "Pinned transport and device trust are active.")',
        'label = tr("conversations_group", "Group")',
        'label = tr("conversations_draft_badge", "Draft")',
        'label = tr("conversations_live_badge", "Live")',
        'label = status,',
        'Devices, sessions, and approval identity',
        'Password, linked devices, and recovery',
        'Read receipts, blocked users, and visibility',
        'Calls, mentions, and message alerts',
        'transportLabel = "Encrypted"',
        'transportDetail = "Pinned transport healthy. 4s ago."',
        'title = "Security Center"',
    ):
        expect(forbidden not in android_conversations + android_chat + android_settings + android_security, f"Android screenshot surfaces still rely on verbose text chips/copy: {forbidden}", errors)
    for forbidden in (
        "Design Ops",
        "Desktop-shell-v2.png",
        "Security center hierarchy is cleaner now.",
        "Rollout shell now uses a softer mint light palette.",
        "Voice review feels closer to a real IM now.",
        "Probe preview ready",
        "Conversation inbox ready",
        "Post-login shell ready",
        "Light post-login shell ready",
        "Calls surface ready",
        "Security Center ready",
        "Platform rollout",
        "Design review",
        "Queue cap",
        "release lane",
        "runtime-shell",
    ):
        expect(forbidden not in windows_app_store + windows_left, f"Windows UI still uses review/demo copy: {forbidden}", errors)
    for forbidden in (
        "Smoke gate passed on API33",
        "Daily sync in 10 minutes. Please post blockers.",
        "Queue cap increased to 512.",
        "Design Ops",
        "Project Aurora",
        "QA",
        "Ops Sync",
        "Threat Guild",
        "Launch Crew",
        "verify API33 smoke gate",
        "Risk board link updated",
        "Transport settled after the reconnect.",
        "Pinned gateway screenshot shared",
        "Need final release note approval.",
        "Drafted the API33 smoke gate. Need one more pass on the reconnect path.",
        "Ship the reconnect note with the retry cap and leave the rest for the next cut.",
        "Queued. I will attach the final checklist after QA signs off.",
        "Morning. I mapped the edge cases into a short checklist.",
        "Also flag the retry storms after reconnect so Ops can review it.",
    ):
        expect(
            forbidden not in android_conversations + android_group + android_main + android_smoke_test,
            f"Android sample content still uses review/demo copy: {forbidden}",
            errors,
        )
    settings_primary_index = windows_center.find("id: settingsPrimaryList")
    summary_index = windows_center.find("id: securitySummaryList")
    devices_index = windows_center.find("id: securityDevicesCard")
    calls_current_index = windows_center.find("id: callsCurrentCard")
    calls_recent_index = windows_center.find("id: callsRecentCard")
    expect(
        summary_index >= 0 and devices_index >= 0 and summary_index < devices_index,
        "Windows security summary list must precede linked devices",
        errors,
    )
    expect(
        settings_primary_index >= 0,
        "Windows settings first screen should keep a primary navigation list",
        errors,
    )
    expect(
        "id: securityDeviceInfoCard" not in windows_center,
        "Windows security first screen should not split device details into a second summary card",
        errors,
    )
    expect(
        calls_current_index >= 0 and calls_recent_index >= 0 and calls_current_index < calls_recent_index,
        "Windows calls current-call card must precede recent calls",
        errors,
    )
    expect(
        "id: callsQuickActionsCard" not in windows_center,
        "Windows calls should not keep a separate quick-actions card above recent history",
        errors,
    )
    expect(
        "Layout.preferredWidth: root.condensedConversationRows ? 20 : 30" in windows_left.split("id: metaColumn", 1)[1].split("Layout.fillHeight: true", 1)[0],
        "Windows detail list meta column should tighten to free more title width",
        errors,
    )
    expect("readonly property bool condensedConversationRows: width <= 300" in windows_left, "Windows left pane should condense low-priority row meta on narrow chat layouts", errors)
    expect("property int leftPaneWidthDrawerTight: 300" in windows_style, "Windows drawer-tight left rail width token missing", errors)


def verify_windows_manifest_and_ci(errors: list[str]) -> None:
    qrc = (ROOT / "client/ui/common/ui_resources.qrc").read_text(encoding="utf-8")
    qml_qmldir = (ROOT / "client/ui/qml/qmldir").read_text(encoding="utf-8")
    stores_qmldir = (ROOT / "client/ui/qml/stores/qmldir").read_text(encoding="utf-8")
    cmake = (ROOT / "client/ui/CMakeLists.txt").read_text(encoding="utf-8")
    workflow = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")

    for alias in (
        'alias="qml/TrustFlowCoordinator.qml"',
        'alias="qml/SecurityDialogCoordinator.qml"',
        'alias="qml/stores/ChatDisplayStore.qml"',
        'alias="qml/stores/CallDisplayStore.qml"',
    ):
        expect(alias in qrc, f"ui_resources.qrc missing preregistered alias: {alias}", errors)

    for entry in (
        "TrustFlowCoordinator 1.0 TrustFlowCoordinator.qml",
        "SecurityDialogCoordinator 1.0 SecurityDialogCoordinator.qml",
        "singleton ChatDisplayStore 1.0 stores/ChatDisplayStore.qml",
        "singleton CallDisplayStore 1.0 stores/CallDisplayStore.qml",
    ):
        expect(entry in qml_qmldir, f"qml/qmldir missing entry: {entry}", errors)

    for entry in (
        "singleton ChatDisplayStore 1.0 ChatDisplayStore.qml",
        "singleton CallDisplayStore 1.0 CallDisplayStore.qml",
    ):
        expect(entry in stores_qmldir, f"qml/stores/qmldir missing entry: {entry}", errors)

    for needle in ("display_contract.cpp", "qml_main.cpp", "quick_client.cpp"):
        expect(needle in cmake, f"client/ui/CMakeLists.txt missing source: {needle}", errors)

    expect("--allow-missing-golden" not in workflow, "ci workflow must not allow missing Windows golden baselines", errors)
    expect("skip screenshot" not in workflow, "ci workflow must not silently skip screenshots", errors)


def verify_runtime_gate_hooks(errors: list[str]) -> None:
    workflow = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
    for needle in (
        "mi_e2ee_ui_contract_evidence",
        "ui-contract-static-evidence.json",
        "windows-runtime-evidence.json",
        "ios-runtime-evidence.json",
        'android_cfg["manifest_file"]',
        "android-arm64-validation.json",
        "actions/download-artifact@v4",
        "Download acceptance artifacts",
        "Verify runtime evidence artifacts",
        'echo "validated=$validated" >> "$GITHUB_OUTPUT"',
        'echo "waived=$waived" >> "$GITHUB_OUTPUT"',
        'echo "waiver_reason=$waiver_reason" >> "$GITHUB_OUTPUT"',
        'echo "gate_passed=$gate_passed" >> "$GITHUB_OUTPUT"',
        "ANDROID_ARM64_WAIVED",
        "ANDROID_ARM64_WAIVER_REASON",
        "ANDROID_ARM64_GATE_PASSED",
        "state_matrix_validated",
        "tuple_matrix_validated",
        "tuple_evidence",
        "runtime manifest tuple keys mismatch",
        "windows-tuple-",
        "ios-tuple-",
        "android_cfg['tuple_marker_prefix']",
        "windows-state-",
        "ios-state-",
        "android_cfg['state_marker_prefix']",
        "tuple_markers_emitted_by_runtime",
        "runtime_marker_source",
        "required_manifest_fields",
        "required_privacy_fields",
        "validated_requires_privacy_true",
        "requested_device_matrix",
        "ftl_requested_device_matrix_non_empty",
        "ftl_device_matrix_arm64_approved",
        "Android runtime manifest missing from runtime producer",
        "Android runtime marker missing",
        "Android runtime tuple marker missing",
        "Android arm64 waiver missing_gcp_credentials must show missing GCP privacy evidence",
        "android_arm64_requirement",
        "required_or_waived",
        "evidence_source",
        '"privacy_evidence"',
        '"evidence_files"',
        "build/ui_contract_evidence/*",
        "build/android_ui_artifacts/*",
    ):
        expect(needle in workflow, f"ci workflow missing runtime gate hook: {needle}", errors)


def verify_android_smoke_runtime_script(errors: list[str]) -> None:
    script = (ROOT / "android/scripts/ci_ui_smoke.sh").read_text(encoding="utf-8")
    for needle in (
        "set_system_theme()",
        "capture_scene()",
        'mi.e2ee.android.extra.SCREENSHOT_MODE',
        'set_system_theme light',
        'set_system_theme dark',
        'capture_scene login',
        'capture_scene chats',
        'capture_scene detail',
        'capture_scene calls',
        'capture_scene settings',
        'capture_scene security_center',
        'android-chats-dark.png',
        'android-security-dark.png',
        'workspace / "ui_contract" / "acceptance.json"',
        '"tuple_inventory"]["android"]',
        "tuple_markers_emitted_by_runtime",
        "runtime_marker_source",
        "required_evidence_satisfied",
        'android_manifest_contract["manifest_file"]',
        "android_manifest_contract['theme_marker_prefix']",
        "android_manifest_contract['state_marker_prefix']",
        "android_manifest_contract['scene_marker_prefix']",
        "android_manifest_contract['tuple_marker_prefix']",
    ):
        expect(needle in script, f"android ci smoke script missing runtime evidence hook: {needle}", errors)


def verify_windows_shared_files(errors: list[str]) -> None:
    for path in (
        ROOT / "client/ui/display_contract.h",
        ROOT / "client/ui/display_contract.cpp",
        ROOT / "client/ui/qml/components/UiText.qml",
        ROOT / "client/ui/qml/components/AppBarPrimary.qml",
        ROOT / "client/ui/qml/components/StatusBanner.qml",
        ROOT / "client/ui/qml/components/SecurityBadge.qml",
        ROOT / "tools/windows_qml_accessibility_policy_check.py",
        ROOT / "tools/windows_qml_text_policy_check.py",
        ROOT / "tools/windows_qml_literal_copy_policy_check.py",
        ROOT / "tools/windows_runtime_matrix_check.py",
        ROOT / "tools/windows_runtime_golden_diff.py",
    ):
        expect(path.exists(), f"missing Windows shared file: {path}", errors)


def main() -> int:
    errors: list[str] = []
    loaded = verify_contract_files(errors)
    if not errors:
        verify_tokens(loaded["tokens.json"], errors)
        verify_routes(loaded["routes.json"], errors)
        verify_components(loaded["components.json"], errors)
        verify_acceptance(loaded["acceptance.json"], errors)
        verify_runtime_artifact_contract(loaded["acceptance.json"], errors)
        verify_copy_keys(loaded["copy_keys.json"], errors)
        verify_ownership(errors)
        verify_windows_ownership_coverage(loaded["acceptance.json"], errors)
        verify_platform_hooks(errors)
        verify_windows_manifest_and_ci(errors)
        verify_runtime_gate_hooks(errors)
        verify_android_smoke_runtime_script(errors)
        verify_windows_shared_files(errors)
        verify_ui_text_whitelist(errors)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print("UI contract verification passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
