"""
Integration Template Model - Admin-managed integration types
"""
from typing import Optional, Dict, Any, List
from datetime import datetime
from enum import Enum

from sqlalchemy import Column, BigInteger, String, TIMESTAMP, Boolean, Text
from sqlalchemy.dialects.postgresql import JSONB
from pydantic import BaseModel, Field

from db.connection import Base


class IntegrationType(str, Enum):
    """Integration type enum"""
    WEBHOOK = "webhook"
    SEDAP = "sedap"
    EMAIL = "email"
    SMS = "sms"
    WHATSAPP = "whatsapp"
    N8N = "n8n"
    CUSTOM = "custom"


class AuthenticationType(str, Enum):
    """Authentication type enum"""
    NONE = "none"
    BEARER_TOKEN = "bearer_token"
    API_KEY = "api_key"
    BASIC_AUTH = "basic_auth"
    OAUTH2 = "oauth2"
    CUSTOM_HEADER = "custom_header"


class IntegrationTemplateORM(Base):
    """SQLAlchemy ORM model for integration_template table"""
    __tablename__ = "integration_template"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    name = Column(String(100), nullable=False, unique=True)
    type = Column(String(50), nullable=False)
    description = Column(Text)

    # Configuration schema defines what fields are required/optional
    # Example: {"endpoint_url": {"type": "string", "required": true}, "timeout": {"type": "integer", "default": 30}}
    config_schema = Column(JSONB, nullable=False, default={})

    # Default payload template (Jinja2 format for webhook, email body, etc.)
    # Variables: {{incident.id}}, {{incident.title}}, {{incident.latitude}}, etc.
    payload_template = Column(Text)

    # Authentication configuration
    auth_type = Column(String(50), nullable=False, default='none')
    auth_schema = Column(JSONB, default={})  # Schema for auth credentials

    # Delivery settings
    timeout_seconds = Column(BigInteger, default=30)
    retry_enabled = Column(Boolean, default=False)
    retry_attempts = Column(BigInteger, default=0)

    # Metadata
    active = Column(Boolean, default=True)
    system_template = Column(Boolean, default=False)  # True for built-in templates (SEDAP)

    created_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    updated_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    created_by = Column(String(100))


class IntegrationTemplateCreate(BaseModel):
    """Request model for creating an integration template"""
    name: str
    type: IntegrationType
    description: Optional[str] = None
    config_schema: Dict[str, Any] = Field(default_factory=dict)
    payload_template: Optional[str] = None
    auth_type: AuthenticationType = AuthenticationType.NONE
    auth_schema: Dict[str, Any] = Field(default_factory=dict)
    timeout_seconds: int = 30
    retry_enabled: bool = False
    retry_attempts: int = 0
    active: bool = True
    created_by: Optional[str] = None


class IntegrationTemplateUpdate(BaseModel):
    """Request model for updating an integration template"""
    name: Optional[str] = None
    description: Optional[str] = None
    config_schema: Optional[Dict[str, Any]] = None
    payload_template: Optional[str] = None
    auth_type: Optional[AuthenticationType] = None
    auth_schema: Optional[Dict[str, Any]] = None
    timeout_seconds: Optional[int] = None
    retry_enabled: Optional[bool] = None
    retry_attempts: Optional[int] = None
    active: Optional[bool] = None


class IntegrationTemplateResponse(BaseModel):
    """Response model for integration template"""
    id: int
    name: str
    type: str
    description: Optional[str]
    config_schema: Dict[str, Any]
    payload_template: Optional[str]
    auth_type: str
    auth_schema: Dict[str, Any]
    timeout_seconds: int
    retry_enabled: bool
    retry_attempts: int
    active: bool
    system_template: bool
    created_at: str
    updated_at: str
    created_by: Optional[str]

    class Config:
        from_attributes = True
