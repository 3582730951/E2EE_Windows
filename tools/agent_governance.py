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
FILE_HINT_RE = re.compile(r"\b[\w./-]+\.(swift|kt|qml|py|cpp|cc|c|h|hpp|json|ini)\b", re.IGNORECASE)


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


def count_marker_hits(normalized_text: str, markers: List[str]) -> int:
    return sum(1 for marker in markers if marker and marker in normalized_text)


def has_commit_hash(text: str) -> bool:
    return bool(COMMIT_RE.search(text or ""))


def has_changed_files_signal(text: str) -> bool:
    t = normalize_text(text)
    return ("changed files" in t) or ("files:" in t) or bool(FILE_HINT_RE.search(text or ""))


def has_action_evidence(text: str) -> bool:
    return has_commit_hash(text) or has_changed_files_signal(text)


def detect_no_response(text: str, min_non_whitespace_chars: int) -> bool:
    compact = re.sub(r"\s+", "", text or "")
    return len(compact) < min_non_whitespace_chars


def detect_context_pollution(text: str, markers: List[str], min_marker_hits: int) -> bool:
    t = normalize_text(text)
    if not t:
        return False
    return count_marker_hits(t, markers) >= min_marker_hits


def detect_lazy_reply(text: str, min_chars: int, markers: List[str], min_marker_hits: int) -> bool:
    t = normalize_text(text)
    if not t:
        return False
    marker_hits = count_marker_hits(t, markers)
    short_or_marker_dense = (len(t) <= min_chars) or (marker_hits >= max(2, min_marker_hits + 1))
    return (marker_hits >= min_marker_hits) and short_or_marker_dense and (not has_action_evidence(text))


def detect_analysis_only(text: str) -> bool:
    t = normalize_text(text)
    if not t:
        return False
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
    return marker_hit and (not has_action_evidence(text))


def detect_repeated_restatement(prev_text: str, current_text: str, min_chars: int, similarity_threshold: float) -> bool:
    prev = (prev_text or "").strip()
    curr = (current_text or "").strip()
    if len(prev) < min_chars or len(curr) < min_chars:
        return False
    ratio = difflib.SequenceMatcher(a=normalize_text(prev), b=normalize_text(curr)).ratio()
    return ratio >= similarity_threshold


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


def field_present(packet: Dict[str, Any], dotted_key: str) -> bool:
    cur: Any = packet
    for part in dotted_key.split("."):
        if not isinstance(cur, dict) or part not in cur:
            return False
        cur = cur.get(part)
    if isinstance(cur, str):
        return bool(cur.strip())
    if isinstance(cur, list):
        return len(cur) > 0
    return cur is not None


def validate_envelope(packet: Dict[str, Any], policy: Dict[str, Any], retry_count: int) -> List[str]:
    violations: List[str] = []
    task_cfg = policy.get("task_envelope", {})
    orchestration_cfg = policy.get("orchestration", {})
    required_fields = task_cfg.get("required_fields", [])
    max_paths = int(task_cfg.get("max_paths_per_task", 3))
    max_goal_chars = int(task_cfg.get("max_goal_chars", 220))

    model = str(orchestration_cfg.get("model", ""))
    pm_role = str(orchestration_cfg.get("pm_role", "pm"))
    allow_non_pm = bool(orchestration_cfg.get("allow_non_pm_orchestrator", False))
    delegate_roles = set(orchestration_cfg.get("delegate_roles", []))
    orchestrator_role = str(packet.get("orchestration", {}).get("orchestrator_role", pm_role))
    target_role = str(packet.get("role", ""))

    if model == "pm_only" and (not allow_non_pm) and orchestrator_role != pm_role:
        violations.append("orchestrator_must_be_pm")
    if target_role and target_role != pm_role and delegate_roles and target_role not in delegate_roles:
        violations.append("target_role_not_delegable")

    paths = packet.get("paths", [])
    if not paths:
        violations.append("paths_required")
    if len(paths) > max_paths:
        violations.append(f"too_many_paths:{len(paths)}>{max_paths}")
    if bool(task_cfg.get("forbid_wildcard_paths", False)):
        for path in paths:
            if ("*" in path) or ("?" in path):
                violations.append(f"wildcard_path_forbidden:{path}")

    goal = str(packet.get("goal", ""))
    if not goal.strip():
        violations.append("goal_required")
    if len(goal) > max_goal_chars:
        violations.append(f"goal_too_long:{len(goal)}>{max_goal_chars}")

    if bool(task_cfg.get("require_base_commit", False)):
        base_commit = str(packet.get("context_packet", {}).get("base_commit", ""))
        if not base_commit:
            violations.append("base_commit_required")

    if bool(task_cfg.get("require_failure_summary_on_retry", False)) and retry_count > 0:
        failure_summary = str(packet.get("context_packet", {}).get("failure_summary", ""))
        if not failure_summary:
            violations.append("failure_summary_required_on_retry")

    for key in required_fields:
        if not field_present(packet, str(key)):
            violations.append(f"missing_required_field:{key}")

    return violations


def resolve_warning_recommendation(warnings_consecutive: int, warning_cfg: Dict[str, Any]) -> str:
    if warnings_consecutive <= 0:
        return "ok"
    max_before_recycle = int(warning_cfg.get("max_before_recycle", 3))
    escalation = warning_cfg.get("escalation", {})
    stage = min(warnings_consecutive, max_before_recycle)
    return str(escalation.get(str(stage), f"warning_{stage}"))


def compute_quality(
    policy: Dict[str, Any],
    reasons: List[str],
    commit_required: bool,
    commit_hash_detected: bool,
    changed_files_signal: bool,
) -> Dict[str, Any]:
    cfg = policy.get("quality_scoring", {})
    if not bool(cfg.get("enabled", True)):
        return {
            "enabled": False,
            "score": None,
            "tier": "disabled",
            "penalties": {},
            "bonuses": {},
        }

    base_score = int(cfg.get("base_score", 100))
    min_score = int(cfg.get("min_score", 0))
    invalid_penalty = int(cfg.get("invalid_penalty", 0))
    reason_penalties_cfg = cfg.get("reason_penalties", {})
    bonus_cfg = cfg.get("bonuses", {})
    thresholds = cfg.get("thresholds", {})
    pass_threshold = int(thresholds.get("pass", 80))
    watch_threshold = int(thresholds.get("watch", 60))

    penalties: Dict[str, int] = {}
    if reasons:
        penalties["invalid"] = invalid_penalty
    for reason in reasons:
        penalties[reason] = int(reason_penalties_cfg.get(reason, 0))

    bonuses: Dict[str, int] = {}
    if commit_required and commit_hash_detected:
        bonuses["commit_hash_present_when_required"] = int(bonus_cfg.get("commit_hash_present_when_required", 0))
    if changed_files_signal:
        bonuses["changed_files_signal_present"] = int(bonus_cfg.get("changed_files_signal_present", 0))

    score = base_score - sum(penalties.values()) + sum(bonuses.values())
    score = max(min_score, min(100, score))
    if score >= pass_threshold:
        tier = "pass"
    elif score >= watch_threshold:
        tier = "watch"
    else:
        tier = "fail"

    return {
        "enabled": True,
        "score": score,
        "tier": tier,
        "penalties": penalties,
        "bonuses": bonuses,
    }


def create_envelope(args: argparse.Namespace) -> int:
    policy = load_json(pathlib.Path(args.policy))
    state_dir = pathlib.Path(args.state_dir)
    model = resolve_role_model(policy, args.role, args.model)
    orchestration_cfg = policy.get("orchestration", {})
    pm_role = str(orchestration_cfg.get("pm_role", "pm"))
    task_cfg = policy.get("task_envelope", {})
    max_paths_per_task = int(task_cfg.get("max_paths_per_task", policy.get("isolation", {}).get("max_paths_per_task", 3)))
    packet = {
        "created_utc": now_utc(),
        "task_id": args.task_id,
        "role": args.role,
        "model": model,
        "goal": args.goal,
        "paths": args.path or [],
        "commit_required": bool(args.commit_required),
        "orchestration": {
            "model": orchestration_cfg.get("model", "pm_only"),
            "pm_role": pm_role,
            "orchestrator_role": args.orchestrator_role or pm_role,
        },
        "constraints": {
            "isolation_level": policy.get("isolation", {}).get("level", "strong"),
            "max_paths_per_task": max_paths_per_task,
            "fresh_agent_by_default": bool(policy.get("isolation", {}).get("fresh_agent_by_default", True)),
            "strict_envelope": bool(task_cfg.get("strict", False)),
        },
        "context_packet": {
            "base_commit": args.base_commit or "",
            "failure_summary": args.failure_summary or "",
            "skills_required": args.skill or [],
            "notes": args.note or [],
        },
    }
    violations = validate_envelope(packet, policy, int(args.retry_count))
    if violations:
        print(json.dumps({"error": "invalid_task_envelope", "violations": violations}, ensure_ascii=True), file=sys.stderr)
        return 2

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
    no_response_cfg = rules.get("no_response", {})
    if no_response_cfg.get("enabled", True):
        if detect_no_response(text, int(no_response_cfg.get("min_non_whitespace_chars", 1))):
            reasons.append("no_response")
    context_pollution_cfg = rules.get("context_pollution", {})
    if context_pollution_cfg.get("enabled", True):
        if detect_context_pollution(
            text,
            list(context_pollution_cfg.get("markers", [])),
            int(context_pollution_cfg.get("min_marker_hits", 1)),
        ):
            reasons.append("context_pollution")
    if rules.get("analysis_only", {}).get("enabled", True) and detect_analysis_only(text):
        reasons.append("analysis_only")
    lazy_reply_cfg = rules.get("lazy_reply", {})
    if lazy_reply_cfg.get("enabled", True):
        if detect_lazy_reply(
            text,
            int(lazy_reply_cfg.get("min_chars", 40)),
            list(lazy_reply_cfg.get("generic_markers", [])),
            int(lazy_reply_cfg.get("min_marker_hits", 1)),
        ):
            reasons.append("lazy_reply")
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
    warning_n = int(state["warnings_consecutive"])
    recommendation = resolve_warning_recommendation(warning_n, warning_cfg)
    commit_hash_detected = has_commit_hash(text)
    changed_files_signal = has_changed_files_signal(text)
    quality = compute_quality(policy, reasons, commit_required, commit_hash_detected, changed_files_signal)

    verdict = {
        "utc": now_utc(),
        "agent_id": args.agent_id,
        "task_id": args.task_id,
        "invalid": is_invalid,
        "reasons": reasons,
        "warnings_consecutive": warning_n,
        "recommendation": recommendation,
        "commit_required": commit_required,
        "commit_hash_detected": commit_hash_detected,
        "changed_files_signal_detected": changed_files_signal,
        "quality": quality,
    }

    state["previous_response"] = text
    state["last_reasons"] = reasons
    state["last_quality_score"] = quality.get("score")
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
    c.add_argument("--retry-count", type=int, default=0)
    c.add_argument("--orchestrator-role", default="")
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
