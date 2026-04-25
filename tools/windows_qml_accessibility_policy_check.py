#!/usr/bin/env python3
from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
ACCEPTANCE = json.loads((ROOT / "ui_contract" / "acceptance.json").read_text(encoding="utf-8"))
SURFACES = [ROOT / path for path in ACCEPTANCE.get("windows", {}).get("acceptance_surfaces", [])]


def block(lines: list[str], start: int, span: int = 18) -> str:
    return "\n".join(lines[start : min(len(lines), start + span)])


def main() -> int:
    errors: list[str] = []
    icon_button = re.compile(r"(?<![\w.])(?:Components\.)?(?:IconButton|RoundIconButton)\s*\{|(?<![\w.])ToolButton\s*\{")
    text_input = re.compile(r"(?:Components\.)?(?:SecureTextField|SecureTextArea)\s*\{|(?:TextField|TextArea)\s*\{")
    primary_action = re.compile(r"(?<![\w.])(?:Components\.)?(?:PrimaryButton|GhostButton|Button)\s*\{")

    for surface in SURFACES:
        if not surface.exists():
            errors.append(f"missing acceptance surface: {surface}")
            continue
        lines = surface.read_text(encoding="utf-8").splitlines()
        for index, line in enumerate(lines):
            snippet = block(lines, index)
            location = f"{surface}:{index + 1}"
            if icon_button.search(line):
                if not any(token in snippet for token in ("Accessible.name", "ToolTip.text", "accessibleName:")):
                    errors.append(f"{location}: icon button missing accessible name policy")
            if text_input.search(line):
                if not any(token in snippet for token in ("Accessible.name", "placeholderText", "accessibleName:")):
                    errors.append(f"{location}: auth/input field missing accessible name policy")
            if primary_action.search(line):
                if not any(token in snippet for token in ("text:", "Accessible.name", "accessibleName:")):
                    errors.append(f"{location}: primary action missing accessible label")

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print("Windows QML accessibility policy check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
