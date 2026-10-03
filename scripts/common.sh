#!/usr/bin/env bash
# Shared helpers for the repo scripts. Sourced, not executed.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_DIR="$REPO_ROOT/engine"
DESKTOP_DIR="$REPO_ROOT/3DQRGenerator"
BACKEND_DIR="$REPO_ROOT/backend"
FRONTEND_DIR="$REPO_ROOT/frontend"

BACKEND_VENV="$BACKEND_DIR/.venv"
PYTHON="$BACKEND_VENV/bin/python"

info() { printf '\033[1;36m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m==>\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m==>\033[0m %s\n' "$*" >&2; exit 1; }

# Prefer the project venv; fall back to whatever python3 is on PATH.
backend_python() {
  if [[ -x "$PYTHON" ]]; then
    printf '%s' "$PYTHON"
  else
    command -v python3 >/dev/null 2>&1 || die "python3 not found; create the backend venv first"
    printf '%s' "$(command -v python3)"
  fi
}

ensure_engine() {
  [[ -x "$ENGINE_DIR/build/bin/qr_engine" ]] \
    || die "engine CLI not built. Run: scripts/build_engine.sh"
}

ensure_frontend_deps() {
  [[ -d "$FRONTEND_DIR/node_modules" ]] \
    || die "frontend dependencies missing. Run: scripts/setup_frontend.sh"
}