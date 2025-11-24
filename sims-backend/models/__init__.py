"""
Models package - Import all models and configure relationships
"""
from sqlalchemy.orm import relationship

# Import all ORM models first
from models.incident_model import IncidentORM, IncidentCreate, IncidentUpdate, IncidentResponse
from models.chat_model import ChatSessionORM, ChatMessageORM
from models.media_model import MediaORM
from models.organization_model import OrganizationORM
from models.integration_template_model import IntegrationTemplateORM
from models.organization_integration_model import OrganizationIntegrationORM
from models.integration_delivery_model import IntegrationDeliveryORM
from models.inbound_webhook_model import InboundWebhookORM

# Configure relationships after all classes are defined
IncidentORM.chat_sessions = relationship(ChatSessionORM, back_populates="incident", cascade="all, delete-orphan")
IncidentORM.media_files = relationship(MediaORM, back_populates="incident", cascade="all, delete-orphan")
IncidentORM.organization = relationship(OrganizationORM, foreign_keys=[IncidentORM.routed_to])

ChatSessionORM.incident = relationship(IncidentORM, back_populates="chat_sessions")
ChatSessionORM.messages = relationship(ChatMessageORM, back_populates="session", cascade="all, delete-orphan")

ChatMessageORM.session = relationship(ChatSessionORM, back_populates="messages")
ChatMessageORM.media_files = relationship(MediaORM, back_populates="chat_message")

MediaORM.incident = relationship(IncidentORM, back_populates="media_files")
MediaORM.chat_message = relationship(ChatMessageORM, back_populates="media_files")

# Integration relationships
OrganizationIntegrationORM.organization = relationship(OrganizationORM, foreign_keys=[OrganizationIntegrationORM.organization_id])
OrganizationIntegrationORM.template = relationship(IntegrationTemplateORM, foreign_keys=[OrganizationIntegrationORM.template_id])

IntegrationDeliveryORM.incident = relationship(IncidentORM, foreign_keys=[IntegrationDeliveryORM.incident_id])
IntegrationDeliveryORM.organization = relationship(OrganizationORM, foreign_keys=[IntegrationDeliveryORM.organization_id])
IntegrationDeliveryORM.integration = relationship(OrganizationIntegrationORM, foreign_keys=[IntegrationDeliveryORM.integration_id])

InboundWebhookORM.organization = relationship(OrganizationORM, foreign_keys=[InboundWebhookORM.auto_assign_to_org])

__all__ = [
    'IncidentORM',
    'IncidentCreate',
    'IncidentUpdate',
    'IncidentResponse',
    'ChatSessionORM',
    'ChatMessageORM',
    'MediaORM',
    'OrganizationORM',
    'IntegrationTemplateORM',
    'OrganizationIntegrationORM',
    'IntegrationDeliveryORM',
    'InboundWebhookORM',
]
