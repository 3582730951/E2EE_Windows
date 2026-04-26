#!/usr/bin/env python3
import argparse
import fnmatch
import os
import re
import sys
from pathlib import Path


PACKAGE_MARKERS = (
    r"(^|[^A-Za-z0-9_])("
    r"payload_hex\s*=|"
    r"file_key\s*=|"
    r"message_plaintext\s*=|"
    r"plaintext_payload\s*=|"
    r"local_path\s*=|"
    r"token\s*=|"
    r"access_token\s*=|"
    r"refresh_token\s*=|"
    r"ops_enable\s*=\s*(1|true|on|yes)|"
    r"debug_log\s*=\s*(1|true|on|yes)"
    r")"
)

RUNTIME_MARKERS = (
    r"(^|[^A-Za-z0-9_])("
    r"payload_hex\s*=|"
    r"file_key\s*=|"
    r"message_plaintext\s*=|"
    r"plaintext_payload\s*=|"
    r"local_path\s*=|"
    r"token\s*=|"
    r"access_token\s*=|"
    r"refresh_token\s*=|"
    r"mysql_password\s*=|"
    r"ops_enable\s*=\s*(1|true|on|yes)|"
    r"debug_log\s*=\s*(1|true|on|yes)"
    r")"
)

PATH_MARKERS = (
    r"(^|[^A-Za-z0-9_])/(home|Users)/[^\s/]+/|"
    r"[A-Za-z]:[\\/][Uu]sers[\\/][^\s\\/]+[\\/]"
)


def compile_patterns(mode: str) -> list[re.Pattern[str]]:
    source = {
        "package": (PACKAGE_MARKERS, PATH_MARKERS),
        "runtime": (RUNTIME_MARKERS, PATH_MARKERS),
        "path": (PATH_MARKERS,),
    }[mode]
    return [re.compile(pattern, re.IGNORECASE | re.MULTILINE) for pattern in source]


def should_skip(path: Path, skip_names: list[str], skip_exts: list[str]) -> bool:
    name = path.name
    lowered = name.lower()
    if any(fnmatch.fnmatch(name, pattern) for pattern in skip_names):
        return True
    return any(lowered.endswith(ext.lower()) for ext in skip_exts)


def iter_files(root: Path, skip_names: list[str], skip_exts: list[str]):
    for dirpath, _, filenames in os.walk(root):
        for filename in filenames:
            path = Path(dirpath) / filename
            if should_skip(path, skip_names, skip_exts):
                continue
            yield path


def read_text(path: Path) -> str:
    data = path.read_bytes()
    if b"\0" in data[:4096]:
        return ""
    return data.decode("utf-8", errors="ignore")


def find_line(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def scan_text(text: str, label: str, origin: str, patterns: list[re.Pattern[str]]) -> bool:
    for pattern in patterns:
        match = pattern.search(text)
        if match:
            line = find_line(text, match.start())
            print(f"{label} contains forbidden plaintext privacy marker: {origin}:{line}",
                  file=sys.stderr)
            return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("package", "runtime", "path"), required=True)
    parser.add_argument("--root", action="append", default=[])
    parser.add_argument("--stdin", action="store_true")
    parser.add_argument("--label", default="scan")
    parser.add_argument("--skip-name", action="append", default=[])
    parser.add_argument("--skip-ext", action="append", default=[])
    args = parser.parse_args()

    patterns = compile_patterns(args.mode)
    if args.stdin:
        text = sys.stdin.read()
        return 0 if scan_text(text, args.label, "<stdin>", patterns) else 1

    if not args.root:
        print("at least one --root or --stdin is required", file=sys.stderr)
        return 2

    for root_value in args.root:
        root = Path(root_value)
        if not root.is_dir():
            print(f"scan root missing: {root}", file=sys.stderr)
            return 1
        for path in iter_files(root, args.skip_name, args.skip_ext):
            try:
                text = read_text(path)
            except OSError:
                continue
            if text and not scan_text(text, args.label, str(path), patterns):
                return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
