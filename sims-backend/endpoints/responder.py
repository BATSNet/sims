"""
Responder API Endpoints
Handles responder portal access for organizations
"""
import logging
from typing import List
from uuid import UUID
from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, status, Query
from sqlalchemy.orm import Session, joinedload
from pydantic import BaseModel

from db.connection import get_db
from auth.responder_auth import validate_responder_token
from models.incident_model import IncidentORM, IncidentResponse, IncidentStatus, IncidentPriority
from models.incident_note_model import IncidentNoteORM, IncidentNoteCreate, IncidentNoteResponse
from models.organization_token_model import OrganizationTokenORM
from models.organization_model import OrganizationORM
from models.media_model import MediaORM
from services.chat_history import get_session_by_incident
from websocket import websocket_manager

logger = logging.getLogger(__name__)

responder_router = APIRouter(prefix="/api/responder", tags=["responder"])


class ChatMessageResponse(BaseModel):
    """Response model for chat messages"""
    role: str
    content: str
    timestamp: str


class ChatMessageCreate(BaseModel):
    """Request model for creating a chat message"""
    content: str
    role: str = "assistant"


class StatusUpdate(BaseModel):
    """Request model for updating incident status"""
    status: IncidentStatus


class PriorityUpdate(BaseModel):
    """Request model for updating incident priority"""
    priority: IncidentPriority


@responder_router.get("/incidents", response_model=List[IncidentResponse])
async def list_organization_incidents(
    token_data: tuple = Depends(validate_responder_token),
    db: Session = Depends(get_db)
):
    """
    List all incidents assigned to the responder's organization.

    Requires valid organization token in query parameter.
    """
    token_record, organization = token_data

    logger.info(f"Listing incidents for organization: {organization.name} (ID: {organization.id})")

    # Query incidents assigned to this organization
    incidents = db.query(IncidentORM).options(
        joinedload(IncidentORM.organization)
    ).filter(
        IncidentORM.routed_to == organization.id
    ).order_by(
        IncidentORM.created_at.desc()
    ).all()

    logger.info(f"Found {len(incidents)} incidents for organization {organization.id}")

    # Convert to response models
    responses = []
    for incident in incidents:
        # Get media files for this incident
        media_files = db.query(MediaORM).filter(
            MediaORM.incident_id == incident.id
        ).all()

        media_urls = [media.file_url for media in media_files]

        response = IncidentResponse(
            id=str(incident.id),
            incident_id=incident.incident_id,
            user_phone=incident.user_phone,
            latitude=incident.latitude,
            longitude=incident.longitude,
            heading=incident.heading,
            title=incident.title,
            description=incident.description,
            status=incident.status,
            priority=incident.priority,
            category=incident.category,
            routed_to=incident.routed_to,
            organization_name=incident.organization.name if incident.organization else None,
            tags=incident.tags or [],
            metadata=incident.meta_data or {},
            media_urls=media_urls,
            created_at=incident.created_at.isoformat(),
            updated_at=incident.updated_at.isoformat()
        )
        responses.append(response)

    return responses


@responder_router.get("/incidents/{incident_id}", response_model=IncidentResponse)
async def get_incident_details(
    incident_id: UUID,
    token_data: tuple = Depends(validate_responder_token),
    db: Session = Depends(get_db)
):
    """
    Get details of a specific incident.

    Verifies that the incident is assigned to the responder's organization.
    """
    token_record, organization = token_data

    # Get incident and verify access
    incident = db.query(IncidentORM).options(
        joinedload(IncidentORM.organization)
    ).filter(
        IncidentORM.id == incident_id
    ).first()

    if not incident:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Incident not found"
        )

    # Verify access
    if incident.routed_to != organization.id:
        logger.warning(
            f"Organization {organization.id} attempted to access incident {incident_id} "
            f"assigned to organization {incident.routed_to}"
        )
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="You do not have access to this incident"
        )

    # Get media files
    media_files = db.query(MediaORM).filter(
        MediaORM.incident_id == incident.id
    ).all()

    media_urls = [media.file_url for media in media_files]

    return IncidentResponse(
        id=str(incident.id),
        incident_id=incident.incident_id,
        user_phone=incident.user_phone,
        latitude=incident.latitude,
        longitude=incident.longitude,
        heading=incident.heading,
        title=incident.title,
        description=incident.description,
        status=incident.status,
        priority=incident.priority,
        category=incident.category,
        routed_to=incident.routed_to,
        organization_name=incident.organization.name if incident.organization else None,
        tags=incident.tags or [],
        metadata=incident.meta_data or {},
        media_urls=media_urls,
        created_at=incident.created_at.isoformat(),
        updated_at=incident.updated_at.isoformat()
    )


@responder_router.get("/incidents/{incident_id}/chat", response_model=List[ChatMessageResponse])
async def get_incident_chat(
    incident_id: UUID,
    token_data: tuple = Depends(validate_responder_token),
    db: Session = Depends(get_db)
):
    """
    Get chat history for an incident.

    Returns all messages between the reporter and responders.
    """
    token_record, organization = token_data

    # Verify incident exists and is assigned to this org
    incident = db.query(IncidentORM).filter(
        IncidentORM.id == incident_id
    ).first()

    if not incident:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Incident not found"
        )

    if incident.routed_to != organization.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="You do not have access to this incident"
        )

    # Get chat session
    session_id = get_session_by_incident(db, incident_id)

    if not session_id:
        logger.info(f"No chat session found for incident {incident_id}")
        return []

    # Get chat history
    from services.chat_history import ChatHistory
    chat_history = ChatHistory(db, session_id)
    messages = chat_history.get_messages()

    logger.info(f"Retrieved {len(messages)} messages for incident {incident_id}")

    return [
        ChatMessageResponse(
            role=msg['role'],
            content=msg['content'],
            timestamp=msg['timestamp'].isoformat()
        )
        for msg in messages
    ]


@responder_router.post("/incidents/{incident_id}/chat", status_code=status.HTTP_201_CREATED)
async def send_chat_message(
    incident_id: UUID,
    message_data: ChatMessageCreate,
    token_data: tuple = Depends(validate_responder_token),
    db: Session = Depends(get_db)
):
    """
    Send a chat message to the incident reporter.

    The message will be visible to the reporter in their app.
    """
    token_record, organization = token_data

    # Verify incident exists and is assigned to this org
    incident = db.query(IncidentORM).filter(
        IncidentORM.id == incident_id
    ).first()

    if not incident:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Incident not found"
        )

    if incident.routed_to != organization.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="You do not have access to this incident"
        )

    # Get or create chat session
    session_id = get_session_by_incident(db, incident_id)

    if not session_id:
        logger.error(f"No chat session found for incident {incident_id}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Chat session not found"
        )

    # Add message to chat
    from services.chat_history import ChatHistory
    chat_history = ChatHistory(db, session_id)
    chat_history.add_message(
        role=message_data.role,
        content=message_data.content
    )

    logger.info(
        f"Organization {organization.id} sent message to incident {incident_id}: "
        f"{message_data.content[:50]}..."
    )

    # Broadcast message via WebSocket
    await websocket_manager.broadcast({
        'type': 'incident_message',
        'incident_id': str(incident_id),
        'message': {
            'role': message_data.role,
            'content': message_data.content,
            'timestamp': datetime.utcnow().isoformat()
        }
    }, topic='incidents')

    # Also broadcast to organization-specific channel
    await websocket_manager.broadcast({
        'type': 'incident_message',
        'incident_id': str(incident_id),
        'message': {
            'role': message_data.role,
            'content': message_data.content,
            'timestamp': datetime.utcnow().isoformat()
        }
    }, topic=f'org_{organization.id}')

    return {"message": "Message sent successfully"}


@responder_router.put("/incidents/{incident_id}/status")
async def update_incident_status(
    incident_id: UUID,
    status_update: StatusUpdate,
    token_data: tuple = Depends(validate_responder_token),
    db: Session = Depends(get_db)
):
    """
    Update the status of an incident.

    Available statuses: open, in_progress, resolved, closed
    """
    token_record, organization = token_data

    # Verify incident exists and is assigned to this org
    incident = db.query(IncidentORM).filter(
        IncidentORM.id == incident_id
    ).first()

    if not incident:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Incident not found"
        )

    if incident.routed_to != organization.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="You do not have access to this incident"
        )

    old_status = incident.status
    incident.status = status_update.status
    incident.updated_at = datetime.utcnow()

    # Add audit log to metadata
    if 'status_history' not in incident.meta_data:
        incident.meta_data['status_history'] = []

    incident.meta_data['status_history'].append({
        'from': old_status,
        'to': status_update.status,
        'changed_by': f"org_{organization.id}",
        'changed_at': datetime.utcnow().isoformat()
    })

    db.commit()

    logger.info(
        f"Organization {organization.id} updated incident {incident_id} "
        f"status from {old_status} to {status_update.status}"
    )

    # Broadcast update via WebSocket
    await websocket_manager.broadcast({
        'type': 'incident_status_changed',
        'incident_id': str(incident_id),
        'old_status': old_status,
        'new_status': status_update.status,
        'organization_id': organization.id
    }, topic='incidents')

    await websocket_manager.broadcast({
        'type': 'incident_status_changed',
        'incident_id': str(incident_id),
        'old_status': old_status,
        'new_status': status_update.status
    }, topic=f'org_{organization.id}')

    return {"message": "Status updated successfully", "status": status_update.status}


@responder_router.put("/incidents/{incident_id}/priority")
async def update_incident_priority(
    incident_id: UUID,
    priority_update: PriorityUpdate,
    token_data: tuple = Depends(validate_responder_token),
    db: Session = Depends(get_db)
):
    """
    Update the priority of an incident.

    Available priorities: critical, high, medium, low
    """
    token_record, organization = token_data

    # Verify incident exists and is assigned to this org
    incident = db.query(IncidentORM).filter(
        IncidentORM.id == incident_id
    ).first()

    if not incident:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Incident not found"
        )

    if incident.routed_to != organization.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="You do not have access to this incident"
        )

    old_priority = incident.priority
    incident.priority = priority_update.priority
    incident.updated_at = datetime.utcnow()

    # Add audit log to metadata
    if 'priority_history' not in incident.meta_data:
        incident.meta_data['priority_history'] = []

    incident.meta_data['priority_history'].append({
        'from': old_priority,
        'to': priority_update.priority,
        'changed_by': f"org_{organization.id}",
        'changed_at': datetime.utcnow().isoformat()
    })

    db.commit()

    logger.info(
        f"Organization {organization.id} updated incident {incident_id} "
        f"priority from {old_priority} to {priority_update.priority}"
    )

    # Broadcast update via WebSocket
    await websocket_manager.broadcast({
        'type': 'incident_priority_changed',
        'incident_id': str(incident_id),
        'old_priority': old_priority,
        'new_priority': priority_update.priority,
        'organization_id': organization.id
    }, topic='incidents')

    await websocket_manager.broadcast({
        'type': 'incident_priority_changed',
        'incident_id': str(incident_id),
        'old_priority': old_priority,
        'new_priority': priority_update.priority
    }, topic=f'org_{organization.id}')

    return {"message": "Priority updated successfully", "priority": priority_update.priority}


@responder_router.post("/incidents/{incident_id}/notes", response_model=IncidentNoteResponse, status_code=status.HTTP_201_CREATED)
async def create_incident_note(
    incident_id: UUID,
    note_data: IncidentNoteCreate,
    token_data: tuple = Depends(validate_responder_token),
    db: Session = Depends(get_db)
):
    """
    Add an internal note to an incident.

    Internal notes are only visible to responders, not to the reporter.
    """
    token_record, organization = token_data

    # Verify incident exists and is assigned to this org
    incident = db.query(IncidentORM).filter(
        IncidentORM.id == incident_id
    ).first()

    if not incident:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Incident not found"
        )

    if incident.routed_to != organization.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="You do not have access to this incident"
        )

    # Create note
    note = IncidentNoteORM(
        incident_id=incident_id,
        organization_id=organization.id,
        note_text=note_data.note_text,
        created_by=note_data.created_by,
        created_at=datetime.utcnow()
    )

    db.add(note)
    db.commit()
    db.refresh(note)

    logger.info(
        f"Organization {organization.id} added note to incident {incident_id}: "
        f"{note_data.note_text[:50]}..."
    )

    return IncidentNoteResponse(
        id=note.id,
        incident_id=note.incident_id,
        organization_id=note.organization_id,
        note_text=note.note_text,
        created_at=note.created_at.isoformat(),
        created_by=note.created_by
    )


@responder_router.get("/incidents/{incident_id}/notes", response_model=List[IncidentNoteResponse])
async def get_incident_notes(
    incident_id: UUID,
    token_data: tuple = Depends(validate_responder_token),
    db: Session = Depends(get_db)
):
    """
    Get all internal notes for an incident.

    Returns only notes created by the responder's organization.
    """
    token_record, organization = token_data

    # Verify incident exists and is assigned to this org
    incident = db.query(IncidentORM).filter(
        IncidentORM.id == incident_id
    ).first()

    if not incident:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Incident not found"
        )

    if incident.routed_to != organization.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="You do not have access to this incident"
        )

    # Get notes for this incident and organization
    notes = db.query(IncidentNoteORM).filter(
        IncidentNoteORM.incident_id == incident_id,
        IncidentNoteORM.organization_id == organization.id
    ).order_by(
        IncidentNoteORM.created_at.desc()
    ).all()

    logger.info(f"Retrieved {len(notes)} notes for incident {incident_id}, org {organization.id}")

    return [
        IncidentNoteResponse(
            id=note.id,
            incident_id=note.incident_id,
            organization_id=note.organization_id,
            note_text=note.note_text,
            created_at=note.created_at.isoformat(),
            created_by=note.created_by
        )
        for note in notes
    ]
