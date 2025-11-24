"""
Integration Delivery Service

Orchestrates delivery of incidents to external systems via configured integrations.
Handles plugin loading, delivery execution, and delivery tracking.
"""
import logging
from typing import Dict, Any, List, Optional
from datetime import datetime
from sqlalchemy.orm import Session

from models.incident_model import IncidentORM
from models.organization_model import OrganizationORM
from models.organization_integration_model import OrganizationIntegrationORM
from models.integration_template_model import IntegrationTemplateORM
from models.integration_delivery_model import IntegrationDeliveryORM, DeliveryStatus
from plugins.integration_registry import IntegrationRegistry
from plugins.base_integration import IntegrationResult

logger = logging.getLogger(__name__)


class IntegrationDeliveryService:
    """Service for delivering incidents via configured integrations"""

    @staticmethod
    def _incident_to_dict(incident: IncidentORM) -> Dict[str, Any]:
        """
        Convert IncidentORM to dictionary for plugin consumption.

        Args:
            incident: IncidentORM instance

        Returns:
            Dictionary representation of incident
        """
        return {
            'id': str(incident.id),
            'incident_id': incident.incident_id,
            'title': incident.title,
            'description': incident.description,
            'priority': incident.priority,
            'status': incident.status,
            'category': incident.category,
            'latitude': incident.latitude,
            'longitude': incident.longitude,
            'heading': incident.heading,
            'user_phone': incident.user_phone,
            'tags': incident.tags or [],
            'created_at': incident.created_at.isoformat() if incident.created_at else None,
            'updated_at': incident.updated_at.isoformat() if incident.updated_at else None,
            'metadata': incident.meta_data or {},
            # Media URLs would be added by caller if needed
            'image_url': None,
            'video_url': None,
            'audio_url': None,
            'audio_transcript': None
        }

    @staticmethod
    def _organization_to_dict(organization: OrganizationORM) -> Dict[str, Any]:
        """
        Convert OrganizationORM to dictionary for plugin consumption.

        Args:
            organization: OrganizationORM instance

        Returns:
            Dictionary representation of organization
        """
        return {
            'id': organization.id,
            'name': organization.name,
            'short_name': organization.short_name,
            'type': organization.type,
            'email': organization.email,
            'phone': organization.phone,
            'emergency_phone': organization.emergency_phone
        }

    @staticmethod
    def _check_trigger_filters(
        incident: Dict[str, Any],
        trigger_filters: Dict[str, Any]
    ) -> bool:
        """
        Check if incident matches trigger filters.

        Args:
            incident: Incident data dictionary
            trigger_filters: Filter configuration

        Returns:
            True if incident matches filters, False otherwise
        """
        if not trigger_filters:
            return True

        # Check priority filter
        if 'priorities' in trigger_filters:
            allowed_priorities = trigger_filters['priorities']
            if incident.get('priority') not in allowed_priorities:
                return False

        # Check category filter
        if 'categories' in trigger_filters:
            allowed_categories = trigger_filters['categories']
            if incident.get('category') not in allowed_categories:
                return False

        # Check status filter
        if 'statuses' in trigger_filters:
            allowed_statuses = trigger_filters['statuses']
            if incident.get('status') not in allowed_statuses:
                return False

        return True

    @classmethod
    async def deliver_incident(
        cls,
        db: Session,
        incident: IncidentORM,
        organization: OrganizationORM
    ) -> List[IntegrationDeliveryORM]:
        """
        Deliver incident to all active integrations for an organization.

        Args:
            db: Database session
            incident: Incident to deliver
            organization: Target organization

        Returns:
            List of IntegrationDeliveryORM records
        """
        deliveries = []

        try:
            # Get all active integrations for organization
            integrations = db.query(OrganizationIntegrationORM).filter(
                OrganizationIntegrationORM.organization_id == organization.id,
                OrganizationIntegrationORM.active == True
            ).all()

            if not integrations:
                logger.info(f"No active integrations for organization {organization.id}")
                return deliveries

            # Convert incident and organization to dictionaries
            incident_dict = cls._incident_to_dict(incident)
            org_dict = cls._organization_to_dict(organization)

            logger.info(
                f"Processing {len(integrations)} integration(s) for incident {incident.incident_id} "
                f"to organization {organization.name}"
            )

            # Process each integration
            for integration in integrations:
                try:
                    # Load template
                    template = db.query(IntegrationTemplateORM).filter(
                        IntegrationTemplateORM.id == integration.template_id
                    ).first()

                    if not template or not template.active:
                        logger.warning(f"Integration template {integration.template_id} not found or inactive")
                        continue

                    # Check trigger filters
                    if not cls._check_trigger_filters(incident_dict, integration.trigger_filters or {}):
                        logger.debug(
                            f"Incident {incident.incident_id} does not match trigger filters "
                            f"for integration {integration.id}"
                        )
                        continue

                    # Create delivery record
                    delivery = IntegrationDeliveryORM(
                        incident_id=incident.id,
                        organization_id=organization.id,
                        integration_id=integration.id,
                        integration_type=template.type,
                        integration_name=integration.name,
                        status=DeliveryStatus.PENDING,
                        started_at=datetime.utcnow()
                    )
                    db.add(delivery)
                    db.flush()  # Get delivery ID

                    # Get plugin from registry
                    plugin = IntegrationRegistry.get_plugin(
                        template.type,
                        integration.config or {},
                        integration.auth_credentials or {}
                    )

                    if not plugin:
                        error_msg = f"No plugin registered for integration type: {template.type}"
                        logger.error(error_msg)
                        delivery.status = DeliveryStatus.FAILED
                        delivery.error_message = error_msg
                        delivery.completed_at = datetime.utcnow()
                        db.commit()
                        deliveries.append(delivery)
                        continue

                    # Determine payload template
                    payload_template = integration.custom_payload_template or template.payload_template

                    # Send via plugin
                    logger.info(f"Sending via {template.type} plugin for integration {integration.id}")
                    result: IntegrationResult = await plugin.send(
                        incident_dict,
                        org_dict,
                        payload_template
                    )

                    # Update delivery record with result
                    delivery.status = DeliveryStatus.SUCCESS if result.success else DeliveryStatus.FAILED
                    delivery.response_code = result.status_code
                    delivery.response_body = result.response_body
                    delivery.error_message = result.error_message
                    delivery.request_url = result.request_url
                    delivery.request_payload = result.request_payload
                    delivery.duration_ms = result.duration_ms
                    delivery.completed_at = datetime.utcnow()

                    # Update integration last delivery status
                    integration.last_delivery_at = datetime.utcnow()
                    integration.last_delivery_status = 'success' if result.success else 'failed'

                    db.commit()
                    deliveries.append(delivery)

                    if result.success:
                        logger.info(
                            f"Successfully delivered incident {incident.incident_id} "
                            f"via integration {integration.id} ({template.type})"
                        )
                    else:
                        logger.error(
                            f"Failed to deliver incident {incident.incident_id} "
                            f"via integration {integration.id}: {result.error_message}"
                        )

                except Exception as e:
                    logger.error(
                        f"Error processing integration {integration.id}: {e}",
                        exc_info=True
                    )
                    # Update delivery record if it exists
                    if 'delivery' in locals():
                        delivery.status = DeliveryStatus.FAILED
                        delivery.error_message = f"Internal error: {str(e)}"
                        delivery.completed_at = datetime.utcnow()
                        db.commit()
                        deliveries.append(delivery)

            return deliveries

        except Exception as e:
            logger.error(f"Error in deliver_incident: {e}", exc_info=True)
            return deliveries

    @classmethod
    async def test_integration(
        cls,
        db: Session,
        integration_id: int
    ) -> tuple[bool, str]:
        """
        Test an integration configuration.

        Args:
            db: Database session
            integration_id: Integration ID to test

        Returns:
            Tuple of (success: bool, message: str)
        """
        try:
            # Load integration
            integration = db.query(OrganizationIntegrationORM).filter(
                OrganizationIntegrationORM.id == integration_id
            ).first()

            if not integration:
                return False, f"Integration {integration_id} not found"

            # Load template
            template = db.query(IntegrationTemplateORM).filter(
                IntegrationTemplateORM.id == integration.template_id
            ).first()

            if not template:
                return False, f"Integration template not found"

            # Get plugin
            plugin = IntegrationRegistry.get_plugin(
                template.type,
                integration.config or {},
                integration.auth_credentials or {}
            )

            if not plugin:
                return False, f"No plugin registered for integration type: {template.type}"

            # Validate configuration
            valid, error_msg = plugin.validate_config()
            if not valid:
                return False, f"Configuration invalid: {error_msg}"

            # Test connection
            success, message = await plugin.test_connection()
            return success, message

        except Exception as e:
            logger.error(f"Error testing integration {integration_id}: {e}", exc_info=True)
            return False, f"Error testing integration: {str(e)}"
