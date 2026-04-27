#!/usr/bin/env python3
import argparse
import fnmatch
import os
import re
import sys


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


def compile_patterns(mode):
    source = {
        "package": (PACKAGE_MARKERS, PATH_MARKERS),
        "runtime": (RUNTIME_MARKERS, PATH_MARKERS),
        "path": (PATH_MARKERS,),
    }[mode]
    return [re.compile(pattern, re.IGNORECASE | re.MULTILINE) for pattern in source]


def should_skip(path, skip_names, skip_exts):
    name = os.path.basename(path)
    lowered = name.lower()
    if any(fnmatch.fnmatch(name, pattern) for pattern in skip_names):
        return True
    return any(lowered.endswith(ext.lower()) for ext in skip_exts)


def iter_files(root, skip_names, skip_exts):
    for dirpath, _, filenames in os.walk(root):
        for filename in filenames:
            path = os.path.join(dirpath, filename)
            if should_skip(path, skip_names, skip_exts):
                continue
            yield path


def read_text(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if data.startswith(b"MI_E2EE_CLIENT_CONFIG_V1\n"):
        return ""
    if b"\0" in data[:4096]:
        return ""
    return data.decode("utf-8", errors="ignore")


def find_line(text, offset):
    return text.count("\n", 0, offset) + 1


def scan_text(text, label, origin, patterns):
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
        root = root_value
        if not os.path.isdir(root):
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
