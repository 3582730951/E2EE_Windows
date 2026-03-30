#!/usr/bin/env python3
from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
ACCEPTANCE = json.loads((ROOT / "ui_contract" / "acceptance.json").read_text(encoding="utf-8"))
SURFACES = [ROOT / path for path in ACCEPTANCE.get("windows", {}).get("acceptance_surfaces", [])]


def main() -> int:
    errors: list[str] = []
    bare_text = re.compile(r"\b(Text|Label)\s*\{")
    constrained_text = re.compile(
        r"Layout\.fillWidth\s*:\s*true|anchors\.(?:left|right)\s*:|width\s*:\s*(?:parent|root|ListView\.view)\."
    )
    for surface in SURFACES:
        if not surface.exists():
            errors.append(f"missing acceptance surface: {surface}")
            continue
        content = surface.read_text(encoding="utf-8")
        lines = content.splitlines()
        if not bare_text.search(content):
            continue
        for index, line in enumerate(lines):
            if not bare_text.search(line):
                continue
            snippet = "\n".join(lines[index : min(len(lines), index + 12)])
            if "contentItem: Text" in snippet:
                continue
            if ".charAt(0)" in snippet or 'text: ""' in snippet or "text: ''" in snippet:
                continue
            if "anchors.centerIn: parent" in snippet:
                continue
            if "textRole:" in snippet or "wrapMode:" in snippet or "elide:" in snippet or "maximumLineCount:" in snippet:
                continue
            if "text:" not in snippet:
                continue
            if not constrained_text.search(snippet):
                continue
            errors.append(f"{surface}:{index + 1}: bare text lacks overflow policy")

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print("Windows QML text policy audit passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
