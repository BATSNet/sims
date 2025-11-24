"""
Organization Integration Model - Organization-specific integration instances
"""
from typing import Optional, Dict, Any, List
from datetime import datetime

from sqlalchemy import Column, BigInteger, String, TIMESTAMP, Boolean, Text, ForeignKey
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.orm import relationship
from pydantic import BaseModel, Field

from db.connection import Base


class OrganizationIntegrationORM(Base):
    """SQLAlchemy ORM model for organization_integration table"""
    __tablename__ = "organization_integration"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    organization_id = Column(BigInteger, ForeignKey('organization.id', ondelete='CASCADE'), nullable=False)
    template_id = Column(BigInteger, ForeignKey('integration_template.id', ondelete='CASCADE'), nullable=False)

    # Custom name for this integration instance
    name = Column(String(200), nullable=False)
    description = Column(Text)

    # Configuration values (validated against template's config_schema)
    # Example: {"endpoint_url": "https://org.example.com/webhook", "timeout": 60}
    config = Column(JSONB, nullable=False, default={})

    # Authentication credentials (encrypted at rest)
    # Example: {"token": "encrypted_bearer_token"} or {"username": "user", "password": "encrypted_pass"}
    auth_credentials = Column(JSONB, default={})

    # Custom payload template override (optional, uses template default if null)
    custom_payload_template = Column(Text)

    # Filters for when to trigger this integration
    # Example: {"priorities": ["critical", "high"], "categories": ["Security", "Military"]}
    trigger_filters = Column(JSONB, default={})

    # Status
    active = Column(Boolean, default=True)
    last_delivery_at = Column(TIMESTAMP(timezone=True))
    last_delivery_status = Column(String(20))  # success, failed, pending

    # Metadata
    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    updated_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    created_by = Column(String(100))

    # Relationships will be configured in models/__init__.py


class OrganizationIntegrationCreate(BaseModel):
    """Request model for creating an organization integration"""
    organization_id: int
    template_id: int
    name: str
    description: Optional[str] = None
    config: Dict[str, Any] = Field(default_factory=dict)
    auth_credentials: Dict[str, Any] = Field(default_factory=dict)
    custom_payload_template: Optional[str] = None
    trigger_filters: Dict[str, Any] = Field(default_factory=dict)
    active: bool = True
    created_by: Optional[str] = None


class OrganizationIntegrationUpdate(BaseModel):
    """Request model for updating an organization integration"""
    name: Optional[str] = None
    description: Optional[str] = None
    config: Optional[Dict[str, Any]] = None
    auth_credentials: Optional[Dict[str, Any]] = None
    custom_payload_template: Optional[str] = None
    trigger_filters: Optional[Dict[str, Any]] = None
    active: Optional[bool] = None


class OrganizationIntegrationResponse(BaseModel):
    """Response model for organization integration"""
    id: int
    organization_id: int
    template_id: int
    template_name: Optional[str] = None
    template_type: Optional[str] = None
    name: str
    description: Optional[str]
    config: Dict[str, Any]
    auth_credentials_configured: bool = Field(default=False)  # Don't expose actual credentials
    custom_payload_template: Optional[str]
    trigger_filters: Dict[str, Any]
    active: bool
    last_delivery_at: Optional[str]
    last_delivery_status: Optional[str]
    created_at: str
    updated_at: str
    created_by: Optional[str]

    class Config:
        from_attributes = True
