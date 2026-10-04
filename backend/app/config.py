"""Application configuration.

The output root points at the *desktop* app's folder so that files written by
the web application land in exactly the same place the original application
wrote them:

    <output root>/output/holder/<ID>.stl|.png
    <output root>/output/tools/<ID>.stl|.png

(That directory is the same one holding the pre-existing H001.png / T01.png /
T001.png produced by the desktop app.)
"""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

# <repo>/backend/app/config.py -> <repo>
REPO_ROOT = Path(__file__).resolve().parents[2]


def _env_path(name: str, default: Path) -> Path:
    raw = os.environ.get(name)
    return Path(raw).expanduser().resolve() if raw else default

try:
    from dotenv import load_dotenv
    env_file = REPO_ROOT / ".env"
    if env_file.exists():
        load_dotenv(env_file)
except ImportError:
    pass


@dataclass(frozen=True)
class Settings:
    # Where "output/holder" and "output/tools" live.
    output_root: Path

    # Path to the compiled C++ engine CLI (qr_engine).
    engine_binary: Path

    # PostgreSQL connection string.
    database_url: str

    # Seconds before an engine subprocess call is considered hung.
    engine_timeout: float

    @property
    def holder_dir(self) -> Path:
        return self.output_root / "output" / "holder"

    @property
    def tools_dir(self) -> Path:
        return self.output_root / "output" / "tools"

    @property
    def cache_dir(self) -> Path:
        return self.output_root / ".webcache"


def load_settings() -> Settings:
    default_engine = REPO_ROOT / "engine" / "build" / "bin" / "qr_engine"
    if not default_engine.exists():
        # Fall back to the copy built through the desktop app's CMake.
        default_engine = REPO_ROOT / "3DQRGenerator" / "build" / "bin" / "qr_engine"

    is_vercel = os.environ.get("VERCEL") == "1"
    default_output = Path("/tmp") if is_vercel else (REPO_ROOT / "3DQRGenerator")

    # On Vercel there is no local PostgreSQL — DATABASE_URL must be set explicitly
    # in the Vercel project's Production Environment Variables to the Supabase
    # Transaction Pooler connection string, e.g.:
    #   postgresql+psycopg://postgres.[ref]:[password]@aws-0-[region].pooler.supabase.com:6543/postgres
    database_url = os.environ.get("DATABASE_URL")

    # Strip accidental leading/trailing whitespace (e.g. "DATABASE_URL= postgresql://...")
    if database_url:
        database_url = database_url.strip()

    if not database_url:
        if is_vercel:
            raise RuntimeError(
                "DATABASE_URL environment variable is not set. "
                "Add it in the Vercel project's Production Environment Variables "
                "using the Supabase Transaction Pooler connection string: "
                "postgresql+psycopg://postgres.[ref]:[password]@aws-0-[region].pooler.supabase.com:6543/postgres"
            )
        # Local development fallback.
        database_url = "postgresql+psycopg://localhost/qrgenerator"

    # Normalise the URL scheme for SQLAlchemy + psycopg v3.
    # Plain "postgresql://" or "postgres://" are not valid driver specifiers for
    # psycopg v3 with SQLAlchemy — they must be "postgresql+psycopg://".
    # This lets the Vercel env var (or local .env) use either form safely.
    for plain in ("postgresql://", "postgres://"):
        if database_url.startswith(plain):
            database_url = "postgresql+psycopg://" + database_url[len(plain):]
            break

    return Settings(
        output_root=_env_path("QR_OUTPUT_ROOT", default_output),
        engine_binary=_env_path("QR_ENGINE_BIN", default_engine),
        database_url=database_url,
        engine_timeout=float(os.environ.get("QR_ENGINE_TIMEOUT", "120")),
    )


settings = load_settings()
