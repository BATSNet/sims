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
            cursor = self.db.cursor()
            cursor.execute(
                """
                SELECT message, created_at
                FROM chat_message
                WHERE session_id = %s
                ORDER BY id ASC
                """,
                (self.session_id,)
            )

            rows = cursor.fetchall()
            cursor.close()

            messages = []
            for row in rows:
                msg_data = row[0]  # JSONB data
                timestamp = row[1]

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
            cursor = self.db.cursor()
            cursor.execute(
                """
                SELECT message
                FROM chat_message
                WHERE session_id = %s
                ORDER BY id ASC
                """,
                (self.session_id,)
            )

            rows = cursor.fetchall()
            cursor.close()

            return [row[0] for row in rows]

        except Exception as e:
            logger.error(f"Error retrieving raw messages: {e}", exc_info=True)
            raise

    def clear(self):
        """Clear all messages for the session."""
        try:
            cursor = self.db.cursor()
            cursor.execute(
                "DELETE FROM chat_message WHERE session_id = %s",
                (self.session_id,)
            )
            self.db.commit()
            cursor.close()

            logger.info(f"Cleared all messages for session {self.session_id}")

        except Exception as e:
            logger.error(f"Error clearing messages: {e}", exc_info=True)
            self.db.rollback()
            raise

    def count_messages(self) -> int:
        """Get the number of messages in the session."""
        try:
            cursor = self.db.cursor()
            cursor.execute(
                "SELECT COUNT(*) FROM chat_message WHERE session_id = %s",
                (self.session_id,)
            )
            count = cursor.fetchone()[0]
            cursor.close()
            return count

        except Exception as e:
            logger.error(f"Error counting messages: {e}", exc_info=True)
            raise


def create_chat_session(db_connection, incident_id: str, user_phone: Optional[str] = None) -> str:
    """
    Create a new chat session for an incident.

    Args:
        db_connection: psycopg2 connection
        incident_id: UUID of the incident
        user_phone: Optional phone number of the user

    Returns:
        session_id (UUID string)
    """
    try:
        cursor = db_connection.cursor()
        cursor.execute(
            """
            INSERT INTO chat_session (incident_id, user_phone)
            VALUES (%s, %s)
            RETURNING session_id
            """,
            (incident_id, user_phone)
        )
        session_id = cursor.fetchone()[0]
        db_connection.commit()
        cursor.close()

        logger.info(f"Created chat session {session_id} for incident {incident_id}")
        return str(session_id)

    except Exception as e:
        logger.error(f"Error creating chat session: {e}", exc_info=True)
        db_connection.rollback()
        raise


def get_session_by_incident(db_connection, incident_id: str) -> Optional[str]:
    """
    Get the session_id for an incident.

    Args:
        db_connection: psycopg2 connection
        incident_id: UUID of the incident

    Returns:
        session_id (UUID string) or None if not found
    """
    try:
        cursor = db_connection.cursor()
        cursor.execute(
            "SELECT session_id FROM chat_session WHERE incident_id = %s LIMIT 1",
            (incident_id,)
        )
        row = cursor.fetchone()
        cursor.close()

        if row:
            return str(row[0])
        return None

    except Exception as e:
        logger.error(f"Error getting session for incident: {e}", exc_info=True)
        raise
