"""
DeepInfra provider implementation.
"""
import logging
import httpx
from typing import List
from .base import BaseLLMProvider, BaseTranscriptionProvider, Message, ChatResponse, TranscriptionResponse

logger = logging.getLogger(__name__)


class DeepInfraProvider(BaseLLMProvider):
    """DeepInfra API provider for chat completions."""

    def __init__(self, api_key: str, model: str, **kwargs):
        super().__init__(api_key, model, **kwargs)
        self.api_base = "https://api.deepinfra.com/v1/openai"

    async def chat_completion(
        self,
        messages: List[Message],
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request to DeepInfra."""
        url = f"{self.api_base}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        # Convert messages to OpenAI-compatible format (DeepInfra uses OpenAI API)
        deepinfra_messages = []
        for msg in messages:
            deepinfra_messages.append({
                "role": msg.role,
                "content": msg.content
            })

        payload = {
            "model": self.model,
            "messages": deepinfra_messages,
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
                model=data.get("model", self.model),
                usage=usage
            )

    async def chat_completion_with_vision(
        self,
        messages: List[Message],
        image_url: str,
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request with image to DeepInfra."""
        url = f"{self.api_base}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        # Convert messages with vision support
        deepinfra_messages = []
        for msg in messages:
            if msg.role == "user" and isinstance(msg.content, str):
                # Add image to user message
                deepinfra_messages.append({
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
                deepinfra_messages.append({
                    "role": msg.role,
                    "content": msg.content
                })

        payload = {
            "model": self.model,
            "messages": deepinfra_messages,
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
                model=data.get("model", self.model),
                usage=usage
            )


class DeepInfraTranscriptionProvider(BaseTranscriptionProvider):
    """DeepInfra Whisper API provider for audio transcription."""

    def __init__(self, api_key: str, model: str = "openai/whisper-large-v3", **kwargs):
        super().__init__(api_key, model, **kwargs)
        self.api_base = "https://api.deepinfra.com/v1/inference"

    async def transcribe_audio(
        self,
        audio_file_path: str,
        **kwargs
    ) -> TranscriptionResponse:
        """Transcribe an audio file using DeepInfra Whisper."""
        url = f"{self.api_base}/{self.model}"
        headers = {
            "Authorization": f"Bearer {self.api_key}"
        }

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            with open(audio_file_path, 'rb') as audio_file:
                files = {
                    'audio': audio_file
                }
                data = {}
                if 'language' in kwargs:
                    data['language'] = kwargs['language']

                response = await client.post(url, headers=headers, files=files, data=data)
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
        """Transcribe an audio file from URL using DeepInfra."""
        url = f"{self.api_base}/{self.model}"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        payload = {
            "audio_url": audio_url
        }
        if 'language' in kwargs:
            payload['language'] = kwargs['language']

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(url, headers=headers, json=payload)
            response.raise_for_status()

            data = response.json()
            return TranscriptionResponse(
                text=data["text"],
                model=self.model
            )
