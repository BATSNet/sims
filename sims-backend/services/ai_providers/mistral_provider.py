"""
Mistral AI provider implementation.
"""
import logging
import httpx
from typing import List
from .base import BaseLLMProvider, Message, ChatResponse

logger = logging.getLogger(__name__)


class MistralProvider(BaseLLMProvider):
    """Mistral AI API provider for chat completions."""

    def __init__(self, api_key: str, model: str, **kwargs):
        super().__init__(api_key, model, **kwargs)
        self.api_base = "https://api.mistral.ai/v1"

    async def chat_completion(
        self,
        messages: List[Message],
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request to Mistral AI."""
        url = f"{self.api_base}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        # Convert messages to Mistral format
        mistral_messages = []
        for msg in messages:
            mistral_messages.append({
                "role": msg.role,
                "content": msg.content
            })

        payload = {
            "model": self.model,
            "messages": mistral_messages,
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
        """
        Send a chat completion request with image to Mistral AI.
        Note: Mistral vision support depends on model (e.g., pixtral-12b-2409)
        """
        url = f"{self.api_base}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        # Convert messages to Mistral format with vision
        mistral_messages = []
        for msg in messages:
            if msg.role == "user" and isinstance(msg.content, str):
                # Add image to user message
                mistral_messages.append({
                    "role": msg.role,
                    "content": [
                        {
                            "type": "text",
                            "text": msg.content
                        },
                        {
                            "type": "image_url",
                            "image_url": image_url
                        }
                    ]
                })
            else:
                mistral_messages.append({
                    "role": msg.role,
                    "content": msg.content
                })

        payload = {
            "model": self.model,
            "messages": mistral_messages,
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
