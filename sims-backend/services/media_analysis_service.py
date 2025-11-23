"""
Media Analysis Service - Analyze images and videos using configurable vision models
"""
import logging
from typing import Optional
from config import Config
from services.ai_providers.factory import ProviderFactory
from services.ai_providers.base import Message
from i18n import i18n

logger = logging.getLogger(__name__)


class MediaAnalysisService:
    """Service for analyzing images and videos to extract descriptions"""

    def __init__(self):
        """Initialize media analysis service with configured provider."""
        self.provider = ProviderFactory.create_llm_provider(
            provider_name=Config.VISION_PROVIDER,
            model=Config.VISION_MODEL,
            temperature=Config.VISION_TEMPERATURE,
            max_tokens=Config.VISION_MAX_TOKENS,
            timeout=Config.VISION_TIMEOUT
        )

        if not self.provider:
            logger.warning(
                f"Failed to initialize vision provider: {Config.VISION_PROVIDER}"
            )

    async def analyze_image(self, image_url: str) -> Optional[str]:
        """
        Analyze an image and return a detailed description.

        Args:
            image_url: URL to the image file

        Returns:
            Description of the image content, or None if analysis failed
        """
        if not self.provider:
            logger.error("Cannot analyze image: provider not initialized")
            return None

        try:
            # Create messages with the improved prompt from config
            # Format the media analysis prompt with language
            formatted_prompt = Config.MEDIA_ANALYSIS_PROMPT.format(
                language=i18n.get_language_name()
            )

            messages = [
                Message(
                    role="system",
                    content="You are an expert image analyst for emergency response and incident reporting. Provide clear, factual, and detailed descriptions focused on incident-relevant information."
                ),
                Message(
                    role="user",
                    content=formatted_prompt
                )
            ]

            # Call vision API
            result = await self.provider.chat_completion_with_vision(
                messages=messages,
                image_url=image_url
            )

            description = result.content
            logger.info(f"Successfully analyzed image: {description[:100]}...")
            return description

        except Exception as e:
            logger.error(f"Error analyzing image: {e}", exc_info=True)
            return None

    async def analyze_video(self, video_url: str) -> Optional[str]:
        """
        Analyze a video and return a description.
        Currently returns a placeholder - full video analysis would require
        frame extraction or video understanding models.

        Args:
            video_url: URL to the video file

        Returns:
            Description of the video content, or None if analysis failed
        """
        # TODO: Implement video analysis
        # Options:
        # 1. Extract key frames and analyze them as images
        # 2. Extract audio and transcribe it
        # 3. Use a video understanding model if available

        logger.info(f"Video analysis not yet implemented for: {video_url}")
        return "Video analysis pending - manual review required"


# Singleton instance
_media_analysis_service: Optional[MediaAnalysisService] = None


def get_media_analyzer() -> MediaAnalysisService:
    """Get or create media analysis service instance"""
    global _media_analysis_service
    if _media_analysis_service is None:
        _media_analysis_service = MediaAnalysisService()
    return _media_analysis_service
