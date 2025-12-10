"""
Integration tests for OllamaProvider.

These tests require a running Ollama container at http://localhost:11434
with at least one model available (default: llama3.2:1b).

To run: pytest tests/services/ai_providers/test_ollama_provider.py -v
"""

import os
import pytest
import httpx

from services.ai_providers.ollama_provider import OllamaProvider
from services.ai_providers.base import Message, ChatResponse


# Configuration - can be overridden via environment variables
OLLAMA_BASE_URL = os.getenv("OLLAMA_BASE_URL", "http://localhost:11434")
OLLAMA_MODEL = os.getenv("OLLAMA_MODEL", "gpt-oss:latest")


@pytest.fixture
def ollama_provider():
    """Create an OllamaProvider instance for testing."""
    return OllamaProvider(
        api_key=None,
        model=OLLAMA_MODEL,
        api_base=OLLAMA_BASE_URL,
        timeout=120,
    )


@pytest.fixture
def ollama_provider_with_api_key():
    """Create an OllamaProvider instance with an API key."""
    return OllamaProvider(
        api_key="test-api-key",
        model=OLLAMA_MODEL,
        api_base=OLLAMA_BASE_URL,
        timeout=120,
    )


async def is_ollama_available() -> bool:
    """Check if Ollama service is available."""
    try:
        async with httpx.AsyncClient(timeout=5) as client:
            response = await client.get(f"{OLLAMA_BASE_URL}/api/tags")
            return response.status_code == 200
    except Exception:
        return False


@pytest.fixture(autouse=True)
async def skip_if_ollama_unavailable():
    """Skip tests if Ollama is not available."""
    if not await is_ollama_available():
        pytest.skip(f"Ollama not available at {OLLAMA_BASE_URL}")


class TestOllamaProviderInit:
    """Tests for OllamaProvider initialization."""

    def test_init_with_required_params(self):
        """Test that provider initializes with required parameters."""
        provider = OllamaProvider(
            api_key=None,
            model="test-model",
            api_base="http://localhost:11434"
        )
        assert provider.model == "test-model"
        assert provider.api_base == "http://localhost:11434"
        assert provider.api_key is None

    @pytest.mark.skip
    def test_init_with_api_key(self):
        """Test that provider initializes with an API key."""
        provider = OllamaProvider(
            api_key="secret-key",
            model="test-model",
            api_base="http://localhost:11434"
        )
        assert provider.api_key == "secret-key"

    @pytest.mark.skip
    def test_init_without_api_base_raises_error(self):
        """Test that missing api_base raises AssertionError."""
        with pytest.raises(AssertionError, match="api_base"):
            OllamaProvider(api_key=None, model="test-model")

    def test_init_with_custom_timeout(self):
        """Test that custom timeout is set correctly."""
        provider = OllamaProvider(
            api_key=None,
            model="test-model",
            api_base="http://localhost:11434",
            timeout=300
        )
        assert provider.timeout == 300


class TestOllamaProviderChatCompletion:
    """Integration tests for chat completion."""

    @pytest.mark.asyncio
    async def test_health(self, ollama_provider):
        """ Test if Ollama is reachable"""
        response = await ollama_provider.health()
        assert response.status_code == 200, response.json()

    @pytest.mark.asyncio
    async def test_simple_chat_completion(self, ollama_provider):
        """Test basic chat completion with a simple prompt."""
        messages = [
            Message(role="user", content="Say 'test' and nothing else.")
        ]

        response = await ollama_provider.chat_completion(messages, stream=False)

        assert isinstance(response, ChatResponse)
        assert response.content is not None
        assert len(response.content) > 0
        assert response.model is not None

    @pytest.mark.asyncio
    async def test_chat_completion_with_system_message(self, ollama_provider):
        """Test chat completion with a system message."""
        messages = [
            Message(
                role="system",
                content="You are a helpful assistant. Always respond in exactly one word."
            ),
            Message(role="user", content="What color is the sky on a clear day?")
        ]

        response = await ollama_provider.chat_completion(messages, stream=False)

        assert isinstance(response, ChatResponse)
        assert response.content is not None
        assert len(response.content) > 0

    @pytest.mark.asyncio
    async def test_chat_completion_with_conversation(self, ollama_provider):
        """Test chat completion with multi-turn conversation."""
        messages = [
            Message(role="user", content="My name is Alice."),
            Message(role="assistant", content="Nice to meet you, Alice!"),
            Message(role="user", content="What is my name?")
        ]

        response = await ollama_provider.chat_completion(messages, stream=False)

        assert isinstance(response, ChatResponse)
        assert response.content is not None
        # The response should reference "Alice" since we told it our name
        assert "alice" in response.content.lower()

    @pytest.mark.asyncio
    async def test_chat_completion_response_model_field(self, ollama_provider):
        """Test that response includes the model name."""
        messages = [
            Message(role="user", content="Hi")
        ]

        response = await ollama_provider.chat_completion(messages)

        assert response.model is not None
        assert len(response.model) > 0

    @pytest.mark.skip
    @pytest.mark.asyncio
    async def test_chat_completion_with_api_key(self, ollama_provider_with_api_key):
        """Test chat completion with API key set (should still work)."""
        messages = [
            Message(role="user", content="Say 'test' and nothing else.")
        ]

        response = await ollama_provider_with_api_key.chat_completion(messages)

        assert isinstance(response, ChatResponse)
        assert response.content is not None


class TestOllamaProviderErrorHandling:
    """Tests for error handling."""

    @pytest.mark.asyncio
    async def test_invalid_model_raises_error(self):
        """Test that using an invalid model raises an HTTP error."""
        provider = OllamaProvider(
            api_key=None,
            model="nonexistent-model-xyz-123",
            api_base=OLLAMA_BASE_URL,
            timeout=30
        )
        messages = [
            Message(role="user", content="Hello")
        ]

        with pytest.raises(httpx.HTTPStatusError):
            await provider.chat_completion(messages)

    @pytest.mark.asyncio
    async def test_invalid_base_url_raises_error(self):
        """Test that invalid base URL raises a connection error."""
        provider = OllamaProvider(
            api_key=None,
            model=OLLAMA_MODEL,
            api_base="http://localhost:63666",
            timeout=5
        )
        messages = [
            Message(role="user", content="Hello")
        ]

        with pytest.raises((httpx.ConnectError, httpx.ConnectTimeout)):
            await provider.chat_completion(messages)

    @pytest.mark.asyncio
    async def test_empty_messages_list(self, ollama_provider):
        """Test behavior with empty messages list."""
        messages = []

        # Ollama may handle this differently - either error or empty response
        # We just verify it doesn't crash unexpectedly
        try:
            response = await ollama_provider.chat_completion(messages)
            # If it succeeds, verify response structure
            assert isinstance(response, ChatResponse)
        except httpx.HTTPStatusError:
            # HTTP error is acceptable for empty messages
            pass
