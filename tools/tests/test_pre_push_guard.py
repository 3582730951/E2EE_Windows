import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
GUARD_SCRIPT = REPO_ROOT / "tools" / "pre_push_guard.py"


def git(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=repo,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def write_file(path: Path, content: str, *, binary: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if binary:
        path.write_bytes(content.encode("utf-8"))
    else:
        path.write_text(content, encoding="utf-8")


class PrePushGuardTest(unittest.TestCase):
    def make_repo(self) -> Path:
        temp_dir = Path(tempfile.mkdtemp(prefix="pre-push-guard-"))
        git(temp_dir, "init")
        git(temp_dir, "config", "user.name", "Codex")
        git(temp_dir, "config", "user.email", "codex@example.com")
        return temp_dir

    def commit_all(self, repo: Path, message: str) -> None:
        git(repo, "add", ".")
        git(repo, "commit", "-m", message)

    def run_guard(self, repo: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                "python3",
                str(GUARD_SCRIPT),
                "--repo",
                str(repo),
                "--base-ref",
                "HEAD~1",
                "--head-ref",
                "HEAD",
            ],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
        )

    def test_allows_code_resources_and_readme_push(self) -> None:
        repo = self.make_repo()
        write_file(repo / "README.md", "# demo\n")
        write_file(repo / "app" / "Info.plist", "<plist></plist>\n")
        write_file(repo / "app" / "strings.json", '{"hello": "world"}\n')
        write_file(
            repo / "src" / "main.cpp",
            textwrap.dedent(
                """
                int add(int left, int right) {
                    return left + right;
                }
                """
            ).strip()
            + "\n",
        )
        self.commit_all(repo, "base")

        write_file(repo / "README.md", "# demo\n\nupdated\n")
        write_file(repo / "app" / "Info.plist", "<plist><dict /></plist>\n")
        write_file(repo / "app" / "strings.json", '{"hello": "repo"}\n')
        write_file(
            repo / "src" / "main.cpp",
            textwrap.dedent(
                """
                int add(int left, int right) {
                    return left + right + 1;
                }
                """
            ).strip()
            + "\n",
        )
        self.commit_all(repo, "head")

        result = self.run_guard(repo)

        self.assertEqual(
            result.returncode,
            0,
            msg=f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )
        self.assertIn("push guard passed", result.stdout.lower())

    def test_blocks_changed_png_file(self) -> None:
        repo = self.make_repo()
        write_file(repo / "src" / "main.cpp", "int main() { return 0; }\n")
        self.commit_all(repo, "base")

        write_file(repo / "pngs" / "preview.png", "fakepng", binary=True)
        self.commit_all(repo, "head")

        result = self.run_guard(repo)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("preview.png", result.stdout)
        self.assertIn("forbidden", result.stdout.lower())

    def test_blocks_non_readme_markdown_and_plain_text(self) -> None:
        repo = self.make_repo()
        write_file(repo / "src" / "main.cpp", "int main() { return 0; }\n")
        self.commit_all(repo, "base")

        write_file(repo / "docs" / "plan.md", "# plan\n")
        write_file(repo / "notes.txt", "scratch\n")
        self.commit_all(repo, "head")

        result = self.run_guard(repo)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("docs/plan.md", result.stdout)
        self.assertIn("notes.txt", result.stdout)

    def test_blocks_logs_and_forbidden_asset_directories(self) -> None:
        repo = self.make_repo()
        write_file(repo / "src" / "main.cpp", "int main() { return 0; }\n")
        self.commit_all(repo, "base")

        write_file(repo / "debug.log.old", "secret-ish output\n")
        write_file(repo / "client" / "ui_example" / "demo.qml", "Item {}\n")
        write_file(repo / "client" / "assets" / "ref" / "palette.json", "{}\n")
        self.commit_all(repo, "head")

        result = self.run_guard(repo)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("debug.log.old", result.stdout)
        self.assertIn("client/ui_example/demo.qml", result.stdout)
        self.assertIn("client/assets/ref/palette.json", result.stdout)

    def test_allows_cmakelists_txt(self) -> None:
        repo = self.make_repo()
        write_file(repo / "src" / "main.cpp", "int main() { return 0; }\n")
        self.commit_all(repo, "base")

        write_file(repo / "src" / "CMakeLists.txt", "add_executable(app main.cpp)\n")
        self.commit_all(repo, "head")

        result = self.run_guard(repo)

        self.assertEqual(
            result.returncode,
            0,
            msg=f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )

    def test_detects_sensitive_token_literal(self) -> None:
        repo = self.make_repo()
        write_file(repo / "src" / "main.cpp", "int main() { return 0; }\n")
        self.commit_all(repo, "base")
        fake_pat = "github_" + "pat_abcdefghijklmnopqrstuvwxyz1234567890"

        write_file(
            repo / "src" / "main.cpp",
            textwrap.dedent(
                """
                const char* kGithubPat =
                    "__FAKE_PAT__";
                int main() { return kGithubPat[0] == '\\0'; }
                """
            ).replace("__FAKE_PAT__", fake_pat).strip()
            + "\n",
        )
        self.commit_all(repo, "head")

        result = self.run_guard(repo)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("[github_pat]", result.stdout)
        self.assertIn("gith...90", result.stdout)
        self.assertIn("sensitive", result.stdout.lower())


if __name__ == "__main__":
    unittest.main()
