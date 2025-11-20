"""
Chat Models - Session and message entities
"""
from typing import Optional
from datetime import datetime
import uuid

from sqlalchemy import Column, BigInteger, TIMESTAMP, ForeignKey, String
from sqlalchemy.dialects.postgresql import UUID, JSONB
from sqlalchemy.orm import relationship

from db.connection import Base


class ChatSessionORM(Base):
    """SQLAlchemy ORM model for chat_session table"""
    __tablename__ = "chat_session"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    session_id = Column(UUID(as_uuid=True), unique=True, nullable=False, default=uuid.uuid4)
    incident_id = Column(UUID(as_uuid=True), ForeignKey('incident.id', ondelete='CASCADE'), nullable=False)
    user_phone = Column(String(20))
    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    last_modified = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    summary = Column(String, nullable=True)
    last_summarized = Column(TIMESTAMP(timezone=True), nullable=True)

    # Relationships configured in models/__init__.py

class ChatMessageORM(Base):
    """SQLAlchemy ORM model for chat_message table"""
    __tablename__ = "chat_message"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    session_id = Column(UUID(as_uuid=True), ForeignKey('chat_session.session_id', ondelete='CASCADE'), nullable=False)
    message = Column(JSONB, nullable=False)
    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)

    # Relationships configured in models/__init__.py
