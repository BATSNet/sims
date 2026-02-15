"""
LoRa/Binary Incident Endpoint
Accepts compact binary payloads from ESP32 devices (WiFi or future LoRa gateway).
Parses the binary format and delegates to the standard create_incident flow.
Also provides audio transcription endpoint for ESP32 devices.
"""
import io
import logging
import struct
import base64
import wave
import tempfile
import os

from fastapi import APIRouter, Request, Depends, HTTPException, status
from fastapi.responses import JSONResponse
from sqlalchemy.orm import Session

from db.connection import get_db
from models.incident_model import IncidentCreate
from endpoints.incident import create_incident
from transcription_service import TranscriptionService
from audio_codecs.lpc10_decoder import decode_to_wav_bytes

logger = logging.getLogger(__name__)

lora_router = APIRouter(prefix="/api/lora", tags=["lora"])

BINARY_FORMAT_VERSION = 0x01

PRIORITY_MAP = {
    0: 'critical',
    1: 'high',
    2: 'medium',
    3: 'low',
}


def parse_binary_incident(data: bytes) -> dict:
    """
    Parse compact binary incident payload.

    Format (little-endian):
      [0]     uint8   version (0x01)
      [1]     uint8   flags (bit0=has_image, bit1=has_audio, bit2-3=priority,
                              bit4=raw 8kHz PCM audio)
      [2-5]   int32   latitude * 1e7
      [6-9]   int32   longitude * 1e7
      [10-11] int16   altitude (meters)
      [12-17] uint8[6] device MAC
      [18]    uint8   desc_len
      [19..]  UTF-8   description (desc_len bytes)
      [..]    uint16  image_len
      [..]    bytes   image_data (image_len bytes)
      [..]    uint32  audio_len
      [..]    bytes   audio_data (audio_len bytes)
    """
    if len(data) < 19:
        raise ValueError(f"Payload too short: {len(data)} bytes (minimum 19)")

    pos = 0

    # Version
    version = data[pos]; pos += 1
    if version != BINARY_FORMAT_VERSION:
        raise ValueError(f"Unsupported format version: {version:#x}")

    # Flags
    flags = data[pos]; pos += 1
    has_image = bool(flags & 0x01)
    has_audio = bool(flags & 0x02)
    is_raw_pcm = bool(flags & 0x10)  # bit4 = raw 16kHz 16-bit PCM audio
    priority_val = (flags >> 2) & 0x03
    priority = PRIORITY_MAP.get(priority_val, 'medium')

    # Lat/lon as int32 LE, scaled by 1e7
    lat_raw = struct.unpack_from('<i', data, pos)[0]; pos += 4
    lon_raw = struct.unpack_from('<i', data, pos)[0]; pos += 4
    latitude = lat_raw / 1e7
    longitude = lon_raw / 1e7

    # Altitude as int16 LE
    altitude = struct.unpack_from('<h', data, pos)[0]; pos += 2

    # Device MAC (6 bytes)
    mac = data[pos:pos + 6]; pos += 6
    device_id = 'xiao-esp32s3-' + ''.join(f'{b:02X}' for b in mac)

    # Description (length-prefixed UTF-8)
    desc_len = data[pos]; pos += 1
    if pos + desc_len > len(data):
        raise ValueError(f"Description extends past payload (desc_len={desc_len}, remaining={len(data) - pos})")
    description = data[pos:pos + desc_len].decode('utf-8', errors='replace')
    pos += desc_len

    # Image (length-prefixed)
    image_bytes = None
    if pos + 2 <= len(data):
        img_len = struct.unpack_from('<H', data, pos)[0]; pos += 2
        if img_len > 0:
            if pos + img_len > len(data):
                raise ValueError(f"Image extends past payload (img_len={img_len}, remaining={len(data) - pos})")
            image_bytes = data[pos:pos + img_len]
            pos += img_len

    # Audio (length-prefixed, uint32 LE)
    audio_bytes = None
    if pos + 4 <= len(data):
        aud_len = struct.unpack_from('<I', data, pos)[0]; pos += 4
        if aud_len > 0:
            if pos + aud_len > len(data):
                raise ValueError(f"Audio extends past payload (aud_len={aud_len}, remaining={len(data) - pos})")
            audio_bytes = data[pos:pos + aud_len]
            pos += aud_len

    return {
        'device_id': device_id,
        'latitude': latitude,
        'longitude': longitude,
        'altitude': float(altitude),
        'priority': priority,
        'description': description,
        'has_image': has_image,
        'has_audio': has_audio,
        'is_raw_pcm': is_raw_pcm,
        'image_bytes': image_bytes,
        'audio_bytes': audio_bytes,
    }


@lora_router.post("/incident")
async def receive_lora_incident(request: Request, db: Session = Depends(get_db)):
    """
    Receive a compact binary incident payload from an ESP32 device.
    Parses the binary format, converts media to base64 for the standard
    create_incident flow, and returns the created incident.
    """
    body = await request.body()

    if not body:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Empty request body"
        )

    logger.info(f"Received binary incident payload: {len(body)} bytes")

    try:
        parsed = parse_binary_incident(body)
    except ValueError as e:
        logger.error(f"Failed to parse binary payload: {e}")
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Invalid binary payload: {str(e)}"
        )

    logger.info(
        f"Parsed binary incident: device={parsed['device_id']}, "
        f"lat={parsed['latitude']:.6f}, lon={parsed['longitude']:.6f}, "
        f"alt={parsed['altitude']}, priority={parsed['priority']}, "
        f"desc_len={len(parsed['description'])}, "
        f"has_image={parsed['has_image']}, has_audio={parsed['has_audio']}, "
        f"is_raw_pcm={parsed['is_raw_pcm']}"
    )

    # Convert raw media bytes to base64 strings for IncidentCreate
    image_b64 = None
    audio_b64 = None
    description = parsed['description']

    if parsed['image_bytes']:
        image_b64 = base64.b64encode(parsed['image_bytes']).decode('ascii')
        logger.info(f"Image: {len(parsed['image_bytes'])} raw bytes -> {len(image_b64)} base64 chars")

    # Handle raw PCM audio: wrap in WAV header and transcribe via Whisper
    if parsed['audio_bytes'] and parsed['is_raw_pcm']:
        logger.info(f"Raw PCM audio: {len(parsed['audio_bytes'])} bytes "
                     f"({len(parsed['audio_bytes']) / (16000 * 2):.1f}s at 16kHz)")
        try:
            wav_data = decode_to_wav_bytes(parsed['audio_bytes'], sample_rate=16000)
            logger.info(f"Wrapped raw PCM in WAV: {len(wav_data)} bytes")

            # Write WAV to temp file for transcription
            tmp_path = None
            try:
                with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
                    tmp.write(wav_data)
                    tmp_path = tmp.name

                service = _get_transcription_service()
                result = await service.transcribe_audio(tmp_path, language="en")

                if result and 'text' in result and result['text'].strip():
                    transcription = result['text'].strip()
                    logger.info(f"Audio transcription: \"{transcription}\"")
                    # Use transcription as description (replaces "voice report" placeholder)
                    description = transcription
                else:
                    logger.warning("Audio transcription returned no text")
            finally:
                if tmp_path and os.path.exists(tmp_path):
                    os.unlink(tmp_path)

            # Store WAV as audio attachment (base64)
            audio_b64 = base64.b64encode(wav_data).decode('ascii')

        except Exception as e:
            logger.error(f"Audio decode/transcribe failed: {e}", exc_info=True)
            # Fall through - incident is still created with original description

    elif parsed['audio_bytes']:
        # Non-PCM audio - just base64 encode as-is
        audio_b64 = base64.b64encode(parsed['audio_bytes']).decode('ascii')
        logger.info(f"Audio: {len(parsed['audio_bytes'])} raw bytes -> {len(audio_b64)} base64 chars")

    # Build IncidentCreate model
    title = f"Voice report from {parsed['device_id']}"
    incident_data = IncidentCreate(
        title=title,
        description=description or title,
        latitude=parsed['latitude'],
        longitude=parsed['longitude'],
        altitude=parsed['altitude'] if parsed['altitude'] != 0 else None,
        image=image_b64,
        audio=audio_b64,
        has_image=parsed['has_image'],
        has_audio=parsed['has_audio'],
        metadata={
            'source': 'binary_lora',
            'device_type': 'sims-smart',
            'device_id': parsed['device_id'],
            'binary_format_version': BINARY_FORMAT_VERSION,
            'payload_size': len(body),
            'raw_pcm_audio': parsed['is_raw_pcm'],
        }
    )

    # Delegate to existing create_incident (handles classification, forwarding, WebSocket)
    return await create_incident(incident_data, db)


# Transcription service singleton
_transcription_service = None


def _get_transcription_service() -> TranscriptionService:
    global _transcription_service
    if _transcription_service is None:
        _transcription_service = TranscriptionService()
    return _transcription_service


@lora_router.post("/transcribe")
async def transcribe_audio(request: Request):
    """
    Transcribe raw PCM audio from an ESP32 device.
    Accepts raw 16-bit mono PCM at 16kHz as application/octet-stream.
    Wraps in WAV header and sends to Whisper for transcription.
    Returns JSON: {"text": "transcribed text"}
    """
    body = await request.body()

    if not body or len(body) < 1600:  # Less than 0.05s of audio
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Audio data too short or empty"
        )

    logger.info(f"Received PCM audio for transcription: {len(body)} bytes "
                f"({len(body) / (16000 * 2):.1f}s at 16kHz)")

    # Wrap raw PCM in WAV header
    wav_buffer = io.BytesIO()
    with wave.open(wav_buffer, 'wb') as wav_file:
        wav_file.setnchannels(1)        # mono
        wav_file.setsampwidth(2)        # 16-bit
        wav_file.setframerate(16000)    # 16kHz
        wav_file.writeframes(body)

    # Write to temp file for transcription service
    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
            tmp.write(wav_buffer.getvalue())
            tmp_path = tmp.name

        logger.info(f"WAV file created: {tmp_path} ({os.path.getsize(tmp_path)} bytes)")

        service = _get_transcription_service()
        result = await service.transcribe_audio(tmp_path)

        if result and 'text' in result and result['text']:
            text = result['text'].strip()
            logger.info(f"Transcription result: \"{text}\"")
            return JSONResponse(content={"text": text})
        else:
            logger.warning("Transcription returned no text")
            return JSONResponse(
                content={"text": "", "error": "No speech detected"},
                status_code=200
            )

    except Exception as e:
        logger.error(f"Transcription failed: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Transcription failed: {str(e)}"
        )
    finally:
        if tmp_path and os.path.exists(tmp_path):
            os.unlink(tmp_path)


