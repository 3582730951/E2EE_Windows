import configparser
import hashlib
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools"


def load_tool_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


verify = load_tool_module("verify_server_config", TOOLS_DIR / "verify_server_config.py")


class ServerToolingTest(unittest.TestCase):
    def run_script(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(TOOLS_DIR / "configure_server.sh"), *args],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )

    def make_temp_tool_root(self, temp: str) -> Path:
        root = Path(temp) / "server-root"
        tool_dir = root / "tools"
        tool_dir.mkdir(parents=True)
        for name in ("configure_server.sh", "configure_server.cmd", "verify_server_config.py", "stress_server.py"):
            shutil.copy2(TOOLS_DIR / name, tool_dir / name)
        os.chmod(tool_dir / "configure_server.sh", 0o700)
        return root

    def run_temp_configure(self, root: Path, input_text: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(root / "tools" / "configure_server.sh")],
            cwd=root,
            input=input_text,
            capture_output=True,
            text=True,
            check=False,
        )

    def read_config(self, path: Path) -> configparser.ConfigParser:
        parser = configparser.ConfigParser()
        parser.read(path, encoding="utf-8")
        return parser

    def test_configure_menu_text_and_defaults(self) -> None:
        result = self.run_script("--print-menu")
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("First-time setup", result.stdout)
        self.assertIn("Reconfigure server", result.stdout)
        self.assertIn("Generate self-signed certificate and pin", result.stdout)
        self.assertIn("Rotate client pinned fingerprint", result.stdout)
        self.assertIn("Initialize / rotate Key Transparency key", result.stdout)
        self.assertIn("Select [1-9]", result.stdout)
        self.assertIn("Validate current configuration", result.stdout)

    def test_start_menu_text_and_defaults(self) -> None:
        result = subprocess.run(
            [str(TOOLS_DIR / "start_server.sh"), "--print-menu"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("Start server", result.stdout)
        self.assertIn("Configure then start", result.stdout)
        self.assertIn("Validate configuration", result.stdout)
        self.assertIn("Stress test server", result.stdout)
        self.assertIn("Select [1-5]", result.stdout)

    def test_cmd_scripts_have_interactive_menu_entries(self) -> None:
        configure_cmd = (TOOLS_DIR / "configure_server.cmd").read_text(encoding="utf-8")
        start_cmd = (TOOLS_DIR / "start_server.cmd").read_text(encoding="utf-8")
        self.assertIn("First-time setup", configure_cmd)
        self.assertIn(":first_time", configure_cmd)
        self.assertIn(":setup_demo", configure_cmd)
        self.assertIn(":setup_mysql", configure_cmd)
        self.assertIn(":reconfigure", configure_cmd)
        self.assertIn(":import_cert", configure_cmd)
        self.assertIn("Generate self-signed certificate and pin", configure_cmd)
        self.assertIn("Rotate client pinned fingerprint", configure_cmd)
        self.assertIn("--set-client-tls", configure_cmd)
        self.assertIn("--show-cert", configure_cmd)
        self.assertIn("kt_root_pub.bin", configure_cmd)
        self.assertIn("Validate configuration", start_cmd)
        self.assertIn("Stress test server", start_cmd)
        self.assertIn("if errorlevel 1 exit /b 0", start_cmd)

    def test_non_interactive_demo_generation_validates_secure_defaults(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mi-e2ee-server-tools-") as temp:
            output = Path(temp) / "config.ini"
            result = self.run_script(
                "--non-interactive",
                "--mode",
                "demo",
                "--output",
                str(output),
            )
            self.assertEqual(result.returncode, 0, msg=f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}")
            parser = configparser.ConfigParser()
            parser.read(output, encoding="utf-8")
            self.assertEqual(parser.get("mode", "mode"), "1")
            self.assertEqual(parser.get("server", "list_port"), "9000")
            self.assertEqual(parser.get("server", "debug_log"), "0")
            self.assertEqual(parser.get("server", "tls_enable"), "1")
            self.assertEqual(parser.get("server", "require_tls"), "1")
            self.assertEqual(parser.get("kcp", "enable"), "0")
            validation = verify.validate_config(output, privacy_strict=True)
            self.assertTrue(validation.ok, msg=validation.errors)

    def test_non_interactive_mysql_generation_validates_secure_defaults(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mi-e2ee-server-tools-") as temp:
            output = Path(temp) / "config.ini"
            result = self.run_script(
                "--non-interactive",
                "--mode",
                "mysql",
                "--output",
                str(output),
                "--mysql-host",
                "db.internal",
                "--mysql-port",
                "3307",
                "--mysql-db",
                "mi_prod",
                "--mysql-user",
                "mi_service",
                "--mysql-password",
                "not_written_to_repo",
            )
            self.assertEqual(result.returncode, 0, msg=f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}")
            parser = self.read_config(output)
            self.assertEqual(parser.get("mode", "mode"), "0")
            self.assertEqual(parser.get("mysql", "mysql_ip"), "db.internal")
            self.assertEqual(parser.get("mysql", "mysql_port"), "3307")
            self.assertEqual(parser.get("mysql", "mysql_database"), "mi_prod")
            self.assertEqual(parser.get("mysql", "mysql_username"), "mi_service")
            self.assertEqual(parser.get("server", "tls_enable"), "1")
            self.assertEqual(parser.get("server", "require_tls"), "1")
            validation = verify.validate_config(output, privacy_strict=True)
            self.assertTrue(validation.ok, msg=validation.errors)

    def test_import_certificate_updates_server_tls_cert_and_client_hybrid_mode(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mi-e2ee-server-tools-") as temp:
            root = self.make_temp_tool_root(temp)
            config = root / "config" / "config.ini"
            bootstrap = subprocess.run(
                [
                    str(root / "tools" / "configure_server.sh"),
                    "--non-interactive",
                    "--mode",
                    "demo",
                    "--output",
                    str(config),
                ],
                cwd=root,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(bootstrap.returncode, 0, msg=bootstrap.stderr)
            imported = root / "server-chain.pem"
            imported.write_bytes(b"MI_E2EE_TEST_IMPORTED_CERT\n")
            ca_bundle = root / "client-ca.pem"
            ca_bundle.write_bytes(b"MI_E2EE_TEST_CA\n")
            client_config = root / "client" / "client_config.ini"

            result = self.run_temp_configure(
                root,
                "\n".join(
                    [
                        "4",
                        "2",
                        str(imported),
                        "3",
                        str(client_config),
                        str(ca_bundle),
                        "2",
                    ]
                )
                + "\n",
            )

            self.assertEqual(result.returncode, 0, msg=f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}")
            server_parser = self.read_config(config)
            self.assertEqual(server_parser.get("server", "tls_cert"), "config/mi_e2ee_server.pem")
            self.assertEqual((root / "config" / "mi_e2ee_server.pem").read_bytes(), imported.read_bytes())
            client_parser = self.read_config(client_config)
            expected_pin = hashlib.sha256(imported.read_bytes()).hexdigest()
            self.assertEqual(client_parser.get("client", "use_tls"), "1")
            self.assertEqual(client_parser.get("client", "require_tls"), "1")
            self.assertEqual(client_parser.get("client", "tls_verify_mode"), "hybrid")
            self.assertEqual(client_parser.get("client", "require_pinned_fingerprint"), "0")
            self.assertEqual(client_parser.get("client", "pinned_fingerprint"), expected_pin)
            self.assertEqual(client_parser.get("client", "tls_ca_bundle_path"), str(ca_bundle))

    def test_reconfigure_updates_listen_port_and_ops_health(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mi-e2ee-server-tools-") as temp:
            root = self.make_temp_tool_root(temp)
            config = root / "config" / "config.ini"
            bootstrap = subprocess.run(
                [
                    str(root / "tools" / "configure_server.sh"),
                    "--non-interactive",
                    "--mode",
                    "demo",
                    "--output",
                    str(config),
                ],
                cwd=root,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(bootstrap.returncode, 0, msg=bootstrap.stderr)

            port_result = self.run_temp_configure(root, "2\n2\n9443\n2\n")
            self.assertEqual(port_result.returncode, 0, msg=port_result.stderr)
            ops_result = self.run_temp_configure(root, "2\n5\n2\n2\n")
            self.assertEqual(ops_result.returncode, 0, msg=ops_result.stderr)

            parser = self.read_config(config)
            self.assertEqual(parser.get("server", "list_port"), "9443")
            self.assertEqual(parser.get("server", "ops_enable"), "1")
            self.assertEqual(parser.get("server", "ops_allow_remote"), "0")
            self.assertGreaterEqual(len(parser.get("server", "ops_token")), 16)
            self.assertTrue(list((root / "config").glob("config.ini.bak.*")))
            validation = verify.validate_config(config, privacy_strict=True)
            self.assertTrue(validation.ok, msg=validation.errors)

    def test_kt_menu_generates_rotates_and_copies_public_key(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mi-e2ee-server-tools-") as temp:
            root = self.make_temp_tool_root(temp)
            client_dir = root / "client-config"
            generate = self.run_temp_configure(root, f"6\n1\n1\n{client_dir}\n2\n")
            self.assertEqual(generate.returncode, 0, msg=f"stdout:\n{generate.stdout}\nstderr:\n{generate.stderr}")
            signing_key = root / "config" / "kt_signing_key.bin"
            root_pub = root / "config" / "kt_root_pub.bin"
            self.assertTrue(signing_key.exists())
            self.assertTrue(root_pub.exists())
            self.assertEqual((client_dir / "kt_root_pub.bin").read_bytes(), root_pub.read_bytes())
            before = signing_key.read_bytes()

            rotate = self.run_temp_configure(root, "6\n2\nOVERWRITE\n2\n2\n")
            self.assertEqual(rotate.returncode, 0, msg=f"stdout:\n{rotate.stdout}\nstderr:\n{rotate.stderr}")
            self.assertNotEqual(before, signing_key.read_bytes())

    def test_validation_rejects_tls_mismatch_and_mysql_placeholders(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mi-e2ee-server-tools-") as temp:
            config = Path(temp) / "bad.ini"
            config.write_text(
                "\n".join(
                    [
                        "[mode]",
                        "mode=0",
                        "[mysql]",
                        "mysql_ip=127.0.0.1",
                        "mysql_port=3306",
                        "mysql_database=mi_e2ee",
                        "mysql_username=change_me_user",
                        "mysql_password=change_me_strong_password",
                        "[server]",
                        "list_port=9000",
                        "offline_dir=database/offline_store",
                        "tls_enable=0",
                        "require_tls=1",
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
            validation = verify.validate_config(config, privacy_strict=True)
            self.assertFalse(validation.ok)
            self.assertIn("server.require_tls=1 requires server.tls_enable=1", validation.errors)
            self.assertTrue(any("mysql.mysql_username" in error for error in validation.errors))
            self.assertTrue(any("mysql.mysql_password" in error for error in validation.errors))

    def test_pin_rotation_updates_client_config_and_normalizes_pin(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mi-e2ee-server-tools-") as temp:
            client_config = Path(temp) / "client_config.ini"
            old_pin = "0" * 64
            new_pin = "AA:" + ":".join(["bb"] * 31)
            client_config.write_text(
                "[client]\n"
                "server_ip=127.0.0.1\n"
                "use_tls=0\n"
                f"pinned_fingerprint={old_pin}\n"
                "tls_verify_mode=ca\n",
                encoding="utf-8",
            )
            verify.rotate_client_pin(client_config, new_pin)
            parser = configparser.ConfigParser()
            parser.read(client_config, encoding="utf-8")
            self.assertEqual(parser.get("client", "use_tls"), "1")
            self.assertEqual(parser.get("client", "require_tls"), "1")
            self.assertEqual(parser.get("client", "require_pinned_fingerprint"), "1")
            self.assertEqual(parser.get("client", "tls_verify_mode"), "ca")
            self.assertEqual(parser.get("client", "pinned_fingerprint"), "aa" + "bb" * 31)

    def test_update_client_tls_config_supports_pin_ca_hybrid_and_preserves_trust_store(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mi-e2ee-client-tls-") as temp:
            client_config = Path(temp) / "client_config.ini"
            pin = "11" * 32
            ca_bundle = str(Path(temp) / "ca.pem")
            client_config.write_text(
                "[client]\n"
                "server_ip=127.0.0.1\n"
                "trust_store=server_trust.ini\n",
                encoding="utf-8",
            )

            verify.update_client_tls_config(client_config, "pin", fingerprint=pin)
            parser = self.read_config(client_config)
            self.assertEqual(parser.get("client", "trust_store"), "server_trust.ini")
            self.assertEqual(parser.get("client", "tls_verify_mode"), "pin")
            self.assertEqual(parser.get("client", "require_pinned_fingerprint"), "1")
            self.assertEqual(parser.get("client", "pinned_fingerprint"), pin)
            self.assertEqual(parser.get("client", "tls_ca_bundle_path"), "")

            verify.update_client_tls_config(client_config, "ca", ca_bundle_path=ca_bundle)
            parser = self.read_config(client_config)
            self.assertEqual(parser.get("client", "trust_store"), "server_trust.ini")
            self.assertEqual(parser.get("client", "tls_verify_mode"), "ca")
            self.assertEqual(parser.get("client", "require_pinned_fingerprint"), "0")
            self.assertEqual(parser.get("client", "pinned_fingerprint"), "")
            self.assertEqual(parser.get("client", "tls_ca_bundle_path"), ca_bundle)

            verify.update_client_tls_config(client_config, "hybrid", fingerprint=pin, ca_bundle_path=ca_bundle)
            parser = self.read_config(client_config)
            self.assertEqual(parser.get("client", "trust_store"), "server_trust.ini")
            self.assertEqual(parser.get("client", "tls_verify_mode"), "hybrid")
            self.assertEqual(parser.get("client", "require_pinned_fingerprint"), "0")
            self.assertEqual(parser.get("client", "pinned_fingerprint"), pin)
            self.assertEqual(parser.get("client", "tls_ca_bundle_path"), ca_bundle)

    def test_stress_dry_run_does_not_require_network(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mi-e2ee-stress-") as temp:
            result = subprocess.run(
                [
                    sys.executable,
                    str(TOOLS_DIR / "stress_server.py"),
                    "--host",
                    "127.0.0.1",
                    "--port",
                    "9000",
                    "--connections",
                    "2",
                    "--messages",
                    "3",
                    "--output-dir",
                    temp,
                    "--dry-run",
                ],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertIn("stress plan:", result.stdout)
            self.assertIn("connections=2", result.stdout)
            self.assertIn("dry_run=1", result.stdout)
            self.assertTrue(list(Path(temp).glob("*.json")))


if __name__ == "__main__":
    unittest.main()
