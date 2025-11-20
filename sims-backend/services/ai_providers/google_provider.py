"""
Google Gemini provider implementation.
"""
import logging
import httpx
import base64
from typing import List
from .base import BaseLLMProvider, Message, ChatResponse

logger = logging.getLogger(__name__)


class GoogleGeminiProvider(BaseLLMProvider):
    """Google Gemini API provider for chat completions."""

    def __init__(self, api_key: str, model: str, **kwargs):
        super().__init__(api_key, model, **kwargs)
        self.api_base = "https://generativelanguage.googleapis.com/v1beta"

    async def chat_completion(
        self,
        messages: List[Message],
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request to Google Gemini."""
        url = f"{self.api_base}/models/{self.model}:generateContent?key={self.api_key}"
        headers = {
            "Content-Type": "application/json"
        }

        # Convert messages to Gemini format
        gemini_contents = []
        system_instruction = None

        for msg in messages:
            if msg.role == "system":
                # Gemini uses systemInstruction separately
                system_instruction = msg.content if isinstance(msg.content, str) else str(msg.content)
            else:
                # Map role (user/assistant -> user/model)
                role = "model" if msg.role == "assistant" else "user"
                gemini_contents.append({
                    "role": role,
                    "parts": [{"text": msg.content}]
                })

        payload = {
            "contents": gemini_contents,
            "generationConfig": {
                "temperature": kwargs.get('temperature', self.temperature),
                "maxOutputTokens": kwargs.get('max_tokens', self.max_tokens)
            }
        }

        if system_instruction:
            payload["systemInstruction"] = {
                "parts": [{"text": system_instruction}]
            }

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(url, json=payload, headers=headers)
            response.raise_for_status()

            data = response.json()

            # Extract content from Gemini response
            if "candidates" in data and len(data["candidates"]) > 0:
                content = data["candidates"][0]["content"]["parts"][0]["text"]

                # Extract usage if available
                usage = None
                if "usageMetadata" in data:
                    usage = {
                        "prompt_tokens": data["usageMetadata"].get("promptTokenCount", 0),
                        "completion_tokens": data["usageMetadata"].get("candidatesTokenCount", 0),
                        "total_tokens": data["usageMetadata"].get("totalTokenCount", 0)
                    }

                return ChatResponse(
                    content=content,
                    model=self.model,
                    usage=usage
                )
            else:
                raise ValueError("No candidates in Gemini response")

    async def chat_completion_with_vision(
        self,
        messages: List[Message],
        image_url: str,
        **kwargs
    ) -> ChatResponse:
        """Send a chat completion request with image to Google Gemini."""
        url = f"{self.api_base}/models/{self.model}:generateContent?key={self.api_key}"
        headers = {
            "Content-Type": "application/json"
        }

        # Download and encode image
        async with httpx.AsyncClient(timeout=30) as client:
            img_response = await client.get(image_url)
            img_response.raise_for_status()
            image_data = base64.b64encode(img_response.content).decode('utf-8')

            # Determine MIME type
            mime_type = "image/jpeg"
            if image_url.lower().endswith('.png'):
                mime_type = "image/png"
            elif image_url.lower().endswith('.webp'):
                mime_type = "image/webp"
            elif image_url.lower().endswith('.gif'):
                mime_type = "image/gif"

        # Convert messages to Gemini format with vision
        gemini_contents = []
        system_instruction = None

        for msg in messages:
            if msg.role == "system":
                system_instruction = msg.content if isinstance(msg.content, str) else str(msg.content)
            elif msg.role == "user" and isinstance(msg.content, str):
                # Add image and text to user message
                role = "user"
                gemini_contents.append({
                    "role": role,
                    "parts": [
                        {
                            "inline_data": {
                                "mime_type": mime_type,
                                "data": image_data
                            }
                        },
                        {
                            "text": msg.content
                        }
                    ]
                })
            else:
                role = "model" if msg.role == "assistant" else "user"
                gemini_contents.append({
                    "role": role,
                    "parts": [{"text": msg.content if isinstance(msg.content, str) else str(msg.content)}]
                })

        payload = {
            "contents": gemini_contents,
            "generationConfig": {
                "temperature": kwargs.get('temperature', self.temperature),
                "maxOutputTokens": kwargs.get('max_tokens', self.max_tokens)
            }
        }

        if system_instruction:
            payload["systemInstruction"] = {
                "parts": [{"text": system_instruction}]
            }

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(url, json=payload, headers=headers)
            response.raise_for_status()

            data = response.json()

            # Extract content from Gemini response
            if "candidates" in data and len(data["candidates"]) > 0:
                content = data["candidates"][0]["content"]["parts"][0]["text"]

                # Extract usage if available
                usage = None
                if "usageMetadata" in data:
                    usage = {
                        "prompt_tokens": data["usageMetadata"].get("promptTokenCount", 0),
                        "completion_tokens": data["usageMetadata"].get("candidatesTokenCount", 0),
                        "total_tokens": data["usageMetadata"].get("totalTokenCount", 0)
                    }

                return ChatResponse(
                    content=content,
                    model=self.model,
                    usage=usage
                )
            else:
                raise ValueError("No candidates in Gemini response")
