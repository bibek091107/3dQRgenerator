#!/usr/bin/env bash
# Install frontend dependencies.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

info "Installing frontend dependencies"
cd "$FRONTEND_DIR"
npm install
info "Frontend ready"