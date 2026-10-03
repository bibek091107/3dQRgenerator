#!/usr/bin/env bash
# Configure and build the shared C++ engine (qr_engine_core, qr_engine, engine_tests).
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

info "Configuring engine"
cmake -S "$ENGINE_DIR" -B "$ENGINE_DIR/build" -DCMAKE_BUILD_TYPE=Release

info "Building engine"
cmake --build "$ENGINE_DIR/build" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

info "Built:"
ls -1 "$ENGINE_DIR/build/bin"