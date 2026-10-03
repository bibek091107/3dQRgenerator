#!/usr/bin/env bash
# Create the backend virtualenv, install dependencies, and prepare the database.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

info "Creating virtualenv at backend/.venv"
python3 -m venv "$BACKEND_DIR/.venv"

info "Installing backend dependencies"
"$BACKEND_VENV/bin/pip" install --upgrade pip >/dev/null
"$BACKEND_VENV/bin/pip" install -r "$BACKEND_DIR/requirements.txt"

if [[ ! -f "$BACKEND_DIR/.env" ]]; then
  cp "$BACKEND_DIR/.env.example" "$BACKEND_DIR/.env"
  info "Created backend/.env from .env.example"
fi

DB_NAME="${QR_DB_NAME:-qrgenerator}"
if command -v createdb >/dev/null 2>&1; then
  if psql -lqt 2>/dev/null | cut -d'|' -f1 | grep -qw "$DB_NAME"; then
    info "Database '$DB_NAME' already exists"
  else
    info "Creating database '$DB_NAME'"
    createdb "$DB_NAME"
  fi
else
  warn "createdb not available; create the database manually if the app cannot connect"
fi

info "Backend ready. Start it with: scripts/run_backend.sh"