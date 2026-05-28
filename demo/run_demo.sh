#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build"
PORT=5555
DATA_DIR="${ROOT}/data/demo_run"
SERVER_PID=""

cleanup() {
  if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

if [[ ! -x "${BUILD}/server/server" ]] || [[ ! -x "${BUILD}/client/client" ]]; then
  echo "Build server and client first:"
  echo "  cmake -S \"${ROOT}\" -B \"${BUILD}\""
  echo "  cmake --build \"${BUILD}\" --target server client -j4"
  exit 1
fi

rm -rf "${DATA_DIR}"
mkdir -p "${DATA_DIR}"

"${BUILD}/server/server" "${PORT}" "${DATA_DIR}" &
SERVER_PID=$!
sleep 1

echo "=== setup.sql ==="
"${BUILD}/client/client" "${PORT}" "${ROOT}/demo/sql/setup.sql"

echo "=== queries.sql ==="
"${BUILD}/client/client" "${PORT}" "${ROOT}/demo/sql/queries.sql"

echo "Demo finished successfully."
