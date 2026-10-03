#!/usr/bin/env bash
# Start the FastAPI backend on http://127.0.0.1:8000
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

ensure_engine
PY="$(backend_python)"

cd "$BACKEND_DIR"
info "Starting backend on http://127.0.0.1:${BACKEND_PORT:-8000}"
exec "$PY" -m uvicorn app.main:app \
  --host 127.0.0.1 \
  --port "${BACKEND_PORT:-8000}" \
  --reload