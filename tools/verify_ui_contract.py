#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
CONTRACT_DIR = ROOT / "ui_contract"


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
        "AppBarPrimary",
        "ConversationRow",
        "MessageBubble",
        "ComposerBar",
        "StatusBanner",
        "SecurityBadge",
        "RightDetailPane",
    ):
        expect(component_id in present, f"required component missing: {component_id}", errors)


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


def verify_copy_keys(copy_keys: dict[str, Any], errors: list[str]) -> None:
    domains = copy_keys.get("canonical_domains", {})
    for domain in ("navigation", "auth", "chat", "security_center"):
        expect(domain in domains, f"copy key domain missing: {domain}", errors)


def verify_platform_hooks(errors: list[str]) -> None:
    ios_app_shell = (ROOT / "ios_root_app/RootAuthApp/AppShell.swift").read_text(encoding="utf-8")
    android_main = (ROOT / "android/app/src/main/java/mi/e2ee/android/MainActivity.kt").read_text(encoding="utf-8")
    windows_qml = (ROOT / "client/ui/qml_main.cpp").read_text(encoding="utf-8")
    expect("MI_E2EE_IOS_SCREENSHOT_MODE" in ios_app_shell, "iOS screenshot hook missing", errors)
    expect("SCREENSHOT_MODE" in android_main, "Android screenshot hook missing", errors)
    expect("MI_E2EE_UI_SMOKE" in windows_qml, "Windows smoke hook missing", errors)


def main() -> int:
    errors: list[str] = []
    loaded = verify_contract_files(errors)
    if not errors:
        verify_tokens(loaded["tokens.json"], errors)
        verify_routes(loaded["routes.json"], errors)
        verify_components(loaded["components.json"], errors)
        verify_acceptance(loaded["acceptance.json"], errors)
        verify_copy_keys(loaded["copy_keys.json"], errors)
        verify_platform_hooks(errors)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print("UI contract verification passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
