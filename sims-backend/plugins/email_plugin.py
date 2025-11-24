"""
Email Integration Plugin

Implements email notification integration with SMTP support.
"""
import time
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from typing import Dict, Any, Optional, Tuple, List
from jinja2 import Template, TemplateSyntaxError
import logging

from plugins.base_integration import IntegrationPlugin, IntegrationResult

logger = logging.getLogger(__name__)


class EmailPlugin(IntegrationPlugin):
    """Plugin for email notification integrations"""

    def __init__(self, integration_config: Dict[str, Any], auth_credentials: Dict[str, Any]):
        super().__init__(integration_config, auth_credentials)
        self.smtp_host = self._extract_config_value('smtp_host', required=True)
        self.smtp_port = self._extract_config_value('smtp_port', default=587)
        self.from_email = self._extract_config_value('from_email', required=True)
        self.to_emails = self._extract_config_value('to_emails', required=True)
        self.use_tls = self._extract_config_value('use_tls', default=True)
        self.timeout = self._extract_config_value('timeout', default=30)

        # Validate to_emails is a list
        if isinstance(self.to_emails, str):
            self.to_emails = [self.to_emails]
        elif not isinstance(self.to_emails, list):
            raise ValueError("to_emails must be a string or list of email addresses")

    def _render_email_content(
        self,
        incident: Dict[str, Any],
        organization: Dict[str, Any],
        template_str: str
    ) -> Tuple[str, str]:
        """
        Render email content using Jinja2 template.

        Args:
            incident: Incident data dictionary
            organization: Organization data dictionary
            template_str: Jinja2 template string (includes subject line)

        Returns:
            Tuple of (subject: str, body: str)

        Raises:
            ValueError: If template rendering fails
        """
        try:
            template = Template(template_str)

            context = {
                'incident': incident,
                'organization': organization
            }

            rendered = template.render(**context)

            # Split subject and body (first line is subject)
            lines = rendered.split('\n', 1)
            if len(lines) == 2 and lines[0].startswith('Subject:'):
                subject = lines[0].replace('Subject:', '').strip()
                body = lines[1].strip()
            else:
                # Default subject if not in template
                subject = f"[SIMS] Incident Alert - {incident.get('incident_id', 'Unknown')}"
                body = rendered.strip()

            return subject, body

        except TemplateSyntaxError as e:
            raise ValueError(f"Template syntax error: {e}")
        except Exception as e:
            raise ValueError(f"Error rendering email template: {e}")

    async def send(
        self,
        incident: Dict[str, Any],
        organization: Dict[str, Any],
        payload_template: Optional[str] = None
    ) -> IntegrationResult:
        """
        Send incident notification via email.

        Args:
            incident: Incident data dictionary
            organization: Organization data dictionary
            payload_template: Jinja2 template for email content (required)

        Returns:
            IntegrationResult with delivery status
        """
        start_time = time.time()

        try:
            if not payload_template:
                raise ValueError("Email integration requires a payload_template")

            # Render email content
            subject, body = self._render_email_content(incident, organization, payload_template)

            # Create email message
            msg = MIMEMultipart('alternative')
            msg['Subject'] = subject
            msg['From'] = self.from_email
            msg['To'] = ', '.join(self.to_emails)

            # Add plain text body
            msg.attach(MIMEText(body, 'plain'))

            # Connect to SMTP server and send
            self.logger.info(f"Sending email to {self.to_emails} via {self.smtp_host}:{self.smtp_port}")

            # Get credentials if using authentication
            username = self.credentials.get('username')
            password = self.credentials.get('password')

            with smtplib.SMTP(self.smtp_host, self.smtp_port, timeout=self.timeout) as server:
                if self.use_tls:
                    server.starttls()

                if username and password:
                    server.login(username, password)

                server.send_message(msg)

            duration_ms = int((time.time() - start_time) * 1000)

            self.logger.info(
                f"Successfully sent email notification for incident {incident.get('incident_id')} "
                f"to {organization.get('name')}"
            )

            return IntegrationResult(
                success=True,
                status_code=250,  # SMTP OK status
                response_body=f"Email sent to {len(self.to_emails)} recipient(s)",
                duration_ms=duration_ms,
                request_url=f"smtp://{self.smtp_host}:{self.smtp_port}",
                request_payload={"subject": subject, "to": self.to_emails}
            )

        except ValueError as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"Configuration error: {str(e)}"
            self.logger.error(error_msg)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=f"smtp://{self.smtp_host}:{self.smtp_port}"
            )

        except smtplib.SMTPAuthenticationError as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"SMTP authentication failed: {str(e)}"
            self.logger.error(error_msg)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=f"smtp://{self.smtp_host}:{self.smtp_port}"
            )

        except smtplib.SMTPException as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"SMTP error: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=f"smtp://{self.smtp_host}:{self.smtp_port}"
            )

        except Exception as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"Error sending email: {type(e).__name__}: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=f"smtp://{self.smtp_host}:{self.smtp_port}"
            )

    async def test_connection(self) -> Tuple[bool, str]:
        """
        Test connection to SMTP server.

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            with smtplib.SMTP(self.smtp_host, self.smtp_port, timeout=5) as server:
                if self.use_tls:
                    server.starttls()

                # Try to authenticate if credentials provided
                username = self.credentials.get('username')
                password = self.credentials.get('password')
                if username and password:
                    server.login(username, password)

                return True, f"Successfully connected to SMTP server {self.smtp_host}:{self.smtp_port}"

        except smtplib.SMTPAuthenticationError as e:
            return False, f"SMTP authentication failed: {str(e)}"
        except Exception as e:
            return False, f"Failed to connect to SMTP server: {str(e)}"

    def validate_config(self) -> Tuple[bool, Optional[str]]:
        """
        Validate email configuration.

        Returns:
            Tuple of (valid: bool, error_message: Optional[str])
        """
        if not self.smtp_host:
            return False, "Missing required configuration: smtp_host"

        if not isinstance(self.smtp_port, int) or self.smtp_port <= 0 or self.smtp_port > 65535:
            return False, "Invalid smtp_port: must be between 1 and 65535"

        if not self.from_email or '@' not in self.from_email:
            return False, "Invalid or missing from_email"

        if not self.to_emails or len(self.to_emails) == 0:
            return False, "Missing required configuration: to_emails"

        # Validate email addresses
        for email in self.to_emails:
            if '@' not in email:
                return False, f"Invalid email address: {email}"

        # Check credentials if authentication is required
        username = self.credentials.get('username')
        password = self.credentials.get('password')
        if username and not password:
            return False, "Password required when username is provided"
        if password and not username:
            return False, "Username required when password is provided"

        return True, None
