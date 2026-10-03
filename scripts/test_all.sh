#!/usr/bin/env bash
# Run every automated check: engine fidelity suite, desktop build,
# backend API tests, and frontend typecheck/lint/production build.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

info "1/4 Engine tests"
"$REPO_ROOT/scripts/test_engine.sh"

info "2/4 Desktop build"
cmake -S "$DESKTOP_DIR" -B "$DESKTOP_DIR/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$DESKTOP_DIR/build" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" >/dev/null
echo "    desktop binary built"

info "3/4 Backend tests"
"$REPO_ROOT/scripts/test_backend.sh" -q

info "4/4 Frontend checks"
ensure_frontend_deps
cd "$FRONTEND_DIR"
npx tsc --noEmit
npm run lint
npm run build >/dev/null
echo "    frontend typecheck, lint and build passed"

info "All checks passed"