#!/usr/bin/env python3
import argparse
import json
import os
import shutil


def default_root():
    env = os.environ.get("MI_E2EE_IME_DIR")
    if env:
        return env
    appdata = os.environ.get("APPDATA") or os.environ.get("LOCALAPPDATA")
    if appdata:
        return os.path.join(appdata, "mi_e2ee_ime")
    return os.path.expanduser("~/.mi_e2ee")


def pack_root(root):
    return os.path.join(root, "ime", "packs")


def config_path(root):
    return os.path.join(root, "ime", "pack_config.json")


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def list_packs(root):
    packs = []
    base = pack_root(root)
    if os.path.isdir(base):
        for name in sorted(os.listdir(base)):
            if os.path.isfile(os.path.join(base, name, "manifest.json")):
                packs.append(name)
    print("Installed packs:")
    for name in packs:
        print(f"- {name}")
    if not packs:
        print("(none)")
    print("Built-in packs:")
    print("- zh_cn")
    print("- en")


def install_pack(root, pack_id, src):
    dst = os.path.join(pack_root(root), pack_id)
    if os.path.exists(dst):
        raise SystemExit(f"Target already exists: {dst}")
    if not os.path.isfile(os.path.join(src, "manifest.json")):
        raise SystemExit("manifest.json not found in source")
    ensure_dir(pack_root(root))
    shutil.copytree(src, dst)
    print(f"Installed {pack_id} -> {dst}")


def remove_pack(root, pack_id):
    dst = os.path.join(pack_root(root), pack_id)
    if not os.path.isdir(dst):
        raise SystemExit(f"Pack not found: {dst}")
    shutil.rmtree(dst)
    print(f"Removed {pack_id}")


def activate_pack(root, pack_id):
    ensure_dir(os.path.dirname(config_path(root)))
    data = {"active_pack": pack_id}
    with open(config_path(root), "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=True, indent=2)
    print(f"Activated {pack_id}")


def main():
    parser = argparse.ArgumentParser(description="IME language pack tool")
    parser.add_argument("--root", default=default_root(), help="IME data root")
    sub = parser.add_subparsers(dest="cmd", required=True)

    sub.add_parser("list", help="List packs")

    install = sub.add_parser("install", help="Install pack")
    install.add_argument("--id", required=True, help="Pack id")
    install.add_argument("--src", required=True, help="Source directory")

    remove = sub.add_parser("remove", help="Remove pack")
    remove.add_argument("--id", required=True, help="Pack id")

    activate = sub.add_parser("activate", help="Activate pack")
    activate.add_argument("--id", required=True, help="Pack id")

    args = parser.parse_args()
    root = os.path.abspath(args.root)

    if args.cmd == "list":
        list_packs(root)
    elif args.cmd == "install":
        install_pack(root, args.id, os.path.abspath(args.src))
    elif args.cmd == "remove":
        remove_pack(root, args.id)
    elif args.cmd == "activate":
        activate_pack(root, args.id)


if __name__ == "__main__":
    main()
