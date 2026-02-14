"""
Incident API Endpoints
Handles incident creation, retrieval, and chat message management
"""
import logging
import uuid
from typing import List, Optional
from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session, joinedload
from geoalchemy2.elements import WKTElement

from db.connection import get_db
from models.incident_model import (
    IncidentORM,
    IncidentCreate,
    IncidentResponse,
    IncidentUpdate,
    IncidentStatus,
    IncidentPriority
)
from models.chat_model import ChatSessionORM
from models.media_model import MediaORM
from services.chat_history import ChatHistory, create_chat_session, get_session_by_incident
from services.classification_service import get_classifier
from services.assignment_service import get_assignment_service
from services.sedap_service import SEDAPService  # Legacy - keeping for backwards compat
from services.integration_delivery_service import IntegrationDeliveryService
from services.media_analysis_service import get_media_analyzer
from transcription_service import TranscriptionService
from i18n import i18n
from websocket import websocket_manager
from pydantic import BaseModel
from config import Config

logger = logging.getLogger(__name__)

incident_router = APIRouter(prefix="/api/incident", tags=["incident"])


async def _deliver_to_organization(
    db: Session,
    incident: IncidentORM,
    org,
    chat: ChatHistory,
    incident_id: str
):
    """
    Helper function to deliver incident to a single organization via integrations.

    Args:
        db: Database session
        incident: Incident to deliver
        org: Target organization
        chat: Chat history for confirmation messages
        incident_id: Formatted incident ID for logging
    """
    logger.info(f"Delivering incident {incident_id} to {org.name} via integrations")

    try:
        # Deliver via all active integrations
        deliveries = await IntegrationDeliveryService.deliver_incident(
            db=db,
            incident=incident,
            organization=org
        )

        # Count successful deliveries
        successful = sum(1 for d in deliveries if d.status == 'success')
        failed = sum(1 for d in deliveries if d.status == 'failed')

        # Add chat message confirming delivery
        if successful > 0:
            confirmation_msg = f"Thank you! We forwarded the information to {org.name} who are taking action as we speak."
            chat.add_message("system", confirmation_msg)
            logger.info(
                f"Successfully delivered incident {incident_id} to {org.name}: "
                f"{successful} successful, {failed} failed"
            )
        elif deliveries:
            logger.error(
                f"All delivery attempts failed for incident {incident_id} to {org.name}"
            )
        else:
            logger.warning(
                f"No active integrations configured for organization {org.name}"
            )

    except Exception as delivery_error:
        logger.error(
            f"Error delivering incident {incident_id} to {org.name}: {delivery_error}",
            exc_info=True
        )


@incident_router.post("/create", response_model=IncidentResponse, status_code=status.HTTP_201_CREATED)
async def create_incident(
    incident_data: IncidentCreate,
    db: Session = Depends(get_db)
):
    """
    Create a new incident with automatic chat session initialization.

    Creates:
    - Incident record
    - Chat session linked to incident
    - Initial chat message with description
    - Optional media links
    """
    try:
        # Use app-generated ID if provided, otherwise generate
        if incident_data.id:
            incident_uuid = uuid.UUID(incident_data.id)
            logger.info(f"Using app-generated incident ID: {incident_uuid}")
        else:
            incident_uuid = uuid.uuid4()
            logger.info(f"Generated backend incident ID: {incident_uuid}")

        # Generate formatted incident ID
        incident_id = f"INC-{str(incident_uuid).replace('-', '')[:8].upper()}"

        logger.info(f"Creating incident {incident_id}")

        # Create PostGIS point if lat/lon provided
        location_geom = None
        if incident_data.latitude is not None and incident_data.longitude is not None:
            # PostGIS uses (lon, lat) order for POINT
            location_geom = WKTElement(
                f'POINT({incident_data.longitude} {incident_data.latitude})',
                srid=4326
            )

        # Create incident ORM object
        user_phone = incident_data.user_phone or incident_data.metadata.get('user_phone')
        logger.info(f"Creating incident with user_phone: {user_phone}")

        # Use "Processing..." as initial description for placeholder text
        initial_description = incident_data.description
        if incident_data.description and incident_data.description.lower() in ['incident with photo', 'incident with video', 'incident with audio']:
            initial_description = 'Processing incident report...'

        db_incident = IncidentORM(
            id=incident_uuid,
            incident_id=incident_id,
            user_phone=user_phone,
            location=location_geom,
            latitude=incident_data.latitude,
            longitude=incident_data.longitude,
            altitude=incident_data.altitude,
            heading=incident_data.heading,
            title=incident_data.title,
            description=initial_description,
            status='processing',  # Start as processing while AI analyzes
            priority='medium',
            category='Unclassified',
            tags=[],
            meta_data=incident_data.metadata or {}
        )

        db.add(db_incident)
        db.flush()  # Get incident ID without committing

        # Create chat session - use app-provided session_id if available
        if incident_data.session_id:
            session_id = incident_data.session_id
            logger.info(f"Using app-generated session ID: {session_id}")
            # Create session with app-provided ID
            session_id = create_chat_session(
                db,
                str(incident_uuid),
                incident_data.user_phone,
                session_id=session_id
            )
        else:
            logger.info(f"Generating backend session ID")
            session_id = create_chat_session(
                db,
                str(incident_uuid),
                incident_data.user_phone
            )

        # Add initial message to chat history
        chat = ChatHistory(db, session_id)
        chat.add_message("user", incident_data.description)

        # Link media if provided
        image_url = None
        video_url = None
        audio_url = None
        image_media_to_analyze = None
        video_media_to_analyze = None
        audio_media_to_analyze = None

        if incident_data.imageUrl:
            # Check if media already exists with this URL
            existing_media = db.query(MediaORM).filter(
                MediaORM.file_url == incident_data.imageUrl
            ).first()

            if existing_media:
                # Link existing media to this incident
                existing_media.incident_id = incident_uuid
                db.flush()
                image_url = existing_media.file_url
                image_media_to_analyze = existing_media
                logger.info(f"Linked existing image media {existing_media.id} to incident {incident_id}")
            else:
                # Create new media record (for backward compatibility with old URLs)
                media = MediaORM(
                    incident_id=incident_uuid,
                    file_path=incident_data.imageUrl,
                    file_url=incident_data.imageUrl,
                    mime_type='image/jpeg',
                    media_type='image',
                    metadata={}
                )
                db.add(media)
                db.flush()
                image_url = incident_data.imageUrl
                image_media_to_analyze = media
                logger.warning(f"Created media with URL-only path for incident {incident_id}: {incident_data.imageUrl}")

        if incident_data.videoUrl:
            # Check if media already exists with this URL
            existing_media = db.query(MediaORM).filter(
                MediaORM.file_url == incident_data.videoUrl
            ).first()

            if existing_media:
                # Link existing media to this incident
                existing_media.incident_id = incident_uuid
                db.flush()
                video_url = existing_media.file_url
                video_media_to_analyze = existing_media
                logger.info(f"Linked existing video media {existing_media.id} to incident {incident_id}")
            else:
                # Create new media record (for backward compatibility with old URLs)
                media = MediaORM(
                    incident_id=incident_uuid,
                    file_path=incident_data.videoUrl,
                    file_url=incident_data.videoUrl,
                    mime_type='video/mp4',
                    media_type='video',
                    metadata={}
                )
                db.add(media)
                db.flush()
                video_url = incident_data.videoUrl
                video_media_to_analyze = media
                logger.warning(f"Created media with URL-only path for incident {incident_id}: {incident_data.videoUrl}")

        if incident_data.audioUrl:
            # Check if media already exists with this URL
            existing_media = db.query(MediaORM).filter(
                MediaORM.file_url == incident_data.audioUrl
            ).first()

            if existing_media:
                # Link existing media to this incident
                existing_media.incident_id = incident_uuid
                db.flush()
                audio_url = existing_media.file_url
                audio_media_to_analyze = existing_media
                logger.info(f"Linked existing audio media {existing_media.id} to incident {incident_id}")
            else:
                # Create new media record (for backward compatibility with old URLs)
                media = MediaORM(
                    incident_id=incident_uuid,
                    file_path=incident_data.audioUrl,
                    file_url=incident_data.audioUrl,
                    mime_type='audio/m4a',
                    media_type='audio',
                    metadata={}
                )
                db.add(media)
                db.flush()
                audio_url = incident_data.audioUrl
                audio_media_to_analyze = media
                logger.warning(f"Created media with URL-only path for incident {incident_id}: {incident_data.audioUrl}")

        # Auto-classify incident using LLM (if categorization enabled)
        classification = None
        transcription = None
        if Config.CATEGORIZATION_ENABLED:
            try:
                logger.info(f"Classifying incident {incident_id} with description: {incident_data.description[:100]}...")
                classifier = get_classifier()

                # Get transcription from audio media if available
                transcription = None
                if incident_data.audioUrl:
                    audio_media = db.query(MediaORM).filter(
                        MediaORM.incident_id == incident_uuid,
                        MediaORM.media_type == 'audio'
                    ).first()
                    if audio_media and hasattr(audio_media, 'transcription'):
                        transcription = audio_media.transcription
                        logger.info(f"Found transcription for incident {incident_id}: {transcription[:100]}...")

                # Classify the incident (with image analysis if available)
                classification = await classifier.classify_incident(
                    description=incident_data.description,
                    transcription=transcription,
                    image_url=image_url,
                    latitude=incident_data.latitude,
                    longitude=incident_data.longitude,
                    heading=incident_data.heading
                )

                # Update incident with classification results
                db_incident.category = classification.category
                db_incident.priority = classification.priority
                db_incident.tags = classification.tags

                logger.info(
                    f"Classification result for {incident_id}: "
                    f"category={classification.category}, "
                    f"priority={classification.priority}, "
                    f"tags={classification.tags}, "
                    f"confidence={classification.confidence:.2f}"
                )

                # Store classification details in metadata
                if not db_incident.meta_data:
                    db_incident.meta_data = {}
                db_incident.meta_data['classification'] = classification.to_dict()
                db_incident.meta_data['classification_timestamp'] = datetime.utcnow().isoformat()

                # Change status from 'processing' to 'open' after initial classification
                if db_incident.status == 'processing':
                    db_incident.status = 'open'
                    logger.info(f"Changed incident {incident_id} status from 'processing' to 'open' after initial classification")

                logger.info(
                    f"Incident {incident_id} classified as '{classification.category}' "
                    f"with priority '{classification.priority}' (confidence: {classification.confidence:.2f})"
                )

            except Exception as classify_error:
                logger.error(f"Classification failed for {incident_id}: {classify_error}", exc_info=True)
                # Continue with incident creation even if classification fails
                if not db_incident.meta_data:
                    db_incident.meta_data = {}
                db_incident.meta_data['classification_error'] = str(classify_error)
                # Keep default values (already set in incident creation)
        else:
            # Categorization disabled - use configured defaults
            logger.info(f"Categorization disabled, using defaults for incident {incident_id}")
            db_incident.category = Config.DEFAULT_CATEGORY
            db_incident.priority = Config.DEFAULT_PRIORITY
            db_incident.tags = []

            # Store in metadata to indicate manual defaults were used
            if not db_incident.meta_data:
                db_incident.meta_data = {}
            db_incident.meta_data['classification'] = {
                'category': Config.DEFAULT_CATEGORY,
                'priority': Config.DEFAULT_PRIORITY,
                'tags': [],
                'confidence': 0.0,
                'reasoning': 'AI categorization disabled - using default values'
            }

            # Change status from 'processing' to 'open' immediately
            if db_incident.status == 'processing':
                db_incident.status = 'open'
                logger.info(f"Changed incident {incident_id} status from 'processing' to 'open' (AI disabled)")

        # Auto-forward incident to organization(s) - runs as background task
        # to avoid blocking the HTTP response (integration delivery may be slow)
        import asyncio

        async def _background_forward(incident_uuid, incident_id, classification):
            """Forward incident to organizations in the background."""
            from db.connection import session as Session
            bg_db = Session()
            try:
                bg_incident = bg_db.query(IncidentORM).filter(IncidentORM.id == incident_uuid).first()
                if not bg_incident:
                    logger.error(f"Background forwarding: incident {incident_id} not found")
                    return

                if Config.CATEGORIZATION_ENABLED and Config.AUTO_FORWARDING_ENABLED and classification:
                    if classification.confidence >= Config.AUTO_ASSIGN_CONFIDENCE_THRESHOLD:
                        logger.info(f"Auto-assigning incident {incident_id} (confidence threshold met)")
                        assignment_service = get_assignment_service()

                        assignment = await assignment_service.assign_incident(
                            incident=bg_incident,
                            classification=classification,
                            db=bg_db
                        )

                        if assignment.organization_id:
                            bg_incident.routed_to = assignment.organization_id
                            bg_incident.status = 'in_progress'

                            if not bg_incident.meta_data:
                                bg_incident.meta_data = {}
                            if 'assignment_history' not in bg_incident.meta_data:
                                bg_incident.meta_data['assignment_history'] = []

                            bg_incident.meta_data['assignment_history'].append({
                                'organization_id': assignment.organization_id,
                                'organization_name': assignment.organization_name,
                                'assigned_at': datetime.utcnow().isoformat(),
                                'assigned_by': 'auto_assignment',
                                'auto_assigned': True,
                                'confidence': assignment.confidence,
                                'reasoning': assignment.reasoning
                            })

                            logger.info(
                                f"Incident {incident_id} auto-assigned to {assignment.organization_name} "
                                f"(confidence: {assignment.confidence:.2f})"
                            )

                            from models.organization_model import OrganizationORM
                            org = bg_db.query(OrganizationORM).filter(
                                OrganizationORM.id == assignment.organization_id
                            ).first()

                            if org:
                                bg_session_id = get_session_by_incident(bg_db, incident_uuid)
                                bg_chat = ChatHistory(bg_db, bg_session_id) if bg_session_id else None
                                await _deliver_to_organization(bg_db, bg_incident, org, bg_chat, incident_id)
                        else:
                            logger.warning(
                                f"Could not auto-assign incident {incident_id}: {assignment.reasoning}"
                            )
                    else:
                        logger.info(
                            f"Incident {incident_id} confidence {classification.confidence:.2f} "
                            f"below threshold {Config.AUTO_ASSIGN_CONFIDENCE_THRESHOLD:.2f}, skipping auto-assignment"
                        )

                elif not Config.CATEGORIZATION_ENABLED and Config.AUTO_FORWARDING_ENABLED:
                    if Config.DEFAULT_ORGANIZATIONS:
                        logger.info(
                            f"Categorization disabled, forwarding incident {incident_id} to "
                            f"{len(Config.DEFAULT_ORGANIZATIONS)} default organization(s)"
                        )

                        from models.organization_model import OrganizationORM
                        default_orgs = bg_db.query(OrganizationORM).filter(
                            OrganizationORM.id.in_(Config.DEFAULT_ORGANIZATIONS),
                            OrganizationORM.active == True
                        ).all()

                        if not default_orgs:
                            logger.warning(
                                f"No active default organizations found for IDs: {Config.DEFAULT_ORGANIZATIONS}"
                            )
                        else:
                            successful_deliveries = []
                            failed_deliveries = []

                            for org in default_orgs:
                                try:
                                    deliveries = await IntegrationDeliveryService.deliver_incident(
                                        db=bg_db,
                                        incident=bg_incident,
                                        organization=org
                                    )

                                    successful = sum(1 for d in deliveries if d.status == 'success')
                                    if successful > 0:
                                        successful_deliveries.append(org.name)
                                    else:
                                        failed_deliveries.append(org.name)

                                except Exception as delivery_error:
                                    logger.error(
                                        f"Error delivering to default org {org.name}: {delivery_error}",
                                        exc_info=True
                                    )
                                    failed_deliveries.append(org.name)

                            bg_incident.status = 'in_progress'
                            if not bg_incident.meta_data:
                                bg_incident.meta_data = {}
                            bg_incident.meta_data['default_forwarding'] = {
                                'forwarded_at': datetime.utcnow().isoformat(),
                                'organizations': [org.id for org in default_orgs],
                                'successful': successful_deliveries,
                                'failed': failed_deliveries
                            }

                            if successful_deliveries:
                                bg_session_id = get_session_by_incident(bg_db, incident_uuid)
                                if bg_session_id:
                                    bg_chat = ChatHistory(bg_db, bg_session_id)
                                    confirmation_msg = (
                                        f"Thank you! We forwarded the information to "
                                        f"{', '.join(successful_deliveries)}."
                                    )
                                    bg_chat.add_message("system", confirmation_msg)
                                logger.info(
                                    f"Successfully forwarded incident {incident_id} to "
                                    f"default organizations: {', '.join(successful_deliveries)}"
                                )
                    else:
                        logger.info(
                            f"No default organizations configured, incident {incident_id} "
                            f"will remain unassigned"
                        )

                elif not Config.AUTO_FORWARDING_ENABLED:
                    logger.info(f"Auto-forwarding disabled, skipping for incident {incident_id}")

                bg_db.commit()

            except Exception as assign_error:
                logger.error(f"Auto-forwarding failed for {incident_id}: {assign_error}", exc_info=True)
                try:
                    bg_incident = bg_db.query(IncidentORM).filter(IncidentORM.id == incident_uuid).first()
                    if bg_incident:
                        if not bg_incident.meta_data:
                            bg_incident.meta_data = {}
                        bg_incident.meta_data['forwarding_error'] = str(assign_error)
                        bg_db.commit()
                except Exception:
                    pass
            finally:
                bg_db.close()

        if Config.AUTO_FORWARDING_ENABLED:
            asyncio.create_task(_background_forward(incident_uuid, incident_id, classification))

        db.commit()

        logger.info(f"Incident {incident_id} created successfully with session {session_id}")

        # Analyze media in background (don't block response)
        async def _background_media_analysis(incident_uuid, incident_id, image_url_to_analyze, audio_url_to_analyze):
            from db.connection import session as Session
            bg_db = Session()
            try:
                if Config.MEDIA_ANALYSIS_ENABLED and image_url_to_analyze:
                    logger.info(f"[BG] Analyzing image for incident {incident_id}")
                    media_analyzer = get_media_analyzer()
                    image_description = await media_analyzer.analyze_image(image_url_to_analyze)
                    if image_description:
                        media = bg_db.query(MediaORM).filter(
                            MediaORM.incident_id == incident_uuid,
                            MediaORM.media_type == 'image'
                        ).first()
                        if media:
                            media.transcription = image_description
                            bg_db.commit()
                            logger.info(f"[BG] Successfully analyzed image for incident {incident_id}")

                if Config.TRANSCRIPTION_ENABLED and audio_url_to_analyze:
                    logger.info(f"[BG] Transcribing audio for incident {incident_id}")
                    transcription_service = TranscriptionService()
                    transcription_text = await transcription_service.transcribe_and_get_text(audio_url_to_analyze)
                    if transcription_text:
                        media = bg_db.query(MediaORM).filter(
                            MediaORM.incident_id == incident_uuid,
                            MediaORM.media_type == 'audio'
                        ).first()
                        if media:
                            media.transcription = transcription_text
                            bg_db.commit()
                            logger.info(f"[BG] Successfully transcribed audio for incident {incident_id}")
            except Exception as media_error:
                logger.error(f"[BG] Error analyzing media (non-critical): {media_error}", exc_info=True)
            finally:
                bg_db.close()

        image_url_for_bg = image_media_to_analyze.file_url if image_media_to_analyze else None
        audio_url_for_bg = audio_media_to_analyze.file_url if audio_media_to_analyze else None
        if image_url_for_bg or audio_url_for_bg:
            asyncio.create_task(_background_media_analysis(
                incident_uuid, incident_id, image_url_for_bg, audio_url_for_bg
            ))

        # Prepare response
        response = IncidentResponse.from_orm(db_incident, image_url, video_url, audio_url, transcription)
        logger.info(f"Incident response user_phone: {response.user_phone}")

        # Broadcast new incident via WebSocket
        try:
            await websocket_manager.broadcast_incident(
                incident_data=response.model_dump(),
                event_type='incident_new'
            )
            logger.info(f"Broadcasted new incident {incident_id} via WebSocket")
        except Exception as ws_error:
            logger.error(f"Failed to broadcast incident via WebSocket: {ws_error}")

        # Start automated response sequence (typing indicator + thank you message)
        try:
            from services.auto_response_service import get_auto_response_service
            from db.connection import session as Session
            auto_response = get_auto_response_service()

            # Schedule as background task (use UUID not formatted ID)
            import asyncio
            asyncio.create_task(
                auto_response.schedule_auto_response(
                    str(incident_uuid),  # Use UUID for WebSocket matching
                    session_id,
                    Session
                )
            )
            logger.info(f"Scheduled auto-response for incident {incident_id}")
        except Exception as ar_error:
            logger.error(f"Failed to schedule auto-response: {ar_error}")

        return response

    except Exception as e:
        db.rollback()
        logger.error(f"Error creating incident: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to create incident: {str(e)}"
        )


@incident_router.get("/{incident_id}", response_model=IncidentResponse)
async def get_incident(
    incident_id: str,
    db: Session = Depends(get_db)
):
    """Get incident by incident_id"""
    try:
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Get associated media
        media_files = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id
        ).all()

        image_url = None
        video_url = None
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'video' and not video_url:
                video_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        return IncidentResponse.from_orm(incident, image_url, video_url, audio_url, audio_transcript)

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error retrieving incident: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to retrieve incident: {str(e)}"
        )


@incident_router.get("/{incident_id}/messages")
async def get_incident_messages(
    incident_id: str,
    db: Session = Depends(get_db)
):
    """Get all chat messages for an incident"""
    try:
        # Get incident UUID
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Get session ID
        session_id = get_session_by_incident(db, str(incident.id))

        if not session_id:
            return {"messages": []}

        # Get messages
        chat = ChatHistory(db, session_id)
        messages = chat.get_messages()

        return {"incident_id": incident_id, "messages": messages}

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error retrieving messages: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to retrieve messages: {str(e)}"
        )


class MessageCreate(BaseModel):
    """Request model for adding a message"""
    role: str  # "user" or "assistant"
    content: str


@incident_router.post("/{incident_id}/message", status_code=status.HTTP_201_CREATED)
async def add_incident_message(
    incident_id: str,
    message: MessageCreate,
    db: Session = Depends(get_db)
):
    """Add a message to the incident chat"""
    try:
        # Get incident
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Get or create session
        session_id = get_session_by_incident(db, str(incident.id))
        if not session_id:
            session_id = create_chat_session(db, str(incident.id))

        # Add message
        chat = ChatHistory(db, session_id)
        chat.add_message(message.role, message.content)

        logger.info(f"Added {message.role} message to incident {incident_id}")

        return {"status": "ok", "incident_id": incident_id}

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error adding message: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to add message: {str(e)}"
        )


@incident_router.put("/{incident_id}", response_model=IncidentResponse)
async def update_incident(
    incident_id: str,
    update_data: IncidentUpdate,
    db: Session = Depends(get_db)
):
    """Update an incident"""
    try:
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Update fields
        update_dict = update_data.dict(exclude_unset=True)
        for field, value in update_dict.items():
            if hasattr(incident, field):
                setattr(incident, field, value)

        incident.updated_at = datetime.utcnow()

        db.commit()
        db.refresh(incident)

        logger.info(f"Updated incident {incident_id}")

        # Get associated media for response
        media_files = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id
        ).all()

        image_url = None
        video_url = None
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'video' and not video_url:
                video_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        # Prepare response
        response = IncidentResponse.from_orm(incident, image_url, video_url, audio_url, audio_transcript)

        # Broadcast incident update via WebSocket
        try:
            await websocket_manager.broadcast_incident(
                incident_data=response.model_dump(),
                event_type='incident_update'
            )
            logger.info(f"Broadcasted incident update {incident_id} via WebSocket")
        except Exception as ws_error:
            logger.error(f"Failed to broadcast incident update via WebSocket: {ws_error}")

        return response

    except HTTPException:
        raise
    except Exception as e:
        db.rollback()
        logger.error(f"Error updating incident: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to update incident: {str(e)}"
        )


@incident_router.get("/", response_model=List[IncidentResponse])
async def list_incidents(
    skip: int = 0,
    limit: int = 100,
    status_filter: Optional[str] = None,
    priority_filter: Optional[str] = None,
    db: Session = Depends(get_db)
):
    """List all incidents with optional filtering"""
    try:
        query = db.query(IncidentORM).options(
            joinedload(IncidentORM.organization)
        )

        if status_filter:
            query = query.filter(IncidentORM.status == status_filter)

        if priority_filter:
            query = query.filter(IncidentORM.priority == priority_filter)

        # Eager load media relationships
        query = query.options(joinedload(IncidentORM.media_files))

        incidents = query.order_by(
            IncidentORM.created_at.desc()
        ).offset(skip).limit(limit).all()

        # Build response with media URLs and chat messages
        result = []
        for inc in incidents:
            image_url = None
            video_url = None
            audio_url = None
            audio_transcript = None

            # Debug: Log media files for first incident
            if inc.incident_id == incidents[0].incident_id:
                logger.info(f"[DEBUG] Incident {inc.incident_id}: hasattr(media_files)={hasattr(inc, 'media_files')}")
                if hasattr(inc, 'media_files'):
                    logger.info(f"[DEBUG] media_files={inc.media_files}, count={len(inc.media_files) if inc.media_files else 0}")

            # Use eager-loaded media_files relationship
            if hasattr(inc, 'media_files') and inc.media_files:
                for media in inc.media_files:
                    transcription_value = getattr(media, 'transcription', None)
                    logger.info(f"[DEBUG] Media for {inc.incident_id}: type={media.media_type}, url={media.file_url}, transcription={transcription_value}")
                    if media.media_type == 'image' and not image_url:
                        image_url = media.file_url
                    elif media.media_type == 'video' and not video_url:
                        video_url = media.file_url
                    elif media.media_type == 'audio' and not audio_url:
                        audio_url = media.file_url
                        # Get transcription from audio media
                        if transcription_value:
                            audio_transcript = transcription_value
                            logger.info(f"[DEBUG] Found transcription for {inc.incident_id}: {audio_transcript[:100] if len(audio_transcript) > 100 else audio_transcript}")

            # ALWAYS regenerate summary from ALL available information
            # This ensures first responders see a concise, up-to-date summary
            try:
                from services.ai_providers.factory import get_provider
                from config import Config

                # Collect ALL information sources
                info_parts = []

                # Get transcription if available
                if audio_transcript:
                    info_parts.append(f"Audio: {audio_transcript}")

                # Get image analysis if available
                if image_url and hasattr(inc, 'meta_data') and inc.meta_data:
                    image_description = inc.meta_data.get('image_description')
                    if image_description:
                        info_parts.append(f"Image: {image_description}")

                # Get chat messages
                session_id = get_session_by_incident(db, inc.id)
                if session_id:
                    chat = ChatHistory(db, session_id)
                    messages = chat.get_messages()
                    if messages:
                        user_messages = [msg['content'] for msg in messages if msg['role'] == 'user']
                        meaningful = [m for m in user_messages if m and m.lower() not in ['incident with photo', 'incident with video', 'incident with audio']]
                        if meaningful:
                            info_parts.append(f"Report: {' '.join(meaningful)}")

                # If we have any information, generate a summary
                if info_parts:
                    combined = ' | '.join(info_parts)

                    try:
                        provider = get_provider(Config.CLASSIFICATION_PROVIDER)
                        summary_prompt = f"""Summarize this incident in EXACTLY 5-8 words using the 5 W's (WER/WHO, WAS/WHAT, WIE/HOW).

Information: {combined}

Rules:
- MAXIMUM 8 words TOTAL
- Answer: WHO is doing WHAT and HOW
- NO complete sentences
- NO filler words (please, check, I think, something, going on)
- Use specific nouns and verbs only

Examples:
- "Unauthorized person accessing server room"
- "Hacker compromising network infrastructure"
- "Suspicious equipment near military barracks"

IMPORTANT: Generate the summary in {i18n.get_language_name()} language.

Output ONLY the summary:"""

                        from services.ai_providers.base import Message
                        response = await provider.chat_completion(
                            messages=[Message(role="user", content=summary_prompt)],
                            temperature=0.1,
                            max_tokens=30
                        )

                        if response and response.content:
                            inc.description = response.content.strip()
                            logger.info(f"[SUMMARY] Generated for {inc.incident_id}: {response.content.strip()}")
                        else:
                            inc.description = combined[:100]
                    except Exception as e:
                        logger.error(f"Failed to generate summary: {e}")
                        inc.description = combined[:100]

            except Exception as e:
                logger.error(f"Failed to build summary for {inc.incident_id}: {e}")

            result.append(IncidentResponse.from_orm(inc, image_url, video_url, audio_url, audio_transcript))

        return result

    except Exception as e:
        logger.error(f"Error listing incidents: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to list incidents: {str(e)}"
        )


class AssignRequest(BaseModel):
    """Request model for assigning incident to organization"""
    organization_id: int
    notes: Optional[str] = None


@incident_router.post("/{incident_id}/assign", response_model=IncidentResponse)
async def assign_incident(
    incident_id: str,
    assign_data: AssignRequest,
    db: Session = Depends(get_db)
):
    """Assign/forward an incident to an organization"""
    try:
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Check if organization exists
        from models.organization_model import OrganizationORM
        org = db.query(OrganizationORM).filter(
            OrganizationORM.id == assign_data.organization_id
        ).first()

        if not org:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Organization {assign_data.organization_id} not found"
            )

        # Assign the incident
        incident.routed_to = assign_data.organization_id
        incident.status = 'in_progress'
        incident.updated_at = datetime.utcnow()

        # Add notes to metadata if provided
        if assign_data.notes:
            if not incident.meta_data:
                incident.meta_data = {}
            if 'assignment_history' not in incident.meta_data:
                incident.meta_data['assignment_history'] = []
            incident.meta_data['assignment_history'].append({
                'organization_id': assign_data.organization_id,
                'organization_name': org.name,
                'assigned_at': datetime.utcnow().isoformat(),
                'notes': assign_data.notes
            })

        # Deliver to organization via configured integrations
        logger.info(f"Delivering incident {incident_id} to {org.name} via integrations")

        try:
            # Deliver via all active integrations
            deliveries = await IntegrationDeliveryService.deliver_incident(
                db=db,
                incident=incident,
                organization=org
            )

            # Count successful deliveries
            successful = sum(1 for d in deliveries if d.status == 'success')
            failed = sum(1 for d in deliveries if d.status == 'failed')

            # Add chat message confirming delivery
            if successful > 0:
                session = get_session_by_incident(db, incident.id)
                if session:
                    chat = ChatHistory(db, session.session_id)
                    confirmation_msg = f"Thank you! We forwarded the information to {org.name} who are taking action as we speak."
                    chat.add_message("system", confirmation_msg)
                logger.info(
                    f"Successfully delivered incident {incident_id} to {org.name}: "
                    f"{successful} successful, {failed} failed"
                )
            elif deliveries:
                logger.error(
                    f"All delivery attempts failed for incident {incident_id} to {org.name}"
                )
            else:
                logger.warning(
                    f"No active integrations configured for organization {org.name}"
                )

        except Exception as delivery_error:
            logger.error(
                f"Error delivering incident {incident_id} to {org.name}: {delivery_error}",
                exc_info=True
            )

        db.commit()
        db.refresh(incident)

        logger.info(f"Assigned incident {incident_id} to organization {org.name}")

        # Get associated media for response
        media_files = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id
        ).all()

        image_url = None
        video_url = None
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'video' and not video_url:
                video_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        # Prepare response
        response = IncidentResponse.from_orm(incident, image_url, video_url, audio_url, audio_transcript)

        # Broadcast assignment via WebSocket
        try:
            await websocket_manager.broadcast_incident(
                incident_data=response.model_dump(),
                event_type='incident_assigned'
            )
            logger.info(f"Broadcasted incident assignment {incident_id} via WebSocket")
        except Exception as ws_error:
            logger.error(f"Failed to broadcast incident assignment via WebSocket: {ws_error}")

        return response

    except HTTPException:
        raise
    except Exception as e:
        db.rollback()
        logger.error(f"Error assigning incident: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to assign incident: {str(e)}"
        )


@incident_router.delete("/{incident_id}/assignment", response_model=IncidentResponse)
async def unassign_incident(
    incident_id: str,
    db: Session = Depends(get_db)
):
    """Remove organization assignment from an incident"""
    try:
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Store unassignment in history
        if incident.routed_to:
            from models.organization_model import OrganizationORM
            org = db.query(OrganizationORM).filter(
                OrganizationORM.id == incident.routed_to
            ).first()

            if not incident.meta_data:
                incident.meta_data = {}
            if 'assignment_history' not in incident.meta_data:
                incident.meta_data['assignment_history'] = []

            incident.meta_data['assignment_history'].append({
                'organization_id': incident.routed_to,
                'organization_name': org.name if org else 'Unknown',
                'unassigned_at': datetime.utcnow().isoformat(),
                'action': 'unassigned'
            })

        # Remove assignment
        incident.routed_to = None
        incident.status = 'open'
        incident.updated_at = datetime.utcnow()

        db.commit()
        db.refresh(incident)

        logger.info(f"Unassigned organization from incident {incident_id}")

        # Get associated media for response
        media_files = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id
        ).all()

        image_url = None
        video_url = None
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'video' and not video_url:
                video_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        # Prepare response
        response = IncidentResponse.from_orm(incident, image_url, video_url, audio_url, audio_transcript)

        # Broadcast unassignment via WebSocket
        try:
            await websocket_manager.broadcast_incident(
                incident_data=response.model_dump(),
                event_type='incident_unassigned'
            )
            logger.info(f"Broadcasted incident unassignment {incident_id} via WebSocket")
        except Exception as ws_error:
            logger.error(f"Failed to broadcast incident unassignment via WebSocket: {ws_error}")

        return response

    except HTTPException:
        raise
    except Exception as e:
        db.rollback()
        logger.error(f"Error unassigning incident: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to unassign incident: {str(e)}"
        )


@incident_router.post("/{incident_id}/summarize")
async def summarize_chat(
    incident_id: str,
    db: Session = Depends(get_db)
):
    """
    Trigger chat summarization for an incident.
    Queues the chat session for summarization by the batch processor.
    """
    try:
        from services.schedule_summarization import get_summarization_service

        # Validate incident exists
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Get chat session
        session = get_session_by_incident(db, str(incident.id))
        if not session:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"No chat session found for incident {incident_id}"
            )

        # Queue for summarization
        summarization_service = get_summarization_service()
        await summarization_service.queue_chat_summarization(
            session_id=session,
            incident_id=str(incident.id),
            db_session=db
        )

        logger.info(f"Queued chat summarization for incident {incident_id}")

        return {
            "success": True,
            "message": f"Chat summarization queued for incident {incident_id}",
            "incident_id": incident_id,
            "session_id": session
        }

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error queuing summarization: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to queue summarization: {str(e)}"
        )


@incident_router.get("/{incident_id}/summary")
async def get_chat_summary(
    incident_id: str,
    db: Session = Depends(get_db)
):
    """
    Get the current chat summary for an incident.
    Returns summary metadata from the incident's meta_data field.
    """
    try:
        # Get incident
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Extract summary from metadata
        meta_data = incident.meta_data or {}
        summary = meta_data.get('chat_summary')
        last_summarized_at = meta_data.get('last_summarized_at')
        message_count = meta_data.get('message_count', 0)

        # Get current message count
        session_id = get_session_by_incident(db, str(incident.id))
        current_message_count = 0
        if session_id:
            current_message_count = db.query(ChatSessionORM).filter(
                ChatSessionORM.session_id == session_id
            ).join(ChatMessageORM).count()

        return {
            "incident_id": incident_id,
            "summary": summary,
            "last_summarized_at": last_summarized_at,
            "messages_at_summary": message_count,
            "current_message_count": current_message_count,
            "summary_available": summary is not None,
            "summary_outdated": current_message_count > message_count if summary else False
        }

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error getting summary: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to get summary: {str(e)}"
        )


@incident_router.post("/{incident_id}/chat")
async def add_chat_message(
    incident_id: str,
    request: dict,
    db: Session = Depends(get_db)
):
    """
    Add a chat message to an incident and trigger re-classification
    """
    try:
        # Find incident by UUID
        incident = db.query(IncidentORM).filter(
            IncidentORM.id == uuid.UUID(incident_id)
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident not found: {incident_id}"
            )

        # Get session ID
        session_id = request.get('session_id')
        message_text = request.get('message')

        if not message_text:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Message text is required"
            )

        logger.info(f"Adding chat message to incident {incident.incident_id}: {message_text[:100]}")

        # Add message to chat history
        chat = ChatHistory(db, session_id)
        chat.add_message("user", message_text)

        # Re-classify incident with updated chat history
        try:
            classifier = get_classifier()

            # Get full chat history for context
            messages = chat.get_messages()
            full_context = "\n".join([
                f"{msg['role']}: {msg['content']}"
                for msg in messages
            ])

            classification = await classifier.classify_incident(
                description=full_context,
                category_hint=incident.category if incident.category != 'Unclassified' else None
            )

            # Update incident with new classification
            incident.category = classification.category
            incident.priority = classification.priority
            incident.tags = classification.tags

            logger.info(
                f"Re-classified incident {incident.incident_id} as '{classification.category}' "
                f"with priority '{classification.priority}' (confidence: {classification.confidence:.2f})"
            )

            # Re-assign if needed
            if classification.confidence >= 0.7:
                assignment_service = get_assignment_service()
                assignment = await assignment_service.assign_incident(
                    incident=incident,
                    classification=classification,
                    db=db
                )

                if assignment.organization_id and assignment.organization_id != incident.routed_to:
                    incident.routed_to = assignment.organization_id
                    logger.info(
                        f"Re-assigned incident {incident.incident_id} to {assignment.organization_name}"
                    )

        except Exception as e:
            logger.error(f"Error re-classifying incident: {e}", exc_info=True)

        db.commit()
        db.refresh(incident)

        # Get associated media for response
        media_files = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id
        ).all()

        image_url = None
        video_url = None
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'video' and not video_url:
                video_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        # Broadcast updated incident
        response = IncidentResponse.from_orm(incident, image_url, video_url, audio_url, audio_transcript)
        await websocket_manager.broadcast_incident(
            incident_data=response.dict(),
            event_type='incident_update'
        )

        return {"status": "ok", "incident_id": incident.incident_id}

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error in add_chat_message: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to add chat message: {str(e)}"
        )


@incident_router.post("/{incident_id}/transcribe-audio")
async def transcribe_incident_audio(
    incident_id: str,
    db: Session = Depends(get_db)
):
    """Transcribe audio for an incident that doesn't have a transcription yet"""
    try:
        # Find the incident
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Find audio media for this incident
        audio_media = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id,
            MediaORM.media_type == 'audio'
        ).first()

        if not audio_media:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"No audio found for incident {incident_id}"
            )

        # Transcribe the audio
        try:
            logger.info(f"Re-transcribing audio for incident {incident_id}")
            transcription_service = TranscriptionService()
            audio_path = audio_media.file_path or audio_media.file_url

            transcription_text = await transcription_service.transcribe_and_get_text(audio_path)
            if transcription_text:
                audio_media.transcription = transcription_text
                db.commit()
                logger.info(f"Successfully transcribed audio for incident {incident_id}")
                return {
                    "status": "success",
                    "incident_id": incident_id,
                    "transcription": transcription_text
                }
            else:
                raise HTTPException(
                    status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                    detail="Failed to transcribe audio"
                )
        except Exception as e:
            logger.error(f"Error transcribing audio for incident {incident_id}: {e}", exc_info=True)
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail=f"Error transcribing audio: {str(e)}"
            )

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error in transcribe endpoint: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to process request: {str(e)}"
        )


@incident_router.post("/{incident_id}/analyze-image")
async def analyze_incident_image(
    incident_id: str,
    db: Session = Depends(get_db)
):
    """Analyze image for an incident that doesn't have a description yet"""
    try:
        # Find the incident
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Find image media for this incident
        image_media = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id,
            MediaORM.media_type == 'image'
        ).first()

        if not image_media:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"No image found for incident {incident_id}"
            )

        # Analyze the image
        try:
            logger.info(f"Re-analyzing image for incident {incident_id}")
            media_analyzer = get_media_analyzer()
            image_url = image_media.file_path or image_media.file_url

            image_description = await media_analyzer.analyze_image(image_url)
            if image_description:
                image_media.transcription = image_description
                db.commit()
                logger.info(f"Successfully analyzed image for incident {incident_id}")
                return {
                    "status": "success",
                    "incident_id": incident_id,
                    "description": image_description
                }
            else:
                raise HTTPException(
                    status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                    detail="Failed to analyze image"
                )
        except Exception as e:
            logger.error(f"Error analyzing image for incident {incident_id}: {e}", exc_info=True)
            raise HTTPException(
                status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
                detail=f"Error analyzing image: {str(e)}"
            )

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error in analyze-image endpoint: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to process request: {str(e)}"
        )


@incident_router.post("/{incident_id}/analyze-all-media")
async def analyze_all_incident_media(
    incident_id: str,
    db: Session = Depends(get_db)
):
    """Analyze all media (images, audio, video) for an incident"""
    try:
        # Find the incident
        incident = db.query(IncidentORM).filter(
            IncidentORM.incident_id == incident_id
        ).first()

        if not incident:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Incident {incident_id} not found"
            )

        # Find all media for this incident
        media_files = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id
        ).all()

        if not media_files:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"No media found for incident {incident_id}"
            )

        results = []
        transcription_service = TranscriptionService()
        media_analyzer = get_media_analyzer()

        for media in media_files:
            try:
                media_path = media.file_path or media.file_url

                if media.media_type == 'audio':
                    logger.info(f"Transcribing audio for incident {incident_id}")
                    text = await transcription_service.transcribe_and_get_text(media_path)
                    if text:
                        media.transcription = text
                        results.append({
                            "type": "audio",
                            "status": "success",
                            "content": text[:100] + "..."
                        })
                    else:
                        results.append({"type": "audio", "status": "failed"})

                elif media.media_type == 'image':
                    logger.info(f"Analyzing image for incident {incident_id}")
                    description = await media_analyzer.analyze_image(media_path)
                    if description:
                        media.transcription = description
                        results.append({
                            "type": "image",
                            "status": "success",
                            "content": description[:100] + "..."
                        })
                    else:
                        results.append({"type": "image", "status": "failed"})

                elif media.media_type == 'video':
                    logger.info(f"Analyzing video for incident {incident_id}")
                    description = await media_analyzer.analyze_video(media_path)
                    if description:
                        media.transcription = description
                        results.append({
                            "type": "video",
                            "status": "success",
                            "content": description[:100] + "..."
                        })
                    else:
                        results.append({"type": "video", "status": "failed"})

            except Exception as e:
                logger.error(f"Error analyzing {media.media_type} for incident {incident_id}: {e}")
                results.append({
                    "type": media.media_type,
                    "status": "error",
                    "error": str(e)
                })

        db.commit()
        return {
            "status": "completed",
            "incident_id": incident_id,
            "results": results
        }

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error in analyze-all-media endpoint: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to process request: {str(e)}"
        )
