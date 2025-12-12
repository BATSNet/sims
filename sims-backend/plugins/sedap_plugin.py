"""
SEDAP Integration Plugin

Implements SEDAP.Express integration for Battle Management Systems (BMS).
Formats incidents as SEDAP CONTACT and TEXT messages per ICD v1.0 specification.
"""
import base64
import time
import httpx
from typing import Dict, Any, Optional, Tuple
import logging

from plugins.base_integration import IntegrationPlugin, IntegrationResult

logger = logging.getLogger(__name__)


class SEDAPPlugin(IntegrationPlugin):
    """Plugin for SEDAP.Express BMS integration"""

    # Message counters (7-bit, wraps at 127)
    _contact_counter = 0
    _text_counter = 0

    def __init__(self, integration_config: Dict[str, Any], auth_credentials: Dict[str, Any]):
        super().__init__(integration_config, auth_credentials)
        self.endpoint_url = self._extract_config_value('endpoint_url', required=True)
        self.timeout = self._extract_config_value('timeout', default=30)
        self.sender_id = self._extract_config_value('sender_id', default='SIMS')
        self.classification = self._extract_config_value('classification', default='U')

    @classmethod
    def _get_next_counter(cls, message_type: str) -> str:
        """Get next sequential message counter in hex format."""
        if message_type == "CONTACT":
            cls._contact_counter = (cls._contact_counter + 1) % 128
            return format(cls._contact_counter, 'X')
        elif message_type == "TEXT":
            cls._text_counter = (cls._text_counter + 1) % 128
            return format(cls._text_counter, 'X')
        return "0"

    @staticmethod
    def _get_timestamp() -> str:
        """Generate 64-bit Unix timestamp with milliseconds in hexadecimal."""
        timestamp_ms = int(time.time() * 1000)
        return format(timestamp_ms, 'X')

    def _format_contact_message(
        self,
        incident: Dict[str, Any]
    ) -> str:
        """
        Format incident as SEDAP CONTACT message per ICD v1.0 specification.

        CONTACT message format (CSV with semicolons):
        CONTACT;<Number>;<Time>;<Sender>;<Classification>;<Acknowledgement>;<MAC>;
        <ContactID>;<DeleteFlag>;<Latitude>;<Longitude>;<Altitude>;
        <relX>;<relY>;<relZ>;<Speed>;<Course>;
        <Heading>;<Roll>;<Pitch>;<Width>;<Length>;<Height>;
        <Name>;<Source>;<SIDC>;<MMSI>;<ICAO>;<Image>;<Comment>
        """
        counter = self._get_next_counter("CONTACT")
        timestamp = self._get_timestamp()
        contact_id = incident.get('incident_id', 'UNKNOWN')

        # Extract location data
        lat = incident.get('latitude', 0.0)
        lon = incident.get('longitude', 0.0)
        heading = incident.get('heading', 0.0)

        # Get reporter phone as sender identifier
        reporter = incident.get('user_phone', '') or incident.get('reporter', '') or self.sender_id

        # Contact metadata
        name = incident.get('title', 'Unknown Incident')
        comment_raw = incident.get('description', '') or 'SIMS'
        comment = base64.b64encode(comment_raw.encode('utf-8')).decode('ascii')

        # Get image data if available (BASE64 encoded)
        image_data = ''
        if incident.get('image_base64'):
            image_data = incident.get('image_base64')

        # Build CONTACT message per spec
        parts = [
            "CONTACT",
            "",                   # Number (empty per example)
            timestamp,            # Time (64-bit Unix timestamp in hex)
            reporter,             # Sender ID (phone number of reporter)
            self.classification,  # Classification (U, R, C, S, T)
            "",                   # Acknowledgement (empty = FALSE)
            "",                   # MAC (Message Authentication Code)
            contact_id,           # ContactID (mandatory)
            "FALSE",              # DeleteFlag (FALSE = current contact)
            str(lat),             # Latitude in decimal degrees
            str(lon),             # Longitude in decimal degrees
            "0",                  # Altitude in meters
            "",                   # Relative X-Distance (empty = using absolute coords)
            "",                   # Relative Y-Distance
            "",                   # Relative Z-Distance
            "",                   # Speed over ground in m/s
            "",                   # Course over ground in degrees
            str(heading) if heading else "",  # Heading in degrees
            "",                   # Roll in degrees
            "",                   # Pitch in degrees
            "",                   # Width in meters
            "",                   # Length in meters
            "",                   # Height in meters
            name,                 # Name of contact
            "M",                  # Source (M=Manual, R=Radar, O=Optical, etc.)
            "SUGP-----------",    # SIDC code (15-char MIL-STD-2525)
            "",                   # MMSI (Maritime Mobile Service Identity)
            "",                   # ICAO (aviation identifier)
            image_data,           # Image (BASE64 encoded JPG/PNG)
            comment               # Comment (BASE64 encoded free text)
        ]

        # Message ends with \r\n per SEDAP example
        return ";".join(parts) + "\r\n"

    def _format_text_message(
        self,
        message: str,
        alert_type: str = "1"  # 1=Alert, 2=Warning, 3=Notice, 4=Chat
    ) -> str:
        """
        Format text as SEDAP TEXT message per ICD v1.0 specification.

        TEXT message format:
        TEXT;<Number>;<Time>;<Sender>;<Classification>;<Acknowledgement>;<MAC>;
        <Recipient>;<Type>;<Encoding>;<Text>
        """
        counter = self._get_next_counter("TEXT")
        timestamp = self._get_timestamp()

        parts = [
            "TEXT",
            counter,              # Number (7-bit counter)
            timestamp,            # Time (64-bit Unix timestamp in hex)
            self.sender_id,       # Sender ID
            self.classification,  # Classification (U, R, C, S, T)
            "FALSE",              # Acknowledgement
            "",                   # MAC (Message Authentication Code)
            "",                   # Recipient (empty = broadcast)
            alert_type,           # Type (1=Alert, 2=Warning, 3=Notice, 4=Chat)
            "NONE",               # Encoding (NONE=not encoded, BASE64=encoded)
            f'"{message}"'        # Text (quoted if contains special chars)
        ]

        # Message ends with \r\n per SEDAP example
        return ";".join(parts) + "\r\n"

    async def send(
        self,
        incident: Dict[str, Any],
        organization: Dict[str, Any],
        payload_template: Optional[str] = None
    ) -> IntegrationResult:
        """
        Send incident to SEDAP endpoint.

        Args:
            incident: Incident data dictionary
            organization: Organization data dictionary
            payload_template: Not used for SEDAP (uses custom CSV format)

        Returns:
            IntegrationResult with delivery status
        """
        start_time = time.time()

        try:
            # Format CONTACT message for the incident
            contact_msg = self._format_contact_message(incident)

            # Format TEXT message for alert
            alert_text = f"New incident: {incident.get('title', 'Untitled')}"
            text_msg = self._format_text_message(alert_text, alert_type="1")

            # Build JSON payload for REST API
            payload = {
                "messages": [
                    {"message": contact_msg},
                    {"message": text_msg}
                ]
            }

            self.logger.info(f"Sending to SEDAP at {self.endpoint_url}")

            # Send to SEDAP endpoint
            async with httpx.AsyncClient(timeout=self.timeout) as client:
                response = await client.post(
                    self.endpoint_url,
                    json=payload,
                    headers={"Content-Type": "application/json"}
                )

                duration_ms = int((time.time() - start_time) * 1000)

                if response.status_code == 200:
                    self.logger.info(
                        f"Successfully forwarded incident {incident.get('incident_id')} "
                        f"to {organization.get('name')} via SEDAP"
                    )
                    return IntegrationResult(
                        success=True,
                        status_code=response.status_code,
                        response_body=response.text,
                        duration_ms=duration_ms,
                        request_url=self.endpoint_url,
                        request_payload=payload
                    )
                else:
                    error_msg = f"SEDAP API returned status {response.status_code}: {response.text}"
                    self.logger.error(error_msg)
                    return IntegrationResult(
                        success=False,
                        status_code=response.status_code,
                        response_body=response.text,
                        error_message=error_msg,
                        duration_ms=duration_ms,
                        request_url=self.endpoint_url,
                        request_payload=payload
                    )

        except httpx.TimeoutException as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"Timeout connecting to SEDAP endpoint: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=self.endpoint_url
            )

        except httpx.ConnectError as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"Connection error to SEDAP at {self.endpoint_url}: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=self.endpoint_url
            )

        except Exception as e:
            duration_ms = int((time.time() - start_time) * 1000)
            error_msg = f"Error forwarding to SEDAP: {type(e).__name__}: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            return IntegrationResult(
                success=False,
                error_message=error_msg,
                duration_ms=duration_ms,
                request_url=self.endpoint_url
            )

    async def test_connection(self) -> Tuple[bool, str]:
        """
        Test connection to SEDAP endpoint.

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            async with httpx.AsyncClient(timeout=5.0) as client:
                response = await client.get(self.endpoint_url)
                return True, f"Connected to SEDAP endpoint (status {response.status_code})"
        except Exception as e:
            return False, f"Failed to connect to SEDAP: {str(e)}"

    def validate_config(self) -> Tuple[bool, Optional[str]]:
        """
        Validate SEDAP configuration.

        Returns:
            Tuple of (valid: bool, error_message: Optional[str])
        """
        if not self.endpoint_url:
            return False, "Missing required configuration: endpoint_url"

        if not isinstance(self.timeout, (int, float)) or self.timeout <= 0:
            return False, "Invalid timeout value: must be a positive number"

        if self.classification not in ['U', 'R', 'C', 'S', 'T']:
            return False, f"Invalid classification: {self.classification}. Must be U, R, C, S, or T"

        return True, None
