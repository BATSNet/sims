"""
Inbound Webhook Model - Manages webhook endpoints for receiving external incidents
"""
from typing import Optional, Dict, Any, List
from datetime import datetime
import secrets

from sqlalchemy import Column, BigInteger, String, TIMESTAMP, Boolean, Text, ForeignKey
from sqlalchemy.dialects.postgresql import JSONB
from pydantic import BaseModel, Field

from db.connection import Base


class InboundWebhookORM(Base):
    """SQLAlchemy ORM model for inbound_webhook table"""
    __tablename__ = "inbound_webhook"

    id = Column(BigInteger, primary_key=True, autoincrement=True)

    # Webhook identity
    name = Column(String(200), nullable=False)
    description = Column(Text)
    webhook_token = Column(String(100), unique=True, nullable=False)  # Used in URL path

    # Security
    auth_token = Column(String(255), nullable=False)  # Bearer token for authentication
    allowed_ips = Column(JSONB, default=[])  # IP whitelist (empty = allow all)

    # Source information
    source_name = Column(String(200))  # e.g., "WhatsApp Business", "n8n Workflow", "Zapier"
    source_type = Column(String(50))  # e.g., "whatsapp", "n8n", "zapier", "custom"

    # Payload transformation
    # Maps external payload structure to SIMS incident format
    # Example: {"title": "$.message.text", "latitude": "$.location.lat", "longitude": "$.location.lng"}
    field_mapping = Column(JSONB, nullable=False, default={})

    # Default values for created incidents
    default_values = Column(JSONB, default={})  # e.g., {"priority": "medium", "category": "External Report"}

    # Auto-assignment
    auto_assign_to_org = Column(BigInteger, ForeignKey('organization.id', ondelete='SET NULL'))

    # Status and metrics
    active = Column(Boolean, default=True)
    total_received = Column(BigInteger, default=0)
    last_received_at = Column(TIMESTAMP(timezone=True))

    # Metadata
    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    updated_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    created_by = Column(String(100))

    # Relationships will be configured in models/__init__.py


def generate_webhook_token() -> str:
    """Generate a URL-safe webhook token"""
    return secrets.token_urlsafe(32)


def generate_auth_token() -> str:
    """Generate a secure authentication token"""
    return secrets.token_urlsafe(48)


class InboundWebhookCreate(BaseModel):
    """Request model for creating an inbound webhook"""
    name: str
    description: Optional[str] = None
    source_name: Optional[str] = None
    source_type: Optional[str] = None
    field_mapping: Dict[str, Any] = Field(default_factory=dict)
    default_values: Dict[str, Any] = Field(default_factory=dict)
    auto_assign_to_org: Optional[int] = None
    allowed_ips: List[str] = Field(default_factory=list)
    active: bool = True
    created_by: Optional[str] = None


class InboundWebhookUpdate(BaseModel):
    """Request model for updating an inbound webhook"""
    name: Optional[str] = None
    description: Optional[str] = None
    source_name: Optional[str] = None
    source_type: Optional[str] = None
    field_mapping: Optional[Dict[str, Any]] = None
    default_values: Optional[Dict[str, Any]] = None
    auto_assign_to_org: Optional[int] = None
    allowed_ips: Optional[List[str]] = None
    active: Optional[bool] = None


class InboundWebhookResponse(BaseModel):
    """Response model for inbound webhook"""
    id: int
    name: str
    description: Optional[str]
    webhook_url: str  # Computed field with full URL
    webhook_token: str
    auth_token: str  # Only shown on creation, hidden afterwards
    source_name: Optional[str]
    source_type: Optional[str]
    field_mapping: Dict[str, Any]
    default_values: Dict[str, Any]
    auto_assign_to_org: Optional[int]
    allowed_ips: List[str]
    active: bool
    total_received: int
    last_received_at: Optional[str]
    created_at: str
    updated_at: str
    created_by: Optional[str]

    class Config:
        from_attributes = True


class InboundWebhookListResponse(BaseModel):
    """Response model for inbound webhook without sensitive data"""
    id: int
    name: str
    description: Optional[str]
    webhook_url: str
    source_name: Optional[str]
    source_type: Optional[str]
    active: bool
    total_received: int
    last_received_at: Optional[str]
    created_at: str

    class Config:
        from_attributes = True
