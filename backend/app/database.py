"""PostgreSQL access (SQLAlchemy 2.x, async psycopg driver)."""

from __future__ import annotations

from collections.abc import AsyncIterator

from sqlalchemy.ext.asyncio import (
    AsyncSession,
    async_sessionmaker,
    create_async_engine,
)
from sqlalchemy.orm import DeclarativeBase

from .config import settings

# NOTE: pool_pre_ping is intentionally off. With the async psycopg driver,
# SQLAlchemy's pre-ping runs an autocommit toggle outside a greenlet context
# and fails with MissingGreenlet. The lifespan handler already verifies the
# connection, and /api/health reports database state on every call.
engine = create_async_engine(settings.database_url, future=True)

SessionLocal = async_sessionmaker(
    engine, class_=AsyncSession, expire_on_commit=False, autoflush=False
)


class Base(DeclarativeBase):
    pass


async def get_session() -> AsyncIterator[AsyncSession]:
    async with SessionLocal() as session:
        yield session


async def init_models() -> None:
    from . import models  # noqa: F401  (registers the tables)

    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
