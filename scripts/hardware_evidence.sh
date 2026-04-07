#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
usage:
  scripts/hardware_evidence.sh init <YYYYMMDD> <SCENARIO-ID>
  scripts/hardware_evidence.sh record <YYYYMMDD> <SCENARIO-ID> <LABEL> -- <command...>
  scripts/hardware_evidence.sh bootstrap <YYYYMMDD>

commands:
  init
    Create evidence/<YYYYMMDD>/<SCENARIO-ID>/ with:
      - commands.txt
      - result.json
      - artifacts/

  record
    Run a command, capture its stdout/stderr to artifacts/<timestamp>-<label>.log,
    and append the exact command, output log path, and exit code to commands.txt.

  bootstrap
    Capture a small environment snapshot under evidence/<YYYYMMDD>/ENV-BOOTSTRAP/
    using the same bounded logging flow.
EOF
}

die() {
  echo "error: $*" >&2
  exit 1
}

json_escape() {
  local value="${1:-}"
  value=${value//\\/\\\\}
  value=${value//\"/\\\"}
  value=${value//$'\n'/\\n}
  value=${value//$'\r'/\\r}
  value=${value//$'\t'/\\t}
  printf '%s' "${value}"
}

slugify() {
  local value="${1:-}"
  value=$(printf '%s' "${value}" | tr '[:upper:]' '[:lower:]')
  value=$(printf '%s' "${value}" | tr -cs 'a-z0-9._-' '-')
  value=${value#-}
  value=${value%-}
  if [[ -z "${value}" ]]; then
    value="step"
  fi
  printf '%s' "${value}"
}

repo_root() {
  git rev-parse --show-toplevel 2>/dev/null || pwd
}

current_branch() {
  git -C "$(repo_root)" branch --show-current 2>/dev/null || echo "unknown"
}

current_commit() {
  git -C "$(repo_root)" rev-parse --short HEAD 2>/dev/null || echo "unknown"
}

validate_date() {
  local date_value="${1:-}"
  [[ "${date_value}" =~ ^[0-9]{8}$ ]] || die "date must be YYYYMMDD, got: ${date_value}"
}

validate_scenario() {
  local scenario="${1:-}"
  [[ -n "${scenario}" ]] || die "scenario id is required"
  [[ "${scenario}" != *"/"* ]] || die "scenario id must not contain /"
}

scenario_dir() {
  local date_value="$1"
  local scenario="$2"
  printf '%s/evidence/%s/%s' "$(repo_root)" "${date_value}" "${scenario}"
}

commands_file() {
  local date_value="$1"
  local scenario="$2"
  printf '%s/commands.txt' "$(scenario_dir "${date_value}" "${scenario}")"
}

result_file() {
  local date_value="$1"
  local scenario="$2"
  printf '%s/result.json' "$(scenario_dir "${date_value}" "${scenario}")"
}

artifacts_dir() {
  local date_value="$1"
  local scenario="$2"
  printf '%s/artifacts' "$(scenario_dir "${date_value}" "${scenario}")"
}

ensure_scenario() {
  local date_value="$1"
  local scenario="$2"
  local root
  local commands_path
  local result_path

  validate_date "${date_value}"
  validate_scenario "${scenario}"

  root=$(scenario_dir "${date_value}" "${scenario}")
  commands_path=$(commands_file "${date_value}" "${scenario}")
  result_path=$(result_file "${date_value}" "${scenario}")

  mkdir -p "${root}" "$(artifacts_dir "${date_value}" "${scenario}")"

  if [[ ! -f "${commands_path}" ]]; then
    cat > "${commands_path}" <<EOF
scenario=${scenario}
date=${date_value}
repo_root=$(repo_root)
git_branch=$(current_branch)
git_commit=$(current_commit)
initialized_at_utc=$(date -u +'%Y-%m-%dT%H:%M:%SZ')

EOF
  fi

  if [[ ! -f "${result_path}" ]]; then
    cat > "${result_path}" <<EOF
{
  "scenario": "$(json_escape "${scenario}")",
  "date_utc": "$(date -u +'%Y-%m-%dT%H:%M:%SZ')",
  "result": "PENDING",
  "hardware": [],
  "firmware_commit": "$(json_escape "$(current_commit)")",
  "notes": "Fill in after execution.",
  "evidence": [
    "commands.txt"
  ]
}
EOF
  fi
}

append_command_header() {
  local date_value="$1"
  local scenario="$2"
  local label="$3"
  local output_rel="$4"
  shift 4

  {
    printf '[%s]\n' "$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
    printf 'label=%s\n' "${label}"
    printf 'cwd=%s\n' "$(pwd)"
    printf 'output_log=%s\n' "${output_rel}"
    printf '$'
    for arg in "$@"; do
      printf ' %q' "${arg}"
    done
    printf '\n'
  } >> "$(commands_file "${date_value}" "${scenario}")"
}

append_command_exit() {
  local date_value="$1"
  local scenario="$2"
  local rc="$3"
  printf '[exit %s]\n\n' "${rc}" >> "$(commands_file "${date_value}" "${scenario}")"
}

run_record() {
  local date_value="$1"
  local scenario="$2"
  local label="$3"
  shift 3

  [[ $# -gt 0 ]] || die "record requires a command after --"

  ensure_scenario "${date_value}" "${scenario}"

  local safe_label
  local artifact_name
  local artifact_path
  local artifact_rel
  local rc

  safe_label=$(slugify "${label}")
  artifact_name="$(date -u +'%Y%m%dT%H%M%SZ')-${safe_label}.log"
  artifact_path="$(artifacts_dir "${date_value}" "${scenario}")/${artifact_name}"
  artifact_rel="artifacts/${artifact_name}"

  append_command_header "${date_value}" "${scenario}" "${label}" "${artifact_rel}" "$@"

  set +e
  "$@" 2>&1 | tee "${artifact_path}"
  rc=${PIPESTATUS[0]}
  set -e

  append_command_exit "${date_value}" "${scenario}" "${rc}"
  return "${rc}"
}

run_bootstrap() {
  local date_value="$1"
  local scenario="ENV-BOOTSTRAP"

  ensure_scenario "${date_value}" "${scenario}"

  run_record "${date_value}" "${scenario}" "pwd" pwd || true
  run_record "${date_value}" "${scenario}" "branch" git -C "$(repo_root)" branch --show-current || true
  run_record "${date_value}" "${scenario}" "commit" git -C "$(repo_root)" rev-parse --short HEAD || true
  run_record "${date_value}" "${scenario}" "git-status" git -C "$(repo_root)" status --short --branch || true
  run_record "${date_value}" "${scenario}" "python-version" bash -lc 'command -v python && python --version' || true
  run_record "${date_value}" "${scenario}" "node-version" bash -lc 'command -v node && node --version' || true
  run_record "${date_value}" "${scenario}" "npm-version" bash -lc 'command -v npm && npm --version' || true
  run_record "${date_value}" "${scenario}" "idf-version" bash -lc 'command -v idf.py && idf.py --version' || true
  run_record "${date_value}" "${scenario}" "serial-ports" bash -lc "ls /dev/cu.* 2>/dev/null | rg 'usb|wch|modem|serial|SLAB|UART' || true" || true
}

main() {
  [[ $# -gt 0 ]] || {
    usage
    exit 1
  }

  local command="$1"
  shift

  case "${command}" in
    init)
      [[ $# -eq 2 ]] || die "init requires <YYYYMMDD> <SCENARIO-ID>"
      ensure_scenario "$1" "$2"
      ;;
    record)
      [[ $# -ge 4 ]] || die "record requires <YYYYMMDD> <SCENARIO-ID> <LABEL> -- <command...>"
      local date_value="$1"
      local scenario="$2"
      local label="$3"
      shift 3
      [[ "${1:-}" == "--" ]] || die "record requires -- before the command"
      shift
      run_record "${date_value}" "${scenario}" "${label}" "$@"
      ;;
    bootstrap)
      [[ $# -eq 1 ]] || die "bootstrap requires <YYYYMMDD>"
      run_bootstrap "$1"
      ;;
    -h|--help|help)
      usage
      ;;
    *)
      die "unknown command: ${command}"
      ;;
  esac
}

main "$@"
