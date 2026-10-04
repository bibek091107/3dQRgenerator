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

import logging
import os
from dataclasses import dataclass
from pathlib import Path

# <repo>/backend/app/config.py -> <repo>
REPO_ROOT = Path(__file__).resolve().parents[2]

# On Vercel (rootDirectory=backend), the deployed function root is at parents[1]
# which contains app/ and bin/. Locally, it's the backend/ directory.
FUNC_ROOT = Path(__file__).resolve().parents[1]

log = logging.getLogger("qrbackend.config")


def _env_path(name: str, default: Path) -> Path:
    raw = os.environ.get(name)
    return Path(raw).expanduser().resolve() if raw else default


def _load_dotenv_files() -> None:
    """Load the local development dotenv file(s), if any.

    Production (Vercel) injects real environment variables, so nothing here is
    required there; this only makes ``python-dotenv``-based local setups work.
    ``load_dotenv`` never overrides variables that are already set, so an
    exported variable always wins over the file.
    """
    try:
        from dotenv import load_dotenv
    except ImportError:
        return

    # Ordered candidates: the repository root file is the one actually used,
    # backend/.env is what scripts/setup_backend.sh creates from .env.example.
    for candidate in (REPO_ROOT / ".env", REPO_ROOT / "backend" / ".env"):
        if candidate.is_file():
            load_dotenv(candidate)


_load_dotenv_files()


def _redact(url: str) -> str:
    """Host/port only — never log or echo the password."""
    try:
        from urllib.parse import urlsplit

        parts = urlsplit(url)
        return f"{parts.scheme}://{parts.hostname}:{parts.port or ''}{parts.path}"
    except Exception:  # pragma: no cover - diagnostics must never crash startup
        return "<unparsable DATABASE_URL>"


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
    is_vercel = os.environ.get("VERCEL") == "1"

    # Pre-packaged Linux binary (shipped with the deployment, e.g. backend/bin/qr_engine).
    packaged_binary = FUNC_ROOT / "bin" / "qr_engine"

    # Local development binary (built via cmake in engine/).
    local_binary = REPO_ROOT / "engine" / "build" / "bin" / "qr_engine"

    # Fallback: desktop app's build output.
    desktop_binary = REPO_ROOT / "3DQRGenerator" / "build" / "bin" / "qr_engine"

    # On Vercel, the packaged binary is the only option.
    # Locally, prefer the local build, then the packaged binary, then desktop fallback.
    if is_vercel:
        default_engine = packaged_binary
    else:
        if local_binary.exists():
            default_engine = local_binary
        elif packaged_binary.exists():
            default_engine = packaged_binary
        else:
            default_engine = desktop_binary

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
                "DATABASE_URL is not configured. Add it in the Vercel project's "
                "Production Environment Variables using the Supabase Transaction "
                "Pooler connection string "
                "(postgresql://<user>:<password>@<pooler-host>:6543/postgres). "
                "The backend refuses to fall back to a local PostgreSQL server."
            )
        # Local development fallback only — never reached in production.
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
log.info(
    "DATABASE_URL target (credentials redacted): %s",
    _redact(settings.database_url),
)
