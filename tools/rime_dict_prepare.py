#!/usr/bin/env python3
import argparse
import json
import os
import sys
import urllib.request
from datetime import datetime
from pathlib import Path


def http_get_json(url):
    req = urllib.request.Request(url)
    req.add_header("User-Agent", "mi_e2ee_dict_prepare/1.0")
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=60) as resp:
        return json.loads(resp.read().decode("utf-8"))


def http_download(url, dst_path):
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url)
    req.add_header("User-Agent", "mi_e2ee_dict_prepare/1.0")
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=120) as resp, dst_path.open("wb") as f:
        while True:
            chunk = resp.read(1024 * 1024)
            if not chunk:
                break
            f.write(chunk)


def sanitize_dict_yaml(path):
    text = path.read_text(encoding="utf-8", errors="replace").splitlines()
    out = []
    in_body = False
    changed = False
    for line in text:
        if not in_body:
            out.append(line)
            if line.strip() == "...":
                in_body = True
            continue
        if not line or line.startswith("#"):
            out.append(line)
            continue
        if "\t" not in line:
            out.append("# " + line)
            changed = True
            continue
        out.append(line)
    if changed:
        path.write_text("\n".join(out) + "\n", encoding="utf-8")


def pick_latest_asset(assets, prefix):
    candidates = [
        a for a in assets
        if a.get("name", "").startswith(prefix) and a.get("name", "").endswith(".dict.yaml")
    ]
    if not candidates:
        return None
    candidates.sort(key=lambda a: a.get("updated_at", ""))
    return candidates[-1]


def prepare_zhwiki_dicts(out_share):
    release = http_get_json(
        "https://api.github.com/repos/felixonmars/fcitx5-pinyin-zhwiki/releases/latest"
    )
    assets = release.get("assets", [])
    mapping = {
        "zhwiki-": "zhwiki.dict.yaml",
        "zhwiktionary-": "zhwiktionary.dict.yaml",
        "zhwikisource-": "zhwikisource.dict.yaml",
        "web-slang-": "web_slang.dict.yaml",
    }
    cn_dir = out_share / "cn_dicts"
    cn_dir.mkdir(parents=True, exist_ok=True)
    for prefix, target_name in mapping.items():
        asset = pick_latest_asset(assets, prefix)
        if not asset:
            raise RuntimeError(f"Missing asset for prefix {prefix}")
        url = asset.get("browser_download_url")
        if not url:
            raise RuntimeError(f"Missing download url for {asset.get('name')}")
        dst = cn_dir / target_name
        http_download(url, dst)
        sanitize_dict_yaml(dst)


def read_english_words(path):
    words = set()
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if not parts:
                continue
            word = parts[0].strip()
            if not word:
                continue
            word = word.lower()
            if any(ch for ch in word if not (ch.isalnum() or ch in "-'")):
                continue
            words.add(word)
    return words


def write_english_dict(out_path, words, name):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    today = datetime.utcnow().strftime("%Y%m%d")
    header = [
        "# Rime dictionary",
        "# encoding: utf-8",
        "",
        "---",
        f"name: {name}",
        f"version: \"{today}\"",
        "sort: by_weight",
        "...",
        "",
    ]
    with out_path.open("w", encoding="utf-8") as f:
        f.write("\n".join(header))
        for word in sorted(words):
            f.write(f"{word}\t{word}\t1\n")


def prepare_english_dict(out_root, out_share):
    cache_dir = out_root / "_cache"
    cache_dir.mkdir(parents=True, exist_ok=True)
    english_url = "https://raw.githubusercontent.com/shewer/rime-english/master/english.txt"
    english_tw_url = "https://raw.githubusercontent.com/shewer/rime-english/master/english_tw.txt"
    english_path = cache_dir / "english.txt"
    english_tw_path = cache_dir / "english_tw.txt"
    http_download(english_url, english_path)
    http_download(english_tw_url, english_tw_path)
    words = read_english_words(english_path)
    words.update(read_english_words(english_tw_path))
    out_path = out_share / "en_dicts" / "en_full.dict.yaml"
    write_english_dict(out_path, words, "en_full")


def inject_import_tables(source_text, extra_entries):
    lines = source_text.splitlines()
    out = []
    in_block = False
    indent = ""
    existing = set()
    inserted = False
    for line in lines:
        if not in_block:
            out.append(line)
            if line.strip() == "import_tables:":
                in_block = True
                indent = line[: len(line) - len(line.lstrip())] + "  "
            continue
        if line.startswith(indent + "- "):
            entry = line.strip()[2:].strip()
            existing.add(entry)
            out.append(line)
            continue
        stripped = line.strip()
        if stripped == "" or stripped.startswith("#"):
            out.append(line)
            continue
        for entry in extra_entries:
            if entry not in existing:
                out.append(indent + "- " + entry)
        inserted = True
        in_block = False
        out.append(line)
    if in_block:
        for entry in extra_entries:
            if entry not in existing:
                out.append(indent + "- " + entry)
        inserted = True
    if inserted:
        return "\n".join(out) + "\n"
    return source_text


def prepare_overlay_configs(repo_root, out_share):
    rime_dir = repo_root / "client" / "ui" / "data" / "rime"
    rime_ice_path = rime_dir / "rime_ice.dict.yaml"
    melt_eng_path = rime_dir / "melt_eng.dict.yaml"
    if not rime_ice_path.exists() or not melt_eng_path.exists():
        raise RuntimeError("Base rime dictionaries not found in repo")
    rime_ice_text = rime_ice_path.read_text(encoding="utf-8")
    melt_eng_text = melt_eng_path.read_text(encoding="utf-8")
    rime_ice_text = inject_import_tables(
        rime_ice_text,
        [
            "cn_dicts/zhwiki",
            "cn_dicts/zhwiktionary",
            "cn_dicts/zhwikisource",
            "cn_dicts/web_slang",
        ],
    )
    melt_eng_text = inject_import_tables(
        melt_eng_text,
        [
            "en_dicts/en_full",
        ],
    )
    out_share.mkdir(parents=True, exist_ok=True)
    (out_share / "rime_ice.dict.yaml").write_text(rime_ice_text, encoding="utf-8")
    (out_share / "melt_eng.dict.yaml").write_text(melt_eng_text, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="Prepare Rime overlay dictionaries")
    parser.add_argument("--out", required=True, help="Overlay root output directory")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    out_root = Path(args.out).resolve()
    out_share = out_root / "share"

    prepare_zhwiki_dicts(out_share)
    prepare_english_dict(out_root, out_share)
    prepare_overlay_configs(repo_root, out_share)
    print(f"Rime overlay prepared at {out_share}")


if __name__ == "__main__":
    main()
