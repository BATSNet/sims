"""
Organization Token Model - Access tokens for responder portal
"""
from typing import Optional
from datetime import datetime

from sqlalchemy import Column, BigInteger, String, TIMESTAMP, Boolean, ForeignKey
from sqlalchemy.orm import relationship
from pydantic import BaseModel

from db.connection import Base


class OrganizationTokenORM(Base):
    """SQLAlchemy ORM model for organization_tokens table"""
    __tablename__ = "organization_tokens"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    organization_id = Column(BigInteger, ForeignKey('organization.id', ondelete='CASCADE'), nullable=False)
    token = Column(String(255), unique=True, nullable=False)
    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    expires_at = Column(TIMESTAMP(timezone=True))
    created_by = Column(String(100))
    last_used_at = Column(TIMESTAMP(timezone=True))
    active = Column(Boolean, default=True)

    # Relationship to organization
    organization = relationship("OrganizationORM", backref="tokens")


class OrganizationTokenCreate(BaseModel):
    """Request model for creating an organization token"""
    organization_id: int
    created_by: Optional[str] = None
    expires_at: Optional[datetime] = None


class OrganizationTokenResponse(BaseModel):
    """Response model for organization token"""
    id: int
    organization_id: int
    token: str
    created_at: str
    expires_at: Optional[str]
    created_by: Optional[str]
    last_used_at: Optional[str]
    active: bool

    class Config:
        from_attributes = True
