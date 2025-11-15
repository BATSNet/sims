"""
Incident Note Model - Internal notes for responders (not visible to reporters)
"""
from typing import Optional
from datetime import datetime
from uuid import UUID

from sqlalchemy import Column, BigInteger, String, TIMESTAMP, Text, ForeignKey
from sqlalchemy.dialects.postgresql import UUID as PGUUID
from sqlalchemy.orm import relationship
from pydantic import BaseModel

from db.connection import Base


class IncidentNoteORM(Base):
    """SQLAlchemy ORM model for incident_notes table"""
    __tablename__ = "incident_notes"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    incident_id = Column(PGUUID(as_uuid=True), ForeignKey('incident.id', ondelete='CASCADE'), nullable=False)
    organization_id = Column(BigInteger, ForeignKey('organization.id', ondelete='CASCADE'), nullable=False)
    note_text = Column(Text, nullable=False)
    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    created_by = Column(String(100))

    # Relationships
    incident = relationship("IncidentORM", backref="notes")
    organization = relationship("OrganizationORM", backref="incident_notes")


class IncidentNoteCreate(BaseModel):
    """Request model for creating an incident note"""
    incident_id: UUID
    organization_id: int
    note_text: str
    created_by: Optional[str] = None


class IncidentNoteResponse(BaseModel):
    """Response model for incident note"""
    id: int
    incident_id: UUID
    organization_id: int
    note_text: str
    created_at: str
    created_by: Optional[str]

    class Config:
        from_attributes = True
