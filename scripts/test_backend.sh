#!/usr/bin/env bash
# Run the backend API test suite against a throwaway database and output folder.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

ensure_engine
PY="$(backend_python)"

TEST_DB="${QR_TEST_DB:-qrgenerator_test}"
TEST_OUT="${QR_TEST_OUTPUT_ROOT:-${TMPDIR:-/tmp}/qr_engine_test_output}"

# ASGITransport skips the FastAPI lifespan, so conftest initialises the schema.
# Use a dedicated database so tests never touch development data.
if command -v createdb >/dev/null 2>&1 \
   && ! psql -lqt 2>/dev/null | cut -d'|' -f1 | grep -qw "$TEST_DB"; then
  info "Creating test database '$TEST_DB'"
  createdb "$TEST_DB"
fi

info "Running backend tests (db=$TEST_DB out=$TEST_OUT)"
cd "$BACKEND_DIR"
QR_OUTPUT_ROOT="$TEST_OUT" \
DATABASE_URL="${QR_TEST_DATABASE_URL:-postgresql+psycopg://localhost/$TEST_DB}" \
  "$PY" -m pytest "$@"