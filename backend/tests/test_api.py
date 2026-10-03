"""End-to-end API tests.

These exercise the web layer against the real C++ engine and a real PostgreSQL
database. Point DATABASE_URL / QR_OUTPUT_ROOT at a scratch database and folder
when running them so the checks stay isolated:

    DATABASE_URL=postgresql+psycopg://localhost/qrgenerator_test \
    QR_OUTPUT_ROOT=/tmp/qr_web_test \
    .venv/bin/pytest -q
"""

from __future__ import annotations

import os
import uuid
from pathlib import Path

import pytest
from httpx import ASGITransport, AsyncClient

from app.config import settings
from app.database import SessionLocal
from app.main import app
from app.models import Batch, QRObjectRecord
from app.routers.batches import _load_batch  # noqa: F401  (import check)

API = "/api"


@pytest.fixture(scope="session")
def anyio_backend() -> str:
    return "asyncio"


@pytest.fixture
async def client() -> AsyncClient:
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as c:
        yield c


def tool_fields(qr_id: str, name: str = "Carbide Insert") -> dict:
    return {
        "id": qr_id,
        "name": name,
        "toolLife": "100",
        "completedLife": "25.5",
        "remainingVisual": "70",
        "remainingML": "74.5",
        "totalUsageHours": "3.256",
        "cncMachineId": "CNC001",
        "cncMachineName": "Haas VF-2",
        "operationType": "Pocketing",
        "trayNo": "TR-01",
    }


def holder_fields(qr_id: str, name: str = "Shell Holder") -> dict:
    return {
        "id": qr_id,
        "name": name,
        "calibrationStatus": "Passed",
        "accumulatedLife": "120.5 mins",
        "serialBatchNumber": "SN-77",
    }


async def cleanup() -> None:
    async with SessionLocal() as session:
        for row in (await session.execute(__import__("sqlalchemy").select(Batch))).scalars().all():
            await session.delete(row)
        await session.commit()


@pytest.fixture(autouse=True)
async def _clean() -> None:
    await cleanup()
    yield
    await cleanup()


# ─────────────────────────────────────────────
#  infrastructure
# ─────────────────────────────────────────────


async def test_health(client: AsyncClient) -> None:
    r = await client.get(f"{API}/health")
    assert r.status_code == 200, r.text
    body = r.json()
    assert body["status"] == "ok", body
    assert body["database"]["ok"] is True
    assert body["engine"]["ok"] is True


async def test_engine_ping_reports_cpp_engine(client: AsyncClient) -> None:
    r = await client.get(f"{API}/engine/ping")
    assert r.status_code == 200, r.text
    body = r.json()
    assert body["available"] is True
    assert "nayuki" in body["qrImplementation"]
    # the C++ defaults must still be the ones the desktop app used
    assert body["pngDefaults"] == "scale=10, quietZone=4"
    assert body["meshDefaults"] == "quietZone=2, baseThickness=0.5"


async def test_size_options_unchanged(client: AsyncClient) -> None:
    r = await client.get(f"{API}/engine/sizes")
    assert [s["sizeMM"] for s in r.json()] == [9.0, 7.0, 5.0, 3.0]
    assert [s["depthMM"] for s in r.json()] == [0.8, 1.0, 1.2, 1.5]


async def test_operation_hierarchy_unchanged(client: AsyncClient) -> None:
    groups = (await client.get(f"{API}/engine/operations")).json()
    assert [g["group"] for g in groups] == [
        "Flat End Mill",
        "Twist Drill",
        "Indexable Turning Tool",
    ]
    assert "Pocketing" in groups[0]["items"]
    assert groups[1]["items"][0] == "Through-Hole & Blind-Hole Drilling"
    assert groups[2]["items"] == [
        "Rough Turning",
        "Finish Turning",
        "Facing",
        "Taper & Profile Turning",
    ]


# ─────────────────────────────────────────────
#  Tool creation
# ─────────────────────────────────────────────


async def test_tool_creation_and_payload_format(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 2})).json()

    r = await client.post(
        f"{API}/batches/{batch['id']}/objects", json={"type": "Tool", "fields": tool_fields("WEBT01")}
    )
    assert r.status_code == 201, r.text
    obj = r.json()
    assert obj["qrId"] == "WEBT01"
    assert obj["position"] == 0

    # N/A defaults
    r2 = await client.post(
        f"{API}/batches/{batch['id']}/objects",
        json={"type": "Tool", "fields": {"id": "WEBT02", "name": "Minimal", "toolLife": "5"}},
    )
    minimal = r2.json()
    assert minimal["trayNo"] == "N/A"
    assert minimal["cncMachineId"] == "N/A"
    assert minimal["cncMachineName"] == "N/A"
    assert minimal["operationType"] == "N/A"


async def test_tool_payload_matches_desktop_format(client: AsyncClient) -> None:
    """After generation the payload must be byte-identical to the C++ format."""
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
    await client.post(
        f"{API}/batches/{batch['id']}/objects", json={"type": "Tool", "fields": tool_fields("WEBPAY")}
    )
    detail = (await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": 0})).json()
    payload = detail["objects"][0]["payload"]

    assert payload == (
        "Type: Tool\n"
        "Tool ID: WEBPAY\n"
        "Tool Name: Carbide Insert\n"
        "Tool Life (meters): 100.00\n"
        "Completed Life (meters): 25.50\n"
        "Remaining Life (Visual Inspection): 70.00 meters\n"
        "Remaining Life (ML Model): 74.50 meters\n"
        "Total Hours of Usage: 3.26 hours\n"
        "CNC Machine ID: CNC001\n"
        "CNC Machine Name: Haas VF-2\n"
        "Type of Operation: Pocketing\n"
        "Tray No: TR-01"
    )


# ─────────────────────────────────────────────
#  Holder creation
# ─────────────────────────────────────────────


async def test_holder_creation_and_payload_format(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Holder", "quantity": 2})).json()
    r = await client.post(
        f"{API}/batches/{batch['id']}/objects",
        json={"type": "Holder", "fields": holder_fields("WEBH01")},
    )
    assert r.status_code == 201, r.text
    assert r.json()["calibrationStatus"] == "Passed"

    # empty optional holder fields default to N/A
    r2 = await client.post(
        f"{API}/batches/{batch['id']}/objects",
        json={"type": "Holder", "fields": {"id": "WEBH02", "name": "Bare"}},
    )
    body = r2.json()
    assert body["calibrationStatus"] == "N/A"
    assert body["accumulatedLife"] == "N/A"
    assert body["serialBatchNumber"] == "N/A"

    detail = (await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": 0})).json()
    first = detail["objects"][0]["payload"]
    assert first == (
        "Type: Holder\n"
        "ID: WEBH01\n"
        "Name: Shell Holder\n"
        "Calibration / Inspection Status: Passed\n"
        "Current Accumulated Life: 120.5 mins\n"
        "Serial / Batch Number: SN-77"
    )


# ─────────────────────────────────────────────
#  Validation
# ─────────────────────────────────────────────


async def test_empty_field_rejected(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
    r = await client.post(
        f"{API}/batches/{batch['id']}/objects",
        json={"type": "Tool", "fields": {"id": "   ", "name": "X"}},
    )
    assert r.status_code == 422
    detail = r.json()["detail"]
    assert detail["code"] == "empty_field"
    assert detail["message"] == "ID and Name cannot be empty.\n\nPlease enter valid information."


async def test_duplicate_id_within_batch_rejected(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 3})).json()
    ok = await client.post(
        f"{API}/batches/{batch['id']}/objects", json={"type": "Tool", "fields": tool_fields("DUPT01")}
    )
    assert ok.status_code == 201

    dup = await client.post(
        f"{API}/batches/{batch['id']}/objects", json={"type": "Tool", "fields": tool_fields("DUPT01")}
    )
    assert dup.status_code == 422
    detail = dup.json()["detail"]
    assert detail["code"] == "duplicate_id"
    assert detail["duplicateId"] == "DUPT01"
    assert "already in use" in detail["message"]


async def test_duplicate_id_wins_over_invalid_number(client: AsyncClient) -> None:
    """Desktop validation order is empty -> duplicate -> numeric (App.cpp:540-574).

    A reused ID that ALSO carries an unparsable Tool Life must still report the
    duplicate ID, because the duplicate check runs first.
    """
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 2})).json()
    ok = await client.post(
        f"{API}/batches/{batch['id']}/objects", json={"type": "Tool", "fields": tool_fields("ORD01")}
    )
    assert ok.status_code == 201

    bad = tool_fields("ORD01")
    bad["toolLife"] = "not-a-number"
    r = await client.post(
        f"{API}/batches/{batch['id']}/objects", json={"type": "Tool", "fields": bad}
    )
    assert r.status_code == 422
    detail = r.json()["detail"]
    assert detail["code"] == "duplicate_id", "duplicate ID must be reported before numeric errors"
    assert detail["duplicateId"] == "ORD01"


async def test_empty_field_wins_over_duplicate_id(client: AsyncClient) -> None:
    """Empty ID/Name is checked before the duplicate lookup, so a blank name wins."""
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 2})).json()
    ok = await client.post(
        f"{API}/batches/{batch['id']}/objects", json={"type": "Tool", "fields": tool_fields("ORD02")}
    )
    assert ok.status_code == 201

    blank = tool_fields("ORD02")
    blank["name"] = "   "
    r = await client.post(
        f"{API}/batches/{batch['id']}/objects", json={"type": "Tool", "fields": blank}
    )
    assert r.status_code == 422
    detail = r.json()["detail"]
    assert detail["code"] == "empty_field", "empty ID/Name must be reported before duplicate lookup"
    assert "cannot be empty" in detail["message"]


async def test_duplicate_id_across_batches_from_disk_rejected(client: AsyncClient) -> None:
    """A file left on disk blocks the same ID in a brand new batch."""
    unique = "PERSIST01"
    for suffix in (".stl", ".png"):
        target = settings.output_root / "output" / "tools" / f"{unique}{suffix}"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b"\x00")
    try:
        batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
        r = await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(unique)},
        )
        assert r.status_code == 422
        assert r.json()["detail"]["code"] == "duplicate_id"
        assert r.json()["detail"]["duplicateId"] == unique
    finally:
        for suffix in (".stl", ".png"):
            target = settings.output_root / "output" / "tools" / f"{unique}{suffix}"
            if target.exists():
                target.unlink()


async def test_id_free_under_other_type(client: AsyncClient) -> None:
    """Tool/Holder folders are independent, matching the desktop app."""
    unique = "SHAREDID1"
    target = settings.output_root / "output" / "tools" / f"{unique}.stl"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(b"\x00")
    try:
        tool_batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
        blocked = await client.post(
            f"{API}/batches/{tool_batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(unique)},
        )
        assert blocked.status_code == 422

        holder_batch = (
            await client.post(f"{API}/batches", json={"type": "Holder", "quantity": 1})
        ).json()
        allowed = await client.post(
            f"{API}/batches/{holder_batch['id']}/objects",
            json={"type": "Holder", "fields": holder_fields(unique)},
        )
        assert allowed.status_code == 201, allowed.text
    finally:
        if target.exists():
            target.unlink()


async def test_numeric_validation_messages(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 20})).json()

    cases = [
        ({"toolLife": "abc"}, "Tool Life (meters) must be a valid number."),
        ({"toolLife": "10", "completedLife": "abc"}, "Completed Life (meters) must be a valid number."),
        (
            {"toolLife": "10", "remainingVisual": "abc"},
            "Remaining Life - Visual Inspection must be a valid number.",
        ),
        (
            {"toolLife": "10", "remainingML": "abc"},
            "Remaining Life - ML Model must be a valid number.",
        ),
        ({"toolLife": "10", "totalUsageHours": "abc"}, "Total Hours of Usage must be a valid number."),
        ({"toolLife": "-1"}, "Tool Life (meters) cannot be negative."),
        (
            {"toolLife": "10", "completedLife": "12.5"},
            "Completed Life (12.50 m) cannot exceed Tool Life (10.00 m).\n\nPlease check your inputs.",
        ),
    ]

    for i, (overrides, expected) in enumerate(cases):
        fields = {"id": f"VAL{i:02d}", "name": "T", "toolLife": "10"}
        fields.update(overrides)
        r = await client.post(
            f"{API}/batches/{batch['id']}/objects", json={"type": "Tool", "fields": fields}
        )
        assert r.status_code == 422, (overrides, r.text)
        detail = r.json()["detail"]
        assert detail["code"] == "validation_error"
        assert detail["message"] == expected, (overrides, detail["message"])


# ─────────────────────────────────────────────
#  Batch generation + navigation + preview
# ─────────────────────────────────────────────


async def test_batch_generation_multiple_objects(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 5})).json()
    ids = [f"BATCH{i:02d}" for i in range(5)]
    for i, qr_id in enumerate(ids):
        r = await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(qr_id, f"Tool {i}")},
        )
        assert r.status_code == 201, r.text
        assert r.json()["position"] == i

    detail = (
        await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": 1})
    ).json()
    assert detail["objectCount"] == 5
    assert detail["state"] == "generated"
    assert detail["sizeMM"] == 7.0
    assert detail["depthMM"] == 1.0

    # every object generated with a real matrix + mesh
    for i, obj in enumerate(detail["objects"]):
        assert obj["qrId"] == ids[i]
        assert obj["position"] == i
        assert obj["payload"] and obj["payload"].startswith("Type: Tool")
        assert obj["matrixSize"] and obj["matrixSize"] > 20
        assert obj["triangleCount"] and obj["triangleCount"] > 100
        assert obj["sizeMM"] == 7.0 and obj["depthMM"] == 1.0


async def test_quantity_limit_enforced(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 2})).json()
    for i in range(2):
        r = await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(f"LIMIT{i}")},
        )
        assert r.status_code == 201
    extra = await client.post(
        f"{API}/batches/{batch['id']}/objects",
        json={"type": "Tool", "fields": tool_fields("LIMIT9")},
    )
    assert extra.status_code == 409
    assert extra.json()["detail"]["code"] == "quantity_reached"


async def test_mesh_endpoint_serves_9_floats_per_vertex(client: AsyncClient) -> None:
    import struct

    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
    obj = (
        await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields("MESH01")},
        )
    ).json()
    detail = (await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": 0})).json()
    tri = detail["objects"][0]["triangleCount"]

    r = await client.get(f"{API}/objects/{obj['id']}/mesh")
    assert r.status_code == 200, r.text
    data = r.content
    assert len(data) == tri * 3 * 9 * 4, (len(data), tri)

    first = struct.unpack("<9f", data[:36])
    assert first[0] >= 0.0 and first[1] >= 0.0
    # 9th float of the first vertex is the colour; base plate is white (1,1,1)
    assert first[6] in (0.0, 1.0)
    assert first[7] in (0.0, 1.0)
    assert first[8] in (0.0, 1.0)


async def test_mesh_requires_generation_first(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
    obj = (
        await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields("NOGEN01")},
        )
    ).json()
    r = await client.get(f"{API}/objects/{obj['id']}/mesh")
    assert r.status_code == 409
    assert r.json()["detail"]["code"] == "not_generated"


async def test_all_size_choices_generate(client: AsyncClient) -> None:
    expected = [(9.0, 0.8), (7.0, 1.0), (5.0, 1.2), (3.0, 1.5)]
    for choice, (size, depth) in enumerate(expected):
        batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
        await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(f"SZ{choice}")},
        )
        detail = (
            await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": choice})
        ).json()
        obj = detail["objects"][0]
        assert (obj["sizeMM"], obj["depthMM"]) == (size, depth), choice
        # smaller plates need more modules per mm, so triangle counts differ
        assert obj["triangleCount"] > 0


# ─────────────────────────────────────────────
#  Export: folders, files, overwrite
# ─────────────────────────────────────────────


async def test_export_stl_writes_to_tools_folder(client: AsyncClient) -> None:
    qr_id = "EXPSTL01"
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
    obj = (
        await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(qr_id)},
        )
    ).json()
    await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": 0})

    r = await client.post(f"{API}/objects/{obj['id']}/export", json={"kind": "stl"})
    assert r.status_code == 200, r.text
    body = r.json()
    assert body["relativePath"] == f"output/tools/{qr_id}.stl"
    assert body["bytes"] > 0

    path = Path(body["path"])
    assert path.exists() and path.stat().st_size == body["bytes"]
    assert path.parent.name == "tools"
    assert path.read_bytes()[0:31] == b"3D QR Code Generator STL Export"

    # download works
    dl = await client.get(f"{API}/objects/{obj['id']}/download?kind=stl")
    assert dl.status_code == 200
    assert len(dl.content) == body["bytes"]
    path.unlink()


async def test_export_png_writes_to_holder_folder(client: AsyncClient) -> None:
    qr_id = "EXPPNG01"
    batch = (await client.post(f"{API}/batches", json={"type": "Holder", "quantity": 1})).json()
    obj = (
        await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Holder", "fields": holder_fields(qr_id)},
        )
    ).json()
    await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": 0})

    r = await client.post(f"{API}/objects/{obj['id']}/export", json={"kind": "png"})
    assert r.status_code == 200, r.text
    body = r.json()
    assert body["relativePath"] == f"output/holder/{qr_id}.png"

    path = Path(body["path"])
    assert path.parent.name == "holder"
    assert path.read_bytes()[:8] == bytes([137, 80, 78, 71, 13, 10, 26, 10])

    # the PNG dimensions follow (matrix + 2*quietZone) * scale
    detail = (await client.get(f"{API}/batches/{batch['id']}")).json()
    matrix = detail["objects"][0]["matrixSize"]
    expected_px = (matrix + 2 * 4) * 10
    data = path.read_bytes()
    width = int.from_bytes(data[16:20], "big")
    assert width == expected_px, (width, expected_px)
    path.unlink()


async def test_export_requires_overwrite_when_file_exists(client: AsyncClient) -> None:
    qr_id = "OVERWR01"
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
    obj = (
        await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(qr_id)},
        )
    ).json()
    await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": 0})

    first = await client.post(f"{API}/objects/{obj['id']}/export", json={"kind": "stl"})
    assert first.status_code == 200
    path = Path(first.json()["path"])
    original = path.read_bytes()

    # second attempt without permission must be refused, not silently overwritten
    second = await client.post(f"{API}/objects/{obj['id']}/export", json={"kind": "stl"})
    assert second.status_code == 409
    detail = second.json()["detail"]
    assert detail["code"] == "file_exists"
    assert detail["path"].endswith(f"{qr_id}.stl")
    assert "Do you want to overwrite it?" in detail["message"]
    assert path.read_bytes() == original, "file must not change without permission"

    # explicit overwrite is accepted
    third = await client.post(
        f"{API}/objects/{obj['id']}/export", json={"kind": "stl", "overwrite": True}
    )
    assert third.status_code == 200, third.text
    assert Path(third.json()["path"]).exists()
    path.unlink()


async def test_export_status_reflects_filesystem(client: AsyncClient) -> None:
    qr_id = "STATUS01"
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
    obj = (
        await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(qr_id)},
        )
    ).json()
    await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": 0})

    before = (await client.get(f"{API}/objects/{obj['id']}/status")).json()
    assert before["stl"]["exists"] is False
    assert before["png"]["exists"] is False
    assert before["stl"]["relativePath"] == f"output/tools/{qr_id}.stl"
    assert before["png"]["relativePath"] == f"output/tools/{qr_id}.png"

    await client.post(f"{API}/objects/{obj['id']}/export", json={"kind": "stl"})
    await client.post(f"{API}/objects/{obj['id']}/export", json={"kind": "png"})

    after = (await client.get(f"{API}/objects/{obj['id']}")).json()
    assert after["stlGenerated"] is True
    assert after["pngGenerated"] is True
    assert after["stlBytes"] > 0 and after["pngBytes"] > 0

    # a file deleted outside the app flips the status back
    Path(after["stlBytes"] and settings.output_root / "output" / "tools" / f"{qr_id}.stl").unlink()
    Path(settings.output_root / "output" / "tools" / f"{qr_id}.png").unlink()
    gone = (await client.get(f"{API}/objects/{obj['id']}/status")).json()
    assert gone["stl"]["exists"] is False
    assert gone["png"]["exists"] is False


async def test_export_before_generate_is_refused(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
    obj = (
        await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields("NOGENEXP")},
        )
    ).json()
    r = await client.post(f"{API}/objects/{obj['id']}/export", json={"kind": "stl"})
    assert r.status_code == 409
    assert r.json()["detail"]["code"] == "not_generated"


async def test_many_objects_export_without_crashing(client: AsyncClient) -> None:
    """Batch export loop across many QR objects (the desktop export queue)."""
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 12})).json()
    ids = [f"MANY{i:02d}" for i in range(12)]
    for qr_id in ids:
        r = await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(qr_id, f"Tool {qr_id}")},
        )
        assert r.status_code == 201, r.text

    detail = (await client.post(f"{API}/batches/{batch['id']}/generate", json={"sizeChoice": 0})).json()
    assert len(detail["objects"]) == 12

    written: list[Path] = []
    try:
        for obj in detail["objects"]:
            r = await client.post(f"{API}/objects/{obj['id']}/export", json={"kind": "stl"})
            assert r.status_code == 200, (obj["qrId"], r.text)
            written.append(Path(r.json()["path"]))
        for p in written:
            assert p.exists() and p.stat().st_size > 0
        assert len({p.name for p in written}) == 12
    finally:
        for p in written:
            if p.exists():
                p.unlink()


async def test_delete_batch_clears_objects(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 2})).json()
    for i in range(2):
        await client.post(
            f"{API}/batches/{batch['id']}/objects",
            json={"type": "Tool", "fields": tool_fields(f"DEL{i}")},
        )
    assert (await client.get(f"{API}/batches/{batch['id']}")).json()["objectCount"] == 2

    assert (await client.delete(f"{API}/batches/{batch['id']}")).status_code == 204
    assert (await client.get(f"{API}/batches/{batch['id']}")).status_code == 404


async def test_type_mismatch_rejected(client: AsyncClient) -> None:
    batch = (await client.post(f"{API}/batches", json={"type": "Tool", "quantity": 1})).json()
    r = await client.post(
        f"{API}/batches/{batch['id']}/objects",
        json={"type": "Holder", "fields": holder_fields("MISMATCH")},
    )
    assert r.status_code == 422
    assert r.json()["detail"]["code"] == "type_mismatch"
