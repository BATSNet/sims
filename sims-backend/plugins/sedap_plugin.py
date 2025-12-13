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

# Category to human-readable name mapping for SEDAP Name field
CATEGORY_TO_NAME = {
    'drone_detection': 'Suspected Drone',
    'suspicious_vehicle': 'Suspicious Vehicle',
    'suspicious_person': 'Suspicious Person',
    'fire_incident': 'Fire Incident',
    'medical_emergency': 'Medical Emergency',
    'infrastructure_damage': 'Infrastructure Damage',
    'cyber_incident': 'Cyber Incident',
    'hazmat_incident': 'Hazmat Incident',
    'natural_disaster': 'Natural Disaster',
    'airport_incident': 'Airport Incident',
    'security_breach': 'Security Breach',
    'civil_unrest': 'Civil Unrest',
    'armed_threat': 'Armed Threat',
    'explosion': 'Explosion',
    'chemical_biological': 'Chemical/Biological',
    'maritime_incident': 'Maritime Incident',
    'theft_burglary': 'Theft/Burglary',
    'unclassified': 'Unclassified Incident'
}

# Category to MIL-STD-2525B/STANAG 2019 SIDC mapping (15-char codes)
CATEGORY_TO_SIDC = {
    'drone_detection': 'SUAPU----------',  # Unknown Air UAV
    'suspicious_vehicle': 'SUGPU----------',  # Unknown Ground Unit
    'suspicious_person': 'SUGPE----------',  # Unknown Ground Personnel
    'fire_incident': 'OHOPF----------',  # Hazard Fire/Flame
    'medical_emergency': 'GFGPUSM--------',  # Friendly Unit Medical
    'infrastructure_damage': 'OHOPI----------',  # Hazard Infrastructure
    'cyber_incident': 'SUGPU----------',  # Unknown Ground Unit (no specific cyber)
    'hazmat_incident': 'OHOPH----------',  # Hazard HAZMAT
    'natural_disaster': 'OHOPN----------',  # Hazard Natural Event
    'airport_incident': 'SUAPI----------',  # Unknown Air Installation
    'security_breach': 'SHGPU----------',  # Hostile Ground Unit
    'civil_unrest': 'SUGPE----------',  # Unknown Ground Personnel
    'armed_threat': 'SHGPU----------',  # Hostile Ground Unit
    'explosion': 'OHOPE----------',  # Hazard Explosion
    'chemical_biological': 'OHOPB----------',  # Hazard Bio/Chem
    'maritime_incident': 'SUSPU----------',  # Unknown Sea Surface Unit
    'theft_burglary': 'SUGPU----------',  # Unknown Ground Unit
    'unclassified': 'SUGP-----------',  # Unknown Ground (default)
}


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

    def _build_structured_comment(self, incident: Dict[str, Any]) -> str:
        """
        Build structured comment with all available context.

        IMPORTANT: Always include transcription and available text regardless of AI status.
        This ensures operators always receive maximum context about the incident.
        """
        parts = []

        # Priority
        priority = incident.get('priority', 'medium')
        parts.append(f"Priority: {priority.upper()}")

        # Category
        category = incident.get('category', 'unclassified')
        parts.append(f"Type: {category.replace('_', ' ').title()}")

        # Description (AI-generated summary OR raw text input)
        description = incident.get('description', '')
        if description:
            parts.append(f"Report: {description}")

        # Audio transcript - ALWAYS include if available (regardless of AI)
        transcript = incident.get('audio_transcript', '')
        if transcript:
            parts.append(f"Voice: {transcript}")

        # Title if different from description
        title = incident.get('title', '')
        if title and title != description:
            parts.append(f"Title: {title}")

        # Timestamp
        created = incident.get('created_at', '')
        if created:
            parts.append(f"Time: {created}")

        # Combine and encode
        comment_text = ' | '.join(parts) or 'SIMS Incident'
        return base64.b64encode(comment_text.encode('utf-8')).decode('ascii')

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
        timestamp = self._get_timestamp()

        # Sender = reporter phone (original source of information)
        sender = incident.get('user_phone', '') or self.sender_id

        # ContactID = incident ID
        contact_id = incident.get('incident_id', 'UNKNOWN')

        # Location data
        lat = incident.get('latitude', 0.0)
        lon = incident.get('longitude', 0.0)
        altitude = incident.get('altitude', 0) or 0
        heading = incident.get('heading', '')

        # Name = dynamic category name for human readability
        category = incident.get('category', 'unclassified')
        # Normalize category to lowercase for lookup
        category_key = category.lower().replace(' ', '_') if category else 'unclassified'
        name = CATEGORY_TO_NAME.get(category_key, 'Incident Report')

        # SIDC = dynamic based on category (MIL-STD-2525B/STANAG 2019)
        sidc = CATEGORY_TO_SIDC.get(category_key, 'SUGP-----------')

        # Image = BASE64 encoded (populated by send() method)
        image_data = incident.get('image_base64', '')

        # Comment = structured summary with all available context (BASE64 encoded)
        comment = self._build_structured_comment(incident)

        # Build CONTACT message per spec
        parts = [
            "CONTACT",
            "",                    # Number (empty per spec example)
            timestamp,             # Time (64-bit Unix timestamp in hex)
            sender,                # Sender = reporter phone
            self.classification,   # Classification (U, R, C, S, T)
            "",                    # Acknowledgement
            "",                    # MAC (Message Authentication Code)
            contact_id,            # ContactID (mandatory)
            "FALSE",               # DeleteFlag (FALSE = current contact)
            str(lat),              # Latitude in decimal degrees
            str(lon),              # Longitude in decimal degrees
            str(altitude),         # Altitude in meters from GPS
            "",                    # Relative X-Distance (using absolute coords)
            "",                    # Relative Y-Distance
            "",                    # Relative Z-Distance
            "",                    # Speed over ground in m/s
            "",                    # Course over ground in degrees
            str(heading) if heading else "",  # Heading in degrees
            "",                    # Roll in degrees
            "",                    # Pitch in degrees
            "",                    # Width in meters
            "",                    # Length in meters
            "",                    # Height in meters
            name,                  # Name = category name for readability
            "M",                   # Source = Manual (app submission)
            sidc,                  # SIDC = dynamic based on category
            "",                    # MMSI (Maritime Mobile Service Identity)
            "",                    # ICAO (aviation identifier)
            image_data,            # Image (BASE64 encoded JPG/PNG)
            comment                # Comment (BASE64 structured summary)
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
            # Fetch image from URL and convert to base64 if available
            if incident.get('image_url') and not incident.get('image_base64'):
                try:
                    async with httpx.AsyncClient(timeout=30) as img_client:
                        img_response = await img_client.get(incident['image_url'])
                        if img_response.status_code == 200:
                            incident['image_base64'] = base64.b64encode(
                                img_response.content
                            ).decode('ascii')
                            self.logger.info(f"Fetched and encoded image from {incident['image_url']}")
                except Exception as img_err:
                    self.logger.warning(f"Failed to fetch image: {img_err}")

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
