"""
WebSocket Manager for SIMS
Handles real-time communication with mobile app and operator dashboard
"""
import asyncio
from collections import defaultdict
from typing import Set, Dict, List
from uuid import UUID
from fastapi import WebSocket, WebSocketDisconnect
from datetime import datetime
import json
import logging

logger = logging.getLogger(__name__)


class WebSocketManager:
    def __init__(self):
        # Active connections by client ID
        self.active_connections: Dict[str, WebSocket] = {}
        # Topic subscriptions by client ID
        self.subscriptions: Dict[str, Set[str]] = {}

    async def connect(self, websocket: WebSocket, client_id: str):
        """Connect a new WebSocket client"""
        self.active_connections[client_id] = websocket
        # Default subscriptions for SIMS clients
        self.subscriptions[client_id] = set(['incidents'])
        logger.info(f"Client connected: {client_id}")

    async def disconnect(self, client_id: str):
        """Disconnect and cleanup a WebSocket client"""
        if client_id in self.active_connections:
            connection = self.active_connections[client_id]

            # Remove from tracking dicts first
            self.active_connections.pop(client_id, None)
            self.subscriptions.pop(client_id, None)

            try:
                await connection.close()
                logger.info(f"Client disconnected: {client_id}")
            except Exception as e:
                logger.error(f"Error closing connection for {client_id}: {e}")

    async def subscribe(self, client_id: str, topic: str):
        """Subscribe client to a topic"""
        if client_id in self.subscriptions:
            self.subscriptions[client_id].add(topic)
            logger.info(f"Client {client_id} subscribed to {topic}")

    async def unsubscribe(self, client_id: str, topic: str):
        """Unsubscribe client from a topic"""
        if client_id in self.subscriptions:
            self.subscriptions[client_id].discard(topic)
            logger.info(f"Client {client_id} unsubscribed from {topic}")

    def _serialize_value(self, value):
        """Serialize datetime and UUID objects to JSON-compatible types"""
        if isinstance(value, datetime):
            return value.isoformat()
        elif isinstance(value, UUID):
            return str(value)
        elif isinstance(value, dict):
            return {k: self._serialize_value(v) for k, v in value.items()}
        elif isinstance(value, list):
            return [self._serialize_value(v) for v in value]
        return value

    async def broadcast_to_client(self, client_id: str, message: dict):
        """Send message to specific client"""
        if client_id in self.active_connections:
            try:
                # Serialize datetime and UUID objects
                serialized_message = self._serialize_value(message)
                await self.active_connections[client_id].send_json(serialized_message)
            except Exception as e:
                logger.error(f"Error sending message to {client_id}: {e}")
                await self.disconnect(client_id)

    async def broadcast_to_all(self, message: dict, topic: str = None):
        """Broadcast message to all connected clients (optionally filtered by topic)"""
        disconnected_clients = []

        for client_id, connection in self.active_connections.items():
            # If topic is specified, only send to subscribed clients
            if topic and topic not in self.subscriptions.get(client_id, set()):
                continue

            try:
                serialized_message = self._serialize_value(message)
                await connection.send_json(serialized_message)
            except Exception as e:
                logger.error(f"Error broadcasting to {client_id}: {e}")
                disconnected_clients.append(client_id)

        # Cleanup disconnected clients
        for client_id in disconnected_clients:
            await self.disconnect(client_id)

    async def broadcast_incident(self, incident_data: dict, event_type: str = 'incident_new'):
        """Broadcast incident updates to subscribed clients"""
        message = {
            'type': event_type,
            'timestamp': datetime.utcnow().isoformat(),
            'data': incident_data
        }
        await self.broadcast_to_all(message, topic='incidents')

    async def broadcast_stats(self, stats_data: dict):
        """Broadcast statistics to subscribed clients"""
        message = {
            'type': 'stats',
            'timestamp': datetime.utcnow().isoformat(),
            'data': stats_data
        }
        await self.broadcast_to_all(message, topic='stats')

    async def broadcast(self, message: dict, topic: str = None):
        """
        Generic broadcast method that sends a message to clients subscribed to a topic.

        This is a convenience wrapper around broadcast_to_all for cleaner API usage.
        Supports organization-specific topics like 'org_1', 'org_2', etc.

        Args:
            message: Dictionary message to send
            topic: Optional topic filter (e.g., 'incidents', 'org_1', 'stats')

        Example:
            await websocket_manager.broadcast({'type': 'update'}, topic='org_5')
        """
        await self.broadcast_to_all(message, topic=topic)


# Global singleton instance
websocket_manager = WebSocketManager()
