"""
Incident Model - Core incident/contact entity
"""
from typing import Optional, List, Dict, Any
from datetime import datetime
from enum import Enum
import uuid

from sqlalchemy import Column, String, Float, TIMESTAMP, ARRAY, Text, BigInteger, ForeignKey
from sqlalchemy.dialects.postgresql import UUID, JSONB
from sqlalchemy.orm import relationship
from pydantic import BaseModel, Field
from geoalchemy2 import Geometry

from db.connection import Base


class IncidentStatus(str, Enum):
    """Incident status enum - aligned with Flutter"""
    OPEN = "open"
    IN_PROGRESS = "in_progress"
    RESOLVED = "resolved"
    CLOSED = "closed"


class IncidentPriority(str, Enum):
    """Incident priority enum - aligned with Flutter"""
    CRITICAL = "critical"
    HIGH = "high"
    MEDIUM = "medium"
    LOW = "low"


class IncidentORM(Base):
    """SQLAlchemy ORM model for incident table"""
    __tablename__ = "incident"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    incident_id = Column(String(50), unique=True, nullable=False)
    user_phone = Column(String(13))

    # Location data
    location = Column(Geometry('POINT', srid=4326))
    latitude = Column(Float)
    longitude = Column(Float)
    heading = Column(Float)

    # Incident details
    title = Column(Text)
    description = Column(Text)

    # Status and priority
    status = Column(String(20), nullable=False, default='open')
    priority = Column(String(20), nullable=False, default='medium')
    category = Column(String(50), nullable=False, default='Unclassified')

    # Routing
    routed_to = Column(BigInteger, ForeignKey('organization.id'))

    # Metadata
    tags = Column(ARRAY(Text), default=[])
    meta_data = Column('metadata', JSONB, default={})

    # Timestamps
    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    updated_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow, onupdate=datetime.utcnow)

    # Relationships
    chat_sessions = relationship("ChatSessionORM", back_populates="incident", cascade="all, delete-orphan")
    media_files = relationship("MediaORM", back_populates="incident", cascade="all, delete-orphan")


# Pydantic models for API

class IncidentCreate(BaseModel):
    """Request model for creating an incident"""
    title: str
    description: str
    imageUrl: Optional[str] = None
    audioUrl: Optional[str] = None
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    heading: Optional[float] = None
    timestamp: Optional[str] = None
    metadata: Optional[Dict[str, Any]] = Field(default_factory=dict)
    user_phone: Optional[str] = None

    class Config:
        json_schema_extra = {
            "example": {
                "title": "Suspicious Activity",
                "description": "Unidentified vehicle near checkpoint",
                "latitude": 52.520,
                "longitude": 13.405,
                "heading": 45.0,
                "metadata": {"source": "mobile_app"}
            }
        }


class IncidentUpdate(BaseModel):
    """Request model for updating an incident"""
    title: Optional[str] = None
    description: Optional[str] = None
    status: Optional[IncidentStatus] = None
    priority: Optional[IncidentPriority] = None
    category: Optional[str] = None
    routed_to: Optional[int] = None
    tags: Optional[List[str]] = None
    metadata: Optional[Dict[str, Any]] = None


class IncidentResponse(BaseModel):
    """Response model for incident (aligned with Flutter expectations)"""
    id: str
    incident_id: str
    title: str
    description: str
    priority: str
    status: str
    created_at: str
    updated_at: str
    location: Optional[str] = None
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    heading: Optional[float] = None
    imageUrl: Optional[str] = None
    audioUrl: Optional[str] = None
    category: Optional[str] = None
    tags: Optional[List[str]] = None
    metadata: Optional[Dict[str, Any]] = None

    class Config:
        from_attributes = True
        json_schema_extra = {
            "example": {
                "id": "123e4567-e89b-12d3-a456-426614174000",
                "incident_id": "INC-2847A9B2",
                "title": "Suspicious Activity",
                "description": "Unidentified vehicle near checkpoint",
                "priority": "high",
                "status": "open",
                "created_at": "2025-11-14T14:25:33Z",
                "updated_at": "2025-11-14T14:25:33Z",
                "latitude": 52.520,
                "longitude": 13.405,
                "category": "Security"
            }
        }

    @classmethod
    def from_orm(cls, incident: IncidentORM, image_url: Optional[str] = None, audio_url: Optional[str] = None):
        """Create response from ORM model"""
        return cls(
            id=str(incident.id),
            incident_id=incident.incident_id,
            title=incident.title or "",
            description=incident.description or "",
            priority=incident.priority,
            status=incident.status,
            created_at=incident.created_at.isoformat() if incident.created_at else datetime.utcnow().isoformat(),
            updated_at=incident.updated_at.isoformat() if incident.updated_at else datetime.utcnow().isoformat(),
            latitude=incident.latitude,
            longitude=incident.longitude,
            heading=incident.heading,
            imageUrl=image_url,
            audioUrl=audio_url,
            category=incident.category,
            tags=incident.tags or [],
            metadata=incident.meta_data or {}
        )
