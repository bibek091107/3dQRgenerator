"""Shared pytest setup.

The tests drive the app through httpx's ASGITransport, which does not run the
FastAPI lifespan, so the schema is created here instead.
"""

from __future__ import annotations

import pytest
import pytest_asyncio

from app.database import engine, init_models


@pytest_asyncio.fixture(scope="session", autouse=True)
async def _schema() -> None:
    await init_models()
    yield
    await engine.dispose()
