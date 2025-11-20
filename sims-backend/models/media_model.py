"""
Media Model - File attachments for incidents and messages
"""
from typing import Optional
from datetime import datetime
from enum import Enum
import uuid

from sqlalchemy import Column, String, BigInteger, TIMESTAMP, ForeignKey, Text
from sqlalchemy.dialects.postgresql import UUID, JSONB
from sqlalchemy.orm import relationship

from db.connection import Base


class MediaType(str, Enum):
    """Media type enum"""
    IMAGE = "image"
    AUDIO = "audio"
    VIDEO = "video"


class MediaORM(Base):
    """SQLAlchemy ORM model for media table"""
    __tablename__ = "media"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    incident_id = Column(UUID(as_uuid=True), ForeignKey('incident.id', ondelete='CASCADE'), nullable=True)
    chat_message_id = Column(BigInteger, ForeignKey('chat_message.id', ondelete='SET NULL'))

    # File information
    file_path = Column(Text, nullable=False)
    file_url = Column(Text, nullable=False)
    mime_type = Column(Text, nullable=False)
    file_size = Column(BigInteger)
    media_type = Column(String(20))

    # Content analysis/transcription for all media types
    # - Audio: speech-to-text transcription
    # - Image: visual description
    # - Video: content summary
    transcription = Column(Text)

    # Metadata
    meta_data = Column('metadata', JSONB, default={})
    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)

    # Relationships configured in models/__init__.py
