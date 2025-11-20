"""
Lightweight Chat History Service for SIMS
Langchain-compatible message format without langchain dependencies
"""
from typing import List, Dict, Any, Optional
from datetime import datetime
import json
import logging

logger = logging.getLogger(__name__)


class ChatMessage:
    """
    Langchain-compatible message format.

    Message structure matches langchain's format:
    {
        "type": "human" | "ai" | "system",
        "data": {
            "content": "message content",
            "additional_kwargs": {}
        }
    }
    """

    def __init__(self, role: str, content: str, additional_kwargs: Optional[Dict[str, Any]] = None):
        """
        Create a chat message.

        Args:
            role: "user", "assistant", or "system"
            content: Message content
            additional_kwargs: Additional metadata
        """
        # Map role to langchain type
        if role == "user":
            self.type = "human"
        elif role == "assistant":
            self.type = "ai"
        elif role == "system":
            self.type = "system"
        else:
            raise ValueError(f"Invalid role: {role}. Must be 'user', 'assistant', or 'system'")

        self.content = content
        self.additional_kwargs = additional_kwargs or {}

    def to_dict(self) -> Dict[str, Any]:
        """Convert message to langchain-compatible dict format."""
        return {
            "type": self.type,
            "data": {
                "content": self.content,
                "additional_kwargs": self.additional_kwargs
            }
        }

    @staticmethod
    def from_dict(data: Dict[str, Any]) -> 'ChatMessage':
        """Create ChatMessage from langchain-compatible dict."""
        msg_type = data.get("type", "human")
        msg_data = data.get("data", {})
        content = msg_data.get("content", "")
        additional_kwargs = msg_data.get("additional_kwargs", {})

        # Map langchain type back to role
        if msg_type == "human":
            role = "user"
        elif msg_type == "ai":
            role = "assistant"
        else:
            role = "system"

        return ChatMessage(role, content, additional_kwargs)

    @property
    def role(self) -> str:
        """Get role from type."""
        if self.type == "human":
            return "user"
        elif self.type == "ai":
            return "assistant"
        else:
            return "system"


class ChatHistory:
    """
    Simple PostgreSQL chat history manager.

    Stores chat messages in langchain-compatible JSONB format.
    Provides methods to add, retrieve, and clear messages.
    """

    def __init__(self, db_connection, session_id: str):
        """
        Initialize chat history for a session.

        Args:
            db_connection: psycopg2 connection
            session_id: UUID string for the chat session
        """
        self.db = db_connection
        self.session_id = session_id

    def add_message(self, role: str, content: str, additional_kwargs: Optional[Dict[str, Any]] = None):
        """
        Add a message to the chat history.

        Args:
            role: "user", "assistant", or "system"
            content: Message content
            additional_kwargs: Optional metadata
        """
        try:
            from models.chat_model import ChatMessageORM
            message = ChatMessage(role, content, additional_kwargs)
            message_dict = message.to_dict()

            # Create ORM object
            chat_message = ChatMessageORM(
                session_id=self.session_id,
                message=message_dict
            )

            self.db.add(chat_message)
            self.db.commit()

            logger.info(f"Added {role} message to session {self.session_id}")

        except Exception as e:
            logger.error(f"Error adding message to chat history: {e}", exc_info=True)
            self.db.rollback()
            raise

    def get_messages(self) -> List[Dict[str, Any]]:
        """
        Get all messages for the session.

        Returns:
            List of message dicts with role, content, and timestamp
        """
        try:
            from models.chat_model import ChatMessageORM

            # Query messages using SQLAlchemy
            chat_messages = self.db.query(ChatMessageORM).filter(
                ChatMessageORM.session_id == self.session_id
            ).order_by(ChatMessageORM.id.asc()).all()

            messages = []
            for chat_msg in chat_messages:
                msg_data = chat_msg.message  # JSONB data
                timestamp = chat_msg.created_at

                # Parse message from langchain format
                msg = ChatMessage.from_dict(msg_data)

                messages.append({
                    "role": msg.role,
                    "content": msg.content,
                    "timestamp": timestamp.isoformat() if timestamp else None,
                    "additional_kwargs": msg.additional_kwargs
                })

            logger.info(f"Retrieved {len(messages)} messages for session {self.session_id}")
            return messages

        except Exception as e:
            logger.error(f"Error retrieving messages: {e}", exc_info=True)
            raise

    def get_messages_raw(self) -> List[Dict[str, Any]]:
        """
        Get messages in raw langchain format (for potential future langchain integration).

        Returns:
            List of raw langchain-compatible message dicts
        """
        try:
            from models.chat_model import ChatMessageORM

            # Query messages using SQLAlchemy
            chat_messages = self.db.query(ChatMessageORM).filter(
                ChatMessageORM.session_id == self.session_id
            ).order_by(ChatMessageORM.id.asc()).all()

            return [chat_msg.message for chat_msg in chat_messages]

        except Exception as e:
            logger.error(f"Error retrieving raw messages: {e}", exc_info=True)
            raise

    def clear(self):
        """Clear all messages for the session."""
        try:
            from models.chat_model import ChatMessageORM

            # Delete messages using SQLAlchemy
            self.db.query(ChatMessageORM).filter(
                ChatMessageORM.session_id == self.session_id
            ).delete()

            self.db.commit()

            logger.info(f"Cleared all messages for session {self.session_id}")

        except Exception as e:
            logger.error(f"Error clearing messages: {e}", exc_info=True)
            self.db.rollback()
            raise

    def count_messages(self) -> int:
        """Get the number of messages in the session."""
        try:
            from models.chat_model import ChatMessageORM

            # Count messages using SQLAlchemy
            count = self.db.query(ChatMessageORM).filter(
                ChatMessageORM.session_id == self.session_id
            ).count()

            return count

        except Exception as e:
            logger.error(f"Error counting messages: {e}", exc_info=True)
            raise


def create_chat_session(db_connection, incident_id: str, user_phone: Optional[str] = None, session_id: Optional[str] = None) -> str:
    """
    Create a new chat session for an incident.

    Args:
        db_connection: SQLAlchemy Session
        incident_id: UUID of the incident
        user_phone: Optional phone number of the user
        session_id: Optional app-generated session ID (UUID string)

    Returns:
        session_id (UUID string)
    """
    try:
        from models.chat_model import ChatSessionORM
        import uuid as uuid_module

        # Use app-provided session_id if available
        session_uuid = uuid_module.UUID(session_id) if session_id else uuid_module.uuid4()

        # Create chat session ORM object
        chat_session = ChatSessionORM(
            session_id=session_uuid,
            incident_id=incident_id,
            user_phone=user_phone
        )

        db_connection.add(chat_session)
        db_connection.commit()
        db_connection.refresh(chat_session)

        logger.info(f"Created chat session {chat_session.session_id} for incident {incident_id}")
        return str(chat_session.session_id)

    except Exception as e:
        logger.error(f"Error creating chat session: {e}", exc_info=True)
        db_connection.rollback()
        raise


def get_session_by_incident(db_connection, incident_id: str) -> Optional[str]:
    """
    Get the session_id for an incident.

    Args:
        db_connection: SQLAlchemy Session
        incident_id: UUID of the incident

    Returns:
        session_id (UUID string) or None if not found
    """
    try:
        from models.chat_model import ChatSessionORM

        # Query for chat session
        chat_session = db_connection.query(ChatSessionORM).filter(
            ChatSessionORM.incident_id == incident_id
        ).first()

        if chat_session:
            return str(chat_session.session_id)
        return None

    except Exception as e:
        logger.error(f"Error getting session for incident: {e}", exc_info=True)
        raise
