"""Bridge to the existing C++ QR/3D/STL engine.

Every QR payload, QR matrix, 3D mesh, STL and PNG in this application is
produced by the original C++ code in ../engine — nothing is re-implemented in
Python. This module is the only place that knows how to talk to it.
"""

from __future__ import annotations

import asyncio
import base64
import json
import shutil
from pathlib import Path
from typing import Any

from .config import settings


class EngineError(RuntimeError):
    """Raised when the engine binary is missing or returns garbage."""

    def __init__(self, message: str, code: str = "engine_error") -> None:
        super().__init__(message)
        self.code = code
        self.message = message


class EngineResponse:
    """Parsed JSON response from qr_engine."""

    def __init__(self, payload: dict[str, Any]) -> None:
        self.raw = payload

    @property
    def ok(self) -> bool:
        return bool(self.raw.get("ok"))

    @property
    def code(self) -> str:
        return str(self.raw.get("code", ""))

    @property
    def message(self) -> str:
        return str(self.raw.get("message", ""))

    def get(self, key: str, default: Any = None) -> Any:
        return self.raw.get(key, default)

    def __getitem__(self, key: str) -> Any:
        return self.raw[key]


def engine_available() -> bool:
    return settings.engine_binary.exists() and os_accessible(settings.engine_binary)


def os_accessible(path: Path) -> bool:
    return shutil.which(str(path)) is not None or path.exists()


async def call_engine(request: dict[str, Any]) -> EngineResponse:
    """Run one qr_engine command and return its parsed response."""
    if not settings.engine_binary.exists():
        raise EngineError(
            f"Engine binary not found at {settings.engine_binary}. "
            "Build it with: cmake -S engine -B engine/build && "
            "cmake --build engine/build",
            code="engine_missing",
        )

    proc = await asyncio.create_subprocess_exec(
        str(settings.engine_binary),
        stdin=asyncio.subprocess.PIPE,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )

    try:
        stdout, stderr = await asyncio.wait_for(
            proc.communicate(json.dumps(request).encode("utf-8")),
            timeout=settings.engine_timeout,
        )
    except asyncio.TimeoutError:
        proc.kill()
        await proc.wait()
        raise EngineError(
            f"Engine timed out after {settings.engine_timeout}s", code="engine_timeout"
        )

    text = stdout.decode("utf-8", errors="replace").strip()
    if not text:
        err = stderr.decode("utf-8", errors="replace").strip()
        raise EngineError(
            f"Engine produced no output (exit {proc.returncode}). stderr: {err}",
            code="engine_no_output",
        )

    try:
        payload = json.loads(text)
    except json.JSONDecodeError as exc:
        raise EngineError(f"Engine returned invalid JSON: {exc}", code="engine_bad_json") from exc

    return EngineResponse(payload)


# ─────────────────────────────────────────────
#  Command helpers
# ─────────────────────────────────────────────


async def ping() -> EngineResponse:
    return await call_engine({"cmd": "ping"})


async def validate(
    qr_type: str,
    fields: dict[str, str],
    known_ids: list[str],
) -> EngineResponse:
    """Trim + default + validate + ID-uniqueness, all inside the C++ engine."""
    return await call_engine(
        {
            "cmd": "validate",
            "type": qr_type,
            "outputRoot": str(settings.output_root),
            "knownIds": known_ids,
            "fields": fields,
        }
    )


async def generate(
    items: list[dict[str, Any]],
    size_mm: float,
    depth_mm: float,
    mesh_dir: Path | None = None,
    include_matrix: bool = False,
) -> EngineResponse:
    """Payload + QR matrix + 3D mesh for a batch of objects."""
    request: dict[str, Any] = {
        "cmd": "generate",
        "sizeMM": size_mm,
        "depthMM": depth_mm,
        "includeMatrix": include_matrix,
        "items": items,
    }
    if mesh_dir is not None:
        settings.cache_dir.mkdir(parents=True, exist_ok=True)
        request["meshDir"] = str(mesh_dir)
    return await call_engine(request)


async def export(
    kind: str,
    item: dict[str, Any],
    overwrite: bool = False,
) -> EngineResponse:
    """Write one STL or PNG. Returns code "file_exists" when it is already there."""
    return await call_engine(
        {
            "cmd": "export",
            "kind": kind,
            "outputRoot": str(settings.output_root),
            "overwrite": overwrite,
            "item": item,
        }
    )


async def id_exists_on_disk(qr_type: str, qr_id: str) -> EngineResponse:
    return await call_engine(
        {
            "cmd": "exists",
            "type": qr_type,
            "id": qr_id,
            "outputRoot": str(settings.output_root),
        }
    )


async def paths_for(qr_type: str, qr_id: str) -> EngineResponse:
    return await call_engine({"cmd": "paths", "type": qr_type, "id": qr_id})


def read_mesh_blob(path: Path) -> bytes | None:
    if not path.exists():
        return None
    return path.read_bytes()


def decode_base64_float_buffer(data: str) -> bytes:
    return base64.b64decode(data)
