"""
Media Analysis Service - Analyze images and videos using vision models
"""
import logging
import httpx
from typing import Optional
from config import Config

logger = logging.getLogger(__name__)


class MediaAnalysisService:
    """Service for analyzing images and videos to extract descriptions"""

    def __init__(self):
        self.api_key = Config.FEATHERLESS_API_KEY
        self.api_base = Config.FEATHERLESS_API_BASE
        self.model = Config.DEFAULT_LLM_MODEL
        self.temperature = 0.7
        self.max_tokens = 500

        if not self.api_key:
            logger.warning("FEATHERLESS_API_KEY not configured. Media analysis will fail.")

    async def analyze_image(self, image_url: str) -> Optional[str]:
        """
        Analyze an image and return a detailed description.

        Args:
            image_url: URL to the image file

        Returns:
            Description of the image content, or None if analysis failed
        """
        if not self.api_key:
            logger.error("Cannot analyze image: FEATHERLESS_API_KEY not configured")
            return None

        try:
            url = f"{self.api_base}/chat/completions"
            headers = {
                "Authorization": f"Bearer {self.api_key}",
                "Content-Type": "application/json"
            }

            prompt = """Analyze this incident image and provide a detailed description including:
1. What objects, people, or structures are visible
2. Any signs of damage, danger, or unusual conditions
3. Environmental context (weather, time of day, location type)
4. Any text or signage visible
5. Overall assessment of what the image shows

Be concise but thorough. Focus on factual observations relevant to emergency response."""

            # Build multimodal content with image
            user_content = [
                {
                    "type": "text",
                    "text": prompt
                },
                {
                    "type": "image_url",
                    "image_url": {
                        "url": image_url
                    }
                }
            ]

            payload = {
                "model": self.model,
                "messages": [
                    {
                        "role": "system",
                        "content": "You are an expert image analyst for emergency incident reporting. Provide clear, factual descriptions."
                    },
                    {
                        "role": "user",
                        "content": user_content
                    }
                ],
                "temperature": self.temperature,
                "max_tokens": self.max_tokens
            }

            async with httpx.AsyncClient(timeout=60.0) as client:
                response = await client.post(url, headers=headers, json=payload)

                if response.status_code == 200:
                    result = response.json()
                    description = result.get("choices", [{}])[0].get("message", {}).get("content", "")
                    logger.info(f"Successfully analyzed image: {description[:100]}...")
                    return description
                else:
                    logger.error(f"Image analysis failed: {response.status_code} - {response.text}")
                    return None

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
