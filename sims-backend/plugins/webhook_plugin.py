"""
Webhook Integration Plugin

Implements generic HTTP webhook integration with Jinja2 template support.
Flexible enough to work with Zapier, Make (Integromat), n8n, and custom endpoints.
"""
import time
import json
import httpx
from typing import Dict, Any, Optional, Tuple
from jinja2 import Template, TemplateSyntaxError, UndefinedError
import logging

from plugins.base_integration import IntegrationPlugin, IntegrationResult

logger = logging.getLogger(__name__)


class WebhookPlugin(IntegrationPlugin):
    """Plugin for generic HTTP webhook integrations"""

    def __init__(self, integration_config: Dict[str, Any], auth_credentials: Dict[str, Any]):
        super().__init__(integration_config, auth_credentials)
        self.endpoint_url = self._extract_config_value('endpoint_url', required=True)
        self.timeout = self._extract_config_value('timeout', default=30)
        self.custom_headers = self._extract_config_value('custom_headers', default={})
        self.method = self._extract_config_value('method', default='POST').upper()

    def _prepare_headers(self, auth_type: str) -> Dict[str, str]:
        """
        Prepare HTTP headers including authentication.

        Args:
            auth_type: Type of authentication (bearer_token, api_key, etc.)

        Returns:
            Dictionary of HTTP headers
        """
        headers = {
            "Content-Type": "application/json",
            "User-Agent": "SIMS-Integration/1.0"
        }

        # Add custom headers
        if self.custom_headers:
            headers.update(self.custom_headers)

        # Add authentication
        if auth_type == 'bearer_token':
            token = self._extract_credential('token', required=True)
            headers['Authorization'] = f'Bearer {token}'
        elif auth_type == 'api_key':
            api_key = self._extract_credential('api_key', required=True)
            header_name = self._extract_credential('header_name', default='X-API-Key')
            headers[header_name] = api_key
        elif auth_type == 'custom_header':
            header_name = self._extract_credential('header_name', required=True)
            header_value = self._extract_credential('header_value', required=True)
            headers[header_name] = header_value

        return headers

    def _render_payload(
        self,
        incident: Dict[str, Any],
        organization: Dict[str, Any],
        template_str: str
    ) -> Dict[str, Any]:
        """
        Render payload using Jinja2 template.

        Args:
            incident: Incident data dictionary
            organization: Organization data dictionary
            template_str: Jinja2 template string

        Returns:
            Rendered payload as dictionary

        Raises:
            ValueError: If template rendering fails
        """
        try:
            # Create Jinja2 template
            template = Template(template_str)

            # Prepare context variables
            context = {
                'incident': incident,
                'organization': organization
            }

            # Render template
            rendered = template.render(**context)

            # Parse as JSON
            payload = json.loads(rendered)

            return payload

        except TemplateSyntaxError as e:
            raise ValueError(f"Template syntax error: {e}")
        except UndefinedError as e:
            raise ValueError(f"Undefined variable in template: {e}")
        except json.JSONDecodeError as e:
            raise ValueError(f"Rendered template is not valid JSON: {e}")
        except Exception as e:
            raise ValueError(f"Error rendering payload template: {e}")

    async def send(
        self,
        incident: Dict[str, Any],
        organization: Dict[str, Any],
        payload_template: Optional[str] = None
    ) -> IntegrationResult:
        """
        Send incident to webhook endpoint.

        Args:
            incident: Incident data dictionary
            organization: Organization data dictionary
            payload_template: Jinja2 template for payload (required for webhook)

        Returns:
            IntegrationResult with delivery status
        """
        start_time = time.time()

        try:
            if not payload_template:
                raise ValueError("Webhook integration requires a payload_template")

            # Render payload using template
            payload = self._render_payload(incident, organization, payload_template)

            # Prepare headers with authentication
            auth_type = self.credentials.get('auth_type', 'none')
            headers = self._prepare_headers(auth_type)

            self.logger.info(f"Sending webhook to {self.endpoint_url}")
            self.logger.debug(f"Payload: {json.dumps(payload, indent=2)}")

            # Send webhook request
            async with httpx.AsyncClient(timeout=self.timeout) as client:
                if self.method == 'POST':
                    response = await client.post(
                        self.endpoint_url,
                        json=payload,
                        headers=headers
                    )
                elif self.method == 'PUT':
                    response = await client.put(
                        self.endpoint_url,
                        json=payload,
                        headers=headers
                    )
                else:
                    raise ValueError(f"Unsupported HTTP method: {self.method}")

                duration_ms = int((time.time() - start_time) * 1000)

                # Check response
                if 200 <= response.status_code < 300:
                    self.logger.info(
                        f"Successfully sent webhook for incident {incident.get('incident_id')} "
                        f"to {organization.get('name')}"
                    )
                    return IntegrationResult(
                        success=True,
                        status_code=response.status_code,
                        response_body=response.text[:1000],  # Truncate to 1000 chars
                        duration_ms=duration_ms,
                        request_url=self.endpoint_url,
                        request_payload=payload
                    )
                else:
                    error_msg = f"Webhook returned status {response.status_code}: {response.text[:500]}"
                    self.logger.error(error_msg)
                    return IntegrationResult(
                        success=False,
                        status_code=response.status_code,
                        response_body=response.text[:1000],
                        error_message=error_msg,
                        duration_ms=duration_ms,
                        request_url=self.endpoint_url,
                        request_payload=payload
                    )

        except ValueError as e:
            # Template/config errors
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"Configuration error: {str(e)}"
            self.logger.error(error_msg)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=self.endpoint_url
            )

        except httpx.TimeoutException as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"Timeout connecting to webhook: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=self.endpoint_url
            )

        except httpx.ConnectError as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"Connection error to webhook at {self.endpoint_url}: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=self.endpoint_url
            )

        except Exception as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"Error sending webhook: {type(e).__name__}: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=self.endpoint_url
            )

    async def test_connection(self) -> Tuple[bool, str]:
        """
        Test connection to webhook endpoint.

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            # Send a minimal test payload
            test_payload = {"test": True, "message": "SIMS integration test"}

            auth_type = self.credentials.get('auth_type', 'none')
            headers = self._prepare_headers(auth_type)

            async with httpx.AsyncClient(timeout=5.0) as client:
                if self.method == 'POST':
                    response = await client.post(
                        self.endpoint_url,
                        json=test_payload,
                        headers=headers
                    )
                else:
                    response = await client.head(self.endpoint_url, headers=headers)

                if 200 <= response.status_code < 300:
                    return True, f"Successfully connected to webhook (status {response.status_code})"
                else:
                    return False, f"Webhook returned status {response.status_code}"

        except Exception as e:
            return False, f"Failed to connect to webhook: {str(e)}"

    def validate_config(self) -> Tuple[bool, Optional[str]]:
        """
        Validate webhook configuration.

        Returns:
            Tuple of (valid: bool, error_message: Optional[str])
        """
        if not self.endpoint_url:
            return False, "Missing required configuration: endpoint_url"

        if not self.endpoint_url.startswith(('http://', 'https://')):
            return False, "endpoint_url must start with http:// or https://"

        if not isinstance(self.timeout, (int, float)) or self.timeout <= 0:
            return False, "Invalid timeout value: must be a positive number"

        if self.method not in ['POST', 'PUT']:
            return False, f"Invalid HTTP method: {self.method}. Must be POST or PUT"

        # Validate authentication if configured
        auth_type = self.credentials.get('auth_type', 'none')
        if auth_type == 'bearer_token':
            if not self.credentials.get('token'):
                return False, "Missing required credential: token (for bearer_token auth)"
        elif auth_type == 'api_key':
            if not self.credentials.get('api_key'):
                return False, "Missing required credential: api_key"
        elif auth_type == 'custom_header':
            if not self.credentials.get('header_name') or not self.credentials.get('header_value'):
                return False, "Missing required credentials: header_name and header_value (for custom_header auth)"

        return True, None
