"""Database models.

These mirror the desktop app's in-memory state:

* ``Batch``  ~ the App's m_CurrentType / m_TargetQuantity / size choice
* ``QRObjectRecord`` ~ one entry of m_PendingObjects (a QRObject)

Field names, defaults and semantics follow QRObject.h exactly.
"""

from __future__ import annotations

import uuid
from datetime import datetime, timezone

from sqlalchemy import (
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Integer,
    String,
    UniqueConstraint,
    func,
)
from sqlalchemy.dialects.postgresql import UUID as PgUUID
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .database import Base


def _utcnow() -> datetime:
    return datetime.now(timezone.utc)


class Batch(Base):
    """A run of N QRs of one type — the desktop app's "Holder 2 of 5" flow."""

    __tablename__ = "batches"

    id: Mapped[uuid.UUID] = mapped_column(
        PgUUID(as_uuid=True), primary_key=True, default=uuid.uuid4
    )
    # "Holder" or "Tool" — mirrors m_CurrentType
    qr_type: Mapped[str] = mapped_column(String(16), nullable=False)
    # mirrors m_TargetQuantity
    target_quantity: Mapped[int] = mapped_column(Integer, nullable=False)

    # Size selection (only meaningful once the size screen was passed)
    size_mm: Mapped[float | None] = mapped_column(Float, nullable=True)
    depth_mm: Mapped[float | None] = mapped_column(Float, nullable=True)

    # "entering" -> still adding objects; "generated" -> QR matrix/mesh built
    state: Mapped[str] = mapped_column(String(16), nullable=False, default="entering")

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=_utcnow, server_default=func.now()
    )

    objects: Mapped[list["QRObjectRecord"]] = relationship(
        back_populates="batch",
        cascade="all, delete-orphan",
        order_by="QRObjectRecord.position",
        lazy="selectin",
    )


class QRObjectRecord(Base):
    """One pending/generated QR object — mirrors the C++ ``QRObject`` struct."""

    __tablename__ = "qr_objects"
    # The desktop app rejected a duplicate ID inside one session (m_UsedIDs).
    # Ids are unique per batch; the persistent half of that check is the
    # filesystem, which the C++ engine still owns.
    __table_args__ = (UniqueConstraint("batch_id", "qr_id", name="uq_batch_qr_id"),)

    id: Mapped[uuid.UUID] = mapped_column(
        PgUUID(as_uuid=True), primary_key=True, default=uuid.uuid4
    )
    batch_id: Mapped[uuid.UUID] = mapped_column(
        PgUUID(as_uuid=True), ForeignKey("batches.id", ondelete="CASCADE"), nullable=False
    )
    # position in m_PendingObjects (m_CurrentIndex order)
    position: Mapped[int] = mapped_column(Integer, nullable=False)

    # ── shared ──
    qr_type: Mapped[str] = mapped_column(String(16), nullable=False)   # obj.type
    qr_id: Mapped[str] = mapped_column(String(128), nullable=False)    # obj.id
    name: Mapped[str] = mapped_column(String(256), nullable=False)      # obj.name

    # ── Holder fields ──
    calibration_status: Mapped[str] = mapped_column(
        String(128), nullable=False, default="N/A"
    )
    accumulated_life: Mapped[str] = mapped_column(String(128), nullable=False, default="N/A")
    serial_batch_number: Mapped[str] = mapped_column(
        String(128), nullable=False, default="N/A"
    )

    # ── Tool fields ──
    tool_life_meters: Mapped[float] = mapped_column(Float, nullable=False, default=0.0)
    completed_life_meters: Mapped[float] = mapped_column(Float, nullable=False, default=0.0)
    remaining_life_visual_meters: Mapped[float] = mapped_column(
        Float, nullable=False, default=0.0
    )
    remaining_life_ml_meters: Mapped[float] = mapped_column(Float, nullable=False, default=0.0)
    total_usage_hours: Mapped[float] = mapped_column(Float, nullable=False, default=0.0)
    cnc_machine_id: Mapped[str] = mapped_column(String(128), nullable=False, default="N/A")
    cnc_machine_name: Mapped[str] = mapped_column(String(128), nullable=False, default="N/A")
    operation_type: Mapped[str] = mapped_column(String(256), nullable=False, default="N/A")
    tray_no: Mapped[str] = mapped_column(String(128), nullable=False, default="N/A")

    # ── physical configuration (obj.sizeMM / obj.depthMM) ──
    size_mm: Mapped[float] = mapped_column(Float, nullable=False, default=9.0)
    depth_mm: Mapped[float] = mapped_column(Float, nullable=False, default=0.8)

    # ── generation results ──
    payload: Mapped[str | None] = mapped_column(String, nullable=True)
    matrix_size: Mapped[int | None] = mapped_column(Integer, nullable=True)
    triangle_count: Mapped[int | None] = mapped_column(Integer, nullable=True)
    # The 3D preview buffer (9 float32 per vertex, the same layout
    # Renderer::SetMesh() uploaded) is a ~MB-scale artifact, so it lives on disk
    # beside the STL/PNG in .webcache/meshes/<ID>.mesh rather than in the
    # database. triangle_count above records that it was built.

    # ── export state (the desktop app trusted the filesystem, not these) ──
    stl_generated: Mapped[bool] = mapped_column(Boolean, nullable=False, default=False)
    png_generated: Mapped[bool] = mapped_column(Boolean, nullable=False, default=False)
    stl_bytes: Mapped[int | None] = mapped_column(Integer, nullable=True)
    png_bytes: Mapped[int | None] = mapped_column(Integer, nullable=True)

    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=_utcnow, server_default=func.now()
    )

    batch: Mapped[Batch] = relationship(back_populates="objects")

    # ── engine interchange ────────────────────────────────────────
    def to_engine_item(self) -> dict:
        """Serialize in the exact shape EngineMain.cpp expects."""
        return {
            "type": self.qr_type,
            "id": self.qr_id,
            "name": self.name,
            "calibrationStatus": self.calibration_status,
            "accumulatedLife": self.accumulated_life,
            "serialBatchNumber": self.serial_batch_number,
            "toolLifeMeters": self.tool_life_meters,
            "completedLifeMeters": self.completed_life_meters,
            "remainingLifeVisualMeters": self.remaining_life_visual_meters,
            "remainingLifeMLMeters": self.remaining_life_ml_meters,
            "totalUsageHours": self.total_usage_hours,
            "cncMachineId": self.cnc_machine_id,
            "cncMachineName": self.cnc_machine_name,
            "operationType": self.operation_type,
            "trayNo": self.tray_no,
            "sizeMM": self.size_mm,
            "depthMM": self.depth_mm,
        }
