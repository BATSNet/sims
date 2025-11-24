"""
Base Integration Plugin

Abstract base class for all integration plugins. All plugins must inherit from
this class and implement the required methods.
"""
from abc import ABC, abstractmethod
from typing import Dict, Any, Optional, Tuple
from dataclasses import dataclass
from datetime import datetime
import logging

logger = logging.getLogger(__name__)


@dataclass
class IntegrationResult:
    """Result of an integration delivery attempt"""
    success: bool
    status_code: Optional[int] = None
    response_body: Optional[str] = None
    error_message: Optional[str] = None
    duration_ms: Optional[int] = None
    request_url: Optional[str] = None
    request_payload: Optional[Dict[str, Any]] = None


class IntegrationPlugin(ABC):
    """
    Abstract base class for integration plugins.

    Each plugin handles a specific integration type (webhook, SEDAP, email, etc.)
    and implements the logic for sending incidents to external systems.
    """

    def __init__(self, integration_config: Dict[str, Any], auth_credentials: Dict[str, Any]):
        """
        Initialize the integration plugin.

        Args:
            integration_config: Configuration dict from organization_integration.config
            auth_credentials: Authentication credentials from organization_integration.auth_credentials
        """
        self.config = integration_config
        self.credentials = auth_credentials
        self.logger = logging.getLogger(f"{self.__class__.__module__}.{self.__class__.__name__}")

    @abstractmethod
    async def send(
        self,
        incident: Dict[str, Any],
        organization: Dict[str, Any],
        payload_template: Optional[str] = None
    ) -> IntegrationResult:
        """
        Send incident to external system.

        Args:
            incident: Incident data dictionary with all fields
            organization: Organization data dictionary
            payload_template: Optional custom payload template (Jinja2 format)

        Returns:
            IntegrationResult with delivery status and details
        """
        pass

    @abstractmethod
    async def test_connection(self) -> Tuple[bool, str]:
        """
        Test connection to external system.

        Returns:
            Tuple of (success: bool, message: str)
        """
        pass

    @abstractmethod
    def validate_config(self) -> Tuple[bool, Optional[str]]:
        """
        Validate integration configuration.

        Returns:
            Tuple of (valid: bool, error_message: Optional[str])
        """
        pass

    def _extract_config_value(self, key: str, required: bool = False, default: Any = None) -> Any:
        """
        Helper to safely extract config value.

        Args:
            key: Configuration key
            required: Whether this config value is required
            default: Default value if not found

        Returns:
            Configuration value

        Raises:
            ValueError: If required config value is missing
        """
        value = self.config.get(key, default)
        if required and value is None:
            raise ValueError(f"Required configuration '{key}' is missing")
        return value

    def _extract_credential(self, key: str, required: bool = False, default: Any = None) -> Any:
        """
        Helper to safely extract credential value.

        Args:
            key: Credential key
            required: Whether this credential is required
            default: Default value if not found

        Returns:
            Credential value

        Raises:
            ValueError: If required credential is missing
        """
        value = self.credentials.get(key, default)
        if required and value is None:
            raise ValueError(f"Required credential '{key}' is missing")
        return value
