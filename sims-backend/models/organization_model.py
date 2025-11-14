"""
Organization Model - Organizations for routing incidents
"""
from typing import Optional, Dict, Any
from datetime import datetime

from sqlalchemy import Column, BigInteger, String, TIMESTAMP
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.ext.declarative import declarative_base
from pydantic import BaseModel

Base = declarative_base()


class OrganizationORM(Base):
    """SQLAlchemy ORM model for organization table"""
    __tablename__ = "organization"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    name = Column(String(100), nullable=False)
    type = Column(String(50))
    contact_info = Column(JSONB, default={})
    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)


class OrganizationCreate(BaseModel):
    """Request model for creating an organization"""
    name: str
    type: Optional[str] = None
    contact_info: Optional[Dict[str, Any]] = {}


class OrganizationResponse(BaseModel):
    """Response model for organization"""
    id: int
    name: str
    type: Optional[str]
    contact_info: Optional[Dict[str, Any]]

    class Config:
        from_attributes = True
