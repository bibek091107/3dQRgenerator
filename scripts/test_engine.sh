#!/usr/bin/env bash
# Run the engine fidelity suite (payloads, validation, paths, QR, mesh, STL/PNG).
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

ensure_engine
"$ENGINE_DIR/build/bin/engine_tests"