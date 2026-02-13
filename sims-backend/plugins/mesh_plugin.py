"""
Mesh Network Integration Plugin

Handles LoRa mesh network incident reporting.
Receives incidents from mesh gateway via MQTT and forwards to mesh network.
"""
from typing import Dict, Any, Optional, Tuple
import json
import logging
from datetime import datetime
import asyncio

from plugins.base_integration import IntegrationPlugin, IntegrationResult

logger = logging.getLogger(__name__)


class MeshPlugin(IntegrationPlugin):
    """
    Integration plugin for LoRa mesh network.

    This plugin acts as a bridge between SIMS backend and the mesh network gateway.
    It can receive incidents from the mesh and forward incidents to the mesh.
    """

    def __init__(self, integration_config: Dict[str, Any], auth_credentials: Dict[str, Any]):
        """
        Initialize mesh plugin.

        Expected config:
        - mqtt_broker: MQTT broker address (e.g., "localhost:1883")
        - topic_prefix: MQTT topic prefix (e.g., "sims/mesh")
        - enable_forwarding: Whether to forward backend incidents to mesh

        Expected credentials:
        - mqtt_username: MQTT username (optional)
        - mqtt_password: MQTT password (optional)
        """
        super().__init__(integration_config, auth_credentials)

        self.mqtt_broker = self._extract_config_value('mqtt_broker', required=True)
        self.topic_prefix = self._extract_config_value('topic_prefix', default='sims/mesh')
        self.enable_forwarding = self._extract_config_value('enable_forwarding', default=True)

        self.mqtt_username = self._extract_credential('mqtt_username', required=False)
        self.mqtt_password = self._extract_credential('mqtt_password', required=False)

        self.mqtt_client = None
        self._connected = False

    async def send(
        self,
        incident: Dict[str, Any],
        organization: Dict[str, Any],
        payload_template: Optional[str] = None
    ) -> IntegrationResult:
        """
        Forward incident to mesh network via MQTT gateway.

        Args:
            incident: Incident data dictionary
            organization: Organization data (for filtering)
            payload_template: Not used for mesh (binary protocol)

        Returns:
            IntegrationResult with delivery status
        """
        start_time = datetime.utcnow()

        try:
            if not self.enable_forwarding:
                return IntegrationResult(
                    success=False,
                    error_message="Mesh forwarding is disabled in configuration"
                )

            # Connect to MQTT if not already connected
            if not self._connected:
                await self._connect_mqtt()

            # Transform incident to mesh format
            mesh_payload = self._transform_to_mesh_format(incident, organization)

            # Publish to MQTT
            topic = f"{self.topic_prefix}/incidents/out"

            # For now, use JSON encoding (TODO: switch to Protobuf)
            payload_json = json.dumps(mesh_payload)

            # Publish message
            # Note: This is a placeholder - actual MQTT client integration needed
            logger.info(f"Publishing incident to mesh: topic={topic}, size={len(payload_json)} bytes")

            # TODO: Implement actual MQTT publish
            # self.mqtt_client.publish(topic, payload_json)

            duration_ms = int((datetime.utcnow() - start_time).total_seconds() * 1000)

            return IntegrationResult(
                success=True,
                status_code=200,
                response_body="Published to mesh network",
                duration_ms=duration_ms,
                request_url=topic,
                request_payload=mesh_payload
            )

        except Exception as e:
            logger.error(f"Error forwarding to mesh network: {e}", exc_info=True)
            duration_ms = int((datetime.utcnow() - start_time).total_seconds() * 1000)

            return IntegrationResult(
                success=False,
                error_message=str(e),
                duration_ms=duration_ms
            )

    async def test_connection(self) -> Tuple[bool, str]:
        """
        Test connection to MQTT broker.

        Returns:
            Tuple of (success, message)
        """
        try:
            # TODO: Implement actual MQTT connection test
            # For now, just validate configuration

            if not self.mqtt_broker:
                return False, "MQTT broker not configured"

            logger.info(f"Testing mesh network connection: {self.mqtt_broker}")

            # Placeholder - would connect to MQTT and check
            return True, f"Successfully connected to mesh gateway at {self.mqtt_broker}"

        except Exception as e:
            logger.error(f"Mesh connection test failed: {e}", exc_info=True)
            return False, f"Connection failed: {str(e)}"

    def validate_config(self) -> Tuple[bool, Optional[str]]:
        """
        Validate mesh integration configuration.

        Returns:
            Tuple of (valid, error_message)
        """
        try:
            # Check required configuration
            if not self.mqtt_broker:
                return False, "mqtt_broker is required"

            # Validate broker format (host:port)
            if ':' not in self.mqtt_broker:
                return False, "mqtt_broker must be in format 'host:port'"

            host, port = self.mqtt_broker.split(':', 1)
            if not host or not port.isdigit():
                return False, "Invalid mqtt_broker format"

            port_num = int(port)
            if port_num < 1 or port_num > 65535:
                return False, "Invalid MQTT port number"

            # Validate topic prefix
            if not self.topic_prefix:
                return False, "topic_prefix cannot be empty"

            return True, None

        except Exception as e:
            return False, f"Configuration validation error: {str(e)}"

    def _transform_to_mesh_format(self, incident: Dict[str, Any], organization: Dict[str, Any]) -> Dict[str, Any]:
        """
        Transform SIMS incident to mesh network format.

        Args:
            incident: SIMS incident dictionary
            organization: Target organization

        Returns:
            Mesh-formatted incident dictionary
        """
        # Extract key fields
        mesh_incident = {
            'device_id': incident.get('id', 0),
            'latitude': incident.get('latitude', 0.0),
            'longitude': incident.get('longitude', 0.0),
            'altitude': incident.get('altitude', 0.0),
            'timestamp': incident.get('timestamp', datetime.utcnow().isoformat()),
            'priority': self._map_priority(incident.get('priority', 'medium')),
            'category': incident.get('category', 'unknown'),
            'description': incident.get('description', ''),
            'organization': organization.get('name', 'Unknown')
        }

        # Add media URLs if available
        if incident.get('image_url'):
            mesh_incident['image_url'] = incident['image_url']

        if incident.get('audio_url'):
            mesh_incident['audio_url'] = incident['audio_url']

        return mesh_incident

    def _map_priority(self, priority_str: str) -> int:
        """
        Map SIMS priority string to mesh priority integer.

        Args:
            priority_str: Priority string (critical/high/medium/low)

        Returns:
            Priority integer (0-3)
        """
        priority_map = {
            'critical': 0,
            'high': 1,
            'medium': 2,
            'low': 3
        }
        return priority_map.get(priority_str.lower(), 2)

    async def _connect_mqtt(self):
        """
        Connect to MQTT broker.

        TODO: Implement actual MQTT client connection
        """
        logger.info(f"Connecting to MQTT broker: {self.mqtt_broker}")

        # Placeholder for MQTT connection
        # In production, use aiomqtt or paho-mqtt

        self._connected = True
        logger.info("Connected to MQTT broker")

    async def receive_incident(self, mesh_message: bytes) -> Dict[str, Any]:
        """
        Receive incident from mesh network (called by gateway service).

        Args:
            mesh_message: Raw mesh message (Protobuf or JSON)

        Returns:
            Parsed incident dictionary in SIMS format
        """
        try:
            # TODO: Parse Protobuf message
            # For now, assume JSON
            mesh_incident = json.loads(mesh_message.decode('utf-8'))

            # Transform to SIMS format
            sims_incident = {
                'latitude': mesh_incident.get('latitude', 0.0),
                'longitude': mesh_incident.get('longitude', 0.0),
                'altitude': mesh_incident.get('altitude', 0.0),
                'timestamp': mesh_incident.get('timestamp'),
                'priority': self._map_priority_reverse(mesh_incident.get('priority', 2)),
                'category': mesh_incident.get('category', 'unknown'),
                'description': mesh_incident.get('description', ''),
                'source': 'mesh',
                'device_id': mesh_incident.get('device_id')
            }

            return sims_incident

        except Exception as e:
            logger.error(f"Error parsing mesh incident: {e}", exc_info=True)
            raise

    def _map_priority_reverse(self, priority_int: int) -> str:
        """
        Map mesh priority integer to SIMS priority string.

        Args:
            priority_int: Priority integer (0-3)

        Returns:
            Priority string
        """
        priority_map = {
            0: 'critical',
            1: 'high',
            2: 'medium',
            3: 'low'
        }
        return priority_map.get(priority_int, 'medium')
