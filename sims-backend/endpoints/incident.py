"""
Incident API Endpoints
Handles incident creation, retrieval, and chat message management
"""
import logging
import uuid
from typing import List, Optional
from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session
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
from websocket import websocket_manager
from pydantic import BaseModel

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
        # Generate IDs
        incident_uuid = uuid.uuid4()
        incident_id = f"INC-{uuid.uuid4().hex[:8].upper()}"

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
        db_incident = IncidentORM(
            id=incident_uuid,
            incident_id=incident_id,
            user_phone=incident_data.user_phone or incident_data.metadata.get('user_phone'),
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

        # Create chat session
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
            image_url = incident_data.imageUrl

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
            audio_url = incident_data.audioUrl

        db.commit()

        logger.info(f"Incident {incident_id} created successfully with session {session_id}")

        # Prepare response
        response = IncidentResponse.from_orm(db_incident, image_url, audio_url)

        # Broadcast new incident via WebSocket
        try:
            await websocket_manager.broadcast_incident(
                incident_data=response.dict(),
                event_type='incident_new'
            )
            logger.info(f"Broadcasted new incident {incident_id} via WebSocket")
        except Exception as ws_error:
            logger.error(f"Failed to broadcast incident via WebSocket: {ws_error}")

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
        for media in media_files:
            if media.media_type == 'image' and not image_url:
                image_url = media.file_url
            elif media.media_type == 'audio' and not audio_url:
                audio_url = media.file_url

        return IncidentResponse.from_orm(incident, image_url, audio_url)

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

        # Prepare response
        response = IncidentResponse.from_orm(incident)

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
        query = db.query(IncidentORM)

        if status_filter:
            query = query.filter(IncidentORM.status == status_filter)

        if priority_filter:
            query = query.filter(IncidentORM.priority == priority_filter)

        incidents = query.order_by(
            IncidentORM.created_at.desc()
        ).offset(skip).limit(limit).all()

        return [IncidentResponse.from_orm(inc) for inc in incidents]

    except Exception as e:
        logger.error(f"Error listing incidents: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to list incidents: {str(e)}"
        )
