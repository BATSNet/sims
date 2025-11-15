"""
Scheduled Summarization and Transcription Service for SIMS

This service handles:
1. Batched audio transcription processing with asyncio
2. Chat history summarization using LLM
3. Efficient resource utilization with delayed batch processing
4. Database updates for Media table with transcriptions
"""
import asyncio
import logging
from datetime import datetime, timedelta
from typing import List, Dict, Optional, Any
from pathlib import Path
from collections import defaultdict
import httpx

from sqlalchemy.orm import Session
from sqlalchemy import and_

from models.media_model import MediaORM, MediaType
from models.chat_model import ChatSessionORM, ChatMessageORM
from models.incident_model import IncidentORM
from transcription_service import TranscriptionService
from config import Config

logger = logging.getLogger(__name__)


class BatchProcessor:
    """
    Batch processor for handling transcription and summarization tasks.

    Collects tasks over a delay period and processes them concurrently
    to make efficient use of API resources.
    """

    def __init__(
        self,
        batch_delay: float = 5.0,
        max_batch_size: int = 10,
        max_concurrent_requests: int = 5
    ):
        """
        Initialize the batch processor.

        Args:
            batch_delay: Seconds to wait before processing a batch (default: 5.0)
            max_batch_size: Maximum items in a single batch (default: 10)
            max_concurrent_requests: Max concurrent API requests (default: 5)
        """
        self.batch_delay = batch_delay
        self.max_batch_size = max_batch_size
        self.max_concurrent_requests = max_concurrent_requests

        self.transcription_queue: List[Dict[str, Any]] = []
        self.summarization_queue: List[Dict[str, Any]] = []
        self.processing_lock = asyncio.Lock()
        self.is_running = False
        self.background_task: Optional[asyncio.Task] = None

        self.transcription_service = TranscriptionService()

    async def start(self):
        """Start the background processing loop."""
        if self.is_running:
            logger.warning("Batch processor already running")
            return

        self.is_running = True
        self.background_task = asyncio.create_task(self._process_loop())
        logger.info("Batch processor started")

    async def stop(self):
        """Stop the background processing loop and process remaining items."""
        if not self.is_running:
            return

        self.is_running = False

        if self.background_task:
            self.background_task.cancel()
            try:
                await self.background_task
            except asyncio.CancelledError:
                pass

        # Process any remaining items
        await self._process_transcription_batch()
        await self._process_summarization_batch()

        logger.info("Batch processor stopped")

    async def queue_transcription(
        self,
        media_id: str,
        file_path: str,
        db_session: Session
    ):
        """
        Queue an audio file for transcription.

        Args:
            media_id: UUID of the media record
            file_path: Path to the audio file
            db_session: Database session (will be detached and recreated)
        """
        async with self.processing_lock:
            self.transcription_queue.append({
                'media_id': media_id,
                'file_path': file_path,
                'queued_at': datetime.utcnow()
            })

            logger.info(
                f"Queued transcription for media {media_id}. "
                f"Queue size: {len(self.transcription_queue)}"
            )

            # Process immediately if batch is full
            if len(self.transcription_queue) >= self.max_batch_size:
                await self._process_transcription_batch()

    async def queue_summarization(
        self,
        session_id: str,
        incident_id: str,
        db_session: Session
    ):
        """
        Queue a chat session for summarization.

        Args:
            session_id: Chat session UUID
            incident_id: Incident UUID
            db_session: Database session
        """
        async with self.processing_lock:
            self.summarization_queue.append({
                'session_id': session_id,
                'incident_id': incident_id,
                'queued_at': datetime.utcnow()
            })

            logger.info(
                f"Queued summarization for session {session_id}. "
                f"Queue size: {len(self.summarization_queue)}"
            )

            # Process immediately if batch is full
            if len(self.summarization_queue) >= self.max_batch_size:
                await self._process_summarization_batch()

    async def _process_loop(self):
        """Background loop that processes batches at regular intervals."""
        while self.is_running:
            try:
                await asyncio.sleep(self.batch_delay)

                # Process both queues
                await self._process_transcription_batch()
                await self._process_summarization_batch()

            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"Error in batch processing loop: {e}", exc_info=True)

    async def _process_transcription_batch(self):
        """Process queued transcription tasks in batch."""
        async with self.processing_lock:
            if not self.transcription_queue:
                return

            batch = self.transcription_queue[:self.max_batch_size]
            self.transcription_queue = self.transcription_queue[self.max_batch_size:]

        logger.info(f"Processing transcription batch of {len(batch)} items")

        # Create semaphore to limit concurrent API requests
        semaphore = asyncio.Semaphore(self.max_concurrent_requests)

        async def process_item(item: Dict[str, Any]):
            async with semaphore:
                await self._transcribe_and_update(
                    item['media_id'],
                    item['file_path']
                )

        # Process all items concurrently (up to max_concurrent_requests)
        await asyncio.gather(
            *[process_item(item) for item in batch],
            return_exceptions=True
        )

        logger.info(f"Completed transcription batch of {len(batch)} items")

    async def _transcribe_and_update(self, media_id: str, file_path: str):
        """
        Transcribe audio file and update database.
        Triggers re-classification if media is linked to an incident.

        Args:
            media_id: Media record UUID
            file_path: Path to audio file
        """
        try:
            # Check if file exists
            if not Path(file_path).exists():
                logger.error(f"Audio file not found: {file_path}")
                return

            # Transcribe audio
            transcription_text = await self.transcription_service.transcribe_and_get_text(
                file_path
            )

            if not transcription_text:
                logger.warning(f"Transcription failed for media {media_id}")
                return

            # Update database with transcription
            from db.connection import session as Session
            db = Session()
            incident_id = None
            try:
                media = db.query(MediaORM).filter(
                    MediaORM.id == media_id
                ).first()

                if media:
                    media.transcription = transcription_text
                    incident_id = media.incident_id
                    db.commit()
                    logger.info(
                        f"Updated transcription for media {media_id}: "
                        f"{transcription_text[:50]}..."
                    )
                else:
                    logger.warning(f"Media record not found: {media_id}")

            except Exception as e:
                logger.error(f"Database error updating media {media_id}: {e}")
                db.rollback()
            finally:
                db.close()

            # Trigger re-classification if media is linked to an incident
            if incident_id:
                logger.info(f"Triggering re-classification for incident {incident_id} after transcription")
                await self._reclassify_incident(incident_id, transcription_text)

        except Exception as e:
            logger.error(
                f"Error transcribing media {media_id}: {e}",
                exc_info=True
            )

    async def _reclassify_incident(self, incident_id: str, transcription: str):
        """
        Re-classify an incident with new transcription data.

        Args:
            incident_id: Incident UUID
            transcription: New transcription text
        """
        try:
            from db.connection import session as Session
            from services.classification_service import get_classifier
            from services.assignment_service import get_assignment_service

            db = Session()
            try:
                incident = db.query(IncidentORM).filter(
                    IncidentORM.id == incident_id
                ).first()

                if not incident:
                    logger.warning(f"Incident not found for re-classification: {incident_id}")
                    return

                # Store old classification for comparison
                old_category = incident.category
                old_priority = incident.priority
                old_classification = incident.meta_data.get('classification', {}) if incident.meta_data else {}

                logger.info(f"Re-classifying incident {incident.incident_id} with transcription")

                # Get classifier and re-classify
                classifier = get_classifier()
                classification = await classifier.classify_incident(
                    description=incident.description,
                    transcription=transcription,
                    latitude=incident.latitude,
                    longitude=incident.longitude,
                    heading=incident.heading
                )

                # Update incident with new classification
                incident.category = classification.category
                incident.priority = classification.priority
                incident.tags = classification.tags

                # Store classification history
                if not incident.meta_data:
                    incident.meta_data = {}

                if 'classification_history' not in incident.meta_data:
                    incident.meta_data['classification_history'] = []

                incident.meta_data['classification_history'].append({
                    'timestamp': datetime.utcnow().isoformat(),
                    'trigger': 'transcription_complete',
                    'old_category': old_category,
                    'new_category': classification.category,
                    'old_priority': old_priority,
                    'new_priority': classification.priority,
                    'confidence': classification.confidence,
                    'transcription_available': True
                })

                incident.meta_data['classification'] = classification.to_dict()
                incident.meta_data['classification_timestamp'] = datetime.utcnow().isoformat()

                db.commit()
                db.refresh(incident)

                logger.info(
                    f"Re-classified incident {incident.incident_id}: "
                    f"{old_category} -> {classification.category}, "
                    f"priority {old_priority} -> {classification.priority} "
                    f"(confidence: {classification.confidence:.2f})"
                )

                # Trigger re-assignment if classification changed significantly
                # and incident was auto-assigned (not manually assigned)
                assignment_history = incident.meta_data.get('assignment_history', [])
                was_auto_assigned = (
                    len(assignment_history) > 0 and
                    assignment_history[-1].get('auto_assigned', False)
                )

                if was_auto_assigned and classification.category != old_category:
                    logger.info(f"Category changed from {old_category} to {classification.category}, triggering re-assignment")
                    await self._reassign_incident(incident, classification, db)

                # Broadcast update via WebSocket
                try:
                    from websocket import websocket_manager
                    await websocket_manager.broadcast_incident(
                        incident_data={
                            'incident_id': incident.incident_id,
                            'category': incident.category,
                            'priority': incident.priority,
                            'tags': incident.tags,
                            'transcription': transcription,
                            'classification': classification.to_dict()
                        },
                        event_type='transcription_complete'
                    )
                except Exception as ws_error:
                    logger.error(f"Failed to broadcast transcription_complete: {ws_error}")

            except Exception as e:
                logger.error(f"Error re-classifying incident {incident_id}: {e}", exc_info=True)
                db.rollback()
            finally:
                db.close()

        except Exception as e:
            logger.error(f"Failed to re-classify incident {incident_id}: {e}", exc_info=True)

    async def _reassign_incident(self, incident: IncidentORM, classification, db: Session):
        """
        Re-assign an incident based on updated classification.
        Only re-assigns if the incident was previously auto-assigned.

        Args:
            incident: Incident ORM object
            classification: Updated classification result
            db: Database session
        """
        try:
            from services.assignment_service import get_assignment_service

            old_assignment = incident.routed_to
            old_org_name = None

            if old_assignment:
                from models.organization_model import OrganizationORM
                old_org = db.query(OrganizationORM).filter(
                    OrganizationORM.id == old_assignment
                ).first()
                old_org_name = old_org.name if old_org else "Unknown"

            logger.info(f"Re-assigning incident {incident.incident_id} due to classification change")

            # Get new assignment
            assignment_service = get_assignment_service()
            assignment = await assignment_service.assign_incident(
                incident=incident,
                classification=classification,
                db=db
            )

            if assignment.organization_id and assignment.organization_id != old_assignment:
                # Update assignment
                incident.routed_to = assignment.organization_id

                # Track in assignment history
                if 'assignment_history' not in incident.meta_data:
                    incident.meta_data['assignment_history'] = []

                incident.meta_data['assignment_history'].append({
                    'organization_id': assignment.organization_id,
                    'organization_name': assignment.organization_name,
                    'assigned_at': datetime.utcnow().isoformat(),
                    'assigned_by': 'auto_reassignment_after_transcription',
                    'auto_assigned': True,
                    'confidence': assignment.confidence,
                    'reasoning': assignment.reasoning,
                    'previous_assignment': {
                        'organization_id': old_assignment,
                        'organization_name': old_org_name
                    }
                })

                db.commit()

                logger.info(
                    f"Re-assigned incident {incident.incident_id} from "
                    f"{old_org_name} to {assignment.organization_name} "
                    f"(confidence: {assignment.confidence:.2f})"
                )

                # Broadcast re-assignment via WebSocket
                try:
                    from websocket import websocket_manager
                    await websocket_manager.broadcast_incident(
                        incident_data={
                            'incident_id': incident.incident_id,
                            'routed_to': assignment.organization_id,
                            'routed_to_name': assignment.organization_name,
                            'old_assignment': old_org_name
                        },
                        event_type='incident_reassigned'
                    )
                except Exception as ws_error:
                    logger.error(f"Failed to broadcast incident_reassigned: {ws_error}")

            elif assignment.organization_id == old_assignment:
                logger.info(f"Re-assignment resulted in same organization: {assignment.organization_name}")
            else:
                logger.warning(f"Could not find better assignment for incident {incident.incident_id}: {assignment.reasoning}")

        except Exception as e:
            logger.error(f"Error re-assigning incident: {e}", exc_info=True)

    async def _process_summarization_batch(self):
        """Process queued summarization tasks in batch."""
        async with self.processing_lock:
            if not self.summarization_queue:
                return

            batch = self.summarization_queue[:self.max_batch_size]
            self.summarization_queue = self.summarization_queue[self.max_batch_size:]

        logger.info(f"Processing summarization batch of {len(batch)} items")

        # Create semaphore to limit concurrent API requests
        semaphore = asyncio.Semaphore(self.max_concurrent_requests)

        async def process_item(item: Dict[str, Any]):
            async with semaphore:
                await self._summarize_and_update(
                    item['session_id'],
                    item['incident_id']
                )

        # Process all items concurrently
        await asyncio.gather(
            *[process_item(item) for item in batch],
            return_exceptions=True
        )

        logger.info(f"Completed summarization batch of {len(batch)} items")

    async def _summarize_and_update(self, session_id: str, incident_id: str):
        """
        Summarize chat session and update incident.

        Args:
            session_id: Chat session UUID
            incident_id: Incident UUID
        """
        try:
            from db.connection import session as Session
            db = Session()

            try:
                # Get all messages in the session
                messages = db.query(ChatMessageORM).filter(
                    ChatMessageORM.session_id == session_id
                ).order_by(ChatMessageORM.created_at.asc()).all()

                if not messages:
                    logger.info(f"No messages to summarize for session {session_id}")
                    return

                # Build conversation text
                conversation_lines = []
                for msg in messages:
                    msg_data = msg.message
                    msg_type = msg_data.get('type', 'human')
                    content = msg_data.get('data', {}).get('content', '')

                    role = 'User' if msg_type == 'human' else 'Assistant'
                    conversation_lines.append(f"{role}: {content}")

                conversation_text = '\n'.join(conversation_lines)

                # Generate summary using LLM
                summary = await self._generate_summary(conversation_text)

                if not summary:
                    logger.warning(f"Failed to generate summary for session {session_id}")
                    return

                # Update incident metadata with summary
                incident = db.query(IncidentORM).filter(
                    IncidentORM.id == incident_id
                ).first()

                if incident:
                    if not incident.meta_data:
                        incident.meta_data = {}

                    incident.meta_data['chat_summary'] = summary
                    incident.meta_data['last_summarized_at'] = datetime.utcnow().isoformat()
                    incident.meta_data['message_count'] = len(messages)

                    db.commit()

                    logger.info(
                        f"Updated summary for incident {incident_id}: "
                        f"{summary[:100]}..."
                    )
                else:
                    logger.warning(f"Incident not found: {incident_id}")

            except Exception as e:
                logger.error(f"Database error summarizing session {session_id}: {e}")
                db.rollback()
            finally:
                db.close()

        except Exception as e:
            logger.error(
                f"Error summarizing session {session_id}: {e}",
                exc_info=True
            )

    async def _generate_summary(self, conversation_text: str) -> Optional[str]:
        """
        Generate a summary of a conversation using LLM.

        Args:
            conversation_text: Full conversation text

        Returns:
            Summary text or None if failed
        """
        if not Config.FEATHERLESS_API_KEY:
            logger.error("Cannot generate summary: FEATHERLESS_API_KEY not configured")
            return None

        prompt = f"""Summarize the following conversation between a user reporting an incident and an AI assistant.
Focus on key facts, actions taken, and important details.
Keep the summary concise (2-3 sentences).

Conversation:
{conversation_text}

Summary:"""

        try:
            async with httpx.AsyncClient(timeout=90.0) as client:
                response = await client.post(
                    f"{Config.FEATHERLESS_API_BASE}/chat/completions",
                    headers={
                        "Authorization": f"Bearer {Config.FEATHERLESS_API_KEY}",
                        "Content-Type": "application/json"
                    },
                    json={
                        "model": Config.DEFAULT_LLM_MODEL,
                        "messages": [
                            {
                                "role": "user",
                                "content": prompt
                            }
                        ],
                        "temperature": 0.3,
                        "max_tokens": 200
                    }
                )

                if response.status_code == 200:
                    result = response.json()
                    summary = result['choices'][0]['message']['content'].strip()
                    return summary
                else:
                    logger.error(
                        f"LLM API error: {response.status_code} - {response.text}"
                    )
                    return None

        except Exception as e:
            logger.error(f"Error calling LLM API: {e}", exc_info=True)
            return None


class ScheduledSummarizationService:
    """
    Main service for scheduled summarization and transcription.

    This service should be started when the application starts and
    provides methods to queue tasks for processing.
    """

    def __init__(
        self,
        batch_delay: float = 5.0,
        max_batch_size: int = 10,
        max_concurrent_requests: int = 5,
        auto_transcribe_on_upload: bool = True
    ):
        """
        Initialize the service.

        Args:
            batch_delay: Seconds to wait before processing a batch
            max_batch_size: Maximum items in a single batch
            max_concurrent_requests: Max concurrent API requests
            auto_transcribe_on_upload: Automatically queue audio for transcription
        """
        self.batch_processor = BatchProcessor(
            batch_delay=batch_delay,
            max_batch_size=max_batch_size,
            max_concurrent_requests=max_concurrent_requests
        )
        self.auto_transcribe_on_upload = auto_transcribe_on_upload

    async def start(self):
        """Start the service."""
        await self.batch_processor.start()
        logger.info("Scheduled summarization service started")

    async def stop(self):
        """Stop the service."""
        await self.batch_processor.stop()
        logger.info("Scheduled summarization service stopped")

    async def queue_audio_transcription(
        self,
        media_id: str,
        file_path: str,
        db_session: Session
    ):
        """
        Queue an audio file for transcription.

        Args:
            media_id: Media record UUID
            file_path: Path to audio file
            db_session: Database session
        """
        await self.batch_processor.queue_transcription(
            media_id,
            file_path,
            db_session
        )

    async def queue_chat_summarization(
        self,
        session_id: str,
        incident_id: str,
        db_session: Session
    ):
        """
        Queue a chat session for summarization.

        Args:
            session_id: Chat session UUID
            incident_id: Incident UUID
            db_session: Database session
        """
        await self.batch_processor.queue_summarization(
            session_id,
            incident_id,
            db_session
        )

    async def process_pending_transcriptions(self, db_session: Session):
        """
        Find and queue all media files that need transcription.
        Useful for processing backlog or on service startup.

        Args:
            db_session: Database session
        """
        try:
            # Find audio media without transcriptions
            pending_media = db_session.query(MediaORM).filter(
                and_(
                    MediaORM.media_type == MediaType.AUDIO,
                    MediaORM.transcription.is_(None)
                )
            ).all()

            logger.info(f"Found {len(pending_media)} audio files pending transcription")

            for media in pending_media:
                await self.queue_audio_transcription(
                    str(media.id),
                    media.file_path,
                    db_session
                )

        except Exception as e:
            logger.error(f"Error processing pending transcriptions: {e}", exc_info=True)

    async def process_recent_chats(
        self,
        db_session: Session,
        since_minutes: int = 60
    ):
        """
        Find and queue recent chat sessions for summarization.

        Args:
            db_session: Database session
            since_minutes: Process chats from the last N minutes
        """
        try:
            cutoff_time = datetime.utcnow() - timedelta(minutes=since_minutes)

            # Find recent sessions with messages
            recent_sessions = db_session.query(ChatSessionORM).filter(
                ChatSessionORM.created_at >= cutoff_time
            ).all()

            logger.info(
                f"Found {len(recent_sessions)} chat sessions from "
                f"the last {since_minutes} minutes"
            )

            for session in recent_sessions:
                await self.queue_chat_summarization(
                    str(session.session_id),
                    str(session.incident_id),
                    db_session
                )

        except Exception as e:
            logger.error(f"Error processing recent chats: {e}", exc_info=True)


# Global service instance
_summarization_service: Optional[ScheduledSummarizationService] = None


def get_summarization_service() -> ScheduledSummarizationService:
    """
    Get the global summarization service instance.

    Returns:
        ScheduledSummarizationService instance
    """
    global _summarization_service

    if _summarization_service is None:
        _summarization_service = ScheduledSummarizationService(
            batch_delay=5.0,  # 5 seconds
            max_batch_size=10,
            max_concurrent_requests=5
        )

    return _summarization_service


async def start_summarization_service():
    """Start the global summarization service."""
    service = get_summarization_service()
    await service.start()
    return service


async def stop_summarization_service():
    """Stop the global summarization service."""
    if _summarization_service:
        await _summarization_service.stop()
