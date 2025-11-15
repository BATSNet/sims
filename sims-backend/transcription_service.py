"""
Audio Transcription Service using DeepInfra API
"""
import os
import logging
import httpx
from typing import Optional
from pathlib import Path
from config import Config

logger = logging.getLogger(__name__)


class TranscriptionService:
    """Service for transcribing audio files using DeepInfra API"""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or os.getenv('DEEPINFRA_API_KEY')
        self.base_url = 'https://api.deepinfra.com/v1/inference'
        self.model = 'openai/whisper-large-v3'

    def _get_public_url(self, audio_file_path: str) -> Optional[str]:
        """
        Convert a local file path to a public URL if PUBLIC_SERVER_URL is configured.

        Args:
            audio_file_path: Local file path

        Returns:
            Public URL or None if can't be generated
        """
        if not Config.PUBLIC_SERVER_URL:
            return None

        try:
            file_path = Path(audio_file_path)
            if not file_path.exists():
                return None

            if 'uploads/audio' in str(file_path):
                filename = file_path.name
                public_url = f"{Config.PUBLIC_SERVER_URL.rstrip('/')}/static/uploads/audio/{filename}"
                logger.info(f'Generated public URL: {public_url}')
                return public_url

            return None
        except Exception as e:
            logger.error(f'Error generating public URL: {e}')
            return None

    async def transcribe_audio(self, audio_file_path: str) -> Optional[dict]:
        """
        Transcribe an audio file using DeepInfra's Whisper API.
        Tries URL-based transcription first (if PUBLIC_SERVER_URL is configured),
        falls back to file upload if URL method fails.

        Args:
            audio_file_path: Path to the audio file to transcribe

        Returns:
            Dictionary containing transcription result with 'text' field,
            or None if transcription failed
        """
        if not self.api_key:
            logger.error('DEEPINFRA_API_KEY not set')
            return None

        try:
            url = f'{self.base_url}/{self.model}'
            headers = {
                'Authorization': f'Bearer {self.api_key}',
            }

            public_url = self._get_public_url(audio_file_path)

            if public_url:
                logger.info(f'Attempting URL-based transcription: {public_url}')
                try:
                    async with httpx.AsyncClient(timeout=60.0) as client:
                        response = await client.post(
                            url,
                            headers=headers,
                            json={'audio_url': public_url}
                        )

                        if response.status_code == 200:
                            result = response.json()
                            logger.info(f'URL-based transcription successful: {result}')
                            return result
                        else:
                            logger.warning(
                                f'URL-based transcription failed: {response.status_code} - {response.text}. '
                                f'Falling back to file upload.'
                            )
                except Exception as e:
                    logger.warning(f'URL-based transcription error: {e}. Falling back to file upload.')

            logger.info(f'Using file upload method for: {audio_file_path}')
            async with httpx.AsyncClient(timeout=60.0) as client:
                with open(audio_file_path, 'rb') as audio_file:
                    files = {
                        'audio': audio_file,
                    }

                    logger.info(f'Transcribing audio file via upload: {audio_file_path}')
                    response = await client.post(
                        url,
                        headers=headers,
                        files=files,
                    )

                    if response.status_code == 200:
                        result = response.json()
                        logger.info(f'File upload transcription successful: {result}')
                        return result
                    else:
                        logger.error(
                            f'File upload transcription failed: {response.status_code} - {response.text}'
                        )
                        return None

        except Exception as e:
            logger.error(f'Error transcribing audio: {e}', exc_info=True)
            return None

    async def transcribe_and_get_text(self, audio_file_path: str) -> Optional[str]:
        """
        Transcribe an audio file and return just the text

        Args:
            audio_file_path: Path to the audio file to transcribe

        Returns:
            Transcribed text, or None if transcription failed
        """
        result = await self.transcribe_audio(audio_file_path)
        if result and 'text' in result:
            return result['text']
        return None
