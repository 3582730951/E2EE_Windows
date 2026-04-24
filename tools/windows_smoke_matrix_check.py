#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
ACCEPTANCE = json.loads((ROOT / "ui_contract" / "acceptance.json").read_text(encoding="utf-8"))
WINDOWS = ACCEPTANCE.get("windows", {})


def png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as handle:
        header = handle.read(24)
    if header[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a png: {path}")
    return struct.unpack(">II", header[16:24])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", required=True)
    parser.add_argument("--scene", required=True)
    parser.add_argument("--file", required=True)
    parser.add_argument("--scale", type=float, required=True)
    args = parser.parse_args()

    capture_dir = Path(args.capture_dir)
    png_path = capture_dir / args.file
    if not png_path.exists():
        print(f"ERROR: missing smoke capture: {png_path}")
        return 1

    viewport = WINDOWS.get("fixture_viewports", {}).get(args.scene)
    if not viewport:
        print(f"ERROR: unknown fixture viewport for scene {args.scene}")
        return 1

    expected_width = int(round(viewport[0] * args.scale))
    expected_height = int(round(viewport[1] * args.scale))
    width, height = png_size(png_path)
    if (width, height) != (expected_width, expected_height):
        print(
            "ERROR: smoke matrix mismatch for "
            f"{png_path}: expected {expected_width}x{expected_height}, got {width}x{height}"
        )
        return 1
    if args.scene == "post_login_light" and "post-login-light" not in png_path.stem:
        print(f"ERROR: post_login_light capture must be a real post-login-light scene: {png_path}")
        return 1
    expected_stem = {
        "post_login": "post-login",
        "chat_detail": "chat-detail",
        "settings_home": "settings-home",
        "calls_home": "calls-home",
        "security_center": "security-center",
    }.get(args.scene)
    if expected_stem and expected_stem not in png_path.stem:
        print(f"ERROR: {args.scene} capture must be a real {expected_stem} scene: {png_path}")
        return 1

    print(f"Windows smoke matrix check passed for {png_path.name}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
