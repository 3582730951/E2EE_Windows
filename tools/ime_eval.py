#!/usr/bin/env python3
# Simple offline IME evaluator (Top1/Top5/KSPC/latency).
import argparse
import ctypes
import os
import shutil
import statistics
import time
import json


def copy_tree(src, dst):
    if not os.path.isdir(src):
        return
    for root, dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        target = dst if rel == "." else os.path.join(dst, rel)
        os.makedirs(target, exist_ok=True)
        for name in files:
            src_path = os.path.join(root, name)
            dst_path = os.path.join(target, name)
            if os.path.exists(dst_path):
                continue
            shutil.copy2(src_path, dst_path)


def apply_pack(resource_root, pack_id, shared_dir, user_dir):
    pack_base = os.path.join(resource_root, "ime_packs", pack_id)
    manifest = os.path.join(pack_base, "manifest.json")
    if not os.path.isfile(manifest):
        return
    with open(manifest, "r", encoding="utf-8") as f:
        data = json.load(f)
    for rule in data.get("rules", []):
        if rule.get("format") != "rime_patch":
            continue
        target = rule.get("target") or os.path.basename(rule.get("resource") or rule.get("path", ""))
        scope = rule.get("scope", "user")
        if scope == "shared":
            dst = os.path.join(shared_dir, target)
        else:
            dst = os.path.join(user_dir, target)
        if os.path.exists(dst):
            continue
        src = rule.get("path") or ""
        if rule.get("resource"):
            res_path = rule["resource"].replace(":/mi/e2ee/ui/ime/", "")
            src = os.path.join(resource_root, res_path)
        else:
            src = os.path.join(pack_base, src)
        if os.path.isfile(src):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
    for entry in data.get("dictionaries", []):
        if entry.get("type") != "custom_phrase":
            continue
        target = entry.get("target", "custom_phrase.txt")
        dst = os.path.join(user_dir, target)
        if os.path.exists(dst):
            continue
        src = entry.get("path") or ""
        if entry.get("resource"):
            res_path = entry["resource"].replace(":/mi/e2ee/ui/ime/", "")
            src = os.path.join(resource_root, res_path)
        else:
            src = os.path.join(pack_base, src)
        if not os.path.isfile(src):
            continue
        with open(src, "r", encoding="utf-8") as f:
            lines = []
            for raw in f:
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split("\t")
                if len(parts) < 2:
                    continue
                code = parts[0].strip()
                phrase = parts[1].strip()
                weight = parts[2].strip() if len(parts) >= 3 else "1"
                lines.append(f"{phrase}\t{code}\t{weight}")
        if lines:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, "w", encoding="utf-8") as out:
                out.write("\n".join(lines) + "\n")


def load_plugin(path):
    dll = ctypes.CDLL(path)
    dll.MiImeApiVersion.restype = ctypes.c_int
    dll.MiImeInitialize.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    dll.MiImeInitialize.restype = ctypes.c_bool
    dll.MiImeShutdown.argtypes = []
    dll.MiImeShutdown.restype = None
    dll.MiImeCreateSession.argtypes = []
    dll.MiImeCreateSession.restype = ctypes.c_void_p
    dll.MiImeDestroySession.argtypes = [ctypes.c_void_p]
    dll.MiImeDestroySession.restype = None
    dll.MiImeGetCandidates.argtypes = [
        ctypes.c_void_p,
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_size_t,
        ctypes.c_int,
    ]
    dll.MiImeGetCandidates.restype = ctypes.c_int
    return dll


def memory_mb():
    if os.name == "nt":
        try:
            import ctypes.wintypes as wt

            class PROCESS_MEMORY_COUNTERS(ctypes.Structure):
                _fields_ = [
                    ("cb", wt.DWORD),
                    ("PageFaultCount", wt.DWORD),
                    ("PeakWorkingSetSize", ctypes.c_size_t),
                    ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t),
                    ("PeakPagefileUsage", ctypes.c_size_t),
                ]

            psapi = ctypes.WinDLL("psapi")
            kernel = ctypes.WinDLL("kernel32")
            psapi.GetProcessMemoryInfo.argtypes = [
                wt.HANDLE,
                ctypes.POINTER(PROCESS_MEMORY_COUNTERS),
                wt.DWORD,
            ]
            psapi.GetProcessMemoryInfo.restype = wt.BOOL
            kernel.GetCurrentProcess.restype = wt.HANDLE
            counters = PROCESS_MEMORY_COUNTERS()
            counters.cb = ctypes.sizeof(counters)
            handle = kernel.GetCurrentProcess()
            if psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
                return counters.WorkingSetSize / (1024 * 1024)
        except Exception:
            return 0.0
    if os.name == "posix":
        try:
            import resource

            usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            if usage > 10 * 1024 * 1024:
                return usage / (1024 * 1024)
            return usage / 1024.0
        except Exception:
            return 0.0
    return 0.0


def get_candidates(dll, session, text, max_candidates=5):
    if not text:
        return []
    buf = ctypes.create_string_buffer(8192)
    count = dll.MiImeGetCandidates(
        session,
        text.encode("utf-8"),
        buf,
        ctypes.c_size_t(len(buf)),
        ctypes.c_int(max_candidates),
    )
    if count <= 0:
        return []
    payload = buf.value.decode("utf-8", errors="ignore").strip()
    if not payload:
        return []
    return [c for c in payload.split("\n") if c]


def parse_dataset(path):
    samples = []
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) == 2:
                text, gold = parts
                target = ""
            else:
                text, target, gold = parts[0], parts[1], parts[2]
            gold_list = [g for g in gold.split("|") if g]
            samples.append((text, target, gold_list))
    return samples


def main():
    parser = argparse.ArgumentParser(description="IME evaluation script")
    parser.add_argument("--plugin", required=True, help="Path to mi_ime_rime DLL")
    parser.add_argument("--dataset", required=True, help="TSV dataset path")
    parser.add_argument("--shared-dir", default="", help="Shared data dir")
    parser.add_argument("--user-dir", default="", help="User data dir")
    parser.add_argument("--resource-root", default="", help="Path to ui/data for rime assets")
    parser.add_argument("--pack-id", default="zh_cn", help="Language pack id")
    parser.add_argument("--max-candidates", type=int, default=5)
    parser.add_argument("--report", default="", help="Write JSON report to path")
    args = parser.parse_args()

    shared_dir = args.shared_dir or os.path.abspath("ime_eval_shared")
    user_dir = args.user_dir or os.path.abspath("ime_eval_user")
    os.makedirs(shared_dir, exist_ok=True)
    os.makedirs(user_dir, exist_ok=True)
    if args.resource_root:
        copy_tree(os.path.join(args.resource_root, "rime"), shared_dir)
        apply_pack(args.resource_root, args.pack_id, shared_dir, user_dir)

    mem_before = memory_mb()
    init_start = time.perf_counter()
    dll = load_plugin(args.plugin)
    if not dll.MiImeInitialize(shared_dir.encode("utf-8"), user_dir.encode("utf-8")):
        raise SystemExit("MiImeInitialize failed")
    init_ms = (time.perf_counter() - init_start) * 1000.0
    mem_after_init = memory_mb()

    session = dll.MiImeCreateSession()
    if not session:
        dll.MiImeShutdown()
        raise SystemExit("MiImeCreateSession failed")

    samples = parse_dataset(args.dataset)
    total = len(samples)
    top1 = 0
    top5 = 0
    kspc_values = []
    latencies = []

    for text, target, gold_list in samples:
        start = time.perf_counter()
        candidates = get_candidates(dll, session, text, args.max_candidates)
        latencies.append((time.perf_counter() - start) * 1000.0)
        if candidates:
            if candidates[0] in gold_list:
                top1 += 1
            if any(c in gold_list for c in candidates[: args.max_candidates]):
                top5 += 1
        if target:
            kspc_values.append(len(text) / max(len(target), 1))

    dll.MiImeDestroySession(session)
    dll.MiImeShutdown()
    mem_after_run = memory_mb()

    if total == 0:
        raise SystemExit("No valid samples found")

    top1_rate = top1 / total * 100.0
    top5_rate = top5 / total * 100.0
    p95 = statistics.quantiles(latencies, n=20)[18] if latencies else 0.0
    p99 = statistics.quantiles(latencies, n=100)[98] if len(latencies) >= 100 else max(latencies) if latencies else 0.0
    kspc = statistics.mean(kspc_values) if kspc_values else 0.0
    avg_latency = statistics.mean(latencies) if latencies else 0.0

    print(f"Samples: {total}")
    print(f"Top1: {top1_rate:.2f}%")
    print(f"Top5: {top5_rate:.2f}%")
    if kspc_values:
        print(f"KSPC: {kspc:.3f}")
    print(f"Latency AVG: {avg_latency:.2f} ms")
    print(f"Latency P95: {p95:.2f} ms")
    print(f"Latency P99: {p99:.2f} ms")
    print(f"Init: {init_ms:.2f} ms")
    print(f"Memory MB (before/init/after): {mem_before:.1f}/{mem_after_init:.1f}/{mem_after_run:.1f}")

    if args.report:
        report = {
            "samples": total,
            "top1_rate": top1_rate,
            "top5_rate": top5_rate,
            "kspc": kspc if kspc_values else None,
            "latency_ms": {
                "avg": avg_latency,
                "p95": p95,
                "p99": p99,
            },
            "init_ms": init_ms,
            "memory_mb": {
                "before": mem_before,
                "after_init": mem_after_init,
                "after_run": mem_after_run,
            },
            "plugin": os.path.abspath(args.plugin),
            "dataset": os.path.abspath(args.dataset),
        }
        with open(args.report, "w", encoding="utf-8") as f:
            json.dump(report, f, ensure_ascii=True, indent=2)


if __name__ == "__main__":
    main()
