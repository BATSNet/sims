"""
Audio Transcription Service using DeepInfra API
"""
import os
import logging
import httpx
from typing import Optional

logger = logging.getLogger(__name__)


class TranscriptionService:
    """Service for transcribing audio files using DeepInfra API"""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or os.getenv('DEEPINFRA_API_KEY')
        self.base_url = 'https://api.deepinfra.com/v1/inference'
        self.model = 'openai/whisper-large-v3'

    async def transcribe_audio(self, audio_file_path: str) -> Optional[dict]:
        """
        Transcribe an audio file using DeepInfra's Whisper API

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

            async with httpx.AsyncClient(timeout=60.0) as client:
                with open(audio_file_path, 'rb') as audio_file:
                    files = {
                        'audio': audio_file,
                    }

                    logger.info(f'Transcribing audio file: {audio_file_path}')
                    response = await client.post(
                        url,
                        headers=headers,
                        files=files,
                    )

                    if response.status_code == 200:
                        result = response.json()
                        logger.info(f'Transcription successful: {result}')
                        return result
                    else:
                        logger.error(
                            f'Transcription failed: {response.status_code} - {response.text}'
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
