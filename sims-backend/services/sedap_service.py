"""
SEDAP API Integration Service

Formats and sends incident data to SEDAP-Express endpoints for
BMS (Battle Management System) integration.
"""
import time
import json
import httpx
from typing import Dict, Any, Optional, Tuple
from datetime import datetime
import logging

from config import Config

logger = logging.getLogger(__name__)


class SEDAPService:
    """Service for formatting and sending incidents to SEDAP API."""

    # Message counters (7-bit, wraps at 127)
    _contact_counter = 0
    _text_counter = 0

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

    @staticmethod
    def _format_contact_message(
        incident: Dict[str, Any],
        sender_id: str,
        classification: str
    ) -> str:
        """
        Format incident as SEDAP CONTACT message.

        CONTACT message format (CSV with semicolons):
        CONTACT;<counter>;<timestamp>;<msg_id>;<classification>;<ack>;<mac>;<persistent>;
        <latitude>;<longitude>;<altitude>;<speed>;<course>;<name>;<source>;<sidc>;
        <image>;<comment>
        """
        counter = SEDAPService._get_next_counter("CONTACT")
        timestamp = SEDAPService._get_timestamp()
        msg_id = format(hash(incident.get('incident_id', '')) & 0xFFFF, 'X')

        # Extract location data
        lat = incident.get('latitude', 0.0)
        lon = incident.get('longitude', 0.0)
        heading = incident.get('heading', 0.0)

        # Contact metadata
        name = incident.get('title', 'Unknown Incident')
        comment = incident.get('description', '')
        category = incident.get('category', 'Unclassified')

        # Build CONTACT message
        parts = [
            "CONTACT",
            counter,
            timestamp,
            msg_id,
            classification,
            "FALSE",  # acknowledgement
            "",       # MAC
            "100",    # persistent (seconds)
            "FALSE",  # static
            str(lat),
            str(lon),
            "0",      # altitude
            "",       # speed
            str(heading),
            "",       # heading rate
            "",       # climb
            "",       # location error
            "",       # altitude error
            "",       # roll
            "",       # pitch
            "",       # yaw
            name,
            sender_id,
            "",       # SIDC code
            "",       # base64 image
            "",       # affiliation
            "",       # uniqueDesignation
            "",       # additionalInformation
            comment
        ]

        return ";".join(parts)

    @staticmethod
    def _format_text_message(
        message: str,
        sender_id: str,
        classification: str,
        alert_type: str = "01"  # 01=Alert, 02=Warning, 03=Notice, 04=Chat
    ) -> str:
        """
        Format text as SEDAP TEXT message.

        TEXT message format:
        TEXT;<counter>;<timestamp>;<msg_id>;<classification>;<ack>;<mac>;<persistent>;
        <alertType>;<sender>;<text>;<timeToLive>
        """
        counter = SEDAPService._get_next_counter("TEXT")
        timestamp = SEDAPService._get_timestamp()
        msg_id = format(int(time.time()) & 0xFFFF, 'X')

        parts = [
            "TEXT",
            counter,
            timestamp,
            msg_id,
            classification,
            "TRUE",      # acknowledgement
            "",          # MAC
            "",          # persistent
            alert_type,
            "NONE",      # sender (NONE = use message sender)
            f'"{message}"',  # quoted message text
            "1000"       # time to live (seconds)
        ]

        return ";".join(parts)

    @staticmethod
    async def forward_incident(
        incident: Dict[str, Any],
        organization: Dict[str, Any]
    ) -> Tuple[bool, Optional[str]]:
        """
        Forward incident to SEDAP API endpoint.

        Args:
            incident: Incident data dictionary
            organization: Organization data dictionary with api_type

        Returns:
            Tuple of (success: bool, error_message: Optional[str])
        """
        try:
            api_type = organization.get('api_type', '')

            if api_type not in Config.API_ENDPOINTS:
                return False, f"Unknown API type: {api_type}"

            api_config = Config.API_ENDPOINTS[api_type]
            endpoint_url = api_config.get('url')

            if not endpoint_url:
                return False, f"No endpoint URL configured for {api_type}"

            sender_id = api_config.get('sender_id', 'SIMS')
            classification = api_config.get('classification', 'U')

            # Format CONTACT message for the incident
            contact_msg = SEDAPService._format_contact_message(
                incident, sender_id, classification
            )

            # Format TEXT message for alert
            org_name = organization.get('name', 'Unknown Organization')
            alert_text = f"New incident: {incident.get('title', 'Untitled')}"
            text_msg = SEDAPService._format_text_message(
                alert_text, sender_id, classification, alert_type="01"
            )

            # Build JSON payload for REST API
            payload = {
                "messages": [
                    {"message": contact_msg},
                    {"message": text_msg}
                ]
            }

            # Send to SEDAP endpoint
            async with httpx.AsyncClient(timeout=10.0) as client:
                response = await client.post(
                    endpoint_url,
                    json=payload,
                    headers={"Content-Type": "application/json"}
                )

                if response.status_code == 200:
                    logger.info(
                        f"Successfully forwarded incident {incident.get('incident_id')} "
                        f"to {org_name} via {api_type}"
                    )
                    return True, None
                else:
                    error_msg = f"SEDAP API returned status {response.status_code}: {response.text}"
                    logger.error(error_msg)
                    return False, error_msg

        except httpx.TimeoutException:
            error_msg = f"Timeout connecting to {api_type} endpoint"
            logger.error(error_msg)
            return False, error_msg

        except Exception as e:
            error_msg = f"Error forwarding to {api_type}: {str(e)}"
            logger.error(error_msg)
            return False, error_msg

    @staticmethod
    async def test_connection(api_type: str) -> Tuple[bool, str]:
        """
        Test connection to SEDAP endpoint.

        Args:
            api_type: API type to test (e.g., 'SEDAP')

        Returns:
            Tuple of (success: bool, message: str)
        """
        if api_type not in Config.API_ENDPOINTS:
            return False, f"Unknown API type: {api_type}"

        api_config = Config.API_ENDPOINTS[api_type]
        endpoint_url = api_config.get('url')

        if not endpoint_url:
            return False, f"No endpoint URL configured for {api_type}"

        try:
            async with httpx.AsyncClient(timeout=5.0) as client:
                response = await client.get(endpoint_url)
                return True, f"Connected to {api_type} endpoint (status {response.status_code})"
        except Exception as e:
            return False, f"Failed to connect to {api_type}: {str(e)}"
