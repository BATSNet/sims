"""
OpenAI provider implementation.
"""
import logging
import httpx
from typing import List, Optional
from .base import BaseLLMProvider, BaseTranscriptionProvider, Message, ChatResponse, TranscriptionResponse

logger = logging.getLogger(__name__)


class OpenAIProvider(BaseLLMProvider):
    """OpenAI API provider for chat completions."""

    def __init__(self, api_key: str, model: str, **kwargs):
        super().__init__(api_key, model, **kwargs)
        self.api_base = "https://api.openai.com/v1"

    async def chat_completion(
        self,
        messages: List[Message],
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request to OpenAI."""
        url = f"{self.api_base}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        # Convert messages to OpenAI format
        openai_messages = []
        for msg in messages:
            openai_messages.append({
                "role": msg.role,
                "content": msg.content
            })

        payload = {
            "model": self.model,
            "messages": openai_messages,
            "temperature": kwargs.get('temperature', self.temperature),
            "max_tokens": kwargs.get('max_tokens', self.max_tokens)
        }

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(url, json=payload, headers=headers)
            response.raise_for_status()

            data = response.json()
            content = data["choices"][0]["message"]["content"]
            usage = data.get("usage")

            return ChatResponse(
                content=content,
                model=data["model"],
                usage=usage
            )

    async def chat_completion_with_vision(
        self,
        messages: List[Message],
        image_url: str,
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request with image to OpenAI."""
        url = f"{self.api_base}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        # Convert messages to OpenAI format with vision
        openai_messages = []
        for msg in messages:
            if msg.role == "user" and isinstance(msg.content, str):
                # Add image to user message
                openai_messages.append({
                    "role": msg.role,
                    "content": [
                        {
                            "type": "text",
                            "text": msg.content
                        },
                        {
                            "type": "image_url",
                            "image_url": {
                                "url": image_url
                            }
                        }
                    ]
                })
            else:
                openai_messages.append({
                    "role": msg.role,
                    "content": msg.content
                })

        payload = {
            "model": self.model,
            "messages": openai_messages,
            "temperature": kwargs.get('temperature', self.temperature),
            "max_tokens": kwargs.get('max_tokens', self.max_tokens)
        }

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(url, json=payload, headers=headers)
            response.raise_for_status()

            data = response.json()
            content = data["choices"][0]["message"]["content"]
            usage = data.get("usage")

            return ChatResponse(
                content=content,
                model=data["model"],
                usage=usage
            )


class OpenAITranscriptionProvider(BaseTranscriptionProvider):
    """OpenAI Whisper API provider for audio transcription."""

    def __init__(self, api_key: str, model: str = "whisper-1", **kwargs):
        super().__init__(api_key, model, **kwargs)
        self.api_base = "https://api.openai.com/v1"

    async def transcribe_audio(
        self,
        audio_file_path: str,
        **kwargs
    ) -> TranscriptionResponse:
        """Transcribe an audio file using OpenAI Whisper."""
        url = f"{self.api_base}/audio/transcriptions"
        headers = {
            "Authorization": f"Bearer {self.api_key}"
        }

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            with open(audio_file_path, 'rb') as audio_file:
                files = {
                    'file': audio_file,
                    'model': (None, self.model)
                }

                response = await client.post(url, headers=headers, files=files)
                response.raise_for_status()

                data = response.json()
                return TranscriptionResponse(
                    text=data["text"],
                    model=self.model
                )

    async def transcribe_audio_url(
        self,
        audio_url: str,
        **kwargs
    ) -> TranscriptionResponse:
        """
        OpenAI doesn't support URL-based transcription directly.
        Download the file first, then transcribe.
        """
        # Download audio file to temp location
        import tempfile
        import os

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.get(audio_url)
            response.raise_for_status()

            # Save to temp file
            with tempfile.NamedTemporaryFile(delete=False, suffix='.wav') as tmp_file:
                tmp_file.write(response.content)
                tmp_path = tmp_file.name

            try:
                # Transcribe the temp file
                result = await self.transcribe_audio(tmp_path, **kwargs)
                return result
            finally:
                # Clean up temp file
                if os.path.exists(tmp_path):
                    os.remove(tmp_path)
