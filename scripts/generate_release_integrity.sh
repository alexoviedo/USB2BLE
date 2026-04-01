#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <artifact-dir> <artifact-name>"
  exit 1
fi

ARTIFACT_DIR="$1"
ARTIFACT_NAME="$2"

if [[ ! -d "${ARTIFACT_DIR}" ]]; then
  echo "artifact directory not found: ${ARTIFACT_DIR}"
  exit 1
fi

pushd "${ARTIFACT_DIR}" >/dev/null
find . -type f ! -name SHA256SUMS ! -name provenance.json -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
popd >/dev/null

cat > "${ARTIFACT_DIR}/provenance.json" <<EOF
{
  "artifact_name": "${ARTIFACT_NAME}",
  "artifact_path": "${ARTIFACT_DIR}",
  "commit_sha": "${GITHUB_SHA:-unknown}",
  "workflow": "${GITHUB_WORKFLOW:-unknown}",
  "run_id": "${GITHUB_RUN_ID:-unknown}",
  "run_attempt": "${GITHUB_RUN_ATTEMPT:-unknown}",
  "actor": "${GITHUB_ACTOR:-unknown}",
  "generated_at": "$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
}
EOF

echo "Generated integrity outputs for ${ARTIFACT_NAME}:"
echo " - ${ARTIFACT_DIR}/SHA256SUMS"
echo " - ${ARTIFACT_DIR}/provenance.json"
