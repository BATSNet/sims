import logging
import os

import httpx
from typing import List, Optional
from .base import BaseLLMProvider, BaseTranscriptionProvider, Message, ChatResponse, TranscriptionResponse

logger = logging.getLogger(__name__)


class OllamaProvider(BaseLLMProvider):

    def __init__(self, api_key: str, model: str, **kwargs):
        super().__init__(api_key, model, **kwargs)
        assert 'api_base' in kwargs, 'OllamaProvider requires \'api_base\' parameter.'
        self.api_base = kwargs['api_base']
        self.api_key = api_key
        self.model = model

    async def health(self):
        """ Send a request to list models and use it for the health check. """
        url = f"{self.api_base}/api/tags"
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.get(url)
            return response

    async def chat_completion(
        self,
        messages: List[Message],
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request to local or remote Ollama service."""
        url = f"{self.api_base}/api/chat"
        headers = {
            "Content-Type": "application/json"
        }
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"

        # Convert messages to Mistral format
        ollama_messages = []
        for msg in messages:
            ollama_messages.append({
                "role": msg.role,
                "content": msg.content
            })

        payload = {
            "model": self.model,
            "messages": ollama_messages,
            "stream": kwargs.get("stream", False)
        }

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(url, json=payload, headers=headers)
            response.raise_for_status()

            data = response.json()
            content = data["message"]["content"]

            return ChatResponse(
                content=content,
                model=data["model"],
            )

    async def chat_completion_with_vision(self, *args, **kwargs):
        pass