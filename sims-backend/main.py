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
from fastapi import UploadFile, File, HTTPException, Depends, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse
from fastapi.websockets import WebSocketState
from pydantic import BaseModel
from sqlalchemy.orm import Session
from dashboard import dashboard
from incident_chat import incident_page
from transcription_service import TranscriptionService
from endpoints.incident import incident_router
from db.connection import get_db
from models.incident_model import IncidentCreate
from websocket import websocket_manager
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

# Create directories
UPLOAD_PATH.mkdir(exist_ok=True)
IMAGE_DIR.mkdir(exist_ok=True)
AUDIO_DIR.mkdir(exist_ok=True)

app.add_static_files('/static', str(RESOURCES_PATH))
app.add_static_files('/static/uploads', str(UPLOAD_PATH))

# Initialize transcription service
transcription_service = TranscriptionService()

# Register incident router
app.include_router(incident_router)


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


@ui.page('/health')
def health():
    """Health check endpoint"""
    return {'status': 'ok', 'service': 'sims-backend'}


@app.get('/api/health')
async def api_health():
    """API Health check endpoint"""
    return {'status': 'ok', 'service': 'sims-api'}


@app.post('/api/upload/image')
async def upload_image(file: UploadFile = File(...)):
    """Upload an image file"""
    try:
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

        file_url = f'/static/uploads/images/{file_id}'

        logger.info(f'Image uploaded successfully: {file_id}')

        return JSONResponse(
            content={
                'success': True,
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
async def upload_audio(file: UploadFile = File(...)):
    """Upload an audio file and optionally transcribe it"""
    try:
        allowed_extensions = {'.m4a', '.mp3', '.wav', '.ogg', '.aac'}
        file_ext = Path(file.filename).suffix.lower()

        if file_ext not in allowed_extensions:
            raise HTTPException(
                status_code=400,
                detail=f'Invalid file type. Allowed: {allowed_extensions}',
            )

        file_id = f'{uuid.uuid4()}{file_ext}'
        file_path = AUDIO_DIR / file_id

        with open(file_path, 'wb') as f:
            content = await file.read()
            f.write(content)

        file_url = f'/static/uploads/audio/{file_id}'

        logger.info(f'Audio uploaded successfully: {file_id}')

        transcription_text = None
        try:
            transcription_text = await transcription_service.transcribe_and_get_text(
                str(file_path)
            )
            if transcription_text:
                logger.info(f'Transcription completed: {transcription_text[:100]}...')
        except Exception as e:
            logger.error(f'Transcription failed: {e}')

        return JSONResponse(
            content={
                'success': True,
                'url': file_url,
                'filename': file_id,
                'metadata': {
                    'size': len(content),
                    'type': file.content_type,
                    'uploaded_at': datetime.now().isoformat(),
                    'transcription': transcription_text,
                },
            }
        )

    except Exception as e:
        logger.error(f'Error uploading audio: {e}', exc_info=True)
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
