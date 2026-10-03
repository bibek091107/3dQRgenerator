"""Pydantic request/response schemas.

The field names mirror the desktop app's input buffers (m_InputID, m_InputName,
m_InputToolLife, ...) so the React form can post them 1:1.
"""

from __future__ import annotations

import uuid
from datetime import datetime
from typing import Literal, Optional

from pydantic import BaseModel, Field

QrType = Literal["Holder", "Tool"]
ExportKind = Literal["stl", "png"]


# ── batches ─────────────────────────────────────────────────────────


class BatchCreate(BaseModel):
    qr_type: QrType = Field(alias="type")
    quantity: int = Field(ge=1, le=5000)

    model_config = {"populate_by_name": True}


class SizeChoice(BaseModel):
    """The four options from DrawSizeSelection()."""

    size_choice: int = Field(default=0, ge=0, le=3, alias="sizeChoice")

    model_config = {"populate_by_name": True}


# ── object entry ─────────────────────────────────────────────────────


class HolderFields(BaseModel):
    id: str = Field(default="", max_length=128)
    name: str = Field(default="", max_length=256)
    calibrationStatus: str = Field(default="", max_length=128)
    accumulatedLife: str = Field(default="", max_length=128)
    serialBatchNumber: str = Field(default="", max_length=128)


class ToolFields(BaseModel):
    id: str = Field(default="", max_length=128)
    name: str = Field(default="", max_length=256)
    toolLife: str = Field(default="", max_length=128)
    completedLife: str = Field(default="", max_length=128)
    remainingVisual: str = Field(default="", max_length=128)
    remainingML: str = Field(default="", max_length=128)
    totalUsageHours: str = Field(default="", max_length=128)
    cncMachineId: str = Field(default="", max_length=128)
    cncMachineName: str = Field(default="", max_length=128)
    operationType: str = Field(default="", max_length=256)
    trayNo: str = Field(default="", max_length=128)


class ObjectCreate(BaseModel):
    type: QrType
    fields: dict[str, str] = Field(default_factory=dict)

    model_config = {"populate_by_name": True}


# ── export ──────────────────────────────────────────────────────────


class ExportRequest(BaseModel):
    kind: ExportKind
    overwrite: bool = False


# ── responses ───────────────────────────────────────────────────────


class EngineErrorBody(BaseModel):
    """Shape the React layer maps straight onto the desktop popups."""

    code: str
    message: str
    duplicateId: Optional[str] = None


class ObjectOut(BaseModel):
    id: uuid.UUID
    batchId: uuid.UUID
    position: int
    type: str
    qrId: str
    name: str

    calibrationStatus: str
    accumulatedLife: str
    serialBatchNumber: str

    toolLifeMeters: float
    completedLifeMeters: float
    remainingLifeVisualMeters: float
    remainingLifeMLMeters: float
    totalUsageHours: float
    cncMachineId: str
    cncMachineName: str
    operationType: str
    trayNo: str

    sizeMM: float
    depthMM: float

    payload: Optional[str] = None
    matrixSize: Optional[int] = None
    triangleCount: Optional[int] = None

    stlGenerated: bool
    pngGenerated: bool
    stlBytes: Optional[int] = None
    pngBytes: Optional[int] = None

    createdAt: Optional[datetime] = None

    model_config = {"from_attributes": True}


class BatchOut(BaseModel):
    id: uuid.UUID
    type: str
    targetQuantity: int
    sizeMM: Optional[float] = None
    depthMM: Optional[float] = None
    state: str
    objectCount: int
    createdAt: Optional[datetime] = None

    model_config = {"from_attributes": True}


class BatchDetail(BatchOut):
    objects: list[ObjectOut] = Field(default_factory=list)


class ExportResultOut(BaseModel):
    ok: bool
    kind: str
    path: str
    relativePath: str
    bytes: int
    code: str = "ok"
    message: str = ""


class SizeOption(BaseModel):
    """Mirrors the radio buttons in DrawSizeSelection()."""

    sizeMM: float
    depthMM: float
