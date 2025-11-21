"""
SIMS Backend - Main Application Entry Point
Situation Incident Management System - Operator Dashboard
"""
import logging
import os
import uuid
from datetime import datetime
from pathlib import Path
from typing import Optional
from nicegui import app, ui
from fastapi import UploadFile, File, Form, HTTPException, Depends, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse
from fastapi.websockets import WebSocketState
from pydantic import BaseModel
from sqlalchemy.orm import Session
from dashboard import dashboard
from incident_chat import incident_page
from organizations import organizations_page
from transcription_service import TranscriptionService
from endpoints.incident import incident_router
from endpoints.organization import organization_router
from endpoints.responder import responder_router
from db.connection import get_db
# Import all models to ensure SQLAlchemy relationships are resolved
import models
from models.incident_model import IncidentCreate
from websocket import websocket_manager
from services.schedule_summarization import (
    start_summarization_service,
    stop_summarization_service,
    get_summarization_service
)
import theme

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Configure static files
RESOURCES_PATH = Path(__file__).parent.parent / 'resources'
UPLOAD_PATH = Path(__file__).parent / 'uploads'
IMAGE_DIR = UPLOAD_PATH / 'images'
AUDIO_DIR = UPLOAD_PATH / 'audio'
VIDEO_DIR = UPLOAD_PATH / 'videos'

# Create directories
UPLOAD_PATH.mkdir(exist_ok=True)
IMAGE_DIR.mkdir(exist_ok=True)
AUDIO_DIR.mkdir(exist_ok=True)
VIDEO_DIR.mkdir(exist_ok=True)

app.add_static_files('/static', str(RESOURCES_PATH))
app.add_static_files('/static/uploads', str(UPLOAD_PATH))

# Initialize transcription service
transcription_service = TranscriptionService()

# Register routers
app.include_router(incident_router)
app.include_router(organization_router)
app.include_router(responder_router)

# Import responder portal to register NiceGUI routes
import responder_portal
responder_portal.init_responder_portal()


# Startup and shutdown events
@app.on_event("startup")
async def startup_event():
    """Initialize services on application startup"""
    try:
        logger.info("Starting summarization service...")
        await start_summarization_service()
        logger.info("Summarization service started successfully")
    except Exception as e:
        logger.error(f"Failed to start summarization service: {e}", exc_info=True)

    # Generate test responder token for development
    try:
        import secrets
        from auth.responder_auth import hash_token
        from models.organization_model import OrganizationORM
        from models.organization_token_model import OrganizationTokenORM

        db = next(get_db())

        # Get first organization or create a test one
        org = db.query(OrganizationORM).first()

        if org:
            # Check if test token already exists
            existing_token = db.query(OrganizationTokenORM).filter(
                OrganizationTokenORM.organization_id == org.id,
                OrganizationTokenORM.created_by == 'system_startup'
            ).first()

            if not existing_token:
                # Generate test token
                plain_token = secrets.token_urlsafe(32)
                token_hash = hash_token(plain_token)

                test_token = OrganizationTokenORM(
                    organization_id=org.id,
                    token=token_hash,
                    created_by='system_startup',
                    created_at=datetime.utcnow(),
                    active=True
                )

                db.add(test_token)
                db.commit()

                logger.info("=" * 80)
                logger.info("TEST RESPONDER TOKEN GENERATED")
                logger.info(f"Organization: {org.name} (ID: {org.id})")
                logger.info(f"Token: {plain_token}")
                logger.info(f"Access URL: http://localhost:8000/responder?token={plain_token}")
                logger.info("=" * 80)
            else:
                logger.info(f"Test token already exists for organization: {org.name}")
        else:
            logger.warning("No organizations found. Create an organization first to generate a test token.")

        db.close()

    except Exception as e:
        logger.error(f"Failed to generate test token: {e}", exc_info=True)


@app.on_event("shutdown")
async def shutdown_event():
    """Cleanup services on application shutdown"""
    try:
        logger.info("Stopping summarization service...")
        await stop_summarization_service()
        logger.info("Summarization service stopped successfully")
    except Exception as e:
        logger.error(f"Error stopping summarization service: {e}", exc_info=True)


@app.websocket("/ws/incidents")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket endpoint for real-time incident updates"""
    client_id = None

    try:
        headers = dict(websocket.headers)
        logger.info(f"WebSocket connection attempt with headers: {headers}")

        # Check if this is a mobile app connection
        app_id = next((value for key, value in headers.items()
                       if key.lower() == 'sims_app_id'), None)

        # For now, accept all connections (authentication can be added later)
        # Generate client ID from app_id or use a UUID
        if app_id:
            client_id = f"mobile_{app_id}_{uuid.uuid4().hex[:8]}"
        else:
            client_id = f"web_{uuid.uuid4().hex[:8]}"

        # Accept the WebSocket connection
        await websocket.accept()
        await websocket_manager.connect(websocket, client_id)

        logger.info(f"WebSocket connection established: {client_id}")

        # Message loop
        while True:
            try:
                data = await websocket.receive_json()
                logger.info(f"WebSocket message from {client_id}: {data}")

                message_type = data.get('type')

                if message_type == 'subscribe':
                    channel = data.get('channel')
                    if channel:
                        await websocket_manager.subscribe(client_id, channel)
                        # Send confirmation
                        await websocket_manager.broadcast_to_client(client_id, {
                            'type': 'subscribed',
                            'channel': channel,
                            'timestamp': datetime.utcnow().isoformat()
                        })

                elif message_type == 'unsubscribe':
                    channel = data.get('channel')
                    if channel:
                        await websocket_manager.unsubscribe(client_id, channel)
                        await websocket_manager.broadcast_to_client(client_id, {
                            'type': 'unsubscribed',
                            'channel': channel,
                            'timestamp': datetime.utcnow().isoformat()
                        })

                elif message_type == 'ping':
                    await websocket_manager.broadcast_to_client(client_id, {
                        'type': 'pong',
                        'timestamp': datetime.utcnow().isoformat()
                    })

            except WebSocketDisconnect:
                logger.info(f"WebSocket disconnected: {client_id}")
                break
            except Exception as e:
                logger.error(f"Message processing error for {client_id}: {e}")
                break

    except Exception as e:
        logger.error(f"WebSocket error: {e}")
        if websocket.client_state != WebSocketState.DISCONNECTED:
            await websocket.close(code=1011, reason="Internal server error")
    finally:
        if client_id:
            await websocket_manager.disconnect(client_id)


@ui.page('/')
async def index():
    """Main dashboard page"""
    async with theme.frame('S.I.M.S. Command'):
        await dashboard()


@ui.page('/incident')
async def incident():
    """Incident reporting chat page - blank for Flutter WebView"""
    # Apply theme colors but no sidebar/header
    theme.apply_theme()
    theme.inject_custom_css()
    ui.dark_mode(True)

    # Just the chat content, no frame
    await incident_page()


@ui.page('/organizations')
async def organizations():
    """Organization management page"""
    async with theme.frame('Organizations'):
        await organizations_page()


@ui.page('/health')
def health():
    """Health check endpoint"""
    return {'status': 'ok', 'service': 'sims-backend'}


@app.get('/api/health')
async def api_health():
    """API Health check endpoint"""
    return {'status': 'ok', 'service': 'sims-api'}


@app.get('/api/media/image/{filename}')
async def serve_image(filename: str):
    """Serve image file"""
    from fastapi.responses import FileResponse
    file_path = IMAGE_DIR / filename
    if not file_path.exists():
        raise HTTPException(status_code=404, detail='Image not found')
    return FileResponse(file_path, media_type='image/jpeg')


@app.get('/api/media/audio/{filename}')
async def serve_audio(filename: str):
    """Serve audio file"""
    from fastapi.responses import FileResponse
    file_path = AUDIO_DIR / filename
    if not file_path.exists():
        raise HTTPException(status_code=404, detail='Audio not found')
    return FileResponse(file_path, media_type='audio/mpeg')


@app.get('/api/media/video/{filename}')
async def serve_video(filename: str):
    """Serve video file"""
    from fastapi.responses import FileResponse
    file_path = VIDEO_DIR / filename
    if not file_path.exists():
        raise HTTPException(status_code=404, detail='Video not found')
    return FileResponse(file_path, media_type='video/mp4')


@app.post('/api/upload/image')
async def upload_image(
    file: UploadFile = File(...),
    incident_id: Optional[str] = Form(None),
    db: Session = Depends(get_db)
):
    """Upload an image file"""
    try:
        from models.media_model import MediaORM, MediaType

        allowed_extensions = {'.jpg', '.jpeg', '.png', '.webp'}
        file_ext = Path(file.filename).suffix.lower()

        if file_ext not in allowed_extensions:
            raise HTTPException(
                status_code=400,
                detail=f'Invalid file type. Allowed: {allowed_extensions}',
            )

        file_id = f'{uuid.uuid4()}{file_ext}'
        file_path = IMAGE_DIR / file_id

        with open(file_path, 'wb') as f:
            content = await file.read()
            f.write(content)

        file_url = f'/api/media/image/{file_id}'

        logger.info(f'[DEBUG] Image upload: file_id={file_id}, incident_id={incident_id}')

        # Create Media record (link to incident if provided)
        media_uuid = uuid.uuid4()
        parsed_incident_id = None
        if incident_id:
            try:
                parsed_incident_id = uuid.UUID(incident_id)
            except ValueError as e:
                logger.error(f'Invalid incident_id UUID format: {incident_id}')
                raise HTTPException(
                    status_code=400,
                    detail=f'Invalid incident_id format. Expected UUID, got: {incident_id}'
                )

        media = MediaORM(
            id=media_uuid,
            incident_id=parsed_incident_id,
            file_path=str(file_path),
            file_url=file_url,
            mime_type=file.content_type or 'image/jpeg',
            file_size=len(content),
            media_type=MediaType.IMAGE,
            meta_data={'original_filename': file.filename}
        )
        db.add(media)
        db.commit()
        db.refresh(media)

        # Broadcast media upload via WebSocket if linked to incident
        if parsed_incident_id:
            try:
                from websocket import websocket_manager
                from models.incident_model import IncidentORM, IncidentResponse

                incident = db.query(IncidentORM).filter(
                    IncidentORM.id == parsed_incident_id
                ).first()

                if incident:
                    # Get all media for this incident
                    media_files = db.query(MediaORM).filter(
                        MediaORM.incident_id == parsed_incident_id
                    ).all()

                    image_url = None
                    audio_url = None
                    audio_transcript = None
                    for m in media_files:
                        if m.media_type == 'image' and not image_url:
                            image_url = m.file_url
                        elif m.media_type == 'audio' and not audio_url:
                            audio_url = m.file_url
                            if hasattr(m, 'transcription') and m.transcription:
                                audio_transcript = m.transcription

                    response = IncidentResponse.from_orm(incident, image_url, audio_url, audio_transcript)
                    await websocket_manager.broadcast_incident(
                        incident_data=response.dict(),
                        event_type='media_upload'
                    )
            except Exception as ws_error:
                logger.error(f"Failed to broadcast media upload: {ws_error}")

        return JSONResponse(
            content={
                'success': True,
                'media_id': str(media_uuid) if incident_id else None,
                'url': file_url,
                'filename': file_id,
                'metadata': {
                    'size': len(content),
                    'type': file.content_type,
                    'uploaded_at': datetime.now().isoformat(),
                },
            }
        )

    except Exception as e:
        logger.error(f'Error uploading image: {e}', exc_info=True)
        raise HTTPException(status_code=500, detail=str(e))


@app.post('/api/upload/audio')
async def upload_audio(
    file: UploadFile = File(...),
    incident_id: Optional[str] = Form(None),
    db: Session = Depends(get_db)
):
    """Upload an audio file and queue it for transcription"""
    try:
        from models.media_model import MediaORM, MediaType

        allowed_extensions = {'.m4a', '.mp3', '.wav', '.ogg', '.aac'}
        file_ext = Path(file.filename).suffix.lower()

        if file_ext not in allowed_extensions:
            raise HTTPException(
                status_code=400,
                detail=f'Invalid file type. Allowed: {allowed_extensions}',
            )

        file_id = f'{uuid.uuid4()}{file_ext}'
        file_path = AUDIO_DIR / file_id
        content = await file.read()

        with open(file_path, 'wb') as f:
            f.write(content)

        file_url = f'/api/media/audio/{file_id}'

        logger.info(f'[DEBUG] Audio upload: file_id={file_id}, incident_id={incident_id}')

        # Create Media record (link to incident if provided)
        media_uuid = uuid.uuid4()
        parsed_incident_id = None
        if incident_id:
            try:
                parsed_incident_id = uuid.UUID(incident_id)
            except ValueError as e:
                logger.error(f'Invalid incident_id UUID format: {incident_id}')
                raise HTTPException(
                    status_code=400,
                    detail=f'Invalid incident_id format. Expected UUID, got: {incident_id}'
                )

        media = MediaORM(
            id=media_uuid,
            incident_id=parsed_incident_id,
            file_path=str(file_path),
            file_url=file_url,
            mime_type=file.content_type or 'audio/m4a',
            file_size=len(content),
            media_type=MediaType.AUDIO,
            transcription=None,  # Will be filled by batch processor
            meta_data={'original_filename': file.filename}
        )
        db.add(media)
        db.commit()
        db.refresh(media)

        # Queue for batch transcription
        try:
            summarization_service = get_summarization_service()
            await summarization_service.queue_audio_transcription(
                media_id=str(media_uuid),
                file_path=str(file_path),
                db_session=db
            )
            logger.info(f'Audio {file_id} queued for transcription')
            transcription_status = 'pending'
        except Exception as e:
            logger.error(f'Failed to queue transcription: {e}')
            transcription_status = 'failed'

        # Broadcast media upload via WebSocket if linked to incident
        if parsed_incident_id:
            try:
                from websocket import websocket_manager
                from models.incident_model import IncidentORM, IncidentResponse

                incident = db.query(IncidentORM).filter(
                    IncidentORM.id == parsed_incident_id
                ).first()

                if incident:
                    # Get all media for this incident
                    media_files = db.query(MediaORM).filter(
                        MediaORM.incident_id == parsed_incident_id
                    ).all()

                    image_url = None
                    audio_url = None
                    audio_transcript = None
                    for m in media_files:
                        if m.media_type == 'image' and not image_url:
                            image_url = m.file_url
                        elif m.media_type == 'audio' and not audio_url:
                            audio_url = m.file_url
                            if hasattr(m, 'transcription') and m.transcription:
                                audio_transcript = m.transcription

                    response = IncidentResponse.from_orm(incident, image_url, audio_url, audio_transcript)
                    await websocket_manager.broadcast_incident(
                        incident_data=response.dict(),
                        event_type='media_upload'
                    )
            except Exception as ws_error:
                logger.error(f"Failed to broadcast media upload: {ws_error}")

        return JSONResponse(
            content={
                'success': True,
                'media_id': str(media_uuid),
                'url': file_url,
                'filename': file_id,
                'metadata': {
                    'size': len(content),
                    'type': file.content_type,
                    'uploaded_at': datetime.now().isoformat(),
                    'transcription_status': transcription_status,
                },
            }
        )

    except Exception as e:
        logger.error(f'Error uploading audio: {e}', exc_info=True)
        raise HTTPException(status_code=500, detail=str(e))


@app.post('/api/upload/video')
async def upload_video(
    file: UploadFile = File(...),
    incident_id: Optional[str] = Form(None),
    db: Session = Depends(get_db)
):
    """Upload a video file"""
    try:
        from models.media_model import MediaORM, MediaType

        logger.info(f'Uploading video file: {file.filename}, content_type: {file.content_type}')

        allowed_extensions = {'.mp4', '.mov', '.avi', '.mkv', '.webm', '.3gp'}
        file_ext = Path(file.filename).suffix.lower()

        # If no extension, try to infer from content type
        if not file_ext and file.content_type:
            content_type_map = {
                'video/mp4': '.mp4',
                'video/quicktime': '.mov',
                'video/x-msvideo': '.avi',
                'video/x-matroska': '.mkv',
                'video/webm': '.webm',
                'video/3gpp': '.3gp',
            }
            file_ext = content_type_map.get(file.content_type, '.mp4')
            logger.info(f'Inferred extension from content type: {file_ext}')

        if file_ext not in allowed_extensions:
            logger.error(f'Invalid file extension: {file_ext} for file: {file.filename}')
            raise HTTPException(
                status_code=400,
                detail=f'Invalid file type. Allowed: {allowed_extensions}',
            )

        file_id = f'{uuid.uuid4()}{file_ext}'
        file_path = VIDEO_DIR / file_id
        content = await file.read()

        with open(file_path, 'wb') as f:
            f.write(content)

        file_url = f'/api/media/video/{file_id}'

        logger.info(f'Video uploaded successfully: {file_id}')

        # Create Media record (link to incident if provided)
        media_uuid = uuid.uuid4()
        parsed_incident_id = None
        if incident_id:
            try:
                parsed_incident_id = uuid.UUID(incident_id)
            except ValueError as e:
                logger.error(f'Invalid incident_id UUID format: {incident_id}')
                raise HTTPException(
                    status_code=400,
                    detail=f'Invalid incident_id format. Expected UUID, got: {incident_id}'
                )

        media = MediaORM(
            id=media_uuid,
            incident_id=parsed_incident_id,
            file_path=str(file_path),
            file_url=file_url,
            mime_type=file.content_type or 'video/mp4',
            file_size=len(content),
            media_type=MediaType.VIDEO,
            transcription=None,
            meta_data={'original_filename': file.filename}
        )
        db.add(media)
        db.commit()
        db.refresh(media)

        return JSONResponse(
            content={
                'success': True,
                'media_id': str(media_uuid),
                'url': file_url,
                'filename': file_id,
                'metadata': {
                    'size': len(content),
                    'type': file.content_type,
                    'uploaded_at': datetime.now().isoformat(),
                },
            }
        )
    except Exception as e:
        logger.error(f'Error uploading video: {e}', exc_info=True)
        raise HTTPException(status_code=500, detail=str(e))


@app.post('/api/incidents')
async def create_incident_legacy(incident: IncidentCreate, db: Session = Depends(get_db)):
    """
    Legacy endpoint for backward compatibility with Flutter app.
    Routes to the incident router endpoint.
    """
    from endpoints.incident import create_incident
    return await create_incident(incident, db)


def main():
    """Run the SIMS backend application"""
    try:
        logger.info('Starting SIMS Backend...')

        ui.run(
            host='0.0.0.0',
            port=8000,
            title='SIMS - Situation Incident Management System',
            favicon=str(RESOURCES_PATH / 'sims-icon.svg'),
            dark=True,
            reload=False,
            show=False,
            show_welcome_message=False,
            storage_secret='demo-secret-key-2025',
        )

    except Exception as e:
        logger.error(f'Failed to start SIMS Backend: {e}', exc_info=True)
        raise


if __name__ in {"__main__", "__mp_main__"}:
    main()
