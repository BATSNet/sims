"""
FeatherAI (Featherless) provider implementation.
"""
import logging
import httpx
from typing import List
from .base import BaseLLMProvider, Message, ChatResponse

logger = logging.getLogger(__name__)


class FeatherAIProvider(BaseLLMProvider):
    """FeatherAI (Featherless) API provider for chat completions."""

    def __init__(self, api_key: str, model: str, **kwargs):
        super().__init__(api_key, model, **kwargs)
        self.api_base = "https://api.featherless.ai/v1"

    async def chat_completion(
        self,
        messages: List[Message],
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request to FeatherAI."""
        url = f"{self.api_base}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        # Convert messages to OpenAI-compatible format (FeatherAI uses OpenAI API)
        feather_messages = []
        for msg in messages:
            feather_messages.append({
                "role": msg.role,
                "content": msg.content
            })

        payload = {
            "model": self.model,
            "messages": feather_messages,
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
        """Send a chat completion request with image to FeatherAI."""
        url = f"{self.api_base}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        # Convert messages with vision support
        feather_messages = []
        for msg in messages:
            if msg.role == "user" and isinstance(msg.content, str):
                # Add image to user message
                feather_messages.append({
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
                feather_messages.append({
                    "role": msg.role,
                    "content": msg.content
                })

        payload = {
            "model": self.model,
            "messages": feather_messages,
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
