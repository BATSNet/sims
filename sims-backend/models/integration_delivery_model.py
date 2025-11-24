"""
Integration Delivery Model - Tracks all delivery attempts
"""
from typing import Optional, Dict, Any
from datetime import datetime
from enum import Enum

from sqlalchemy import Column, BigInteger, String, TIMESTAMP, Text, ForeignKey, Integer
from sqlalchemy.dialects.postgresql import UUID, JSONB
from pydantic import BaseModel, Field

from db.connection import Base


class DeliveryStatus(str, Enum):
    """Delivery status enum"""
    PENDING = "pending"
    SUCCESS = "success"
    FAILED = "failed"
    TIMEOUT = "timeout"
    RETRYING = "retrying"


class IntegrationDeliveryORM(Base):
    """SQLAlchemy ORM model for integration_delivery table"""
    __tablename__ = "integration_delivery"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    incident_id = Column(UUID(as_uuid=True), ForeignKey('incident.id', ondelete='CASCADE'), nullable=False)
    organization_id = Column(BigInteger, ForeignKey('organization.id', ondelete='CASCADE'), nullable=False)
    integration_id = Column(BigInteger, ForeignKey('organization_integration.id', ondelete='SET NULL'))

    # Integration details (snapshot at delivery time)
    integration_type = Column(String(50), nullable=False)
    integration_name = Column(String(200))

    # Delivery details
    status = Column(String(20), nullable=False, default='pending')
    attempt_number = Column(Integer, default=1)

    # Request/response data
    request_payload = Column(JSONB)  # The actual payload sent
    request_url = Column(Text)  # Endpoint URL used
    response_code = Column(Integer)  # HTTP status code or equivalent
    response_body = Column(Text)  # Response from external system
    error_message = Column(Text)  # Error details if failed

    # Timing
    started_at = Column(TIMESTAMP(timezone=True), nullable=False, default=datetime.utcnow)
    completed_at = Column(TIMESTAMP(timezone=True))
    duration_ms = Column(Integer)  # Delivery duration in milliseconds

    # Metadata (use delivery_metadata to avoid SQLAlchemy reserved name)
    delivery_metadata = Column('metadata', JSONB, default={})  # Additional context, retry info, etc.


class IntegrationDeliveryResponse(BaseModel):
    """Response model for integration delivery"""
    id: int
    incident_id: str
    organization_id: int
    integration_id: Optional[int]
    integration_type: str
    integration_name: Optional[str]
    status: str
    attempt_number: int
    request_url: Optional[str]
    response_code: Optional[int]
    error_message: Optional[str]
    started_at: str
    completed_at: Optional[str]
    duration_ms: Optional[int]
    delivery_metadata: Dict[str, Any] = Field(default_factory=dict, alias='metadata', serialization_alias='metadata')

    class Config:
        from_attributes = True
        populate_by_name = True


class IntegrationDeliveryListResponse(BaseModel):
    """Response model for listing deliveries"""
    deliveries: list[IntegrationDeliveryResponse]
    total: int
    page: int
    page_size: int
