"""
Organization Model - Organizations for routing incidents
"""
from typing import Optional, Dict, Any, List
from datetime import datetime

from sqlalchemy import Column, BigInteger, String, TIMESTAMP, Boolean, Float, Text, ARRAY, Integer
from sqlalchemy.dialects.postgresql import JSONB
from pydantic import BaseModel
from geoalchemy2 import Geometry

from db.connection import Base


class OrganizationORM(Base):
    """SQLAlchemy ORM model for organization table"""
    __tablename__ = "organization"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    name = Column(String(200), nullable=False)
    short_name = Column(String(100))
    type = Column(String(50), nullable=False)

    # Contact information
    contact_person = Column(String(200))
    phone = Column(String(20))
    email = Column(String(100))
    emergency_phone = Column(String(20))

    # Address
    address = Column(Text)
    city = Column(String(100))
    country = Column(String(100))

    # Location
    location = Column(Geometry('POINT', srid=4326))

    # Capabilities and metadata
    capabilities = Column(ARRAY(Text))
    response_area = Column(Text)
    active = Column(Boolean, default=True)
    notes = Column(Text)

    # Additional contacts
    additional_contacts = Column(JSONB, default=[])

    # External API Integration (SEDAP, KATWARN, etc.)
    # Actual endpoint configs are stored in config files
    api_enabled = Column(Boolean, default=False)
    api_type = Column(String(50))

    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    updated_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)


class OrganizationCreate(BaseModel):
    """Request model for creating an organization"""
    name: str
    short_name: Optional[str] = None
    type: str
    contact_person: Optional[str] = None
    phone: Optional[str] = None
    email: Optional[str] = None
    emergency_phone: Optional[str] = None
    address: Optional[str] = None
    city: Optional[str] = None
    country: Optional[str] = "Germany"
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    capabilities: Optional[List[str]] = []
    response_area: Optional[str] = None
    active: Optional[bool] = True
    notes: Optional[str] = None
    api_enabled: Optional[bool] = False
    api_type: Optional[str] = None


class OrganizationUpdate(BaseModel):
    """Request model for updating an organization"""
    name: Optional[str] = None
    short_name: Optional[str] = None
    type: Optional[str] = None
    contact_person: Optional[str] = None
    phone: Optional[str] = None
    email: Optional[str] = None
    emergency_phone: Optional[str] = None
    address: Optional[str] = None
    city: Optional[str] = None
    country: Optional[str] = None
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    capabilities: Optional[List[str]] = None
    response_area: Optional[str] = None
    active: Optional[bool] = None
    notes: Optional[str] = None
    api_enabled: Optional[bool] = None
    api_type: Optional[str] = None


class OrganizationResponse(BaseModel):
    """Response model for organization"""
    id: int
    name: str
    short_name: Optional[str]
    type: str
    contact_person: Optional[str]
    phone: Optional[str]
    email: Optional[str]
    emergency_phone: Optional[str]
    address: Optional[str]
    city: Optional[str]
    country: Optional[str]
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    capabilities: Optional[List[str]]
    response_area: Optional[str]
    active: bool
    notes: Optional[str]
    api_enabled: Optional[bool] = False
    api_type: Optional[str] = None
    created_at: str
    updated_at: str

    class Config:
        from_attributes = True
