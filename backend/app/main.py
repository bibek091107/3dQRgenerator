"""FastAPI application entry point."""

from __future__ import annotations

import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy import text

from . import engine_bridge, storage
from .config import settings
from .database import SessionLocal, engine, init_models
from .routers import batches, engine as engine_router, objects

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
log = logging.getLogger("qrbackend")


@asynccontextmanager
async def lifespan(app: FastAPI):
    storage.ensure_output_dirs()
    try:
        await init_models()
        log.info("PostgreSQL schema ready")
    except Exception as exc:  # pragma: no cover - startup diagnostics
        log.error("Database unavailable: %s", exc)

    if settings.engine_binary.exists():
        log.info("C++ engine binary: %s", settings.engine_binary)
    else:
        log.warning(
            "C++ engine binary missing at %s — build it with "
            "`cmake -S engine -B engine/build && cmake --build engine/build`",
            settings.engine_binary,
        )
    log.info("Output root: %s", settings.output_root)
    yield
    await engine.dispose()


app = FastAPI(
    title="3D QR Generator API",
    description=(
        "Web backend for the existing C++ 3D QR / STL generator. "
        "All QR payloads, QR matrices, 3D meshes, STL and PNG output come from "
        "the original C++ engine."
    ),
    version="1.0.0",
    lifespan=lifespan,
)

import os

# Origins allowed to call the API. FRONTEND_URL holds the deployed
# Tools Management 2 frontend origin (comma-separated if there is more than
# one, e.g. a custom domain plus the *.vercel.app alias). It is set in the
# Vercel project's Production Environment Variables; the localhost entries stay
# for `next dev`.
_allowed_origins = [
    "http://localhost:3000",
    "http://127.0.0.1:3000",
]
for _raw in (os.environ.get("FRONTEND_URL") or "").split(","):
    _origin = _raw.strip().rstrip("/")
    if _origin and _origin not in _allowed_origins:
        _allowed_origins.append(_origin)

log.info("CORS allow_origins: %s", _allowed_origins)

app.add_middleware(
    CORSMiddleware,
    allow_origins=_allowed_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
    # Without this the browser hides Content-Disposition from the frontend's
    # fetch(), so a device download would fall back to a generic filename
    # instead of the engine's "<qrId>.<ext>".
    expose_headers=["Content-Disposition"],
)

from .routers import supabase_sync
app.include_router(supabase_sync.router)
app.include_router(engine_router.router)
app.include_router(batches.router)
app.include_router(objects.router)


@app.get("/api/health")
async def health() -> dict:
    db_ok = True
    db_error = None
    try:
        async with SessionLocal() as session:
            await session.execute(text("SELECT 1"))
    except Exception as exc:
        db_ok = False
        db_error = str(exc)

    engine_ok = settings.engine_binary.exists()

    # Redact credentials from the URL before returning it in the response.
    import re
    safe_url = re.sub(r"://[^@]+@", "://<redacted>@", settings.database_url)

    return {
        "status": "ok" if db_ok else "degraded",
        "database": {"ok": db_ok, "error": db_error, "url": safe_url},
        "engine": {
            "ok": engine_ok,
            "binary": str(settings.engine_binary),
        },
        "outputRoot": str(settings.output_root),
    }
