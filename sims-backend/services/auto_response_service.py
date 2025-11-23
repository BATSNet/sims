"""
Automated Chat Response Service for SIMS

Handles automated responses to incident reports:
- Typing indicator after first media upload
- Thank you message after organization assignment (30s delay)
- Fallback message if no assignment after 45s
"""
import asyncio
import logging
from datetime import datetime
from typing import Optional
from uuid import UUID

from sqlalchemy.orm import Session

from models.incident_model import IncidentORM
from models.chat_model import ChatMessageORM
from models.organization_model import OrganizationORM
from websocket import websocket_manager
from i18n import i18n

logger = logging.getLogger(__name__)


class AutoResponseService:
    """Service for sending automated chat responses"""

    def __init__(self):
        self.pending_responses = {}  # Track pending responses by incident_id
        self.response_tasks = {}  # Track asyncio tasks

    async def send_typing_indicator(self, incident_id: str, session_id: str):
        """
        Send typing indicator to show system is processing

        Args:
            incident_id: Incident ID
            session_id: Chat session ID
        """
        try:
            message = {
                'type': 'incident_message',
                'incident_id': incident_id,
                'session_id': session_id,
                'message': {
                    'type': 'ai',
                    'data': {
                        'content': '...',
                        'is_typing': True,
                    }
                },
                'timestamp': datetime.utcnow().isoformat()
            }

            # Broadcast to incidents channel where mobile app is subscribed
            await websocket_manager.broadcast_to_all(message, topic='incidents')
            logger.info(f"Sent typing indicator for incident {incident_id}")
        except Exception as e:
            logger.error(f"Error sending typing indicator: {e}", exc_info=True)

    async def schedule_auto_response(
        self,
        incident_id: str,
        session_id: str,
        db_session_factory
    ):
        """
        Schedule automated response after incident creation

        Waits for organization assignment (max 30s), then sends thank you message.
        If no assignment after 45s, sends fallback message.

        Args:
            incident_id: Incident ID (UUID string)
            session_id: Chat session ID (UUID string)
            db_session_factory: Factory to create new database sessions
        """
        try:
            # Send typing indicator immediately
            await self.send_typing_indicator(incident_id, session_id)

            # Wait up to 30 seconds for organization assignment
            for i in range(60):  # Check every 0.5s for 30s
                await asyncio.sleep(0.5)

                # Check if organization assigned
                db = db_session_factory()
                try:
                    incident = db.query(IncidentORM).filter(
                        IncidentORM.id == UUID(incident_id)
                    ).first()

                    if incident and incident.routed_to:
                        # Get organization name
                        org = db.query(OrganizationORM).filter(
                            OrganizationORM.id == incident.routed_to
                        ).first()

                        org_name = org.name if org else "the responsible organization"

                        # Send thank you message
                        await self._send_thank_you_message(
                            incident_id,
                            session_id,
                            org_name,
                            db
                        )
                        logger.info(f"Sent organization notification for {incident_id}")
                        return
                finally:
                    db.close()

            # If we get here, 30s passed without assignment
            # Wait another 15s (total 45s) then send fallback
            await asyncio.sleep(15)

            # Check one more time if assigned
            db = db_session_factory()
            try:
                incident = db.query(IncidentORM).filter(
                    IncidentORM.id == UUID(incident_id)
                ).first()

                if incident and incident.routed_to:
                    org = db.query(OrganizationORM).filter(
                        OrganizationORM.id == incident.routed_to
                    ).first()
                    org_name = org.name if org else "the responsible organization"
                    await self._send_thank_you_message(incident_id, session_id, org_name, db)
                else:
                    # Send fallback message
                    await self._send_fallback_message(incident_id, session_id, db)
                    logger.info(f"Sent fallback message for {incident_id}")
            finally:
                db.close()

        except asyncio.CancelledError:
            logger.info(f"Auto-response cancelled for incident {incident_id}")
        except Exception as e:
            logger.error(f"Error in auto-response for {incident_id}: {e}", exc_info=True)

    async def _send_thank_you_message(
        self,
        incident_id: str,
        session_id: str,
        organization_name: str,
        db: Session
    ):
        """Send thank you message with organization notification"""
        try:
            message_text = i18n.t('messages.auto_response.org_notified', org_name=organization_name)

            # Save to database
            chat_message = ChatMessageORM(
                session_id=UUID(session_id),
                message={
                    'type': 'ai',
                    'data': {
                        'content': message_text
                    }
                },
                created_at=datetime.utcnow()
            )
            db.add(chat_message)
            db.commit()
            db.refresh(chat_message)

            # Broadcast via WebSocket
            message = {
                'type': 'incident_message',
                'incident_id': incident_id,
                'session_id': session_id,
                'message': {
                    'type': 'ai',
                    'data': {
                        'content': message_text
                    }
                },
                'timestamp': datetime.utcnow().isoformat()
            }

            # Broadcast to incidents channel where mobile app is subscribed
            await websocket_manager.broadcast_to_all(message, topic='incidents')

        except Exception as e:
            logger.error(f"Error sending thank you message: {e}", exc_info=True)

    async def _send_fallback_message(
        self,
        incident_id: str,
        session_id: str,
        db: Session
    ):
        """Send fallback message when no assignment within timeout"""
        try:
            message_text = i18n.t('messages.auto_response.fallback')

            # Save to database
            chat_message = ChatMessageORM(
                session_id=UUID(session_id),
                message={
                    'type': 'ai',
                    'data': {
                        'content': message_text
                    }
                },
                created_at=datetime.utcnow()
            )
            db.add(chat_message)
            db.commit()
            db.refresh(chat_message)

            # Broadcast via WebSocket
            message = {
                'type': 'incident_message',
                'incident_id': incident_id,
                'session_id': session_id,
                'message': {
                    'type': 'ai',
                    'data': {
                        'content': message_text
                    }
                },
                'timestamp': datetime.utcnow().isoformat()
            }

            # Broadcast to incidents channel where mobile app is subscribed
            await websocket_manager.broadcast_to_all(message, topic='incidents')

        except Exception as e:
            logger.error(f"Error sending fallback message: {e}", exc_info=True)


# Global instance
_auto_response_service: Optional[AutoResponseService] = None


def get_auto_response_service() -> AutoResponseService:
    """Get the global auto-response service instance"""
    global _auto_response_service

    if _auto_response_service is None:
        _auto_response_service = AutoResponseService()

    return _auto_response_service
