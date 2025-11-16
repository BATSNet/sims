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
from services.sedap_service import SEDAPService
from services.media_analysis_service import get_media_analyzer
from transcription_service import TranscriptionService
from websocket import websocket_manager
from pydantic import BaseModel
from config import Config

logger = logging.getLogger(__name__)

incident_router = APIRouter(prefix="/api/incident", tags=["incident"])


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

        db_incident = IncidentORM(
            id=incident_uuid,
            incident_id=incident_id,
            user_phone=user_phone,
            location=location_geom,
            latitude=incident_data.latitude,
            longitude=incident_data.longitude,
            heading=incident_data.heading,
            title=incident_data.title,
            description=incident_data.description,
            status='open',
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
        audio_url = None

        if incident_data.imageUrl:
            media = MediaORM(
                incident_id=incident_uuid,
                file_path=incident_data.imageUrl,
                file_url=incident_data.imageUrl,
                mime_type='image/jpeg',
                media_type='image',
                metadata={}
            )
            db.add(media)
            db.flush()  # Flush to get the media ID
            image_url = incident_data.imageUrl

            # Analyze image content
            try:
                logger.info(f"Analyzing image for incident {incident_id}")
                media_analyzer = get_media_analyzer()
                image_description = await media_analyzer.analyze_image(incident_data.imageUrl)
                if image_description:
                    media.transcription = image_description
                    logger.info(f"Successfully analyzed image for incident {incident_id}: {image_description[:100]}...")
                else:
                    logger.warning(f"Failed to analyze image for incident {incident_id}")
            except Exception as e:
                logger.error(f"Error analyzing image for incident {incident_id}: {e}", exc_info=True)

        if incident_data.audioUrl:
            media = MediaORM(
                incident_id=incident_uuid,
                file_path=incident_data.audioUrl,
                file_url=incident_data.audioUrl,
                mime_type='audio/m4a',
                media_type='audio',
                metadata={}
            )
            db.add(media)
            db.flush()  # Flush to get the media ID
            audio_url = incident_data.audioUrl

            # Transcribe audio asynchronously
            try:
                logger.info(f"Transcribing audio for incident {incident_id}")
                transcription_service = TranscriptionService()
                # Check if audioUrl is a local file path or URL
                audio_path = incident_data.audioUrl
                if audio_path.startswith('http'):
                    # It's a URL, we need to handle this differently
                    logger.warning(f"Audio URL is a web URL, transcription might not work: {audio_path}")

                transcription_text = await transcription_service.transcribe_and_get_text(audio_path)
                if transcription_text:
                    media.transcription = transcription_text
                    logger.info(f"Successfully transcribed audio for incident {incident_id}: {transcription_text[:100]}...")
                else:
                    logger.warning(f"Failed to transcribe audio for incident {incident_id}")
            except Exception as e:
                logger.error(f"Error transcribing audio for incident {incident_id}: {e}", exc_info=True)

        # Auto-classify incident using LLM
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

            logger.info(
                f"Incident {incident_id} classified as '{classification.category}' "
                f"with priority '{classification.priority}' (confidence: {classification.confidence:.2f})"
            )

            # Auto-assign to organization if confidence is high enough
            if Config.AUTO_ASSIGN_ENABLED and classification.confidence >= Config.AUTO_ASSIGN_CONFIDENCE_THRESHOLD:
                logger.info(f"Auto-assigning incident {incident_id} (confidence threshold met)")
                assignment_service = get_assignment_service()

                assignment = await assignment_service.assign_incident(
                    incident=db_incident,
                    classification=classification,
                    db=db
                )

                if assignment.organization_id:
                    # Assign incident to organization
                    db_incident.routed_to = assignment.organization_id
                    db_incident.status = 'in_progress'

                    # Store assignment details in metadata
                    if 'assignment_history' not in db_incident.meta_data:
                        db_incident.meta_data['assignment_history'] = []

                    db_incident.meta_data['assignment_history'].append({
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

                    # Forward to external API if organization has it enabled
                    from models.organization_model import OrganizationORM
                    org = db.query(OrganizationORM).filter(
                        OrganizationORM.id == assignment.organization_id
                    ).first()

                    if org and org.api_enabled and org.api_type:
                        logger.info(f"Forwarding incident {incident_id} to {org.api_type} API")

                        # Prepare incident data for forwarding
                        incident_dict = {
                            'incident_id': incident_id,
                            'title': db_incident.title or 'Untitled',
                            'description': db_incident.description or '',
                            'latitude': db_incident.latitude,
                            'longitude': db_incident.longitude,
                            'heading': db_incident.heading,
                            'category': db_incident.category,
                            'priority': db_incident.priority
                        }

                        org_dict = {
                            'id': org.id,
                            'name': org.name,
                            'api_type': org.api_type
                        }

                        # Forward incident
                        success, error_msg = await SEDAPService.forward_incident(incident_dict, org_dict)

                        # Store forwarding status in metadata
                        if 'api_forwards' not in db_incident.meta_data:
                            db_incident.meta_data['api_forwards'] = []

                        db_incident.meta_data['api_forwards'].append({
                            'organization_id': org.id,
                            'organization_name': org.name,
                            'api_type': org.api_type,
                            'forwarded_at': datetime.utcnow().isoformat(),
                            'status': 'success' if success else 'failed',
                            'error_message': error_msg
                        })

                        # Add chat message confirming forwarding
                        if success:
                            confirmation_msg = f"Thank you! We forwarded the information to {org.name} who are taking action as we speak."
                            chat.add_message("system", confirmation_msg)
                            logger.info(f"Successfully forwarded incident {incident_id} to {org.name}")
                        else:
                            logger.error(f"Failed to forward incident {incident_id} to {org.api_type}: {error_msg}")
                else:
                    logger.warning(
                        f"Could not auto-assign incident {incident_id}: {assignment.reasoning}"
                    )
            else:
                if not Config.AUTO_ASSIGN_ENABLED:
                    logger.info(f"Auto-assignment disabled, skipping for incident {incident_id}")
                else:
                    logger.info(
                        f"Incident {incident_id} confidence {classification.confidence:.2f} "
                        f"below threshold {Config.AUTO_ASSIGN_CONFIDENCE_THRESHOLD:.2f}, skipping auto-assignment"
                    )

        except Exception as classify_error:
            logger.error(f"Classification/assignment failed for {incident_id}: {classify_error}", exc_info=True)
            # Continue with incident creation even if classification fails
            db_incident.meta_data['classification_error'] = str(classify_error)

        db.commit()

        logger.info(f"Incident {incident_id} created successfully with session {session_id}")

        # Prepare response
        response = IncidentResponse.from_orm(db_incident, image_url, audio_url, transcription)
        logger.info(f"Incident response user_phone: {response.user_phone}")

        # Broadcast new incident via WebSocket
        try:
            await websocket_manager.broadcast_incident(
                incident_data=response.dict(),
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
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        return IncidentResponse.from_orm(incident, image_url, audio_url, audio_transcript)

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
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        # Prepare response
        response = IncidentResponse.from_orm(incident, image_url, audio_url, audio_transcript)

        # Broadcast incident update via WebSocket
        try:
            await websocket_manager.broadcast_incident(
                incident_data=response.dict(),
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

        # Build response with media URLs
        result = []
        for inc in incidents:
            image_url = None
            audio_url = None
            audio_transcript = None

            # Use eager-loaded media_files relationship
            if hasattr(inc, 'media_files') and inc.media_files:
                for media in inc.media_files:
                    if media.media_type == 'image' and not image_url:
                        image_url = media.file_url
                    elif media.media_type == 'audio' and not audio_url:
                        audio_url = media.file_url
                        # Get transcription from audio media
                        if hasattr(media, 'transcription') and media.transcription:
                            audio_transcript = media.transcription

            result.append(IncidentResponse.from_orm(inc, image_url, audio_url, audio_transcript))

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

        # Forward to external API if organization has it enabled
        if org.api_enabled and org.api_type:
            logger.info(f"Forwarding incident {incident_id} to {org.api_type} API")

            # Prepare incident data for forwarding
            incident_dict = {
                'incident_id': incident.incident_id,
                'title': incident.title or 'Untitled',
                'description': incident.description or '',
                'latitude': incident.latitude,
                'longitude': incident.longitude,
                'heading': incident.heading,
                'category': incident.category,
                'priority': incident.priority
            }

            org_dict = {
                'id': org.id,
                'name': org.name,
                'api_type': org.api_type
            }

            # Forward incident
            success, error_msg = await SEDAPService.forward_incident(incident_dict, org_dict)

            # Store forwarding status in metadata
            if not incident.meta_data:
                incident.meta_data = {}
            if 'api_forwards' not in incident.meta_data:
                incident.meta_data['api_forwards'] = []

            incident.meta_data['api_forwards'].append({
                'organization_id': org.id,
                'organization_name': org.name,
                'api_type': org.api_type,
                'forwarded_at': datetime.utcnow().isoformat(),
                'status': 'success' if success else 'failed',
                'error_message': error_msg
            })

            # Add chat message confirming forwarding
            if success:
                session = get_session_by_incident(db, incident.id)
                if session:
                    chat = ChatHistory(db, session.session_id)
                    confirmation_msg = f"Thank you! We forwarded the information to {org.name} who are taking action as we speak."
                    chat.add_message("system", confirmation_msg)
                    logger.info(f"Successfully forwarded incident {incident_id} to {org.name}")
            else:
                logger.error(f"Failed to forward incident {incident_id} to {org.api_type}: {error_msg}")

        db.commit()
        db.refresh(incident)

        logger.info(f"Assigned incident {incident_id} to organization {org.name}")

        # Get associated media for response
        media_files = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id
        ).all()

        image_url = None
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        # Prepare response
        response = IncidentResponse.from_orm(incident, image_url, audio_url, audio_transcript)

        # Broadcast assignment via WebSocket
        try:
            await websocket_manager.broadcast_incident(
                incident_data=response.dict(),
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
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        # Prepare response
        response = IncidentResponse.from_orm(incident, image_url, audio_url, audio_transcript)

        # Broadcast unassignment via WebSocket
        try:
            await websocket_manager.broadcast_incident(
                incident_data=response.dict(),
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
        audio_url = None
        audio_transcript = None
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url
                if hasattr(media, 'transcription') and media.transcription:
                    audio_transcript = media.transcription

        # Broadcast updated incident
        response = IncidentResponse.from_orm(incident, image_url, audio_url, audio_transcript)
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
