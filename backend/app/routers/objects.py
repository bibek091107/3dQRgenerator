"""Per-object operations: 3D preview mesh, export, file status and download.

Export semantics follow App::DoExportSTL / DoExportPNG and the overwrite popup
in App::DrawPopups exactly:

* the C++ engine creates the folder, writes the file and verifies it landed;
* the overwrite prompt is raised ONLY when the file really exists on disk;
* "Overwrite" removes the file and re-exports; "Cancel" skips the item.
"""

from __future__ import annotations

import uuid
from pathlib import Path

from fastapi import APIRouter, Depends, HTTPException
from fastapi.responses import FileResponse
from sqlalchemy.ext.asyncio import AsyncSession

from .. import engine_bridge, storage
from ..config import settings
from ..database import get_session
from ..models import QRObjectRecord
from ..schemas import ExportRequest, ExportResultOut, ObjectOut

router = APIRouter(prefix="/api/objects", tags=["objects"])


def _object_out(obj: QRObjectRecord) -> ObjectOut:
    return ObjectOut(
        id=obj.id,
        batchId=obj.batch_id,
        position=obj.position,
        type=obj.qr_type,
        qrId=obj.qr_id,
        name=obj.name,
        calibrationStatus=obj.calibration_status,
        accumulatedLife=obj.accumulated_life,
        serialBatchNumber=obj.serial_batch_number,
        toolLifeMeters=obj.tool_life_meters,
        completedLifeMeters=obj.completed_life_meters,
        remainingLifeVisualMeters=obj.remaining_life_visual_meters,
        remainingLifeMLMeters=obj.remaining_life_ml_meters,
        totalUsageHours=obj.total_usage_hours,
        cncMachineId=obj.cnc_machine_id,
        cncMachineName=obj.cnc_machine_name,
        operationType=obj.operation_type,
        trayNo=obj.tray_no,
        sizeMM=obj.size_mm,
        depthMM=obj.depth_mm,
        payload=obj.payload,
        matrixSize=obj.matrix_size,
        triangleCount=obj.triangle_count,
        stlGenerated=obj.stl_generated,
        pngGenerated=obj.png_generated,
        stlBytes=obj.stl_bytes,
        pngBytes=obj.png_bytes,
        createdAt=obj.created_at,
    )


async def _load_object(session: AsyncSession, object_id: uuid.UUID) -> QRObjectRecord:
    obj = await session.get(QRObjectRecord, object_id)
    if obj is None:
        raise HTTPException(
            status_code=404, detail={"code": "not_found", "message": "Object not found"}
        )
    return obj


def mesh_path_for(obj: QRObjectRecord) -> Path:
    # Include type to avoid collision between Holder and Tool with identical IDs.
    return settings.cache_dir / "meshes" / f"{obj.qr_type}_{obj.qr_id}.mesh"


# ─────────────────────────────────────────────
#  Read
# ─────────────────────────────────────────────


@router.get("/{object_id}", response_model=ObjectOut)
async def get_object(object_id: uuid.UUID, session: AsyncSession = Depends(get_session)) -> ObjectOut:
    obj = await _load_object(session, object_id)
    storage.refresh_export_flags(obj)
    await session.commit()
    return _object_out(obj)


@router.get("/{object_id}/status")
async def object_status(
    object_id: uuid.UUID, session: AsyncSession = Depends(get_session)
) -> dict:
    """STL/PNG paths + on-disk status, as shown in the preview panel."""
    obj = await _load_object(session, object_id)
    stl = storage.stl_path(obj.qr_type, obj.qr_id)
    png = storage.png_path(obj.qr_type, obj.qr_id)
    return {
        "id": obj.id,
        "qrId": obj.qr_id,
        "type": obj.qr_type,
        "stl": {
            "relativePath": storage.relative_stl(obj.qr_type, obj.qr_id),
            "path": str(stl),
            **storage.file_status(stl),
        },
        "png": {
            "relativePath": storage.relative_png(obj.qr_type, obj.qr_id),
            "path": str(png),
            **storage.file_status(png),
        },
    }


@router.get("/{object_id}/mesh")
async def object_mesh(object_id: uuid.UUID, session: AsyncSession = Depends(get_session)):
    """Raw preview geometry: float32 triplets, 9 floats per vertex.

    Layout is exactly what Renderer::SetMesh() uploaded to OpenGL:
    px py pz nx ny nz cr cg cb.  React Three Fiber turns this into a
    BufferGeometry, so the browser shows the same mesh the desktop app drew.
    """
    obj = await _load_object(session, object_id)
    if obj.triangle_count is None:
        raise HTTPException(
            status_code=409,
            detail={
                "code": "not_generated",
                "message": "Generate the QR codes before opening the 3D preview.",
            },
        )

    path = mesh_path_for(obj)
    if not path.exists():
        # Cache was cleared — rebuild it from the stored fields via the engine.
        try:
            res = await engine_bridge.call_engine(
                {"cmd": "mesh", "item": obj.to_engine_item()}
            )
        except engine_bridge.EngineError as exc:
            raise HTTPException(status_code=503, detail={"code": exc.code, "message": exc.message})
        if not res.ok:
            raise HTTPException(
                status_code=500, detail={"code": res.code, "message": res.message}
            )
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(engine_bridge.decode_base64_float_buffer(res.get("data", "")))

    return FileResponse(
        path,
        media_type="application/octet-stream",
        filename=f"{obj.qr_id}.mesh",
    )


@router.get("/{object_id}/download")
async def download_file(
    object_id: uuid.UUID,
    kind: str = "stl",
    session: AsyncSession = Depends(get_session),
) -> FileResponse:
    """Download an already-exported file from the shared output folder."""
    if kind not in ("stl", "png"):
        raise HTTPException(status_code=422, detail={"code": "bad_kind", "message": "kind must be stl or png"})
    obj = await _load_object(session, object_id)
    path = storage.stl_path(obj.qr_type, obj.qr_id) if kind == "stl" else storage.png_path(obj.qr_type, obj.qr_id)
    if not path.exists():
        raise HTTPException(
            status_code=404,
            detail={"code": "not_found", "message": f"{path.name} has not been generated yet."},
        )
    media = "model/stl" if kind == "stl" else "image/png"
    return FileResponse(path, media_type=media, filename=path.name)


# ─────────────────────────────────────────────
#  Export
# ─────────────────────────────────────────────


@router.post("/{object_id}/export", response_model=ExportResultOut)
async def export_object(
    object_id: uuid.UUID,
    body: ExportRequest,
    session: AsyncSession = Depends(get_session),
) -> ExportResultOut:
    obj = await _load_object(session, object_id)

    if obj.triangle_count is None:
        raise HTTPException(
            status_code=409,
            detail={
                "code": "not_generated",
                "message": "Generate the QR codes before exporting.",
            },
        )

    try:
        res = await engine_bridge.export(
            kind=body.kind, item=obj.to_engine_item(), overwrite=body.overwrite
        )
    except engine_bridge.EngineError as exc:
        raise HTTPException(status_code=503, detail={"code": exc.code, "message": exc.message})

    if not res.ok:
        code = res.code or "export_failed"
        if code == "file_exists":
            # Mirrors the "FILE ALREADY EXISTS" popup: the client decides
            # whether to overwrite or to cancel/skip.
            raise HTTPException(
                status_code=409,
                detail={
                    "code": "file_exists",
                    "message": f"{res.get('path')}\n\nDo you want to overwrite it?",
                    "path": res.get("path"),
                    "relativePath": res.get("relativePath"),
                    "bytes": res.get("bytes"),
                    "kind": body.kind,
                    "objectId": str(obj.id),
                },
            )
        raise HTTPException(
            status_code=500, detail={"code": code, "message": res.message}
        )

    storage.refresh_export_flags(obj)
    await session.commit()

    return ExportResultOut(
        ok=True,
        kind=body.kind,
        path=str(res.get("path", "")),
        relativePath=str(res.get("relativePath", "")),
        bytes=int(res.get("bytes", 0)),
    )
