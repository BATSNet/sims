"""
Integration Registry

Manages registration and lookup of integration plugins.
"""
from typing import Dict, Type, Optional
import logging

from plugins.base_integration import IntegrationPlugin

logger = logging.getLogger(__name__)


class IntegrationRegistry:
    """
    Registry for integration plugins.

    Provides a centralized way to register and retrieve integration plugins
    by their type (webhook, sedap, email, etc.)
    """

    _plugins: Dict[str, Type[IntegrationPlugin]] = {}

    @classmethod
    def register(cls, integration_type: str, plugin_class: Type[IntegrationPlugin]) -> None:
        """
        Register an integration plugin.

        Args:
            integration_type: Type identifier (webhook, sedap, email, etc.)
            plugin_class: Plugin class that inherits from IntegrationPlugin
        """
        if not issubclass(plugin_class, IntegrationPlugin):
            raise TypeError(f"Plugin class must inherit from IntegrationPlugin")

        cls._plugins[integration_type.lower()] = plugin_class
        logger.info(f"Registered integration plugin: {integration_type} -> {plugin_class.__name__}")

    @classmethod
    def get_plugin(
        cls,
        integration_type: str,
        config: Dict,
        credentials: Dict
    ) -> Optional[IntegrationPlugin]:
        """
        Get an integration plugin instance.

        Args:
            integration_type: Type identifier
            config: Integration configuration
            credentials: Authentication credentials

        Returns:
            IntegrationPlugin instance or None if not found
        """
        plugin_class = cls._plugins.get(integration_type.lower())
        if not plugin_class:
            logger.warning(f"No plugin registered for integration type: {integration_type}")
            return None

        try:
            return plugin_class(config, credentials)
        except Exception as e:
            logger.error(f"Error instantiating plugin {integration_type}: {e}", exc_info=True)
            return None

    @classmethod
    def list_plugins(cls) -> Dict[str, Type[IntegrationPlugin]]:
        """
        List all registered plugins.

        Returns:
            Dictionary of integration type to plugin class
        """
        return cls._plugins.copy()

    @classmethod
    def is_registered(cls, integration_type: str) -> bool:
        """
        Check if an integration type is registered.

        Args:
            integration_type: Type identifier

        Returns:
            True if registered, False otherwise
        """
        return integration_type.lower() in cls._plugins
