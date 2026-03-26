#!/usr/bin/env python3
"""
Repository-local PM governance helper for multi-agent execution.

Features:
1) Create narrow task envelopes/context packets.
2) Record agent events/results.
3) Classify invalid responses and compute warn/recycle recommendations.
"""

import argparse
import datetime as dt
import difflib
import json
import pathlib
import re
import sys
from typing import Any, Dict, List, Optional


DEFAULT_POLICY_PATH = pathlib.Path(__file__).with_name("agent_governance_policy.json")
DEFAULT_STATE_DIR = pathlib.Path(__file__).with_name("agent_governance_state")
COMMIT_RE = re.compile(r"\b[0-9a-f]{7,40}\b", re.IGNORECASE)


def now_utc() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()


def load_json(path: pathlib.Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def save_json(path: pathlib.Path, data: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=True, sort_keys=False)
        f.write("\n")


def append_jsonl(path: pathlib.Path, row: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(row, ensure_ascii=True))
        f.write("\n")


def normalize_text(s: str) -> str:
    return " ".join((s or "").strip().lower().split())


def detect_analysis_only(text: str) -> bool:
    t = normalize_text(text)
    if not t:
        return True
    has_commit = bool(COMMIT_RE.search(t))
    has_file_hint = ("changed files" in t) or ("files:" in t) or (".swift" in t) or (".kt" in t) or (".qml" in t) or (".py" in t)
    analysis_markers = [
        "analysis",
        "diagnosis",
        "plan",
        "next step",
        "remaining",
        "will do",
        "i will",
        "status",
        "currently",
    ]
    marker_hit = any(m in t for m in analysis_markers)
    return marker_hit and (not has_commit) and (not has_file_hint)


def detect_repeated_restatement(prev_text: str, current_text: str, min_chars: int, similarity_threshold: float) -> bool:
    prev = (prev_text or "").strip()
    curr = (current_text or "").strip()
    if len(prev) < min_chars or len(curr) < min_chars:
        return False
    ratio = difflib.SequenceMatcher(a=normalize_text(prev), b=normalize_text(curr)).ratio()
    return ratio >= similarity_threshold


def has_commit_hash(text: str) -> bool:
    return bool(COMMIT_RE.search(text or ""))


def resolve_role_model(policy: Dict[str, Any], role: str, explicit_model: Optional[str]) -> str:
    if explicit_model:
        return explicit_model
    routing = policy.get("routing", {})
    return routing.get(role, routing.get("worker", "gpt-5.3-codex"))


def agent_state_path(state_dir: pathlib.Path, agent_id: str) -> pathlib.Path:
    return state_dir / "agents" / f"{agent_id}.json"


def load_agent_state(state_dir: pathlib.Path, agent_id: str) -> Dict[str, Any]:
    p = agent_state_path(state_dir, agent_id)
    if not p.exists():
        return {
            "agent_id": agent_id,
            "warnings_consecutive": 0,
            "previous_response": "",
            "updated_utc": now_utc(),
        }
    return load_json(p)


def save_agent_state(state_dir: pathlib.Path, state: Dict[str, Any]) -> None:
    p = agent_state_path(state_dir, state["agent_id"])
    save_json(p, state)


def create_envelope(args: argparse.Namespace) -> int:
    policy = load_json(pathlib.Path(args.policy))
    state_dir = pathlib.Path(args.state_dir)
    model = resolve_role_model(policy, args.role, args.model)
    packet = {
        "created_utc": now_utc(),
        "task_id": args.task_id,
        "role": args.role,
        "model": model,
        "goal": args.goal,
        "paths": args.path or [],
        "commit_required": bool(args.commit_required),
        "constraints": {
            "isolation_level": policy.get("isolation", {}).get("level", "strong"),
            "max_paths_per_task": policy.get("isolation", {}).get("max_paths_per_task", 3),
            "fresh_agent_by_default": bool(policy.get("isolation", {}).get("fresh_agent_by_default", True)),
        },
        "context_packet": {
            "base_commit": args.base_commit or "",
            "failure_summary": args.failure_summary or "",
            "skills_required": args.skill or [],
            "notes": args.note or [],
        },
    }
    out = pathlib.Path(args.out) if args.out else (state_dir / "tasks" / f"{args.task_id}.json")
    save_json(out, packet)
    print(str(out))
    return 0


def record_event(args: argparse.Namespace) -> int:
    state_dir = pathlib.Path(args.state_dir)
    row = {
        "utc": now_utc(),
        "agent_id": args.agent_id,
        "task_id": args.task_id,
        "event_type": args.event_type,
        "message": args.message or "",
        "meta": json.loads(args.meta_json) if args.meta_json else {},
    }
    append_jsonl(state_dir / "events" / f"{args.agent_id}.jsonl", row)
    print("ok")
    return 0


def record_result(args: argparse.Namespace) -> int:
    state_dir = pathlib.Path(args.state_dir)
    row = {
        "utc": now_utc(),
        "agent_id": args.agent_id,
        "task_id": args.task_id,
        "status": args.status,
        "commit_hash": args.commit_hash or "",
        "changed_files": args.changed_file or [],
        "summary": args.summary or "",
    }
    append_jsonl(state_dir / "results" / f"{args.agent_id}.jsonl", row)
    print("ok")
    return 0


def evaluate_response(args: argparse.Namespace) -> int:
    policy = load_json(pathlib.Path(args.policy))
    state_dir = pathlib.Path(args.state_dir)
    state = load_agent_state(state_dir, args.agent_id)

    text = args.response_text or ""
    if args.response_file:
        text = pathlib.Path(args.response_file).read_text(encoding="utf-8", errors="replace")

    commit_required = bool(args.commit_required)
    if args.envelope:
        env = load_json(pathlib.Path(args.envelope))
        commit_required = bool(env.get("commit_required", commit_required))

    rules = policy.get("invalid_response_rules", {})
    reasons: List[str] = []
    if bool(args.timed_out) and rules.get("timeout", {}).get("enabled", True):
        reasons.append("timeout")
    if rules.get("analysis_only", {}).get("enabled", True) and detect_analysis_only(text):
        reasons.append("analysis_only")
    if commit_required and rules.get("no_commit_hash", {}).get("enabled", True):
        if rules.get("no_commit_hash", {}).get("apply_when_commit_required", True) and not has_commit_hash(text):
            reasons.append("no_commit_hash")
    rep_cfg = rules.get("repeated_restatement", {})
    if rep_cfg.get("enabled", True):
        if detect_repeated_restatement(
            state.get("previous_response", ""),
            text,
            int(rep_cfg.get("min_chars", 80)),
            float(rep_cfg.get("similarity_threshold", 0.9)),
        ):
            reasons.append("repeated_restatement")

    is_invalid = len(reasons) > 0
    if is_invalid:
        state["warnings_consecutive"] = int(state.get("warnings_consecutive", 0)) + 1
    elif bool(args.reset_on_valid):
        state["warnings_consecutive"] = 0

    warning_cfg = policy.get("warnings", {})
    max_before_recycle = int(warning_cfg.get("max_before_recycle", 3))
    warning_n = int(state["warnings_consecutive"])
    if warning_n >= max_before_recycle:
        recommendation = "recycle"
    elif warning_n == 2:
        recommendation = "warning_2"
    elif warning_n == 1:
        recommendation = "warning_1"
    else:
        recommendation = "ok"

    verdict = {
        "utc": now_utc(),
        "agent_id": args.agent_id,
        "task_id": args.task_id,
        "invalid": is_invalid,
        "reasons": reasons,
        "warnings_consecutive": warning_n,
        "recommendation": recommendation,
        "commit_required": commit_required,
        "commit_hash_detected": has_commit_hash(text),
    }

    state["previous_response"] = text
    state["updated_utc"] = now_utc()
    save_agent_state(state_dir, state)
    append_jsonl(state_dir / "evaluations" / f"{args.agent_id}.jsonl", verdict)
    print(json.dumps(verdict, ensure_ascii=True))
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Agent governance helper")
    p.add_argument("--policy", default=str(DEFAULT_POLICY_PATH))
    p.add_argument("--state-dir", default=str(DEFAULT_STATE_DIR))

    sub = p.add_subparsers(dest="cmd", required=True)

    c = sub.add_parser("create-envelope")
    c.add_argument("--task-id", required=True)
    c.add_argument("--role", required=True)
    c.add_argument("--goal", required=True)
    c.add_argument("--path", action="append", default=[])
    c.add_argument("--skill", action="append", default=[])
    c.add_argument("--note", action="append", default=[])
    c.add_argument("--base-commit", default="")
    c.add_argument("--failure-summary", default="")
    c.add_argument("--model", default="")
    c.add_argument("--out", default="")
    c.add_argument("--commit-required", action="store_true")
    c.set_defaults(func=create_envelope)

    e = sub.add_parser("record-event")
    e.add_argument("--agent-id", required=True)
    e.add_argument("--task-id", required=True)
    e.add_argument("--event-type", required=True)
    e.add_argument("--message", default="")
    e.add_argument("--meta-json", default="")
    e.set_defaults(func=record_event)

    r = sub.add_parser("record-result")
    r.add_argument("--agent-id", required=True)
    r.add_argument("--task-id", required=True)
    r.add_argument("--status", required=True)
    r.add_argument("--commit-hash", default="")
    r.add_argument("--changed-file", action="append", default=[])
    r.add_argument("--summary", default="")
    r.set_defaults(func=record_result)

    v = sub.add_parser("evaluate-response")
    v.add_argument("--agent-id", required=True)
    v.add_argument("--task-id", required=True)
    v.add_argument("--response-text", default="")
    v.add_argument("--response-file", default="")
    v.add_argument("--envelope", default="")
    v.add_argument("--commit-required", action="store_true")
    v.add_argument("--timed-out", action="store_true")
    v.add_argument("--reset-on-valid", action="store_true")
    v.set_defaults(func=evaluate_response)

    return p


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    sys.exit(main())
