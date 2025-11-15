"""
Models package - Import all models and configure relationships
"""
from sqlalchemy.orm import relationship

# Import all ORM models first
from models.incident_model import IncidentORM, IncidentCreate, IncidentUpdate, IncidentResponse
from models.chat_model import ChatSessionORM, ChatMessageORM
from models.media_model import MediaORM
from models.organization_model import OrganizationORM

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

__all__ = [
    'IncidentORM',
    'IncidentCreate',
    'IncidentUpdate',
    'IncidentResponse',
    'ChatSessionORM',
    'ChatMessageORM',
    'MediaORM',
    'OrganizationORM',
]
