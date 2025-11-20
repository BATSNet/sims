"""
Anthropic Claude provider implementation.
"""
import logging
import httpx
from typing import List
from .base import BaseLLMProvider, Message, ChatResponse

logger = logging.getLogger(__name__)


class AnthropicProvider(BaseLLMProvider):
    """Anthropic Claude API provider for chat completions."""

    def __init__(self, api_key: str, model: str, **kwargs):
        super().__init__(api_key, model, **kwargs)
        self.api_base = "https://api.anthropic.com/v1"
        self.api_version = "2023-06-01"

    async def chat_completion(
        self,
        messages: List[Message],
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request to Anthropic Claude."""
        url = f"{self.api_base}/messages"
        headers = {
            "x-api-key": self.api_key,
            "anthropic-version": self.api_version,
            "Content-Type": "application/json"
        }

        # Separate system message from other messages
        system_message = None
        claude_messages = []

        for msg in messages:
            if msg.role == "system":
                system_message = msg.content if isinstance(msg.content, str) else str(msg.content)
            else:
                claude_messages.append({
                    "role": msg.role,
                    "content": msg.content
                })

        payload = {
            "model": self.model,
            "messages": claude_messages,
            "temperature": kwargs.get('temperature', self.temperature),
            "max_tokens": kwargs.get('max_tokens', self.max_tokens)
        }

        if system_message:
            payload["system"] = system_message

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(url, json=payload, headers=headers)
            response.raise_for_status()

            data = response.json()
            content = data["content"][0]["text"]
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
        """Send a chat completion request with image to Anthropic Claude."""
        url = f"{self.api_base}/messages"
        headers = {
            "x-api-key": self.api_key,
            "anthropic-version": self.api_version,
            "Content-Type": "application/json"
        }

        # Download image and convert to base64 for Claude
        import base64
        async with httpx.AsyncClient(timeout=30) as client:
            img_response = await client.get(image_url)
            img_response.raise_for_status()
            image_data = base64.b64encode(img_response.content).decode('utf-8')

            # Determine media type from URL or default to jpeg
            media_type = "image/jpeg"
            if image_url.lower().endswith('.png'):
                media_type = "image/png"
            elif image_url.lower().endswith('.webp'):
                media_type = "image/webp"
            elif image_url.lower().endswith('.gif'):
                media_type = "image/gif"

        # Separate system message from other messages
        system_message = None
        claude_messages = []

        for msg in messages:
            if msg.role == "system":
                system_message = msg.content if isinstance(msg.content, str) else str(msg.content)
            elif msg.role == "user" and isinstance(msg.content, str):
                # Add image to user message
                claude_messages.append({
                    "role": msg.role,
                    "content": [
                        {
                            "type": "image",
                            "source": {
                                "type": "base64",
                                "media_type": media_type,
                                "data": image_data
                            }
                        },
                        {
                            "type": "text",
                            "text": msg.content
                        }
                    ]
                })
            else:
                claude_messages.append({
                    "role": msg.role,
                    "content": msg.content
                })

        payload = {
            "model": self.model,
            "messages": claude_messages,
            "temperature": kwargs.get('temperature', self.temperature),
            "max_tokens": kwargs.get('max_tokens', self.max_tokens)
        }

        if system_message:
            payload["system"] = system_message

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(url, json=payload, headers=headers)
            response.raise_for_status()

            data = response.json()
            content = data["content"][0]["text"]
            usage = data.get("usage")

            return ChatResponse(
                content=content,
                model=data["model"],
                usage=usage
            )
