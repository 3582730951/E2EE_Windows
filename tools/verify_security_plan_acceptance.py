#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path


FORBIDDEN_STATUSES = {
    "manual",
    "manual_only",
    "pending",
    "real_device_required",
    "todo",
    "unverified",
}


REQUIRED_RISKS = {
    "root_or_admin",
    "kernel_driver",
    "physical_memory",
    "unlocked_ui_capture",
}


def fail(message):
    print(f"security plan acceptance failed: {message}", file=sys.stderr)
    return 1


def read_text(path):
    try:
        return path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return path.read_text(encoding="utf-8", errors="ignore")


def require_contains(root, evidence, gate_id):
    rel_path = evidence.get("path")
    if not isinstance(rel_path, str) or not rel_path:
        return fail(f"{gate_id}: evidence entry is missing path")
    if Path(rel_path).is_absolute() or ".." in Path(rel_path).parts:
        return fail(f"{gate_id}: evidence path must stay inside repository: {rel_path}")
    path = root / rel_path
    if not path.exists():
        return fail(f"{gate_id}: evidence path is missing: {rel_path}")

    needles = evidence.get("contains", [])
    if not isinstance(needles, list):
        return fail(f"{gate_id}: contains must be a list for {rel_path}")
    if not needles:
        return 0

    body = read_text(path)
    for needle in needles:
        if not isinstance(needle, str) or not needle:
            return fail(f"{gate_id}: contains entries must be non-empty strings")
        if needle not in body:
            return fail(f"{gate_id}: {rel_path} does not contain {needle!r}")
    return 0


def validate_gate(root, gate):
    gate_id = gate.get("id")
    if not isinstance(gate_id, str) or not gate_id:
        return fail("gate is missing id")

    status = gate.get("status")
    if status in FORBIDDEN_STATUSES:
        return fail(f"{gate_id}: forbidden status {status!r}")
    if status not in {"automated", "ci_substituted"}:
        return fail(f"{gate_id}: status must be automated or ci_substituted")

    evidence = gate.get("evidence")
    if not isinstance(evidence, list) or not evidence:
        return fail(f"{gate_id}: gate must list evidence")

    for entry in evidence:
        if not isinstance(entry, dict):
            return fail(f"{gate_id}: evidence entry must be an object")
        result = require_contains(root, entry, gate_id)
        if result != 0:
            return result
    return 0


def validate_ci(root, profile):
    ci = profile.get("ci_required_jobs")
    if not isinstance(ci, list) or not ci:
        return fail("ci_required_jobs must be a non-empty list")
    workflow = root / ".github" / "workflows" / "ci.yml"
    if not workflow.exists():
        return fail(".github/workflows/ci.yml is missing")
    body = read_text(workflow)
    for job in ci:
        if not isinstance(job, str) or not job:
            return fail("ci_required_jobs entries must be non-empty strings")
        if f"{job}:" not in body:
            return fail(f"required CI job is missing: {job}")

    substitutions = profile.get("simulator_substitutions")
    if not isinstance(substitutions, list) or not substitutions:
        return fail("simulator_substitutions must be a non-empty list")
    names = set()
    for sub in substitutions:
        if not isinstance(sub, dict):
            return fail("simulator_substitutions entries must be objects")
        name = sub.get("name")
        if not isinstance(name, str) or not name:
            return fail("simulator substitution is missing name")
        names.add(name)
        for needle in sub.get("ci_contains", []):
            if not isinstance(needle, str) or not needle:
                return fail(f"{name}: ci_contains entries must be non-empty strings")
            if needle not in body:
                return fail(f"{name}: CI workflow does not contain {needle!r}")

    required_subs = {"android_emulator", "ios_simulator", "windows_ci_runtime"}
    missing = sorted(required_subs - names)
    if missing:
        return fail(f"missing simulator substitutions: {', '.join(missing)}")
    return 0


def validate_risks(profile):
    risks = profile.get("non_automatable_bounds")
    if not isinstance(risks, list) or not risks:
        return fail("non_automatable_bounds must be a non-empty list")
    seen = set()
    for risk in risks:
        if not isinstance(risk, dict):
            return fail("non_automatable_bounds entries must be objects")
        risk_id = risk.get("id")
        status = risk.get("status")
        if not isinstance(risk_id, str) or not risk_id:
            return fail("risk entry is missing id")
        if status != "accepted_risk":
            return fail(f"{risk_id}: risk status must be accepted_risk")
        seen.add(risk_id)
    missing = sorted(REQUIRED_RISKS - seen)
    if missing:
        return fail(f"missing non-automatable bounds: {', '.join(missing)}")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=Path(__file__).resolve().parents[1])
    parser.add_argument("--profile", default=None)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    profile_path = (
        Path(args.profile).resolve()
        if args.profile
        else root / "tools" / "security_plan_acceptance.json"
    )
    if not profile_path.exists():
        return fail(f"missing profile: {profile_path}")

    try:
        profile = json.loads(profile_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return fail(f"profile is not valid JSON: {exc}")

    if profile.get("acceptance_claim") != "ci_and_simulator_complete":
        return fail("acceptance_claim must be ci_and_simulator_complete")

    gates = profile.get("gates")
    if not isinstance(gates, list) or not gates:
        return fail("gates must be a non-empty list")
    for gate in gates:
        if not isinstance(gate, dict):
            return fail("gate entries must be objects")
        result = validate_gate(root, gate)
        if result != 0:
            return result

    for validator in (validate_ci,):
        result = validator(root, profile)
        if result != 0:
            return result
    return validate_risks(profile)


if __name__ == "__main__":
    sys.exit(main())
