"""
Mesh Gateway Service

Bridges LoRa mesh network to SIMS backend via MQTT.
Receives incidents from mesh devices and forwards to backend database.
Forwards backend incidents to mesh network.
"""
import asyncio
import logging
import json
from typing import Optional, Dict, Any
from datetime import datetime

logger = logging.getLogger(__name__)


class MeshGatewayService:
    """
    Service that bridges mesh network to SIMS backend.

    Responsibilities:
    - Subscribe to MQTT topics for mesh network messages
    - Parse incoming mesh incidents (Protobuf/JSON)
    - Store incidents in database via incident service
    - Forward backend incidents to mesh network
    - Monitor mesh network health and status
    """

    def __init__(self, config: Dict[str, Any]):
        """
        Initialize mesh gateway service.

        Args:
            config: Configuration dictionary from config.yaml
        """
        self.config = config
        self.mqtt_broker = config.get('mqtt_broker', 'localhost:1883')
        self.topic_prefix = config.get('topic_prefix', 'sims/mesh')

        self.mqtt_client = None
        self.running = False
        self.connected_nodes = {}  # device_id -> last_seen timestamp

    async def start(self):
        """Start the mesh gateway service."""
        logger.info("Starting mesh gateway service...")

        try:
            # Connect to MQTT broker
            await self._connect_mqtt()

            # Subscribe to mesh topics
            await self._subscribe_topics()

            self.running = True
            logger.info(f"Mesh gateway service started (broker: {self.mqtt_broker})")

            # Start message processing loop
            await self._process_messages()

        except Exception as e:
            logger.error(f"Failed to start mesh gateway service: {e}", exc_info=True)
            raise

    async def stop(self):
        """Stop the mesh gateway service."""
        logger.info("Stopping mesh gateway service...")
        self.running = False

        if self.mqtt_client:
            # TODO: Disconnect MQTT client
            pass

        logger.info("Mesh gateway service stopped")

    async def _connect_mqtt(self):
        """Connect to MQTT broker."""
        logger.info(f"Connecting to MQTT broker: {self.mqtt_broker}")

        # TODO: Implement actual MQTT connection using aiomqtt
        # import aiomqtt
        # self.mqtt_client = aiomqtt.Client(self.mqtt_broker)
        # await self.mqtt_client.connect()

        logger.info("Connected to MQTT broker")

    async def _subscribe_topics(self):
        """Subscribe to mesh network MQTT topics."""
        topics = [
            f"{self.topic_prefix}/incidents/in",  # Incidents from mesh
            f"{self.topic_prefix}/status",         # Network status
            f"{self.topic_prefix}/nodes",          # Node heartbeats
        ]

        for topic in topics:
            logger.info(f"Subscribing to topic: {topic}")
            # TODO: self.mqtt_client.subscribe(topic)

    async def _process_messages(self):
        """Process incoming MQTT messages."""
        logger.info("Starting message processing loop...")

        while self.running:
            try:
                # TODO: Implement actual MQTT message processing
                # async for message in self.mqtt_client.messages:
                #     await self._handle_message(message)

                # Placeholder - sleep to prevent busy loop
                await asyncio.sleep(1)

            except Exception as e:
                logger.error(f"Error processing mesh message: {e}", exc_info=True)
                await asyncio.sleep(5)  # Back off on error

    async def _handle_message(self, message):
        """
        Handle incoming MQTT message.

        Args:
            message: MQTT message object
        """
        topic = message.topic
        payload = message.payload

        logger.debug(f"Received message: topic={topic}, size={len(payload)} bytes")

        try:
            if f"{self.topic_prefix}/incidents/in" in topic:
                await self._handle_incident(payload)
            elif f"{self.topic_prefix}/nodes" in topic:
                await self._handle_node_heartbeat(payload)
            elif f"{self.topic_prefix}/status" in topic:
                await self._handle_status(payload)
            else:
                logger.warning(f"Unknown topic: {topic}")

        except Exception as e:
            logger.error(f"Error handling message on topic {topic}: {e}", exc_info=True)

    async def _handle_incident(self, payload: bytes):
        """
        Handle incoming incident from mesh network.

        Args:
            payload: Raw incident data (Protobuf or JSON)
        """
        try:
            # Parse incident (TODO: use Protobuf)
            incident_data = json.loads(payload.decode('utf-8'))

            logger.info(f"Received mesh incident: device_id={incident_data.get('device_id')}, "
                       f"lat={incident_data.get('latitude')}, lon={incident_data.get('longitude')}")

            # Transform to SIMS format
            sims_incident = {
                'latitude': incident_data.get('latitude', 0.0),
                'longitude': incident_data.get('longitude', 0.0),
                'altitude': incident_data.get('altitude', 0.0),
                'timestamp': incident_data.get('timestamp', datetime.utcnow().isoformat()),
                'priority': self._map_priority(incident_data.get('priority', 2)),
                'category': incident_data.get('category', 'unknown'),
                'description': incident_data.get('description', ''),
                'source': 'mesh_network',
                'device_id': incident_data.get('device_id'),
                'raw_data': incident_data
            }

            # TODO: Store in database
            # incident_id = await incident_service.create_incident(sims_incident)
            # logger.info(f"Stored mesh incident: id={incident_id}")

            # TODO: Forward to integrations (Hydris, SEDAP, etc.)
            # await integration_service.forward_incident(incident_id)

            logger.info(f"Processed mesh incident from device {incident_data.get('device_id')}")

        except Exception as e:
            logger.error(f"Error handling mesh incident: {e}", exc_info=True)

    async def _handle_node_heartbeat(self, payload: bytes):
        """
        Handle node heartbeat message.

        Args:
            payload: Heartbeat data
        """
        try:
            heartbeat = json.loads(payload.decode('utf-8'))
            device_id = heartbeat.get('device_id')

            # Update connected nodes list
            self.connected_nodes[device_id] = datetime.utcnow()

            logger.debug(f"Heartbeat from device {device_id}, "
                        f"total connected nodes: {len(self.connected_nodes)}")

        except Exception as e:
            logger.error(f"Error handling heartbeat: {e}", exc_info=True)

    async def _handle_status(self, payload: bytes):
        """
        Handle mesh network status message.

        Args:
            payload: Status data
        """
        try:
            status = json.loads(payload.decode('utf-8'))
            logger.info(f"Mesh network status: {status}")

            # TODO: Update dashboard with network health

        except Exception as e:
            logger.error(f"Error handling status: {e}", exc_info=True)

    async def forward_to_mesh(self, incident: Dict[str, Any]):
        """
        Forward incident from backend to mesh network.

        Args:
            incident: SIMS incident dictionary
        """
        try:
            # Transform to mesh format
            mesh_incident = {
                'device_id': 0,  # Backend origin
                'latitude': incident.get('latitude', 0.0),
                'longitude': incident.get('longitude', 0.0),
                'altitude': incident.get('altitude', 0.0),
                'timestamp': incident.get('timestamp', datetime.utcnow().isoformat()),
                'priority': self._map_priority_to_int(incident.get('priority', 'medium')),
                'category': incident.get('category', 'unknown'),
                'description': incident.get('description', '')
            }

            # Publish to MQTT
            topic = f"{self.topic_prefix}/incidents/out"
            payload = json.dumps(mesh_incident).encode('utf-8')

            # TODO: self.mqtt_client.publish(topic, payload)

            logger.info(f"Forwarded incident to mesh network: {incident.get('id')}")

        except Exception as e:
            logger.error(f"Error forwarding to mesh: {e}", exc_info=True)

    def get_connected_nodes(self) -> int:
        """
        Get number of currently connected mesh nodes.

        Returns:
            Number of connected nodes
        """
        # Clean up stale nodes (no heartbeat in 5 minutes)
        now = datetime.utcnow()
        stale_nodes = [
            device_id for device_id, last_seen in self.connected_nodes.items()
            if (now - last_seen).total_seconds() > 300
        ]

        for device_id in stale_nodes:
            del self.connected_nodes[device_id]

        return len(self.connected_nodes)

    def get_network_status(self) -> Dict[str, Any]:
        """
        Get mesh network status.

        Returns:
            Status dictionary
        """
        return {
            'connected_nodes': self.get_connected_nodes(),
            'mqtt_broker': self.mqtt_broker,
            'running': self.running,
            'nodes': list(self.connected_nodes.keys())
        }

    def _map_priority(self, priority_int: int) -> str:
        """Map mesh priority int to SIMS priority string."""
        priority_map = {
            0: 'critical',
            1: 'high',
            2: 'medium',
            3: 'low'
        }
        return priority_map.get(priority_int, 'medium')

    def _map_priority_to_int(self, priority_str: str) -> int:
        """Map SIMS priority string to mesh priority int."""
        priority_map = {
            'critical': 0,
            'high': 1,
            'medium': 2,
            'low': 3
        }
        return priority_map.get(priority_str.lower(), 2)


# Singleton instance
_mesh_gateway_instance: Optional[MeshGatewayService] = None


async def get_mesh_gateway() -> MeshGatewayService:
    """
    Get mesh gateway service instance.

    Returns:
        MeshGatewayService instance
    """
    global _mesh_gateway_instance

    if _mesh_gateway_instance is None:
        # TODO: Load config from config.yaml
        config = {
            'mqtt_broker': 'localhost:1883',
            'topic_prefix': 'sims/mesh'
        }
        _mesh_gateway_instance = MeshGatewayService(config)
        await _mesh_gateway_instance.start()

    return _mesh_gateway_instance
