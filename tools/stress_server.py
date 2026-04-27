#!/usr/bin/env python3
import argparse
import json
import os
import socket
import subprocess
import sys
import threading
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


@dataclass
class StressPlan:
    host: str
    port: int
    connections: int
    messages: int
    payload_size: int
    timeout: float


def validate_plan(plan: StressPlan) -> None:
    if not 1 <= plan.port <= 65535:
        raise ValueError("port must be between 1 and 65535")
    if plan.connections < 1:
        raise ValueError("connections must be at least 1")
    if plan.messages < 0:
        raise ValueError("messages must be non-negative")
    if plan.payload_size < 0:
        raise ValueError("payload-size must be non-negative")
    if plan.timeout <= 0:
        raise ValueError("timeout must be positive")


def run_worker(plan: StressPlan, payload: bytes, failures: list[str], index: int) -> None:
    try:
        with socket.create_connection((plan.host, plan.port), timeout=plan.timeout) as sock:
            sock.settimeout(plan.timeout)
            for _ in range(plan.messages):
                if payload:
                    sock.sendall(payload)
    except OSError as exc:
        failures.append(f"worker {index}: {exc}")


def run_stress(plan: StressPlan, output_dir: str = "stress_results") -> int:
    validate_plan(plan)
    payload = b"x" * plan.payload_size
    failures: list[str] = []
    threads = [
        threading.Thread(target=run_worker, args=(plan, payload, failures, idx), daemon=True)
        for idx in range(plan.connections)
    ]
    start = time.monotonic()
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    elapsed = time.monotonic() - start
    attempted = plan.connections * plan.messages
    result = {
        "plan": asdict(plan),
        "attempted_messages": attempted,
        "connections": plan.connections,
        "elapsed_sec": round(elapsed, 3),
        "error_rate": (len(failures) / plan.connections) if plan.connections else 1.0,
        "errors": failures[:20],
    }
    write_result(result, output_dir)
    print(json.dumps(result, sort_keys=True))
    if failures:
        for failure in failures[:20]:
            print(f"error: {failure}", file=sys.stderr)
        return 1
    return 0


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Lightweight MI_E2EE server TCP stress helper.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument("--connections", type=int, default=16)
    parser.add_argument("--messages", type=int, default=1)
    parser.add_argument("--payload-size", type=int, default=32)
    parser.add_argument("--file-size", type=int, default=8 * 1024 * 1024)
    parser.add_argument("--chunk-size", type=int, default=1024 * 1024)
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--output-dir", default="stress_results")
    parser.add_argument("--dry-run", action="store_true", help="print the stress plan without opening sockets")
    parser.add_argument(
        "--business-scenario",
        choices=("private", "group", "offline", "mixed", "all"),
        help="run protocol/business stress through mi_e2ee_business_stress",
    )
    return parser


def find_business_stress_tool() -> Path | None:
    env = os.environ.get("MI_E2EE_BUSINESS_STRESS")
    candidates: list[Path] = []
    if env:
        candidates.append(Path(env))
    script_dir = Path(__file__).resolve().parent
    root = script_dir.parent
    exe_names = ["mi_e2ee_business_stress", "mi_e2ee_business_stress.exe"]
    for name in exe_names:
        candidates.extend(
            [
                script_dir / name,
                root / name,
                root / "build" / "server" / "tools" / name,
                root / "build" / "server-debug" / "tools" / name,
                root / "build" / "full-test-server" / "tools" / name,
            ]
        )
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def run_business_stress(
    scenario: str,
    clients: int,
    messages: int,
    payload_size: int,
    file_size: int,
    chunk_size: int,
    output_dir: str,
) -> int:
    tool = find_business_stress_tool()
    if tool is None:
        print("error: mi_e2ee_business_stress not found; build server/tools first", file=sys.stderr)
        return 1
    cmd = [
        str(tool),
        "--scenario",
        scenario,
        "--clients",
        str(clients),
        "--messages",
        str(messages),
        "--payload-size",
        str(payload_size),
        "--file-size",
        str(file_size),
        "--chunk-size",
        str(chunk_size),
        "--output-dir",
        output_dir,
    ]
    completed = subprocess.run(cmd, text=True, check=False)
    return completed.returncode


def write_result(result: dict, output_dir: str = "stress_results") -> Path:
    out_dir = Path(output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out_path = out_dir / f"{stamp}.json"
    out_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return out_path


def interactive_main() -> int:
    print("MI E2EE Stress Test")
    print()
    print("1) TLS idle clients")
    print("2) KCP idle clients")
    print("3) Private chat throughput")
    print("4) Group chat throughput")
    print("5) Offline file upload/download")
    print("6) Mixed scenario")
    print("7) Exit")
    choice = input("Select [1-7]: ").strip() or "7"
    if choice == "7":
        return 0
    clients = int(input("Clients [100]: ").strip() or "100")
    duration = int(input("Duration seconds [600]: ").strip() or "600")
    payload = int(input("Payload size bytes [1024]: ").strip() or "1024")
    if choice in {"3", "4", "5", "6"}:
        scenario = {
            "3": "private",
            "4": "group",
            "5": "offline",
            "6": "mixed",
        }[choice]
        file_size = max(payload * max(1, duration), 1024 * 1024)
        return run_business_stress(
            scenario,
            clients,
            max(1, duration),
            payload,
            file_size,
            1024 * 1024,
            "stress_results",
        )
    plan = StressPlan("127.0.0.1", 9000, clients, max(1, duration), payload, 3.0)
    return run_stress(plan)


def main(argv: Iterable[str] | None = None) -> int:
    if argv is None and len(sys.argv) == 1 and sys.stdin.isatty():
        return interactive_main()
    args = build_arg_parser().parse_args(argv)
    plan = StressPlan(
        host=args.host,
        port=args.port,
        connections=args.connections,
        messages=args.messages,
        payload_size=args.payload_size,
        timeout=args.timeout,
    )
    try:
        validate_plan(plan)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    if args.business_scenario:
        return run_business_stress(
            args.business_scenario,
            args.connections,
            max(1, args.messages),
            args.payload_size,
            args.file_size,
            args.chunk_size,
            args.output_dir,
        )
    print(
        "stress plan: "
        f"host={plan.host} port={plan.port} connections={plan.connections} "
        f"messages={plan.messages} payload_size={plan.payload_size} timeout={plan.timeout}"
    )
    if args.dry_run:
        result = {"plan": asdict(plan), "dry_run": True, "error_rate": 0.0}
        write_result(result, args.output_dir)
        print("dry_run=1")
        return 0
    return run_stress(plan, args.output_dir)


if __name__ == "__main__":
    raise SystemExit(main())
