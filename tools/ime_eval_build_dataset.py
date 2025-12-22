#!/usr/bin/env python3
import argparse
import json
import os
import random


def iter_entries(path):
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if "\t" not in line:
                continue
            parts = [p.strip() for p in line.split("\t")]
            if len(parts) < 2:
                continue
            word = parts[0]
            code = parts[1].replace(" ", "")
            if not word or not code:
                continue
            yield word, code


def load_sources(paths):
    entries = []
    for path in paths:
        if not os.path.isfile(path):
            continue
        for word, code in iter_entries(path):
            entries.append((word, code))
    return entries


def build_dataset(entries, size, min_len, max_len, seed):
    rng = random.Random(seed)
    filtered = []
    seen = set()
    for word, code in entries:
        if min_len and len(word) < min_len:
            continue
        if max_len and len(word) > max_len:
            continue
        key = (word, code)
        if key in seen:
            continue
        seen.add(key)
        filtered.append((word, code))
    rng.shuffle(filtered)
    if size and len(filtered) > size:
        filtered = filtered[:size]
    return filtered


def infer_sources(root):
    candidates = []
    if not root:
        return candidates
    for sub in ["cn_dicts/8105.dict.yaml", "cn_dicts/base.dict.yaml", "cn_dicts/ext.dict.yaml",
                "cn_dicts/tencent.dict.yaml", "cn_dicts/others.dict.yaml", "cn_dicts/41448.dict.yaml"]:
        path = os.path.join(root, sub)
        if os.path.isfile(path):
            candidates.append(path)
    return candidates


def main():
    parser = argparse.ArgumentParser(description="Build IME evaluation dataset from rime dicts")
    parser.add_argument("--rime-root", default="", help="Path to ui/data/rime")
    parser.add_argument("--dict", action="append", default=[], help="Additional dict yaml paths")
    parser.add_argument("--out", required=True, help="Output TSV dataset path")
    parser.add_argument("--size", type=int, default=10000)
    parser.add_argument("--min-len", type=int, default=1)
    parser.add_argument("--max-len", type=int, default=6)
    parser.add_argument("--seed", type=int, default=20251223)
    parser.add_argument("--report", default="", help="Write JSON stats to path")
    args = parser.parse_args()

    sources = []
    sources.extend(infer_sources(args.rime_root))
    sources.extend(args.dict)
    entries = load_sources(sources)
    dataset = build_dataset(entries, args.size, args.min_len, args.max_len, args.seed)

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        for word, code in dataset:
            f.write(f"{code}\t{word}\t{word}\n")

    if args.report:
        stats = {
            "entries_total": len(entries),
            "entries_selected": len(dataset),
            "min_len": args.min_len,
            "max_len": args.max_len,
            "sources": sources,
        }
        with open(args.report, "w", encoding="utf-8") as f:
            json.dump(stats, f, ensure_ascii=True, indent=2)

    print(f"Sources: {len(sources)}")
    print(f"Total entries: {len(entries)}")
    print(f"Dataset entries: {len(dataset)}")


if __name__ == "__main__":
    main()
