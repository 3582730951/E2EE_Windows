#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
CONTRACT_DIR = ROOT / "ui_contract"
OWNERSHIP_PATH = ROOT / "tools" / "agent_ownership_windows_ui.json"


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def expect(condition: bool, message: str, errors: list[str]) -> None:
    if not condition:
        errors.append(message)


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
    for platform, required_scene in (
        ("ios", "detail"),
        ("android", "calls"),
        ("windows", "post_login_light"),
    ):
        expect(required_scene in scenes.get(platform, []), f"fixture scene missing: {platform}.{required_scene}", errors)
    metrics = acceptance.get("metrics", {})
    expect(metrics.get("minimum_hit_target_ios") == 44, "iOS minimum hit target must be 44", errors)
    expect(metrics.get("minimum_hit_target_android") == 48, "Android minimum hit target must be 48", errors)
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
    for scene in ("post_login", "post_login_light", "security_center"):
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


def verify_copy_keys(copy_keys: dict[str, Any], errors: list[str]) -> None:
    domains = copy_keys.get("canonical_domains", {})
    for domain in ("navigation", "auth", "chat", "security_center"):
        expect(domain in domains, f"copy key domain missing: {domain}", errors)
    ios_hints = copy_keys.get("platform_key_hints", {}).get("ios", {})
    for key, value in ios_hints.items():
        expect(value != "hardcoded-currently", f"iOS platform_key_hints.{key} must not be hardcoded-currently", errors)


def verify_ownership(errors: list[str]) -> None:
    expect(OWNERSHIP_PATH.exists(), f"missing ownership manifest: {OWNERSHIP_PATH}", errors)
    if not OWNERSHIP_PATH.exists():
        return
    ownership = load_json(OWNERSHIP_PATH)
    for key in (
        "allowed_paths",
        "forbidden_paths",
        "no_edit_surfaces",
        "legacy_excluded_surfaces",
        "single_writer_manifests",
    ):
        expect(key in ownership, f"ownership manifest missing key: {key}", errors)
    allowed = ownership.get("allowed_paths", {})
    for agent_name in ("Agent1", "Agent2", "Agent3"):
        expect(agent_name in allowed, f"ownership manifest missing agent: {agent_name}", errors)
    for path in ownership.get("single_writer_manifests", []):
        expect((ROOT / path).exists(), f"single writer manifest path missing: {path}", errors)
    for path in ownership.get("forbidden_paths", []):
        expect((ROOT / path).exists(), f"forbidden path missing from repo: {path}", errors)


def verify_platform_hooks(errors: list[str]) -> None:
    ios_app_shell = (ROOT / "ios_root_app/RootAuthApp/AppShell.swift").read_text(encoding="utf-8")
    android_main = (ROOT / "android/app/src/main/java/mi/e2ee/android/MainActivity.kt").read_text(encoding="utf-8")
    windows_qml = (ROOT / "client/ui/qml_main.cpp").read_text(encoding="utf-8")
    expect("MI_E2EE_IOS_SCREENSHOT_MODE" in ios_app_shell, "iOS screenshot hook missing", errors)
    expect("SCREENSHOT_MODE" in android_main, "Android screenshot hook missing", errors)
    expect("MI_E2EE_UI_SMOKE" in windows_qml, "Windows smoke hook missing", errors)
    expect("post-login-light" in windows_qml, "Windows post-login-light capture name missing", errors)
    expect("uiSmokeLocale" in windows_qml, "Windows smoke locale hook missing", errors)
    expect("uiSmokeScene" in windows_qml, "Windows smoke scene hook missing", errors)


def verify_windows_manifest_and_ci(errors: list[str]) -> None:
    qrc = (ROOT / "client/ui/common/ui_resources.qrc").read_text(encoding="utf-8")
    qml_qmldir = (ROOT / "client/ui/qml/qmldir").read_text(encoding="utf-8")
    stores_qmldir = (ROOT / "client/ui/qml/stores/qmldir").read_text(encoding="utf-8")
    cmake = (ROOT / "client/ui/CMakeLists.txt").read_text(encoding="utf-8")
    workflow = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")

    for alias in (
        'alias="qml/TrustFlowCoordinator.qml"',
        'alias="qml/SecurityDialogCoordinator.qml"',
        'alias="qml/SmokeAdapter.qml"',
        'alias="qml/stores/ChatDisplayStore.qml"',
        'alias="qml/stores/CallDisplayStore.qml"',
        'alias="qml/stores/SmokeSceneStore.qml"',
    ):
        expect(alias in qrc, f"ui_resources.qrc missing preregistered alias: {alias}", errors)

    for entry in (
        "TrustFlowCoordinator 1.0 TrustFlowCoordinator.qml",
        "SecurityDialogCoordinator 1.0 SecurityDialogCoordinator.qml",
        "SmokeAdapter 1.0 SmokeAdapter.qml",
        "singleton ChatDisplayStore 1.0 stores/ChatDisplayStore.qml",
        "singleton CallDisplayStore 1.0 stores/CallDisplayStore.qml",
        "singleton SmokeSceneStore 1.0 stores/SmokeSceneStore.qml",
    ):
        expect(entry in qml_qmldir, f"qml/qmldir missing entry: {entry}", errors)

    for entry in (
        "singleton ChatDisplayStore 1.0 ChatDisplayStore.qml",
        "singleton CallDisplayStore 1.0 CallDisplayStore.qml",
        "singleton SmokeSceneStore 1.0 SmokeSceneStore.qml",
    ):
        expect(entry in stores_qmldir, f"qml/stores/qmldir missing entry: {entry}", errors)

    for needle in ("display_contract.cpp", "qml_main.cpp", "quick_client.cpp"):
        expect(needle in cmake, f"client/ui/CMakeLists.txt missing source: {needle}", errors)

    for scene in ("login", "post_login", "post_login_light", "security_center"):
        expect(
            f'Invoke-UiSmoke "{scene}"' in workflow,
            f"build smoke missing explicit scene invocation: {scene}",
            errors,
        )
        expect(
            f'Invoke-DistUiSmoke "{scene}"' in workflow,
            f"dist smoke missing explicit scene invocation: {scene}",
            errors,
        )
    for env_name in (
        "MI_E2EE_UI_SMOKE_LOCALE",
        "MI_E2EE_UI_SMOKE_THEME",
        "MI_E2EE_UI_SMOKE_SCALE",
    ):
        expect(env_name in workflow, f"ci workflow missing smoke env loop hook: {env_name}", errors)


def verify_windows_shared_files(errors: list[str]) -> None:
    for path in (
        ROOT / "client/ui/display_contract.h",
        ROOT / "client/ui/display_contract.cpp",
        ROOT / "client/ui/qml/components/UiText.qml",
        ROOT / "client/ui/qml/components/AppBarPrimary.qml",
        ROOT / "client/ui/qml/components/StatusBanner.qml",
        ROOT / "client/ui/qml/components/SecurityBadge.qml",
        ROOT / "tools/windows_qml_accessibility_audit.py",
        ROOT / "tools/windows_qml_text_policy_audit.py",
        ROOT / "tools/windows_qml_literal_copy_audit.py",
        ROOT / "tools/windows_smoke_matrix_check.py",
        ROOT / "tools/windows_smoke_golden_diff.py",
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
        verify_copy_keys(loaded["copy_keys.json"], errors)
        verify_ownership(errors)
        verify_platform_hooks(errors)
        verify_windows_manifest_and_ci(errors)
        verify_windows_shared_files(errors)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print("UI contract verification passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
