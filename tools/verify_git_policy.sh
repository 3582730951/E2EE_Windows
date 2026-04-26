#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: verify_git_policy.sh [--changed-file-list PATH] [--branch NAME]

When --changed-file-list is omitted, changed paths are read from:
  git diff --name-only --diff-filter=ACMRTUXB origin/e2ee_dev...HEAD

When --branch is omitted, the current git branch is used.
EOF
}

changed_file_list=""
branch=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --changed-file-list)
      changed_file_list="${2:-}"
      shift 2
      ;;
    --branch)
      branch="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ -z "$branch" ]]; then
  branch="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || true)"
fi
if [[ "$branch" != "e2ee_dev" ]]; then
  echo "forbidden branch: $branch (expected e2ee_dev)" >&2
  exit 1
fi

read_changed_files() {
  if [[ -n "$changed_file_list" ]]; then
    if [[ ! -f "$changed_file_list" ]]; then
      echo "changed file list missing: $changed_file_list" >&2
      exit 1
    fi
    sed 's#\\#/#g' "$changed_file_list"
    return
  fi
  git diff --name-only --diff-filter=ACMRTUXB origin/e2ee_dev...HEAD |
    sed 's#\\#/#g'
}

is_readme_md() {
  local path="$1"
  local base
  base="$(basename "$path")"
  [[ "$base" == "README.md" ]]
}

is_allowed_txt() {
  local path="$1"
  local base
  base="$(basename "$path")"
  [[ "$base" == "CMakeLists.txt" ]]
}

while IFS= read -r path; do
  [[ -z "$path" ]] && continue
  path="${path#./}"
  lower="$(printf '%s' "$path" | tr '[:upper:]' '[:lower:]')"

  if [[ "$path" != core/* ]]; then
    echo "forbidden path outside core/: $path" >&2
    exit 1
  fi
  case "$path" in
    core/client/ui_example/*|core/client/ui_example)
      echo "forbidden ui_example path: $path" >&2
      exit 1
      ;;
    core/client/assets/ref/*|core/client/assets/ref)
      echo "forbidden reference asset path: $path" >&2
      exit 1
      ;;
  esac

  case "$lower" in
    *.md)
      if ! is_readme_md "$path"; then
        echo "forbidden markdown file: $path" >&2
        exit 1
      fi
      ;;
    *.txt)
      if ! is_allowed_txt "$path"; then
        echo "forbidden file type: $path" >&2
        exit 1
      fi
      ;;
    *.log|*.log.*|*.png)
      echo "forbidden file type: $path" >&2
      exit 1
      ;;
  esac
done < <(read_changed_files)

exit 0
