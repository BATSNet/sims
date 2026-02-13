"""
Plugin Registration

Registers all available integration plugins with the IntegrationRegistry.
This module should be imported at application startup.
"""
import logging

from plugins.integration_registry import IntegrationRegistry
from plugins.webhook_plugin import WebhookPlugin
from plugins.sedap_plugin import SEDAPPlugin
from plugins.email_plugin import EmailPlugin

logger = logging.getLogger(__name__)


def register_all_plugins():
    """Register all available integration plugins"""
    try:
        # Register webhook plugin
        IntegrationRegistry.register('webhook', WebhookPlugin)
        IntegrationRegistry.register('n8n', WebhookPlugin)  # n8n uses webhook
        IntegrationRegistry.register('custom', WebhookPlugin)  # custom also uses webhook

        # Register SEDAP plugin
        IntegrationRegistry.register('sedap', SEDAPPlugin)

        # Register email plugin
        IntegrationRegistry.register('email', EmailPlugin)

        logger.info("Successfully registered all integration plugins")
        logger.info(f"Available plugins: {', '.join(IntegrationRegistry.list_plugins().keys())}")

    except Exception as e:
        logger.error(f"Error registering plugins: {e}", exc_info=True)
        raise


# Auto-register on import
register_all_plugins()
