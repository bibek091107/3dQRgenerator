#!/usr/bin/env bash
# End-to-end UI verification.
#
# Boots a clean backend + production frontend against a throwaway database and
# output tree, drives the real app in Chromium, then shuts everything down.
#
# Requires: playwright (installed on demand into a temp dir) and a built engine.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

ensure_engine

WEB_PORT="${WEB_PORT:-3000}"
API_PORT="${API_PORT:-8000}"
TEST_DB="${QR_TEST_DB:-qrgenerator_test}"
TEST_OUT="${QR_TEST_OUTPUT_ROOT:-${TMPDIR:-/tmp}/qr_ui_test_output}"
SHOT_DIR="${SHOT_DIR:-${TMPDIR:-/tmp}/qr_ui_shots}"
PW_DIR="${TMPDIR:-/tmp}/qr_playwright"
LOG_DIR="${LOG_DIR:-${TMPDIR:-/tmp}/qr_ui_logs}"

export WEB_BASE="http://127.0.0.1:$WEB_PORT"
export API_BASE="http://127.0.0.1:$API_PORT"

PY="$(backend_python)"
PIDS=()
cleanup() {
  local code=$?
  for pid in "${PIDS[@]:-}"; do
    [[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
  done
  # `npm run start` and uvicorn spawn children that outlive the parent PID.
  pkill -f "next-server" 2>/dev/null || true
  pkill -f "uvicorn app.main:app --host 127.0.0.1 --port $API_PORT" 2>/dev/null || true
  wait 2>/dev/null || true
  exit "$code"
}
trap cleanup EXIT INT TERM

# ── throwaway database + output tree ──────────────────────────────────────
if command -v psql >/dev/null 2>&1; then
  if ! psql -lqt 2>/dev/null | cut -d'|' -f1 | grep -qw "$TEST_DB"; then
    info "Creating test database '$TEST_DB'"
    createdb "$TEST_DB"
  fi
  info "Resetting schema in '$TEST_DB'"
  psql -d "$TEST_DB" -c "DROP SCHEMA public CASCADE; CREATE SCHEMA public;" >/dev/null
fi
rm -rf "$TEST_OUT"
mkdir -p "$TEST_OUT" "$SHOT_DIR" "$LOG_DIR"

# ── playwright ────────────────────────────────────────────────────────────
if [[ ! -d "$PW_DIR/node_modules/playwright" ]]; then
  info "Installing playwright into $PW_DIR (one time)"
  mkdir -p "$PW_DIR"
  (cd "$PW_DIR" && npm install --silent playwright@latest) >"$LOG_DIR/playwright-install.log" 2>&1
fi

# ── backend ───────────────────────────────────────────────────────────────
info "Starting backend on $API_BASE"
(cd "$BACKEND_DIR" && DATABASE_URL="postgresql+psycopg://localhost/$TEST_DB" \
  QR_OUTPUT_ROOT="$TEST_OUT" "$PY" -m uvicorn app.main:app \
  --host 127.0.0.1 --port "$API_PORT" --log-level warning) \
  >"$LOG_DIR/backend.log" 2>&1 </dev/null &
PIDS+=($!)

# ── frontend (production build, matching the acceptance run) ──────────────
ensure_frontend_deps
info "Building frontend"
(cd "$FRONTEND_DIR" && npm run build) >"$LOG_DIR/frontend-build.log" 2>&1

info "Starting frontend on $WEB_BASE"
(cd "$FRONTEND_DIR" && npm run start -- --port "$WEB_PORT") \
  >"$LOG_DIR/frontend.log" 2>&1 </dev/null &
PIDS+=($!)

info "Waiting for services"
for _ in $(seq 1 60); do
  api_ok=0; web_ok=0
  curl -sf -o /dev/null --max-time 2 "$API_BASE/api/health" && api_ok=1
  curl -sf -o /dev/null --max-time 2 "$WEB_BASE" && web_ok=1
  [[ $api_ok -eq 1 && $web_ok -eq 1 ]] && break
  sleep 1
done
curl -sf -o /dev/null --max-time 2 "$API_BASE/api/health" \
  || die "backend did not become healthy at $API_BASE"
curl -sf -o /dev/null --max-time 2 "$WEB_BASE" \
  || die "frontend did not become healthy at $WEB_BASE"

info "Running UI acceptance suite (screenshots -> $SHOT_DIR)"
rm -f "$SHOT_DIR"/*.png 2>/dev/null || true
(cd "$PW_DIR" && NODE_PATH="$PW_DIR/node_modules" node "$FRONTEND_DIR/tests/e2e/acceptance.js")