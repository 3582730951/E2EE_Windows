#!/usr/bin/env python3
import argparse
import copy
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def fail(message):
    print(f"automated red-team audit failed: {message}", file=sys.stderr)
    return 1


def run_cmd(argv, *, expect_success, label, cwd):
    result = subprocess.run(
        argv,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    ok = result.returncode == 0
    if ok == expect_success:
        return True

    expectation = "succeed" if expect_success else "fail"
    print(f"[{label}] expected command to {expectation}", file=sys.stderr)
    print(f"command: {' '.join(str(x) for x in argv)}", file=sys.stderr)
    print(f"exit: {result.returncode}", file=sys.stderr)
    if result.stdout:
        print("--- stdout ---", file=sys.stderr)
        print(result.stdout[-4000:], file=sys.stderr)
    if result.stderr:
        print("--- stderr ---", file=sys.stderr)
        print(result.stderr[-4000:], file=sys.stderr)
    return False


def write_file(path, body):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")


def copytree(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def verifier_cmd(root, profile=None):
    cmd = [sys.executable, str(root / "tools" / "verify_security_plan_acceptance.py"), "--root", str(root)]
    if profile is not None:
        cmd += ["--profile", str(profile)]
    return cmd


def audit_acceptance_profile(root, work):
    base_path = root / "tools" / "security_plan_acceptance.json"
    base = json.loads(base_path.read_text(encoding="utf-8"))
    if not run_cmd(verifier_cmd(root), expect_success=True,
                   label="baseline security plan profile", cwd=root):
        return False

    cases = []

    profile = copy.deepcopy(base)
    profile["gates"][0]["status"] = "todo"
    cases.append(("forbidden gate status", profile))

    profile = copy.deepcopy(base)
    profile["gates"][0]["evidence"][0]["path"] = "server/tests/missing_red_team_fixture.cpp"
    cases.append(("missing evidence file", profile))

    profile = copy.deepcopy(base)
    profile["simulator_substitutions"] = [
        sub for sub in profile["simulator_substitutions"]
        if sub.get("name") != "android_emulator"
    ]
    cases.append(("missing Android emulator substitution", profile))

    profile = copy.deepcopy(base)
    profile["non_automatable_bounds"] = [
        risk for risk in profile["non_automatable_bounds"]
        if risk.get("id") != "root_or_admin"
    ]
    cases.append(("missing endpoint risk boundary", profile))

    profile = copy.deepcopy(base)
    profile["acceptance_claim"] = "manual_complete"
    cases.append(("manual acceptance claim", profile))

    for index, (label, profile) in enumerate(cases):
        path = work / "profiles" / f"attack_{index}.json"
        write_file(path, json.dumps(profile, indent=2) + "\n")
        if not run_cmd(verifier_cmd(root, path), expect_success=False,
                       label=f"profile attack: {label}", cwd=root):
            return False
    return True


def make_runtime_fixture(root):
    write_file(root / "client_run" / "history" / "0001.so", "ciphertext\n")
    write_file(root / "server_run" / "offline_store" / "queue.bin", "ciphertext\n")


def audit_runtime_privacy(root, work):
    clean = work / "runtime_clean"
    make_runtime_fixture(clean)
    verify = root / "tools" / "verify_runtime_privacy.sh"
    if not run_cmd(["bash", str(verify), "--root", str(clean)],
                   expect_success=True, label="clean runtime privacy fixture",
                   cwd=root):
        return False

    attacks = [
        ("runtime log artifact", "client_run/runtime.log", "secret\n"),
        ("diagnostics directory", "server_run/diagnostics/health.bin", "ok\n"),
        ("plaintext marker", "client_run/state/plaintext.bin", "message_plaintext=hello\n"),
        ("payload marker", "client_run/state/payload.bin", "payload_hex=00112233\n"),
        ("local path marker", "client_run/state/path.bin", "local_path=/home/alice/private/photo.jpg\n"),
    ]
    for index, (label, rel, body) in enumerate(attacks):
        attack = work / "runtime_attacks" / str(index)
        copytree(clean, attack)
        write_file(attack / rel, body)
        if not run_cmd(["bash", str(verify), "--root", str(attack)],
                       expect_success=False, label=f"runtime attack: {label}",
                       cwd=root):
            return False
    return True


def make_package_fixture(root):
    write_file(root / "mi_e2ee_client" / "state" / "history.bin", "encrypted\n")
    write_file(root / "mi_e2ee_server" / "state" / "queue.bin", "ciphertext\n")


def audit_posix_package_privacy(root, work):
    clean = work / "package_clean"
    make_package_fixture(clean)
    verify = root / "tools" / "verify_package_posix.sh"
    if not run_cmd(["bash", str(verify), "--dist", str(clean), "--privacy-only"],
                   expect_success=True, label="clean POSIX package privacy fixture",
                   cwd=root):
        return False

    attacks = [
        ("client log", "mi_e2ee_client/cache/runtime.log", "secret\n"),
        ("ops health tool", "mi_e2ee_server/tools/mi_e2ee_ops_health_view", "opaque\n"),
        ("debug config", "mi_e2ee_server/config/debug.ini", "[server]\ndebug_log=1\n"),
        ("token marker", "mi_e2ee_server/state/auth_cache", "token=raw-session-token\n"),
    ]
    for index, (label, rel, body) in enumerate(attacks):
        attack = work / "package_attacks" / str(index)
        copytree(clean, attack)
        write_file(attack / rel, body)
        if not run_cmd(["bash", str(verify), "--dist", str(attack), "--privacy-only"],
                       expect_success=False, label=f"POSIX package attack: {label}",
                       cwd=root):
            return False
    return True


ANDROID_MANIFEST = """<manifest xmlns:android="http://schemas.android.com/apk/res/android">
  <application
    android:allowBackup="false"
    android:fullBackupContent="@xml/no_backup_rules"
    android:dataExtractionRules="@xml/no_data_extraction"
    android:usesCleartextTraffic="false" />
</manifest>
"""

ANDROID_RULES = """<full-backup-content>
  <exclude domain="file" path="." />
  <exclude domain="database" path="." />
  <exclude domain="sharedpref" path="." />
  <exclude domain="external" path="." />
</full-backup-content>
"""


def make_android_fixture(root):
    write_file(root / "AndroidManifest.xml", ANDROID_MANIFEST)
    write_file(root / "res" / "xml" / "no_backup_rules.xml", ANDROID_RULES)
    write_file(root / "res" / "xml" / "no_data_extraction.xml", ANDROID_RULES)
    write_file(root / "classes.dex", "opaque bytecode\n")


def audit_android_package_privacy(root, work):
    clean = work / "android_clean"
    make_android_fixture(clean)
    verify = root / "tools" / "verify_android_package_privacy.sh"
    if not run_cmd(["bash", str(verify), "--root", str(clean)],
                   expect_success=True, label="clean Android package fixture",
                   cwd=root):
        return False

    attacks = [
        ("telemetry artifact", "firebase-analytics.marker", "opaque\n"),
        ("plaintext marker", "assets/state.bin", "plaintext_payload=hello\n"),
    ]
    for index, (label, rel, body) in enumerate(attacks):
        attack = work / "android_attacks" / str(index)
        copytree(clean, attack)
        write_file(attack / rel, body)
        if not run_cmd(["bash", str(verify), "--root", str(attack)],
                       expect_success=False, label=f"Android package attack: {label}",
                       cwd=root):
            return False

    cleartext = work / "android_attacks" / "cleartext"
    copytree(clean, cleartext)
    write_file(cleartext / "AndroidManifest.xml",
               ANDROID_MANIFEST.replace('android:usesCleartextTraffic="false"',
                                        'android:usesCleartextTraffic="true"'))
    return run_cmd(["bash", str(verify), "--root", str(cleartext)],
                   expect_success=False, label="Android cleartext attack",
                   cwd=root)


IOS_INFO_PLIST = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleIdentifier</key>
  <string>mi.e2ee.root.RootAuthApp</string>
  <key>NSFileProtectionKey</key>
  <string>NSFileProtectionComplete</string>
</dict>
</plist>
"""


def make_ios_fixture(root):
    app = root / "Payload" / "RootAuthApp.app"
    write_file(app / "Info.plist", IOS_INFO_PLIST)
    write_file(app / "RootAuthApp", "opaque executable\n")


def audit_ios_package_privacy(root, work):
    clean = work / "ios_clean"
    make_ios_fixture(clean)
    verify = root / "tools" / "verify_ios_package_privacy.sh"
    if not run_cmd(["bash", str(verify), "--root", str(clean)],
                   expect_success=True, label="clean iOS package fixture",
                   cwd=root):
        return False

    attacks = [
        ("crash artifact", "Payload/RootAuthApp.app/crash.log", "secret\n"),
        ("telemetry marker", "Payload/RootAuthApp.app/sentrysdk.bin", "opaque\n"),
        ("local path marker", "Payload/RootAuthApp.app/state.bin",
         "local_path=/Users/alice/Private/photo.jpg\n"),
    ]
    for index, (label, rel, body) in enumerate(attacks):
        attack = work / "ios_attacks" / str(index)
        copytree(clean, attack)
        write_file(attack / rel, body)
        if not run_cmd(["bash", str(verify), "--root", str(attack)],
                       expect_success=False, label=f"iOS package attack: {label}",
                       cwd=root):
            return False

    ats = work / "ios_attacks" / "ats"
    copytree(clean, ats)
    write_file(
        ats / "Payload" / "RootAuthApp.app" / "Info.plist",
        IOS_INFO_PLIST.replace(
            "</dict>",
            "  <key>NSAllowsArbitraryLoads</key>\n  <true/>\n</dict>",
        ),
    )
    return run_cmd(["bash", str(verify), "--root", str(ats)],
                   expect_success=False, label="iOS arbitrary-loads attack",
                   cwd=root)


def audit_ci_output(root, work):
    verify = root / "tools" / "verify_ci_output.sh"
    clean = work / "ci" / "clean.out"
    write_file(clean, "100% tests passed, 0 tests failed out of 39\n")
    if not run_cmd(["bash", str(verify), "--input", str(clean)],
                   expect_success=True, label="clean CI output fixture",
                   cwd=root):
        return False

    attacks = [
        ("file key", "file_key=0011223344556677\n"),
        ("PAT", "github_pat_0123456789abcdefghijklmnop\n"),
        ("bearer", "Authorization: Bearer abcdefghijklmnop1234567890\n"),
        ("home path", "wrote C:\\Users\\Alice\\Desktop\\secret.txt\n"),
    ]
    for index, (label, body) in enumerate(attacks):
        path = work / "ci" / f"attack_{index}.out"
        write_file(path, body)
        if not run_cmd(["bash", str(verify), "--input", str(path)],
                       expect_success=False, label=f"CI output attack: {label}",
                       cwd=root):
            return False
    return True


def audit_git_policy(root, work):
    verify = root / "tools" / "verify_git_policy.sh"
    clean = work / "git" / "clean.files"
    write_file(
        clean,
        "core/server/tests/automated_red_team_audit_test.cpp\n"
        "core/tools/automated_red_team_audit.py\n",
    )
    if not run_cmd(["bash", str(verify), "--changed-file-list", str(clean),
                    "--branch", "e2ee_dev"],
                   expect_success=True, label="clean git policy fixture",
                   cwd=root):
        return False

    attacks = [
        ("wrong branch", clean, "main"),
        ("outside core", work / "git" / "outside.files", "e2ee_dev"),
        ("forbidden image", work / "git" / "image.files", "e2ee_dev"),
    ]
    write_file(attacks[1][1], "docs/outside.cpp\n")
    write_file(attacks[2][1], "core/client/assets/ref/ref_login.png\n")
    for label, path, branch in attacks:
        if not run_cmd(["bash", str(verify), "--changed-file-list", str(path),
                        "--branch", branch],
                       expect_success=False, label=f"git policy attack: {label}",
                       cwd=root):
            return False
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = Path(args.root).resolve()

    with tempfile.TemporaryDirectory(prefix="mi_e2ee_red_team_") as tmp:
        work = Path(tmp)
        checks = [
            audit_acceptance_profile,
            audit_runtime_privacy,
            audit_posix_package_privacy,
            audit_android_package_privacy,
            audit_ios_package_privacy,
            audit_ci_output,
            audit_git_policy,
        ]
        for check in checks:
            if not check(root, work / check.__name__):
                return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
