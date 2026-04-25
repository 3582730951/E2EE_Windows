#!/usr/bin/env python3
"""Guard Git pushes against repository-forbidden artifacts and secrets."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


FORBIDDEN_PATH_PREFIXES = (
    "client/assets/ref/",
    "client/ui_example/",
)

PRIVACY_ARTIFACT_TERMS = (
    "audit",
    "diagnostic",
    "diagnostics",
    "telemetry",
    "crash",
    "ops_health",
)

PRIVACY_ARTIFACT_ROOTS = {
    "artifact",
    "artifacts",
    "build",
    "captures",
    "dist",
    "out",
    "release",
}

PRIVACY_ARTIFACT_SUFFIXES = {
    ".7z",
    ".apk",
    ".dmp",
    ".dump",
    ".exe",
    ".gz",
    ".html",
    ".ipa",
    ".json",
    ".tar",
    ".tgz",
    ".zip",
}

PATTERN_MAP = {
    "github_pat": re.compile(r"github_pat_[A-Za-z0-9_]{20,}"),
    "github_classic_pat": re.compile(r"\bgh[pousr]_[A-Za-z0-9_]{20,}\b"),
    "openai_key": re.compile(r"\bsk-(?:proj-)?[A-Za-z0-9_-]{20,}\b"),
    "google_api_key": re.compile(r"\bAIza[0-9A-Za-z\-_]{20,}\b"),
    "aws_access_key": re.compile(r"\bAKIA[0-9A-Z]{16}\b"),
    "private_key_block": re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----"),
    "bearer_token": re.compile(
        r"Authorization\s*[:=]\s*[\"']?\s*Bearer\s+[A-Za-z0-9._-]{12,}",
        re.IGNORECASE,
    ),
}

ASSIGNMENT_PATTERN = re.compile(
    r"""
    \b
    (
        password
        |passwd
        |token
        |secret
        |api[_-]?key
        |access[_-]?key
        |private[_-]?key
        |client[_-]?secret
    )
    \b
    \s*[:=]\s*
    (?:
        ["'](?P<quoted>[^"'\\]{8,})["']
        |
        (?P<bare>[^\s"'#,;]{8,})
    )
    """,
    re.IGNORECASE | re.VERBOSE,
)

PLACEHOLDER_PREFIXES = (
    "",
    "***",
    "<",
    "${",
    "change_me",
    "changeme",
    "dummy",
    "example",
    "mock",
    "placeholder",
    "sample",
    "test",
    "todo",
    "your_",
)

PLACEHOLDER_EXACT = {
    "0",
    "1",
    "false",
    "localhost",
    "none",
    "null",
    "nullptr",
    "true",
}

VARIABLE_VALUE_PATTERN = re.compile(
    r"^[A-Za-z_][A-Za-z0-9_]*(?:\(\))?$|^\$\{[^}]+\}$|^[A-Z_][A-Z0-9_]*$"
)

HUNK_PATTERN = re.compile(r"@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")


@dataclass(frozen=True)
class ChangedPath:
    status: str
    path: str


@dataclass(frozen=True)
class SensitiveFinding:
    path: str
    line_number: int
    reason: str
    preview: str


def run_git(repo: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        cwd=repo,
        check=check,
        capture_output=True,
        text=True,
    )


def git_stdout(repo: Path, *args: str) -> str:
    return run_git(repo, *args).stdout.strip()


def resolve_head_ref(repo: Path, head_ref: str | None) -> str:
    return head_ref or "HEAD"


def resolve_base_ref(repo: Path, base_ref: str | None, head_ref: str) -> str:
    if base_ref:
        return base_ref
    upstream = run_git(
        repo,
        "rev-parse",
        "--abbrev-ref",
        "--symbolic-full-name",
        "@{upstream}",
        check=False,
    )
    if upstream.returncode == 0:
        upstream_ref = upstream.stdout.strip()
        return git_stdout(repo, "merge-base", head_ref, upstream_ref)
    previous = run_git(repo, "rev-parse", "HEAD~1", check=False)
    if previous.returncode == 0:
        return previous.stdout.strip()
    return head_ref


def list_changed_paths(repo: Path, base_ref: str, head_ref: str) -> list[ChangedPath]:
    output = git_stdout(
        repo,
        "diff",
        "--name-status",
        "--find-renames",
        "--diff-filter=ACMR",
        f"{base_ref}..{head_ref}",
    )
    changed: list[ChangedPath] = []
    for raw_line in output.splitlines():
        if not raw_line.strip():
            continue
        fields = raw_line.split("\t")
        status = fields[0]
        path = fields[-1]
        changed.append(ChangedPath(status=status, path=path))
    return changed


def is_allowed_push_path(path: str) -> bool:
    normalized = path.replace("\\", "/")
    if normalized.startswith(FORBIDDEN_PATH_PREFIXES):
        return False
    file_name = Path(normalized).name
    suffix = Path(normalized).suffix.lower()
    suffixes = [part.lower() for part in Path(normalized).suffixes]
    if suffix == ".png":
        return False
    if suffix == ".md":
        return file_name.lower() == "readme.md"
    if suffix == ".txt":
        return file_name == "CMakeLists.txt"
    if suffix == ".log" or any(part.startswith(".log") for part in suffixes):
        return False
    if is_forbidden_privacy_artifact(normalized):
        return False
    if "__pycache__" in Path(normalized).parts or suffix == ".pyc":
        return False
    return True


def is_forbidden_privacy_artifact(path: str) -> bool:
    normalized = path.replace("\\", "/")
    parts = tuple(part.lower() for part in Path(normalized).parts)
    file_name = Path(normalized).name.lower()
    suffix = Path(normalized).suffix.lower()
    if suffix in {".dmp", ".dump"}:
        return True
    if not any(term in file_name for term in PRIVACY_ARTIFACT_TERMS):
        return False
    if suffix in PRIVACY_ARTIFACT_SUFFIXES:
        return True
    return any(part in PRIVACY_ARTIFACT_ROOTS for part in parts[:-1])


def is_placeholder_value(value: str) -> bool:
    lowered = value.strip().lower()
    if lowered in PLACEHOLDER_EXACT:
        return True
    if lowered.startswith(PLACEHOLDER_PREFIXES):
        return True
    if VARIABLE_VALUE_PATTERN.fullmatch(value.strip()):
        return True
    if len(value.strip()) < 8:
        return True
    return False


def mask_secret(secret: str) -> str:
    if len(secret) <= 8:
        return "*" * len(secret)
    return f"{secret[:4]}...{secret[-2:]}"


def redact_preview(line: str) -> str:
    redacted = line
    for pattern in PATTERN_MAP.values():
        redacted = pattern.sub(lambda match: mask_secret(match.group(0)), redacted)

    def replace_assignment(match: re.Match[str]) -> str:
        key = match.group(1)
        value = match.group("quoted") or match.group("bare") or ""
        quote = '"' if match.group("quoted") else ""
        masked = mask_secret(value)
        return f"{key}={quote}{masked}{quote}"

    redacted = ASSIGNMENT_PATTERN.sub(replace_assignment, redacted)
    return redacted.strip()


def find_sensitive_matches(line: str) -> list[tuple[str, str]]:
    matches: list[tuple[str, str]] = []
    for reason, pattern in PATTERN_MAP.items():
        for match in pattern.finditer(line):
            matches.append((reason, match.group(0)))
    for match in ASSIGNMENT_PATTERN.finditer(line):
        value = match.group("quoted") or match.group("bare") or ""
        if not is_placeholder_value(value):
            matches.append((f"suspicious_{match.group(1).lower()}_assignment", value))
    return matches


def scan_added_lines(repo: Path, base_ref: str, head_ref: str, path: str) -> list[SensitiveFinding]:
    diff = git_stdout(repo, "diff", "--unified=0", "--no-color", f"{base_ref}..{head_ref}", "--", path)
    findings: list[SensitiveFinding] = []
    current_line = 0
    for raw_line in diff.splitlines():
        if raw_line.startswith("@@"):
            hunk = HUNK_PATTERN.search(raw_line)
            if not hunk:
                continue
            current_line = int(hunk.group(1))
            continue
        if raw_line.startswith("+++") or raw_line.startswith("---"):
            continue
        if raw_line.startswith("+"):
            added_line = raw_line[1:]
            matches = find_sensitive_matches(added_line)
            for reason, secret in matches:
                findings.append(
                    SensitiveFinding(
                        path=path,
                        line_number=current_line,
                        reason=reason,
                        preview=redact_preview(added_line).replace(secret, mask_secret(secret)),
                    )
                )
            current_line += 1
            continue
        if raw_line.startswith(" "):
            current_line += 1
    return findings


def format_report(forbidden_paths: list[str], findings: list[SensitiveFinding]) -> str:
    lines = ["push guard failed"]
    if forbidden_paths:
        lines.append("")
        lines.append("forbidden changed paths:")
        for path in forbidden_paths:
            lines.append(f"  - {path}")
    if findings:
        lines.append("")
        lines.append("sensitive additions:")
        for finding in findings:
            lines.append(
                f"  - {finding.path}:{finding.line_number} [{finding.reason}] {finding.preview}"
            )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Block Git pushes that include repository-forbidden artifacts or unsanitized sensitive data."
    )
    parser.add_argument("--repo", default=".", help="Repository root to inspect.")
    parser.add_argument("--base-ref", help="Base ref for the push diff.")
    parser.add_argument("--head-ref", help="Head ref for the push diff.")
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    head_ref = resolve_head_ref(repo, args.head_ref)
    base_ref = resolve_base_ref(repo, args.base_ref, head_ref)
    changed_paths = list_changed_paths(repo, base_ref, head_ref)

    if not changed_paths:
        print("push guard passed: no changed paths in the selected diff range.")
        return 0

    forbidden_paths = sorted(
        changed.path for changed in changed_paths if not is_allowed_push_path(changed.path)
    )

    sensitive_findings: list[SensitiveFinding] = []
    for changed in changed_paths:
        if is_allowed_push_path(changed.path):
            sensitive_findings.extend(scan_added_lines(repo, base_ref, head_ref, changed.path))

    if forbidden_paths or sensitive_findings:
        print(format_report(forbidden_paths, sensitive_findings))
        return 1

    print(
        "push guard passed: checked "
        f"{len(changed_paths)} changed path(s); no forbidden artifacts or sensitive additions found."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
