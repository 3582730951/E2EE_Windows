#!/usr/bin/env python3
import argparse
import configparser
import hashlib
import os
import re
import shutil
import stat
import subprocess
import sys
import time
from pathlib import Path
from typing import Iterable


SHA256_HEX_RE = re.compile(r"^[0-9a-f]{64}$")
PLACEHOLDER_VALUES = {
    "",
    "change_me",
    "change_me_user",
    "change_me_strong_password",
    "change_me_to_a_random_secret",
}
WEAK_MYSQL_PASSWORDS = {
    "123456",
    "admin",
    "demo",
    "mysql",
    "pass",
    "password",
    "root",
    "test",
}


class ValidationResult:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    @property
    def ok(self) -> bool:
        return not self.errors

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)


def normalize_bool(value: str) -> str:
    return value.strip().split("#", 1)[0].strip().lower()


def truthy(value: str) -> bool:
    return normalize_bool(value) in {"1", "true", "yes", "on"}


def read_ini(path: Path) -> configparser.ConfigParser:
    parser = configparser.ConfigParser(inline_comment_prefixes=("#", ";"))
    parser.optionxform = str
    with path.open("r", encoding="utf-8") as handle:
        parser.read_file(handle)
    return parser


def require_section(parser: configparser.ConfigParser, section: str, result: ValidationResult) -> bool:
    if not parser.has_section(section):
        result.error(f"missing [{section}] section")
        return False
    return True


def value(parser: configparser.ConfigParser, section: str, key: str, default: str = "") -> str:
    if not parser.has_section(section):
        return default
    return parser.get(section, key, fallback=default).strip()


def weak_mysql_credentials(username: str, password: str) -> bool:
    return username.strip().lower() in {"root", "admin"} and password.strip().lower() in WEAK_MYSQL_PASSWORDS


def resolve_relative(raw: str, root_dir: Path, config_dir: Path) -> Path:
    path = Path(raw)
    if path.is_absolute():
        return path
    candidates = [root_dir / path, config_dir / path]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def validate_port(raw: str, key: str, result: ValidationResult, *, allow_zero: bool = False) -> None:
    try:
        port = int(raw)
    except ValueError:
        result.error(f"{key} must be an integer")
        return
    lower = 0 if allow_zero else 1
    if port < lower or port > 65535:
        result.error(f"{key} must be between {lower} and 65535")


def validate_non_negative_int(raw: str, key: str, result: ValidationResult) -> None:
    try:
        parsed = int(raw)
    except ValueError:
        result.error(f"{key} must be an integer")
        return
    if parsed < 0:
        result.error(f"{key} must be non-negative")


def validate_config(path: Path, *, privacy_strict: bool = False) -> ValidationResult:
    result = ValidationResult()
    if not path.exists():
        result.error(f"config not found: {path}")
        return result

    try:
        parser = read_ini(path)
    except (OSError, configparser.Error) as exc:
        result.error(f"failed to read config: {exc}")
        return result

    has_mode = require_section(parser, "mode", result)
    has_server = require_section(parser, "server", result)
    if not has_mode or not has_server:
        return result

    mode = value(parser, "mode", "mode", "1")
    if mode not in {"0", "1"}:
        result.error("mode.mode must be 0 (mysql) or 1 (demo)")

    list_port = value(parser, "server", "list_port", "")
    if not list_port:
        result.error("server.list_port is required")
    else:
        validate_port(list_port, "server.list_port", result)

    validate_non_negative_int(
        value(parser, "server", "rotation_threshold", "10000"),
        "server.rotation_threshold",
        result,
    )
    validate_non_negative_int(
        value(parser, "server", "offline_blob_temp_budget_bytes", "4294967296"),
        "server.offline_blob_temp_budget_bytes",
        result,
    )

    base_dir = path.parent.parent if path.parent.name == "config" else path.parent

    offline_dir = value(parser, "server", "offline_dir", "")
    if not offline_dir:
        result.error("server.offline_dir is required")
    else:
        offline_path = Path(offline_dir)
        if not offline_path.is_absolute():
            offline_path = (base_dir / offline_path).resolve()
        try:
            offline_path.mkdir(parents=True, exist_ok=True)
            probe = offline_path / ".mi_e2ee_write_probe"
            probe.write_text("ok", encoding="ascii")
            probe.unlink(missing_ok=True)
        except OSError as exc:
            result.error(f"server.offline_dir is not writable: {offline_dir}: {exc}")
        if Path(offline_dir).is_absolute():
            result.warn("server.offline_dir is absolute; package builds normally use a relative database path")

    kt_signing_key = value(parser, "server", "kt_signing_key", "kt_signing_key.bin")
    kt_path = resolve_relative(kt_signing_key, base_dir, path.parent)
    if not kt_path.exists():
        result.error(f"server.kt_signing_key missing: {kt_signing_key}")

    tls_enable = truthy(value(parser, "server", "tls_enable", "0"))
    require_tls = truthy(value(parser, "server", "require_tls", "0"))
    tls_cert = value(parser, "server", "tls_cert", "")
    if require_tls and not tls_enable:
        result.error("server.require_tls=1 requires server.tls_enable=1")
    if tls_enable and not tls_cert:
        result.error("server.tls_cert is required when TLS is enabled")
    if tls_enable and tls_cert:
        cert_path = resolve_relative(tls_cert, base_dir, path.parent)
        if not cert_path.exists():
            result.error(f"server.tls_cert missing: {tls_cert}")
        else:
            try:
                _ = fingerprint_from_cert(cert_path)
            except (OSError, subprocess.CalledProcessError, ValueError) as exc:
                result.error(f"server.tls_cert fingerprint failed: {exc}")

    if privacy_strict and truthy(value(parser, "server", "debug_log", "0")):
        result.error("server.debug_log must stay disabled in strict/privacy packaging")

    ops_enable = truthy(value(parser, "server", "ops_enable", "0"))
    ops_allow_remote = truthy(value(parser, "server", "ops_allow_remote", "0"))
    ops_token = value(parser, "server", "ops_token", "")
    if ops_enable:
        if len(ops_token) < 16 or ops_token in PLACEHOLDER_VALUES:
            result.error("server.ops_token must be a non-placeholder value with at least 16 characters")
        if ops_allow_remote:
            result.warn("server.ops_allow_remote=1 exposes the ops endpoint beyond loopback")
    elif ops_token == "change_me_to_a_random_secret":
        result.warn("server.ops_token is still the placeholder; this is acceptable only while ops_enable=0")

    if parser.has_section("kcp"):
        validate_port(value(parser, "kcp", "listen_port", "0"), "kcp.listen_port", result, allow_zero=True)
        kcp_enable = truthy(value(parser, "kcp", "enable", "0"))
        kcp_allow_insecure = truthy(value(parser, "kcp", "allow_insecure", "0"))
        if kcp_enable and not kcp_allow_insecure:
            result.error("kcp.enable=1 requires kcp.allow_insecure=1")
        if kcp_enable and require_tls and kcp_allow_insecure:
            result.error("kcp.allow_insecure=1 conflicts with server.require_tls=1")

    if mode == "0":
        if not require_section(parser, "mysql", result):
            return result
        for key in ("mysql_ip", "mysql_port", "mysql_database", "mysql_username", "mysql_password"):
            raw = value(parser, "mysql", key, "")
            if raw in PLACEHOLDER_VALUES:
                result.error(f"mysql.{key} must be configured for mode=0")
        if weak_mysql_credentials(
            value(parser, "mysql", "mysql_username", ""),
            value(parser, "mysql", "mysql_password", ""),
        ):
            result.error("mysql credentials are too weak for mode=0")
        validate_port(value(parser, "mysql", "mysql_port", "0"), "mysql.mysql_port", result)

    if os.name != "nt":
        for label, sensitive_path in (
            ("config.ini", path),
            ("server.kt_signing_key", kt_path),
        ):
            if sensitive_path.exists():
                mode_bits = sensitive_path.stat().st_mode
                if mode_bits & (stat.S_IWGRP | stat.S_IWOTH):
                    result.error(f"{label} is group/world writable: {sensitive_path}")
        if tls_enable and tls_cert:
            cert_path = Path(tls_cert)
            if not cert_path.is_absolute():
                cert_path = resolve_relative(tls_cert, base_dir, path.parent)
            if cert_path.exists():
                mode_bits = cert_path.stat().st_mode
                if mode_bits & (stat.S_IWGRP | stat.S_IWOTH):
                    result.error(f"server.tls_cert is group/world writable: {cert_path}")

    return result


def normalize_fingerprint(raw: str) -> str:
    cleaned = re.sub(r"[^0-9a-fA-F]", "", raw).lower()
    if not SHA256_HEX_RE.fullmatch(cleaned):
        raise ValueError("fingerprint must be a SHA-256 hex digest")
    return cleaned


def fingerprint_from_cert(path: Path) -> str:
    if not path.exists():
        raise FileNotFoundError(path)
    try:
        completed = subprocess.run(
            ["openssl", "x509", "-in", str(path), "-outform", "DER"],
            check=True,
            capture_output=True,
        )
        return hashlib.sha256(completed.stdout).hexdigest()
    except (OSError, subprocess.CalledProcessError):
        return hashlib.sha256(path.read_bytes()).hexdigest()


def fingerprint_sas80_hex(sha256_hex: str) -> str:
    normalized = normalize_fingerprint(sha256_hex)
    digest = hashlib.sha256(b"MI_SERVER_CERT_SAS_V1" + bytes.fromhex(normalized)).hexdigest()
    return "-".join(digest[:20][i : i + 4] for i in range(0, 20, 4))


def rotate_client_pin(client_config: Path, fingerprint: str) -> None:
    normalized = normalize_fingerprint(fingerprint)
    parser = configparser.ConfigParser()
    parser.optionxform = str
    if client_config.exists():
        backup = client_config.with_name(f"{client_config.name}.bak.{int(time.time())}")
        shutil.copy2(client_config, backup)
        with client_config.open("r", encoding="utf-8") as handle:
            parser.read_file(handle)
    if not parser.has_section("client"):
        parser.add_section("client")
    parser.set("client", "use_tls", "1")
    parser.set("client", "require_tls", "1")
    parser.set("client", "require_pinned_fingerprint", "1")
    parser.set("client", "pinned_fingerprint", normalized)
    if not parser.get("client", "tls_verify_mode", fallback="").strip():
        parser.set("client", "tls_verify_mode", "pin")
    with client_config.open("w", encoding="utf-8", newline="\n") as handle:
        parser.write(handle, space_around_delimiters=False)


def update_client_tls_config(
    client_config: Path,
    verify_mode: str,
    *,
    fingerprint: str | None = None,
    ca_bundle_path: str | None = None,
) -> None:
    mode = verify_mode.strip().lower()
    if mode not in {"pin", "ca", "hybrid"}:
        raise ValueError("client TLS verify mode must be pin, ca, or hybrid")
    normalized_pin = normalize_fingerprint(fingerprint or "") if mode in {"pin", "hybrid"} else ""
    if mode in {"ca", "hybrid"} and not (ca_bundle_path or "").strip():
        raise ValueError("tls_ca_bundle_path is required for ca and hybrid modes")

    parser = configparser.ConfigParser()
    parser.optionxform = str
    if client_config.exists():
        backup = client_config.with_name(f"{client_config.name}.bak.{int(time.time())}")
        shutil.copy2(client_config, backup)
        with client_config.open("r", encoding="utf-8") as handle:
            parser.read_file(handle)
    if not parser.has_section("client"):
        parser.add_section("client")

    parser.set("client", "use_tls", "1")
    parser.set("client", "require_tls", "1")
    parser.set("client", "tls_verify_mode", mode)
    if mode == "pin":
        parser.set("client", "require_pinned_fingerprint", "1")
        parser.set("client", "pinned_fingerprint", normalized_pin)
        parser.set("client", "tls_ca_bundle_path", "")
    elif mode == "hybrid":
        parser.set("client", "require_pinned_fingerprint", "0")
        parser.set("client", "pinned_fingerprint", normalized_pin)
        parser.set("client", "tls_ca_bundle_path", ca_bundle_path or "")
    else:
        parser.set("client", "require_pinned_fingerprint", "0")
        parser.set("client", "pinned_fingerprint", "")
        parser.set("client", "tls_ca_bundle_path", ca_bundle_path or "")

    client_config.parent.mkdir(parents=True, exist_ok=True)
    with client_config.open("w", encoding="utf-8", newline="\n") as handle:
        parser.write(handle, space_around_delimiters=False)


def print_result(result: ValidationResult) -> None:
    for warning in result.warnings:
        print(f"warning: {warning}")
    for error in result.errors:
        print(f"error: {error}")
    print("server config validation passed" if result.ok else "server config validation failed")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate MI_E2EE server config and rotate client TLS pins.")
    parser.add_argument("--config", type=Path, help="server config.ini to validate")
    parser.add_argument("--privacy-strict", action="store_true", help="reject debug logging and strict package risks")
    parser.add_argument("--client-config", type=Path, help="client_config.ini to inspect or update")
    parser.add_argument("--expect-client-pin", help="expected pinned_fingerprint value in the client config")
    parser.add_argument("--rotate-client-pin", help="write this SHA-256 fingerprint to the client config")
    parser.add_argument("--set-client-tls", choices=("pin", "ca", "hybrid"), help="write client TLS verification mode")
    parser.add_argument("--tls-ca-bundle-path", help="CA bundle path to write for ca/hybrid client TLS modes")
    parser.add_argument("--cert", type=Path, help="derive the rotate/expected fingerprint from a server certificate")
    parser.add_argument("--show-cert", type=Path, help="print SHA-256 fingerprint and SAS for a server certificate")
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_arg_parser().parse_args(argv)
    exit_code = 0

    if args.config:
        result = validate_config(args.config, privacy_strict=args.privacy_strict)
        print_result(result)
        if not result.ok:
            exit_code = 1

    if args.show_cert:
        try:
            fp = fingerprint_from_cert(args.show_cert)
            print(f"sha256={fp}")
            print(f"sas={fingerprint_sas80_hex(fp)}")
        except (OSError, subprocess.CalledProcessError, ValueError) as exc:
            print(f"error: failed to inspect certificate: {exc}", file=sys.stderr)
            exit_code = 1

    derived_pin = fingerprint_from_cert(args.cert) if args.cert else None
    rotate_pin = args.rotate_client_pin or derived_pin
    if args.set_client_tls and args.client_config:
        try:
            update_client_tls_config(
                args.client_config,
                args.set_client_tls,
                fingerprint=rotate_pin,
                ca_bundle_path=args.tls_ca_bundle_path,
            )
            print(f"client TLS mode updated: {args.set_client_tls}")
        except (OSError, ValueError) as exc:
            print(f"error: failed to configure client TLS: {exc}", file=sys.stderr)
            exit_code = 1
    elif rotate_pin and args.client_config:
        try:
            rotate_client_pin(args.client_config, rotate_pin)
            print(f"client pin updated: {normalize_fingerprint(rotate_pin)}")
        except (OSError, ValueError) as exc:
            print(f"error: failed to rotate client pin: {exc}", file=sys.stderr)
            exit_code = 1

    expected_pin = args.expect_client_pin or (derived_pin if args.expect_client_pin is not None else None)
    if expected_pin and args.client_config:
        try:
            parser = read_ini(args.client_config)
            actual = normalize_fingerprint(value(parser, "client", "pinned_fingerprint", ""))
            expected = normalize_fingerprint(expected_pin)
            if actual != expected:
                print("error: client pinned_fingerprint does not match expected server fingerprint")
                exit_code = 1
            else:
                print("client pinned_fingerprint matches expected server fingerprint")
        except (OSError, ValueError, configparser.Error) as exc:
            print(f"error: failed to check client pin: {exc}", file=sys.stderr)
            exit_code = 1

    if not args.config and not args.set_client_tls and not rotate_pin and not expected_pin and not args.show_cert:
        build_arg_parser().print_help()
        return 2
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
