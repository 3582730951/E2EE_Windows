#!/usr/bin/env python3
"""Unattended MI E2EE backend + Flutter integration runner.

The runner uses throwaway state under core/build/e2e/<run-id> by default and
removes it before exit. Retained CI evidence is sanitized and excludes secrets,
paths, stdout/stderr, and ops material.
"""

from __future__ import annotations

import argparse
import base64
import contextlib
import dataclasses
import json
import os
import secrets
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Callable, Iterable


@dataclasses.dataclass
class CommandResult:
    name: str
    command: list[str]
    cwd: str
    returncode: int
    stdout: str
    stderr: str
    duration_sec: float


@dataclasses.dataclass
class MysqlRuntime:
    host: str
    port: int
    database: str
    username: str
    password: str
    stop: Callable[[], None]
    label: str


class RunnerError(RuntimeError):
    pass


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def run_id() -> str:
    return f"run-{int(time.time())}-{secrets.token_hex(4)}"


def free_port() -> int:
    with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def run_command(
    name: str,
    command: Iterable[str],
    *,
    cwd: Path,
    env: dict[str, str] | None,
    timeout_sec: int,
) -> CommandResult:
    cmd = [str(part) for part in command]
    started = time.monotonic()
    proc = subprocess.run(
        cmd,
        cwd=str(cwd),
        env=env,
        text=True,
        capture_output=True,
        timeout=timeout_sec,
        check=False,
    )
    return CommandResult(
        name=name,
        command=cmd,
        cwd=str(cwd),
        returncode=proc.returncode,
        stdout=proc.stdout,
        stderr=proc.stderr,
        duration_sec=time.monotonic() - started,
    )


def require_ok(result: CommandResult) -> None:
    if result.returncode != 0:
        raise RunnerError(f"E2E_COMMAND_FAILED:{result.name}:{result.returncode}")


def require_cargo_opaque_metadata(repo: Path, env: dict[str, str]) -> CommandResult | None:
    manifest = repo / "shard" / "opaque_pake" / "Cargo.toml"
    if not manifest.exists():
        return None
    if not tool_exists("cargo"):
        raise RunnerError("Rust OPAQUE build requires cargo, but cargo is not on PATH")
    base_cmd = [
        "cargo",
        "metadata",
        "--locked",
        "--manifest-path",
        str(manifest),
        "--format-version",
        "1",
    ]
    result = run_command(
        "cargo opaque metadata offline",
        [*base_cmd[:3], "--offline", *base_cmd[3:]],
        cwd=repo,
        env=env,
        timeout_sec=90,
    )
    if result.returncode != 0 and "lock file version" in result.stderr:
        raise RunnerError(
            "Rust OPAQUE build requires a Cargo version compatible with "
            f"{manifest.parent / 'Cargo.lock'}.\n"
            f"cargo stderr:\n{result.stderr}"
        )
    if result.returncode != 0 and "no matching package named" in result.stderr:
        result = run_command(
            "cargo opaque metadata",
            base_cmd,
            cwd=repo,
            env=env,
            timeout_sec=180,
        )
    require_ok(result)
    return result


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    with contextlib.suppress(OSError):
        path.chmod(0o600)


def write_json(path: Path, value: object) -> None:
    write_text(path, json.dumps(value, ensure_ascii=False, indent=2) + "\n")


def cleanup_run_dir(run_dir: Path) -> None:
    try:
        if run_dir.is_symlink():
            target = run_dir.resolve()
            run_dir.unlink()
            shutil.rmtree(target, ignore_errors=True)
        else:
            shutil.rmtree(run_dir, ignore_errors=True)
    except OSError:
        pass


def wait_tcp(host: str, port: int, timeout_sec: int) -> None:
    deadline = time.monotonic() + timeout_sec
    last_error: OSError | None = None
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((host, port), timeout=1):
                return
        except OSError as exc:
            last_error = exc
            time.sleep(0.25)
    raise RunnerError(f"TCP endpoint {host}:{port} not ready: {last_error}")


def tool_exists(name: str) -> bool:
    return shutil.which(name) is not None


def chmod_effective(dir_path: Path) -> bool:
    probe = dir_path / f".chmod-probe-{secrets.token_hex(4)}"
    try:
        probe.write_text("probe", encoding="utf-8")
        probe.chmod(0o600)
        return (probe.stat().st_mode & 0o777) == 0o600
    except OSError:
        return False
    finally:
        with contextlib.suppress(OSError):
            probe.unlink()


def prepare_run_dir(repo: Path, run_name: str) -> Path:
    logical = repo / "build" / "e2e" / run_name
    logical.parent.mkdir(parents=True, exist_ok=True)
    if logical.exists() or logical.is_symlink():
        if logical.is_symlink() or logical.is_file():
            logical.unlink()
        else:
            shutil.rmtree(logical)
    logical.mkdir(parents=True, exist_ok=True)
    if chmod_effective(logical):
        return logical

    shutil.rmtree(logical)
    physical = Path(tempfile.mkdtemp(prefix=f"mi-e2ee-{run_name}-"))
    physical.chmod(0o700)
    logical.symlink_to(physical, target_is_directory=True)
    return logical


def docker_image_exists(tool: str, image: str) -> bool:
    return (
        subprocess.run(
            [tool, "image", "inspect", image],
            text=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        ).returncode
        == 0
    )


def start_container_mysql(tool: str, run_dir: Path) -> MysqlRuntime | None:
    image = os.environ.get("MI_E2EE_E2E_MARIADB_IMAGE", "mariadb:11")
    if not tool_exists(tool) or not docker_image_exists(tool, image):
        return None
    port = free_port()
    password = secrets.token_urlsafe(18)
    name = f"mi-e2ee-e2e-{secrets.token_hex(6)}"
    env = [
        "-e",
        f"MARIADB_ROOT_PASSWORD={password}",
        "-e",
        "MARIADB_DATABASE=mi_e2ee_e2e",
        "-e",
        "MARIADB_USER=mi_e2ee",
        "-e",
        f"MARIADB_PASSWORD={password}",
    ]
    cmd = [
        tool,
        "run",
        "--rm",
        "--name",
        name,
        "-p",
        f"127.0.0.1:{port}:3306",
        *env,
        "-d",
        image,
    ]
    result = run_command(
        f"{tool} mariadb start", cmd, cwd=run_dir, env=os.environ.copy(), timeout_sec=60
    )
    if result.returncode != 0:
        return None

    def stop() -> None:
        subprocess.run([tool, "rm", "-f", name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    try:
        wait_tcp("127.0.0.1", port, 90)
    except Exception:
        stop()
        raise
    return MysqlRuntime(
        host="127.0.0.1",
        port=port,
        database="mi_e2ee_e2e",
        username="mi_e2ee",
        password=password,
        stop=stop,
        label=f"{tool}:{image}",
    )


def start_local_mysql(run_dir: Path) -> MysqlRuntime | None:
    server = shutil.which("mariadbd") or shutil.which("mysqld")
    client = shutil.which("mariadb") or shutil.which("mysql")
    if not server or not client:
        return None
    datadir = run_dir / "mysql" / "data"
    socket_path = run_dir / "mysql" / "mysql.sock"
    port = free_port()
    datadir.mkdir(parents=True, exist_ok=True)
    init_cmd = [server, "--initialize-insecure", f"--datadir={datadir}"]
    if "mariadb" in Path(server).name:
        init_cmd.insert(1, "--user=root")
    init = run_command("mysql init", init_cmd, cwd=run_dir, env=os.environ.copy(), timeout_sec=90)
    if init.returncode != 0 and tool_exists("mariadb-install-db"):
        shutil.rmtree(datadir, ignore_errors=True)
        datadir.mkdir(parents=True, exist_ok=True)
        init = run_command(
            "mariadb install db",
            [
                "mariadb-install-db",
                f"--datadir={datadir}",
                "--auth-root-authentication-method=normal",
                "--skip-test-db",
                "--user=root",
            ],
            cwd=run_dir,
            env=os.environ.copy(),
            timeout_sec=90,
        )
    if init.returncode != 0:
        return None
    server_cmd = [
        server,
        f"--datadir={datadir}",
        f"--socket={socket_path}",
        f"--pid-file={run_dir / 'mysql' / 'mysql.pid'}",
        f"--port={port}",
        "--bind-address=127.0.0.1",
        "--skip-networking=0",
    ]
    if "mariadb" in Path(server).name:
        server_cmd.insert(1, "--user=root")
    proc = subprocess.Popen(
        server_cmd,
        cwd=str(run_dir),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    def stop() -> None:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()

    try:
        wait_tcp("127.0.0.1", port, 60)
        password = secrets.token_urlsafe(18)
        sql = (
            "CREATE DATABASE IF NOT EXISTS mi_e2ee_e2e;"
            f"CREATE USER IF NOT EXISTS 'mi_e2ee'@'%' IDENTIFIED BY '{password}';"
            "GRANT ALL PRIVILEGES ON mi_e2ee_e2e.* TO 'mi_e2ee'@'%';"
            "FLUSH PRIVILEGES;"
        )
        require_ok(
            run_command(
                "mysql bootstrap",
                [client, "-uroot", f"--socket={socket_path}", "-e", sql],
                cwd=run_dir,
                env=os.environ.copy(),
                timeout_sec=30,
            )
        )
    except Exception:
        stop()
        raise
    return MysqlRuntime(
        host="127.0.0.1",
        port=port,
        database="mi_e2ee_e2e",
        username="mi_e2ee",
        password=password,
        stop=stop,
        label="local-mysqld",
    )


def start_mysql(mode: str, run_dir: Path) -> MysqlRuntime | None:
    if mode == "skip":
        return None
    attempts: list[str] = []
    if mode in {"auto", "docker"}:
        attempts.append("docker")
    if mode in {"auto", "podman"}:
        attempts.append("podman")
    for tool in attempts:
        runtime = start_container_mysql(tool, run_dir)
        if runtime:
            return runtime
    if mode in {"auto", "local"}:
        runtime = start_local_mysql(run_dir)
        if runtime:
            return runtime
    raise RunnerError(
        "MySQL gate failed: no usable Docker/Podman MariaDB image or local "
        "mysqld/mariadbd runtime was available. The runner does not request "
        "manual credentials."
    )


def create_configs(run_dir: Path, mysql: MysqlRuntime | None, server_port: int) -> dict[str, object]:
    server_dir = run_dir / "server"
    offline_dir = server_dir / "offline_store"
    offline_dir.mkdir(parents=True, exist_ok=True)
    if mysql is None:
        raise RunnerError("server e2e requires MySQL mode=0; --mysql skip is only for dry setup")
    kt_signing_key = server_dir / "kt_signing_key.bin"
    kt_signing_key.write_bytes(secrets.token_bytes(4032))
    with contextlib.suppress(OSError):
        kt_signing_key.chmod(0o600)
    server_config = f"""[mode]
mode=0
[mysql]
mysql_ip={mysql.host}
mysql_port={mysql.port}
mysql_database={mysql.database}
mysql_username={mysql.username}
mysql_password={mysql.password}
[server]
list_port={server_port}
rotation_threshold=17
offline_dir={offline_dir}
debug_log=0
tls_enable=0
require_tls=0
tls_cert={server_dir / 'mi_e2ee_server.pfx'}
key_protection=none
metadata_key_hex={secrets.token_hex(32)}
kt_signing_key={kt_signing_key}
"""
    write_text(server_dir / "config.ini", server_config)

    clients: dict[str, dict[str, str]] = {}
    for role in ("alice", "bob", "linked"):
        client_dir = run_dir / "clients" / role
        state_dir = client_dir / "e2ee_state"
        state_dir.mkdir(parents=True, exist_ok=True)
        username = f"e2e_{role}_{secrets.token_hex(4)}"
        password = secrets.token_urlsafe(18)
        cfg = f"""[client]
server_ip=127.0.0.1
server_port={server_port}
use_tls=0
require_tls=0
trust_store={client_dir / 'server_trust.ini'}
auth_mode=opaque

[proxy]
type=none
host=
port=0
username=
password=

[device_sync]
enabled=1
role={'linked' if role == 'linked' else 'primary'}
key_path={state_dir / 'device_sync_key.bin'}

[kt]
require_signature=0
"""
        write_text(client_dir / "client_config.ini", cfg)
        clients[role] = {
            "username": username,
            "password": password,
            "config": str(client_dir / "client_config.ini"),
        }

    payload_dir = run_dir / "payloads"
    payload_dir.mkdir(parents=True, exist_ok=True)
    (payload_dir / "sample.bin").write_bytes(secrets.token_bytes(4096))
    png = base64.b64decode(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII="
    )
    (payload_dir / "sample.png").write_bytes(png)
    (payload_dir / "synthetic_media.pcm").write_bytes(secrets.token_bytes(2048))

    return {
        "server_config": str(server_dir / "config.ini"),
        "server_port": server_port,
        "clients": clients,
        "payload_dir": str(payload_dir),
    }


def configure_and_build(repo: Path, run_dir: Path, env: dict[str, str]) -> list[CommandResult]:
    results: list[CommandResult] = []
    cargo_home = run_dir / "cargo_home"
    cargo_home.mkdir(parents=True, exist_ok=True)
    env = dict(env)
    env.setdefault("CARGO_HOME", str(cargo_home))
    sdk_runtime_root = run_dir / "runtime" / "sdk_c_api"
    sdk_runtime_root.mkdir(parents=True, exist_ok=True)
    env["MI_E2EE_E2E_RUNTIME_ROOT"] = str(sdk_runtime_root)
    env["MI_E2EE_E2E_KEEP_RUNTIME_DIR"] = "1"
    cargo_result = require_cargo_opaque_metadata(repo, env)
    if cargo_result is not None:
        results.append(cargo_result)

    server_build = run_dir / "build" / "server-mysql"
    client_build = run_dir / "build" / "client-sdk"
    commands = [
        (
            "configure server",
            [
                "cmake",
                "-S",
                str(repo / "server"),
                "-B",
                str(server_build),
                "-DMI_E2EE_ENABLE_MYSQL=ON",
            ],
        ),
        ("build server", ["cmake", "--build", str(server_build), "--config", "Release"]),
        (
            "configure client sdk",
            [
                "cmake",
                "-S",
                str(repo / "client"),
                "-B",
                str(client_build),
                "-DMI_E2EE_BUILD_UI=OFF",
                "-DMI_E2EE_BUILD_SDK_SHARED=ON",
            ],
        ),
        ("build client sdk", ["cmake", "--build", str(client_build), "--config", "Release"]),
        ("ctest client sdk", ["ctest", "--output-on-failure", "--test-dir", str(client_build)]),
    ]
    for name, cmd in commands:
        result = run_command(name, cmd, cwd=repo, env=env, timeout_sec=900)
        results.append(result)
        require_ok(result)
    return results


def find_sdk_library(run_dir: Path) -> Path:
    client_build = run_dir / "build" / "client-sdk"
    names = (
        "libmi_e2ee_client_sdk.so",
        "libmi_e2ee_client_sdk.dylib",
        "mi_e2ee_client_sdk.dll",
    )
    candidates: list[Path] = []
    for name in names:
        candidates.extend(
            [
                client_build / name,
                client_build / "Release" / name,
                client_build / "Debug" / name,
            ]
        )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    for name in names:
        matches = list(client_build.rglob(name))
        if matches:
            return matches[0]
    raise RunnerError("client SDK dynamic library was not produced")


def add_library_search_path(env: dict[str, str], directory: Path) -> None:
    if sys.platform == "darwin":
        key = "DYLD_LIBRARY_PATH"
    elif os.name == "nt":
        key = "PATH"
    else:
        key = "LD_LIBRARY_PATH"
    current = env.get(key)
    env[key] = str(directory) if not current else f"{directory}{os.pathsep}{current}"


def configure_android_sdk_env(env: dict[str, str]) -> None:
    sdk_root = Path("/usr/lib/android-sdk")
    if sdk_root.exists():
        env.setdefault("ANDROID_HOME", str(sdk_root))
        env.setdefault("ANDROID_SDK_ROOT", str(sdk_root))


def server_exe(run_dir: Path) -> Path:
    candidates = [
        run_dir / "build" / "server-mysql" / "mi_e2ee_server",
        run_dir / "build" / "server-mysql" / "Release" / "mi_e2ee_server.exe",
        run_dir / "build" / "server-mysql" / "Debug" / "mi_e2ee_server.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise RunnerError("server executable was not produced")


def start_server(run_dir: Path, config_path: str, port: int) -> subprocess.Popen[str]:
    exe = server_exe(run_dir)
    proc = subprocess.Popen(
        [str(exe), str(config_path)],
        cwd=str(Path(config_path).parent),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    deadline = time.monotonic() + 60
    last_error: OSError | None = None
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            proc.communicate()
            raise RunnerError(f"E2E_SERVER_EXITED:{proc.returncode}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=1):
                return proc
        except OSError as exc:
            last_error = exc
            time.sleep(0.25)
    terminate_process(proc)
    proc.communicate()
    raise RunnerError("E2E_SERVER_NOT_READY")
    return proc


def terminate_process(proc: subprocess.Popen[str]) -> None:
    if proc.poll() is not None:
        return
    if os.name == "nt":
        proc.terminate()
    else:
        proc.send_signal(signal.SIGTERM)
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()


def run_flutter_gates(repo: Path, env: dict[str, str]) -> list[CommandResult]:
    app = repo / "app_flutter"
    commands = [
        ("flutter analyze", ["flutter", "analyze"]),
        ("flutter test", ["flutter", "test", "--concurrency=1"]),
    ]
    results: list[CommandResult] = []
    for name, cmd in commands:
        result = run_command(name, cmd, cwd=app, env=env, timeout_sec=900)
        results.append(result)
        require_ok(result)
    return results


def run_android_gate(repo: Path, env: dict[str, str]) -> list[CommandResult]:
    if not tool_exists("flutter"):
        raise RunnerError("Android gate requested but flutter is not on PATH")
    result = run_command(
        "flutter build apk",
        ["flutter", "build", "apk", "--debug"],
        cwd=repo / "app_flutter",
        env=env,
        timeout_sec=1200,
    )
    require_ok(result)
    return [result]


def run_ios_gate(repo: Path, env: dict[str, str]) -> list[CommandResult]:
    if sys.platform != "darwin":
        raise RunnerError("iOS simulator gate requested on a non-Darwin host")
    result = run_command(
        "flutter build ios simulator",
        ["flutter", "build", "ios", "--simulator", "--debug"],
        cwd=repo / "app_flutter",
        env=env,
        timeout_sec=1200,
    )
    require_ok(result)
    return [result]


def command_to_json(result: CommandResult) -> dict[str, object]:
    return {
        "name": result.name,
        "returncode": result.returncode,
        "duration_sec": round(result.duration_sec, 3),
    }


def config_to_json(config: dict[str, object]) -> dict[str, object]:
    clients = config.get("clients", {})
    client_count = len(clients) if isinstance(clients, dict) else 0
    return {
        "server": "configured",
        "client_count": client_count,
        "payloads": "generated",
    }


def privacy_scan_run_dir(run_dir: Path) -> None:
    forbidden_terms = ("audit", "diagnostic", "diagnostics", "telemetry", "crash", "ops_health")
    for path in run_dir.rglob("*"):
        if not path.is_file():
            continue
        rel_parts = path.relative_to(run_dir).parts
        if rel_parts and rel_parts[0] in {"build", "cargo_home"}:
            continue
        name = path.name.lower()
        if name.endswith(".log") or ".log." in name or name.endswith((".dmp", ".dump")):
            raise RunnerError("E2E_PRIVACY_ARTIFACT")
        if any(term in name for term in forbidden_terms):
            raise RunnerError("E2E_PRIVACY_ARTIFACT")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mysql", choices=["auto", "docker", "podman", "local", "skip"], default="auto")
    parser.add_argument("--desktop", action="store_true")
    parser.add_argument("--android-emulator", action="store_true")
    parser.add_argument("--ios-simulator", action="store_true")
    parser.add_argument("--update-goldens", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--keep-artifacts", action="store_true")
    parser.add_argument("--run-id", default=run_id())
    args = parser.parse_args(argv)

    repo = repo_root()
    run_dir = prepare_run_dir(repo, args.run_id)

    evidence: dict[str, object] = {
        "run_id": args.run_id,
        "commands": [],
        "status": "running",
    }
    mysql: MysqlRuntime | None = None
    server: subprocess.Popen[str] | None = None
    def stop_runtime() -> None:
        nonlocal mysql, server
        if server is not None:
            terminate_process(server)
            server = None
        if mysql is not None:
            mysql.stop()
            mysql = None

    try:
        mysql = start_mysql(args.mysql, run_dir)
        evidence["mysql"] = mysql.label if mysql else "skip"
        config = create_configs(run_dir, mysql, free_port())
        evidence["config"] = config_to_json(config)
        env = os.environ.copy()
        configure_android_sdk_env(env)
        env["MI_E2EE_CLIENT_CONFIG"] = config["clients"]["alice"]["config"]  # type: ignore[index]
        env["MI_E2EE_SDK_MODE"] = "ffi"
        if args.update_goldens:
            env["MI_E2EE_UPDATE_GOLDENS"] = "1"

        if not args.skip_build:
            build_results = configure_and_build(repo, run_dir, env)
            evidence["commands"].extend(command_to_json(item) for item in build_results)  # type: ignore[union-attr]
            sdk_library = find_sdk_library(run_dir)
            env["MI_E2EE_SDK_LIBRARY"] = str(sdk_library)
            add_library_search_path(env, sdk_library.parent)
            evidence["sdk_library"] = "produced"

        server = start_server(run_dir, config["server_config"], int(config["server_port"]))

        if args.desktop:
            flutter_results = run_flutter_gates(repo, env)
            evidence["commands"].extend(command_to_json(item) for item in flutter_results)  # type: ignore[union-attr]
        if args.android_emulator:
            android_results = run_android_gate(repo, env)
            evidence["commands"].extend(command_to_json(item) for item in android_results)  # type: ignore[union-attr]
        if args.ios_simulator:
            ios_results = run_ios_gate(repo, env)
            evidence["commands"].extend(command_to_json(item) for item in ios_results)  # type: ignore[union-attr]

        evidence["status"] = "passed"
        if args.keep_artifacts:
            stop_runtime()
            privacy_scan_run_dir(run_dir)
            write_json(run_dir / "e2e_evidence.json", evidence)
        print(json.dumps(evidence, ensure_ascii=False, indent=2))
        return 0
    except Exception as exc:
        evidence["status"] = "failed"
        evidence["error"] = str(exc)
        if args.keep_artifacts:
            stop_runtime()
            privacy_scan_run_dir(run_dir)
            write_json(run_dir / "e2e_evidence.json", evidence)
        print(json.dumps(evidence, ensure_ascii=False, indent=2), file=sys.stderr)
        return 1
    finally:
        stop_runtime()
        if not args.keep_artifacts:
            cleanup_run_dir(run_dir)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
