#!/usr/bin/env python3
"""Self-check for tools/agent_governance.py."""

import json
import pathlib
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parent
CLI = ROOT / "agent_governance.py"
POLICY = ROOT / "agent_governance_policy.json"


def run(args):
    cmd = [sys.executable, str(CLI), "--policy", str(POLICY)] + args
    return subprocess.run(cmd, check=True, capture_output=True, text=True)


def parse_last_json(stdout: str):
    return json.loads(stdout.strip().splitlines()[-1])


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="agent-governance-selfcheck-") as tmp:
        state_dir = pathlib.Path(tmp) / "state"
        envelope = pathlib.Path(tmp) / "task-t1.json"
        repeated_analysis = (
            "I analyzed the issue, verified all current screenshots, and confirmed "
            "the root cause in the shell container layout. I will implement the "
            "fix in the next step after one more pass on spacing."
        )

        out = run([
            "--state-dir", str(state_dir),
            "create-envelope",
            "--task-id", "t1",
            "--role", "worker",
            "--goal", "Fix chat inset",
            "--path", "ios_root_app/RootAuthApp/ClientWorkspaceViews.swift",
            "--commit-required",
            "--out", str(envelope),
        ])
        env_path = pathlib.Path(out.stdout.strip().splitlines()[-1])
        if not env_path.exists():
            raise AssertionError("envelope not created")

        v1 = run([
            "--state-dir", str(state_dir),
            "evaluate-response",
            "--agent-id", "a1",
            "--task-id", "t1",
            "--response-text", repeated_analysis,
            "--envelope", str(envelope),
            "--reset-on-valid",
        ])
        j1 = parse_last_json(v1.stdout)
        if not j1["invalid"] or j1["recommendation"] != "warning_1":
            raise AssertionError("first warning expectation failed")
        if "analysis_only" not in j1["reasons"]:
            raise AssertionError("missing analysis_only reason")
        if "no_commit_hash" not in j1["reasons"]:
            raise AssertionError("missing no_commit_hash reason")

        v2 = run([
            "--state-dir", str(state_dir),
            "evaluate-response",
            "--agent-id", "a1",
            "--task-id", "t1",
            "--response-text", repeated_analysis,
            "--envelope", str(envelope),
            "--reset-on-valid",
        ])
        j2 = parse_last_json(v2.stdout)
        if not j2["invalid"] or j2["recommendation"] != "warning_2":
            raise AssertionError("second warning expectation failed")
        if "repeated_restatement" not in j2["reasons"]:
            raise AssertionError("missing repeated_restatement reason")

        v3 = run([
            "--state-dir", str(state_dir),
            "evaluate-response",
            "--agent-id", "a1",
            "--task-id", "t1",
            "--response-text", "",
            "--envelope", str(envelope),
            "--timed-out",
            "--reset-on-valid",
        ])
        j3 = parse_last_json(v3.stdout)
        if j3["recommendation"] != "recycle":
            raise AssertionError("third strike should recycle")
        if "timeout" not in j3["reasons"]:
            raise AssertionError("missing timeout reason")

        v4 = run([
            "--state-dir", str(state_dir),
            "evaluate-response",
            "--agent-id", "a1",
            "--task-id", "t1",
            "--response-text", "commit fc243a8 changed files: ios_root_app/RootAuthApp/ClientWorkspaceViews.swift",
            "--envelope", str(envelope),
            "--reset-on-valid",
        ])
        j4 = parse_last_json(v4.stdout)
        if j4["invalid"]:
            raise AssertionError("valid commit response should pass")
        if j4["warnings_consecutive"] != 0:
            raise AssertionError("valid response should reset warnings")

    print("agent_governance_selfcheck: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
