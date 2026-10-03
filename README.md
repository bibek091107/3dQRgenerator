# 3D QR Code Generator

Generates scannable 3D QR codes for **Holder** and **Tool** tags used in CNC
tooling management, with a live WebGL preview and STL/PNG export.

The project exists in two forms that share one C++ engine:

| | |
|---|---|
| `3DQRGenerator/` | Original native desktop app (C++ / OpenGL / Dear ImGui) |
| `frontend/` + `backend/` | Web application (Next.js / FastAPI / PostgreSQL) |
| `engine/` | Shared C++ core — QR, payload, validation, mesh, STL, PNG |

The web app does **not** reimplement any QR or export logic. It renders the
3D preview with React Three Fiber but calls the engine binary for every
byte-exact operation, so both apps produce identical files.

## Architecture

```
frontend (Next.js 16, React 19, three/R3F)
    │  HTTP/JSON
    ▼
backend (FastAPI, async SQLAlchemy)
    │  subprocess, line-delimited JSON
    ▼
engine/build/bin/qr_engine (C++17)
    │
    ├── QR encoding      → payload text
    ├── mesh building    → .mesh preview buffers (9 floats/vertex)
    ├── STL export       → binary STL
    └── PNG export       → 2D tag image

PostgreSQL stores batch/object metadata only. Generated files live on disk.
```

### Canonical commands (engine CLI)

| Command | Purpose |
|---|---|
| `ping` | liveness |
| `validate` | trim → required → duplicate ID → numeric validation |
| `generate` | write payload + QR mesh to disk |
| `export` | binary STL or PNG, with overwrite reporting |
| `mesh` | 9-float interleaved preview buffer |
| `exists` | filesystem ID check |
| `paths` | resolved output paths for a type |

## Prerequisites

- CMake 3.16+ and a C++17 compiler
- Python 3.11+
- PostgreSQL 14+
- Node.js 20+

## Setup

```bash
scripts/build_engine.sh        # shared engine
scripts/setup_backend.sh       # venv, deps, .env, database
scripts/setup_frontend.sh      # npm install
```

`scripts/setup_backend.sh` copies `backend/.env.example` to `backend/.env`.
Adjust `DATABASE_URL` if your PostgreSQL role differs from your OS username.

## Running

Start each piece in its own terminal:

```bash
scripts/run_desktop.sh         # native desktop app
scripts/run_backend.sh         # API on http://127.0.0.1:8000
scripts/run_frontend.sh        # web app on http://127.0.0.1:3000
scripts/run_frontend.sh prod   # production build + next start
```

The frontend calls `NEXT_PUBLIC_API_BASE` (default `http://127.0.0.1:8000`).
Because `NEXT_PUBLIC_*` values are inlined at build time, set it **before**
`next build` when the API is not on localhost.

The backend allows CORS only from `localhost:3000` and `127.0.0.1:3000`
(`backend/app/main.py`).

## Tests

```bash
scripts/test_engine.sh         # 98 fidelity checks: payloads, validation, paths, QR, mesh, STL/PNG
scripts/test_backend.sh        # 27 API tests against a throwaway database
scripts/test_ui.sh             # 16-section browser acceptance run against a clean stack
scripts/test_all.sh            # engine + desktop build + backend + frontend typecheck/lint/build
```

The backend suite uses the `qrgenerator_test` database (override with
`QR_TEST_DB`) and a temporary output root, so it never touches real output.
`ASGITransport` does not run the FastAPI lifespan, so `tests/conftest.py`
initialises the schema itself.

`scripts/test_ui.sh` resets that database, builds and starts the frontend,
drives the real app in Chromium (`frontend/tests/e2e/acceptance.js`), and then
shuts everything down. It installs Playwright into a temp directory on first
run. Screenshots land in `$SHOT_DIR` (default `$TMPDIR/qr_ui_shots`).

> Note: `page.screenshot()` returns an all-black image in this headless
> environment, so the suite asserts against the DOM and the live WebGL context
> rather than screenshot pixels. Rendering is verified by instrumenting
> `drawElements`/`drawArrays` and checking for a clean `gl.getError()`.

## Output layout

Both apps write into the same tree, rooted at `QR_OUTPUT_ROOT`
(default `3DQRGenerator/`):

```
3DQRGenerator/output/holder/<ID>.stl
3DQRGenerator/output/holder/<ID>.png
3DQRGenerator/output/tools/<ID>.stl
3DQRGenerator/output/tools/<ID>.png
3DQRGenerator/output/<holder|tools>/<type>_<ID>.mesh   (preview cache)
```

Preview mesh caches are namespaced by type so a Holder and a Tool may share an
ID without colliding.

## Size choices

| Choice | Matrix | Depth | Note |
|---|---|---|---|
| 1 | 9 × 9 | 0.8 mm | default |
| 2 | 7 × 7 | 1.0 mm | |
| 3 | 5 × 5 | 1.2 mm | |
| 4 | 3 × 3 | 1.5 mm | warning: 3 mm is too small to scan reliably |

## Behaviour notes

- **The 3D tag is fully interactive.** Drag to orbit, scroll to zoom, right-drag
  to pan. The control panel is a HUD that only captures pointer events over
  itself, so the rest of the viewport belongs to the canvas.
- **Exports land on the operator's device.** The engine writes the canonical
  copy into the shared server-side output folder, then the browser pulls that
  same byte-exact file into its own Downloads folder under the engine's
  filename (`<qrId>.stl` / `<qrId>.png`). The success popup names what was saved.
  Batch exports download one file per object.
- **ID uniqueness** is checked within the active batch, then against files
  already on disk for the same type. Overwriting is never implicit: the UI
  prompts, and the API reports `file_exists`.
- **Validation order** matches the desktop app exactly: empty ID/Name →
  duplicate ID → numeric validation.
- **Empty optional fields** become `N/A` in the payload.
- **Numeric parsing** mirrors the original `std::stod` in a try/catch, so
  `"12abc"` parses as `12`.
- **Completed Life** may not exceed **Tool Life**.
- Tool QR IDs may reuse a Holder ID; the two types are independent.

## Repository layout

```
engine/          C++ core + CLI + fidelity tests
3DQRGenerator/   native desktop app (links qr_engine_core)
backend/         FastAPI service, SQLAlchemy models, API tests
frontend/        Next.js app, R3F preview, API client
frontend/tests/  browser acceptance suite
scripts/         build/run/test helpers
```

## Troubleshooting

| Symptom | Cause |
|---|---|
| `engine CLI not built` | run `scripts/build_engine.sh` |
| `frontend dependencies missing` | run `scripts/setup_frontend.sh` |
| API 422 `duplicate_id` | ID is used in this batch or already on disk |
| API 422 `empty_field` | ID or Name is blank after trimming |
| Export returns `file_exists` | respond to the overwrite prompt or delete the file |
| Blank preview | WebGL unavailable; verify the browser has hardware acceleration |