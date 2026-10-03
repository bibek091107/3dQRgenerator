#!/usr/bin/env bash
# Build and launch the original native desktop application.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

if [[ ! -d "$ENGINE_DIR/build" ]]; then
  info "Engine not configured yet; building it first"
  "$REPO_ROOT/scripts/build_engine.sh"
fi

info "Configuring desktop app"
cmake -S "$DESKTOP_DIR" -B "$DESKTOP_DIR/build" -DCMAKE_BUILD_TYPE=Release

info "Building desktop app"
cmake --build "$DESKTOP_DIR/build" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

BIN="$DESKTOP_DIR/build/bin/3DQRGenerator"
[[ -x "$BIN" ]] || die "desktop binary not found at $BIN"
info "Launching $BIN"
exec "$BIN" "$@"