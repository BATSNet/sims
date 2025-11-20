"""
Provider factory for creating AI provider instances.
"""
import os
import logging
from typing import Optional
from .base import BaseLLMProvider, BaseTranscriptionProvider
from .openai_provider import OpenAIProvider, OpenAITranscriptionProvider
from .anthropic_provider import AnthropicProvider
from .mistral_provider import MistralProvider
from .deepinfra_provider import DeepInfraProvider, DeepInfraTranscriptionProvider
from .featherai_provider import FeatherAIProvider

logger = logging.getLogger(__name__)


class ProviderFactory:
    """Factory for creating AI provider instances."""

    # API key environment variable mapping
    API_KEY_ENV = {
        'openai': 'OPENAI_API_KEY',
        'anthropic': 'ANTHROPIC_API_KEY',
        'mistral': 'MISTRAL_API_KEY',
        'deepinfra': 'DEEPINFRA_API_KEY',
        'featherai': 'FEATHERLESS_API_KEY'
    }

    @staticmethod
    def create_llm_provider(
        provider_name: str,
        model: str,
        **kwargs
    ) -> Optional[BaseLLMProvider]:
        """
        Create an LLM provider instance.

        Args:
            provider_name: Name of the provider (openai, anthropic, mistral, deepinfra, featherai)
            model: Model identifier
            **kwargs: Additional configuration (temperature, max_tokens, timeout, etc.)

        Returns:
            Provider instance or None if API key is missing
        """
        provider_name = provider_name.lower()

        # Get API key from environment
        api_key_env = ProviderFactory.API_KEY_ENV.get(provider_name)
        if not api_key_env:
            logger.error(f"Unknown provider: {provider_name}")
            return None

        api_key = os.getenv(api_key_env)
        if not api_key:
            logger.error(f"API key not found for {provider_name} (env var: {api_key_env})")
            return None

        # Create provider instance
        try:
            if provider_name == 'openai':
                return OpenAIProvider(api_key, model, **kwargs)
            elif provider_name == 'anthropic':
                return AnthropicProvider(api_key, model, **kwargs)
            elif provider_name == 'mistral':
                return MistralProvider(api_key, model, **kwargs)
            elif provider_name == 'deepinfra':
                return DeepInfraProvider(api_key, model, **kwargs)
            elif provider_name == 'featherai':
                return FeatherAIProvider(api_key, model, **kwargs)
            else:
                logger.error(f"Unsupported LLM provider: {provider_name}")
                return None
        except Exception as e:
            logger.error(f"Failed to create {provider_name} provider: {e}", exc_info=True)
            return None

    @staticmethod
    def create_transcription_provider(
        provider_name: str,
        model: str,
        **kwargs
    ) -> Optional[BaseTranscriptionProvider]:
        """
        Create a transcription provider instance.

        Args:
            provider_name: Name of the provider (openai, deepinfra)
            model: Model identifier
            **kwargs: Additional configuration (timeout, etc.)

        Returns:
            Provider instance or None if API key is missing
        """
        provider_name = provider_name.lower()

        # Get API key from environment
        api_key_env = ProviderFactory.API_KEY_ENV.get(provider_name)
        if not api_key_env:
            logger.error(f"Unknown transcription provider: {provider_name}")
            return None

        api_key = os.getenv(api_key_env)
        if not api_key:
            logger.error(f"API key not found for {provider_name} (env var: {api_key_env})")
            return None

        # Create provider instance
        try:
            if provider_name == 'openai':
                return OpenAITranscriptionProvider(api_key, model, **kwargs)
            elif provider_name == 'deepinfra':
                return DeepInfraTranscriptionProvider(api_key, model, **kwargs)
            else:
                logger.error(f"Unsupported transcription provider: {provider_name}")
                return None
        except Exception as e:
            logger.error(f"Failed to create {provider_name} transcription provider: {e}", exc_info=True)
            return None
