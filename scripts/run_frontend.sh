#!/usr/bin/env bash
# Start the Next.js frontend on http://127.0.0.1:3000
#
# NEXT_PUBLIC_* values are inlined at build time, so set them before `next build`
# if the API is not on http://127.0.0.1:8000.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

ensure_frontend_deps
cd "$FRONTEND_DIR"

MODE="${1:-dev}"
PORT="${FRONTEND_PORT:-3000}"

if [[ "$MODE" == "prod" ]]; then
  info "Building frontend for production"
  npm run build
  info "Starting frontend (production) on http://127.0.0.1:$PORT"
  exec npm run start -- --port "$PORT"
fi

info "Starting frontend (dev) on http://127.0.0.1:$PORT"
exec npm run dev -- --port "$PORT"