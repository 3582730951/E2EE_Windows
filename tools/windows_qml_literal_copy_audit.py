#!/usr/bin/env python3
from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
ACCEPTANCE = json.loads((ROOT / "ui_contract" / "acceptance.json").read_text(encoding="utf-8"))
SURFACES = [ROOT / path for path in ACCEPTANCE.get("windows", {}).get("acceptance_surfaces", [])]
ALLOWLIST = {"", "?", "PC", "LD", "FILE", "N/A"}


def main() -> int:
    errors: list[str] = []
    pattern = re.compile(r"\b(text|title|placeholderText)\s*:\s*\"([^\"]*)\"")
    for surface in SURFACES:
        if not surface.exists():
            errors.append(f"missing acceptance surface: {surface}")
            continue
        for line_no, line in enumerate(surface.read_text(encoding="utf-8").splitlines(), start=1):
            for match in pattern.finditer(line):
                literal = match.group(2)
                if literal in ALLOWLIST:
                    continue
                errors.append(f"{surface}:{line_no}: literal user-visible copy is forbidden: {literal!r}")

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print("Windows QML literal copy audit passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
