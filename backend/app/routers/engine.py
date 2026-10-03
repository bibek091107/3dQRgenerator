"""Engine status + the fixed configuration lists the UI renders."""

from __future__ import annotations

from fastapi import APIRouter, HTTPException

from .. import engine_bridge, storage

router = APIRouter(prefix="/api/engine", tags=["engine"])


@router.get("/ping")
async def ping() -> dict:
    """Health check that also proves the C++ engine binary is reachable."""
    try:
        res = await engine_bridge.ping()
    except engine_bridge.EngineError as exc:
        raise HTTPException(status_code=503, detail={"code": exc.code, "message": exc.message})
    return {
        "available": res.ok,
        "engine": res.get("engine"),
        "version": res.get("version"),
        "qrImplementation": res.get("qrImplementation"),
        "pngDefaults": res.get("pngDefaults"),
        "meshDefaults": res.get("meshDefaults"),
        "outputRoot": str(storage.settings.output_root),
        "binary": str(storage.settings.engine_binary),
    }


@router.get("/sizes")
async def sizes() -> list[dict]:
    """The four size options from the desktop app's Size Selection screen."""
    return [
        {"sizeChoice": i, "sizeMM": sz, "depthMM": dp}
        for i, (sz, dp) in enumerate(storage.SIZE_CHOICES)
    ]


@router.get("/operations")
async def operations() -> list[dict]:
    """The hierarchical Type of Operation dropdown (App.cpp:458-508)."""
    return [
        {
            "group": "Flat End Mill",
            "items": [
                "Slotting & Keyway Cutting",
                "Pocketing",
                "Peripheral Milling",
                "Facing",
            ],
        },
        {
            "group": "Twist Drill",
            "items": [
                "Through-Hole & Blind-Hole Drilling",
                "Pre-Drilling",
                "Peck Drilling",
            ],
        },
        {
            "group": "Indexable Turning Tool",
            "items": [
                "Rough Turning",
                "Finish Turning",
                "Facing",
                "Taper & Profile Turning",
            ],
        },
    ]


@router.get("/output")
async def output_status() -> dict:
    """Where generated files go, mirroring GetSTLPath/GetPNGPath."""
    storage.ensure_output_dirs()
    return {
        "outputRoot": str(storage.settings.output_root),
        "holder": str(storage.settings.holder_dir),
        "tools": str(storage.settings.tools_dir),
    }
