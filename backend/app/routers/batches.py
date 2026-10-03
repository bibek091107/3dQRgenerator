"""Batch lifecycle + object entry + QR generation.

Mirrors the desktop app's state machine:

    MainMenu -> Quantity -> (Holder|Tool)Details -> SizeSelection -> Preview

A ``Batch`` is that flow's state; ``QRObjectRecord`` rows are its
``m_PendingObjects``.
"""

from __future__ import annotations

import uuid

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from .. import engine_bridge, storage
from ..config import settings
from ..database import get_session
from ..models import Batch, QRObjectRecord
from ..schemas import BatchCreate, BatchDetail, BatchOut, ObjectCreate, ObjectOut, SizeChoice

router = APIRouter(prefix="/api/batches", tags=["batches"])

VALID_TYPES = ("Holder", "Tool")


def _batch_out(batch: Batch, object_count: int) -> BatchOut:
    return BatchOut(
        id=batch.id,
        type=batch.qr_type,
        targetQuantity=batch.target_quantity,
        sizeMM=batch.size_mm,
        depthMM=batch.depth_mm,
        state=batch.state,
        objectCount=object_count,
        createdAt=batch.created_at,
    )


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


async def _load_batch(session: AsyncSession, batch_id: uuid.UUID) -> Batch:
    batch = await session.get(Batch, batch_id)
    if batch is None:
        raise HTTPException(status_code=404, detail={"code": "not_found", "message": "Batch not found"})
    return batch


# ─────────────────────────────────────────────
#  Quantity screen
# ─────────────────────────────────────────────


@router.post("", status_code=201, response_model=BatchOut)
async def create_batch(body: BatchCreate, session: AsyncSession = Depends(get_session)) -> Batch:
    if body.qr_type not in VALID_TYPES:
        raise HTTPException(
            status_code=422,
            detail={"code": "validation_error", "message": 'Type must be either "Holder" or "Tool".'},
        )
    storage.ensure_output_dirs()
    batch = Batch(qr_type=body.qr_type, target_quantity=body.quantity, state="entering")
    session.add(batch)
    await session.commit()
    return _batch_out(batch, 0)


@router.get("", response_model=list[BatchOut])
async def list_batches(session: AsyncSession = Depends(get_session)) -> list[BatchOut]:
    rows = await session.execute(select(Batch).order_by(Batch.created_at.desc()))
    batches = rows.scalars().unique().all()
    return [_batch_out(b, len(b.objects)) for b in batches]


@router.get("/{batch_id}", response_model=BatchDetail)
async def get_batch(batch_id: uuid.UUID, session: AsyncSession = Depends(get_session)) -> BatchDetail:
    batch = await _load_batch(session, batch_id)
    for obj in batch.objects:
        storage.refresh_export_flags(obj)
    await session.commit()
    return BatchDetail(
        **_batch_out(batch, len(batch.objects)).model_dump(),
        objects=[_object_out(o) for o in batch.objects],
    )


@router.delete("/{batch_id}", status_code=204)
async def delete_batch(batch_id: uuid.UUID, session: AsyncSession = Depends(get_session)) -> None:
    """Equivalent to "Back to Main Menu" — discards the pending objects."""
    batch = await _load_batch(session, batch_id)
    await session.delete(batch)
    await session.commit()


# ─────────────────────────────────────────────
#  Details screen — one object at a time
# ─────────────────────────────────────────────


@router.post("/{batch_id}/objects", status_code=201, response_model=ObjectOut)
async def create_object(
    batch_id: uuid.UUID,
    body: ObjectCreate,
    session: AsyncSession = Depends(get_session),
) -> ObjectOut:
    batch = await _load_batch(session, batch_id)

    if batch.state != "entering":
        raise HTTPException(
            status_code=409,
            detail={
                "code": "batch_already_generated",
                "message": "This batch has already been generated. Start a new batch to add more QRs.",
            },
        )

    if body.type != batch.qr_type:
        raise HTTPException(
            status_code=422,
            detail={
                "code": "type_mismatch",
                "message": f"Batch expects type {batch.qr_type}.",
            },
        )

    if len(batch.objects) >= batch.target_quantity:
        raise HTTPException(
            status_code=409,
            detail={
                "code": "quantity_reached",
                "message": f"This batch already holds all {batch.target_quantity} requested QRs.",
            },
        )

    known_ids = [o.qr_id for o in batch.objects]

    # All normalization, defaulting, numeric validation and ID-uniqueness
    # checking happens inside the C++ engine so the web app and the desktop
    # app can never disagree.
    try:
        res = await engine_bridge.validate(batch.qr_type, body.fields, known_ids)
    except engine_bridge.EngineError as exc:
        raise HTTPException(status_code=503, detail={"code": exc.code, "message": exc.message})

    if not res.ok:
        raise HTTPException(
            status_code=422,
            detail={
                "code": res.code,
                "message": res.message,
                "duplicateId": res.get("duplicateId"),
            },
        )

    data = res.get("object", {})

    # Supabase Duplicate Check
    from ..supabase_client import check_exists, insert_record
    is_duplicate = await check_exists(batch.qr_type, data["id"])
    if is_duplicate:
        raise HTTPException(
            status_code=409,
            detail={
                "code": "duplicate_id",
                "message": f"{batch.qr_type} ID already exists.",
                "duplicateId": data["id"]
            }
        )

    obj = QRObjectRecord(
        batch_id=batch.id,
        position=len(batch.objects),
        qr_type=batch.qr_type,
        qr_id=data["id"],
        name=data["name"],
        calibration_status=data.get("calibrationStatus", "N/A"),
        accumulated_life=data.get("accumulatedLife", "N/A"),
        serial_batch_number=data.get("serialBatchNumber", "N/A"),
        tool_life_meters=data.get("toolLifeMeters", 0.0),
        completed_life_meters=data.get("completedLifeMeters", 0.0),
        remaining_life_visual_meters=data.get("remainingLifeVisualMeters", 0.0),
        remaining_life_ml_meters=data.get("remainingLifeMLMeters", 0.0),
        total_usage_hours=data.get("totalUsageHours", 0.0),
        cnc_machine_id=data.get("cncMachineId", "N/A"),
        cnc_machine_name=data.get("cncMachineName", "N/A"),
        operation_type=data.get("operationType", "N/A"),
        tray_no=data.get("trayNo", "N/A"),
    )
    
    # Map fields for Supabase
    if batch.qr_type.lower() == "tool":
        sb_data = {
            "tool_id": data["id"],
            "tool_name": data["name"],
            "tool_life_m": data.get("toolLifeMeters", 0.0),
            "completed_life_m": data.get("completedLifeMeters", 0.0),
            "remaining_life_visual_m": data.get("remainingLifeVisualMeters", 0.0),
            "remaining_life_ml_m": data.get("remainingLifeMLMeters", 0.0),
            "total_hours_of_usage": data.get("totalUsageHours", 0.0),
            "cnc_machine_id": data.get("cncMachineId", "N/A"),
            "cnc_machine_name": data.get("cncMachineName", "N/A"),
            "type_of_operation": data.get("operationType", "N/A"),
            "tray_no": data.get("trayNo", "N/A")
        }
    else:
        sb_data = {
            "holder_id": data["id"],
            "holder_name": data["name"],
            "calibration_inspection_status": data.get("calibrationStatus", "N/A"),
            "current_accumulated_life": data.get("accumulatedLife", "N/A"),
            "serial_number_batch_number": data.get("serialBatchNumber", "N/A")
        }

    try:
        await insert_record(batch.qr_type, sb_data)
    except Exception as e:
        import traceback
        traceback.print_exc()
        raise HTTPException(
            status_code=500,
            detail={
                "code": "supabase_insert_failed",
                "message": f"Failed to insert record into Supabase: {str(e)}"
            }
        )

    session.add(obj)
    await session.commit()
    await session.refresh(obj)
    return _object_out(obj)


# ─────────────────────────────────────────────
#  Size selection + "Generate QRs and Preview"
# ─────────────────────────────────────────────


@router.post("/{batch_id}/generate", response_model=BatchDetail)
async def generate_batch(
    batch_id: uuid.UUID,
    body: SizeChoice,
    session: AsyncSession = Depends(get_session),
) -> BatchDetail:
    batch = await _load_batch(session, batch_id)

    if not batch.objects:
        raise HTTPException(
            status_code=409,
            detail={"code": "empty_batch", "message": "Add at least one QR before generating."},
        )

    size_mm, depth_mm = storage.SIZE_CHOICES[body.size_choice]

    mesh_dir = settings.cache_dir / "meshes"
    items = [o.to_engine_item() for o in batch.objects]

    try:
        res = await engine_bridge.generate(
            items, size_mm=size_mm, depth_mm=depth_mm, mesh_dir=mesh_dir
        )
    except engine_bridge.EngineError as exc:
        raise HTTPException(status_code=503, detail={"code": exc.code, "message": exc.message})

    if not res.ok:
        raise HTTPException(
            status_code=500,
            detail={"code": res.code or "generate_failed", "message": res.message},
        )

    # Apply the chosen size to every object, exactly like DrawSizeSelection()
    results = {item["id"]: item for item in res.get("items", [])}
    batch.size_mm = size_mm
    batch.depth_mm = depth_mm
    batch.state = "generated"

    for obj in batch.objects:
        obj.size_mm = size_mm
        obj.depth_mm = depth_mm
        info = results.get(obj.qr_id)
        if info:
            obj.payload = info.get("payload")
            obj.matrix_size = info.get("matrixSize")
            obj.triangle_count = info.get("triangleCount")

    await session.commit()
    return BatchDetail(
        **_batch_out(batch, len(batch.objects)).model_dump(),
        objects=[_object_out(o) for o in batch.objects],
    )


@router.post("/{batch_id}/regenerate", response_model=BatchDetail)
async def regenerate_batch(
    batch_id: uuid.UUID,
    body: SizeChoice,
    session: AsyncSession = Depends(get_session),
) -> BatchDetail:
    """Re-run generation, e.g. after changing the size selection."""
    return await generate_batch(batch_id, body, session)
