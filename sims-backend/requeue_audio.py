"""
Script to re-queue existing audio files for transcription.
Run this after starting the backend to process audio files that were uploaded before the service started.
"""
import asyncio
import logging
from db.connection import get_db
from models.media_model import MediaORM, MediaType
from services.schedule_summarization import get_summarization_service

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


async def requeue_audio_files():
    """Find all audio files without transcription and queue them for processing."""
    db = next(get_db())

    try:
        # Find all audio files with NULL transcription
        audio_files = db.query(MediaORM).filter(
            MediaORM.media_type == MediaType.AUDIO,
            MediaORM.transcription.is_(None)
        ).all()

        logger.info(f"Found {len(audio_files)} audio files without transcription")

        if not audio_files:
            logger.info("No audio files to re-queue")
            return

        # Get the summarization service
        service = get_summarization_service()

        # Queue each audio file
        queued_count = 0
        for media in audio_files:
            try:
                await service.queue_audio_transcription(
                    media_id=str(media.id),
                    file_path=media.file_path,
                    db_session=db
                )
                logger.info(f"Queued audio file {media.id} ({media.file_url})")
                queued_count += 1
            except Exception as e:
                logger.error(f"Failed to queue {media.id}: {e}")

        logger.info(f"Successfully queued {queued_count}/{len(audio_files)} audio files")

    finally:
        db.close()


if __name__ == "__main__":
    asyncio.run(requeue_audio_files())
