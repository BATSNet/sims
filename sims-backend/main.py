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
from fastapi import UploadFile, File, HTTPException
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from dashboard import dashboard
from incident_chat import incident_page
from transcription_service import TranscriptionService
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


class IncidentCreate(BaseModel):
    title: str
    description: str
    imageUrl: Optional[str] = None
    audioUrl: Optional[str] = None
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    heading: Optional[float] = None
    timestamp: Optional[str] = None
    metadata: Optional[dict] = None


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
async def create_incident(incident: IncidentCreate):
    """Create a new incident report"""
    try:
        incident_id = f'INC-{uuid.uuid4().hex[:8].upper()}'

        incident_data = {
            'id': incident_id,
            'title': incident.title,
            'description': incident.description,
            'imageUrl': incident.imageUrl,
            'audioUrl': incident.audioUrl,
            'latitude': incident.latitude,
            'longitude': incident.longitude,
            'heading': incident.heading,
            'timestamp': incident.timestamp or datetime.now().isoformat(),
            'metadata': incident.metadata or {},
            'status': 'open',
            'priority': 'medium',
        }

        logger.info(f'Incident created: {incident_id}')

        return JSONResponse(content=incident_data, status_code=201)

    except Exception as e:
        logger.error(f'Error creating incident: {e}', exc_info=True)
        raise HTTPException(status_code=500, detail=str(e))


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
