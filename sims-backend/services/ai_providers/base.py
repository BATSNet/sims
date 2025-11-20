"""
Base classes for AI providers.
"""
from abc import ABC, abstractmethod
from typing import Optional, List, Dict, Any
from dataclasses import dataclass


@dataclass
class Message:
    """Represents a message in a conversation."""
    role: str  # system, user, assistant
    content: str | List[Dict[str, Any]]  # Text or multimodal content


@dataclass
class ChatResponse:
    """Response from a chat completion."""
    content: str
    model: str
    usage: Optional[Dict[str, int]] = None


@dataclass
class TranscriptionResponse:
    """Response from audio transcription."""
    text: str
    model: str


class BaseLLMProvider(ABC):
    """Base class for LLM providers (OpenAI, Anthropic, Mistral, etc.)"""

    def __init__(self, api_key: str, model: str, **kwargs):
        self.api_key = api_key
        self.model = model
        self.temperature = kwargs.get('temperature', 0.7)
        self.max_tokens = kwargs.get('max_tokens', 1000)
        self.timeout = kwargs.get('timeout', 60)

    @abstractmethod
    async def chat_completion(
        self,
        messages: List[Message],
        **kwargs
    ) -> ChatResponse:
        """
        Send a chat completion request.

        Args:
            messages: List of messages in the conversation
            **kwargs: Additional provider-specific parameters

        Returns:
            ChatResponse with the completion
        """
        pass

    @abstractmethod
    async def chat_completion_with_vision(
        self,
        messages: List[Message],
        image_url: str,
        **kwargs
    ) -> ChatResponse:
        """
        Send a chat completion request with image analysis.

        Args:
            messages: List of messages in the conversation
            image_url: URL to the image
            **kwargs: Additional provider-specific parameters

        Returns:
            ChatResponse with the completion
        """
        pass


class BaseTranscriptionProvider(ABC):
    """Base class for audio transcription providers."""

    def __init__(self, api_key: str, model: str, **kwargs):
        self.api_key = api_key
        self.model = model
        self.timeout = kwargs.get('timeout', 60)

    @abstractmethod
    async def transcribe_audio(
        self,
        audio_file_path: str,
        **kwargs
    ) -> TranscriptionResponse:
        """
        Transcribe an audio file.

        Args:
            audio_file_path: Path to the audio file
            **kwargs: Additional provider-specific parameters

        Returns:
            TranscriptionResponse with the transcribed text
        """
        pass

    @abstractmethod
    async def transcribe_audio_url(
        self,
        audio_url: str,
        **kwargs
    ) -> TranscriptionResponse:
        """
        Transcribe an audio file from URL.

        Args:
            audio_url: URL to the audio file
            **kwargs: Additional provider-specific parameters

        Returns:
            TranscriptionResponse with the transcribed text
        """
        pass
