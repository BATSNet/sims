"""
Audio Transcription Service using configurable AI providers
"""
import os
import logging
from typing import Optional
from pathlib import Path
from config import Config
from services.ai_providers.factory import ProviderFactory

logger = logging.getLogger(__name__)


class TranscriptionService:
    """Service for transcribing audio files using configured AI provider"""

    def __init__(self):
        """Initialize transcription service with configured provider."""
        self.provider = ProviderFactory.create_transcription_provider(
            provider_name=Config.TRANSCRIPTION_PROVIDER,
            model=Config.TRANSCRIPTION_MODEL,
            timeout=Config.TRANSCRIPTION_TIMEOUT
        )

        if not self.provider:
            logger.error(
                f"Failed to initialize transcription provider: {Config.TRANSCRIPTION_PROVIDER}"
            )

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
        Transcribe an audio file using the configured AI provider.
        Tries URL-based transcription first (if PUBLIC_SERVER_URL is configured and provider supports it),
        falls back to file upload if URL method fails.

        Args:
            audio_file_path: Path to the audio file to transcribe

        Returns:
            Dictionary containing transcription result with 'text' field,
            or None if transcription failed
        """
        if not self.provider:
            logger.error('Transcription provider not initialized')
            return None

        try:
            # Try URL-based transcription if available
            public_url = self._get_public_url(audio_file_path)

            if public_url and hasattr(self.provider, 'transcribe_audio_url'):
                logger.info(f'Attempting URL-based transcription: {public_url}')
                try:
                    result = await self.provider.transcribe_audio_url(public_url)
                    logger.info(f'URL-based transcription successful: {result.text[:100]}...')
                    return {'text': result.text, 'model': result.model}
                except Exception as e:
                    logger.warning(
                        f'URL-based transcription failed: {e}. Falling back to file upload.'
                    )

            # Fall back to file upload
            logger.info(f'Using file upload method for: {audio_file_path}')
            result = await self.provider.transcribe_audio(audio_file_path)
            logger.info(f'File upload transcription successful: {result.text[:100]}...')
            return {'text': result.text, 'model': result.model}

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
