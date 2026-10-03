"""Filesystem helpers.

The desktop app treats the filesystem as the source of truth for export status
and for ID uniqueness. This module mirrors that behaviour for the web app; the
folder layout itself is owned by the C++ engine (OutputPaths.cpp).
"""

from __future__ import annotations

from pathlib import Path

from .config import settings

# The four radio-button options from App::DrawSizeSelection()
SIZE_CHOICES: list[tuple[float, float]] = [
    (9.0, 0.8),
    (7.0, 1.0),
    (5.0, 1.2),
    (3.0, 1.5),
]


def folder_for(qr_type: str) -> Path:
    """output/holder for Holder, output/tools for Tool (engine's naming)."""
    if qr_type == "Holder":
        return settings.holder_dir
    if qr_type == "Tool":
        return settings.tools_dir
    return settings.output_root


def stl_path(qr_type: str, qr_id: str) -> Path:
    return folder_for(qr_type) / f"{qr_id}.stl"


def png_path(qr_type: str, qr_id: str) -> Path:
    return folder_for(qr_type) / f"{qr_id}.png"


def relative_stl(qr_type: str, qr_id: str) -> str:
    try:
        rel = stl_path(qr_type, qr_id).relative_to(settings.output_root)
    except ValueError:
        return str(stl_path(qr_type, qr_id))
    return rel.as_posix()


def relative_png(qr_type: str, qr_id: str) -> str:
    try:
        rel = png_path(qr_type, qr_id).relative_to(settings.output_root)
    except ValueError:
        return str(png_path(qr_type, qr_id))
    return rel.as_posix()


def file_status(path: Path) -> dict:
    """Status exactly as the preview panel reported it: does it exist?"""
    try:
        if path.exists():
            return {"exists": True, "bytes": path.stat().st_size}
        return {"exists": False, "bytes": None}
    except OSError:
        return {"exists": False, "bytes": None}


def refresh_export_flags(record) -> None:
    """Reconcile DB flags with what is actually on disk."""
    stl = file_status(stl_path(record.qr_type, record.qr_id))
    png = file_status(png_path(record.qr_type, record.qr_id))
    record.stl_generated = bool(stl["exists"])
    record.png_generated = bool(png["exists"])
    record.stl_bytes = stl["bytes"]
    record.png_bytes = png["bytes"]


def ensure_output_dirs() -> None:
    settings.holder_dir.mkdir(parents=True, exist_ok=True)
    settings.tools_dir.mkdir(parents=True, exist_ok=True)
