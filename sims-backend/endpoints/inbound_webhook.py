"""
Inbound Webhook API Endpoints

Provides REST API for managing and receiving inbound webhooks.
"""
import logging
from typing import Dict, Any, List, Optional
from fastapi import APIRouter, Depends, HTTPException, status, Request, Header
from sqlalchemy.orm import Session
import jsonpath_ng
from datetime import datetime

from db.connection import get_db
from models.inbound_webhook_model import (
    InboundWebhookORM,
    InboundWebhookCreate,
    InboundWebhookUpdate,
    InboundWebhookResponse,
    InboundWebhookListResponse,
    generate_webhook_token,
    generate_auth_token
)
from models.incident_model import IncidentORM, IncidentCreate
from services.classification_service import IncidentClassifier
from services.assignment_service import AutoAssignmentService

logger = logging.getLogger(__name__)

inbound_webhook_router = APIRouter(prefix="/api/webhook", tags=["inbound_webhook"])


def _apply_field_mapping(payload: Dict[str, Any], field_mapping: Dict[str, str]) -> Dict[str, Any]:
    """
    Apply field mapping using JSONPath expressions.

    Args:
        payload: Incoming webhook payload
        field_mapping: Mapping of incident field to JSONPath expression

    Returns:
        Dictionary with mapped fields
    """
    result = {}

    for incident_field, jsonpath_expr in field_mapping.items():
        try:
            # Parse JSONPath expression
            jsonpath_expression = jsonpath_ng.parse(jsonpath_expr)
            matches = jsonpath_expression.find(payload)

            if matches:
                # Use first match
                result[incident_field] = matches[0].value
        except Exception as e:
            logger.warning(f"Error applying field mapping for {incident_field}: {e}")

    return result


# ============================================================================
# INBOUND WEBHOOK MANAGEMENT
# ============================================================================

@inbound_webhook_router.post("/inbound", response_model=InboundWebhookResponse, status_code=status.HTTP_201_CREATED)
async def create_inbound_webhook(
    webhook: InboundWebhookCreate,
    db: Session = Depends(get_db)
):
    """Create a new inbound webhook"""
    try:
        # Generate tokens
        webhook_token = generate_webhook_token()
        auth_token = generate_auth_token()

        # Create webhook
        db_webhook = InboundWebhookORM(
            name=webhook.name,
            description=webhook.description,
            webhook_token=webhook_token,
            auth_token=auth_token,
            source_name=webhook.source_name,
            source_type=webhook.source_type,
            field_mapping=webhook.field_mapping,
            default_values=webhook.default_values,
            auto_assign_to_org=webhook.auto_assign_to_org,
            allowed_ips=webhook.allowed_ips,
            active=webhook.active,
            created_by=webhook.created_by
        )

        db.add(db_webhook)
        db.commit()
        db.refresh(db_webhook)

        logger.info(f"Created inbound webhook: {webhook.name}")

        # Construct webhook URL
        webhook_url = f"/api/webhook/inbound/{webhook_token}"

        return InboundWebhookResponse(
            id=db_webhook.id,
            name=db_webhook.name,
            description=db_webhook.description,
            webhook_url=webhook_url,
            webhook_token=db_webhook.webhook_token,
            auth_token=db_webhook.auth_token,  # Only shown on creation
            source_name=db_webhook.source_name,
            source_type=db_webhook.source_type,
            field_mapping=db_webhook.field_mapping,
            default_values=db_webhook.default_values,
            auto_assign_to_org=db_webhook.auto_assign_to_org,
            allowed_ips=db_webhook.allowed_ips,
            active=db_webhook.active,
            total_received=db_webhook.total_received,
            last_received_at=db_webhook.last_received_at.isoformat() if db_webhook.last_received_at else None,
            created_at=db_webhook.created_at.isoformat(),
            updated_at=db_webhook.updated_at.isoformat(),
            created_by=db_webhook.created_by
        )

    except Exception as e:
        logger.error(f"Error creating inbound webhook: {e}", exc_info=True)
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to create inbound webhook: {str(e)}"
        )


@inbound_webhook_router.get("/inbound", response_model=List[InboundWebhookListResponse])
async def list_inbound_webhooks(
    active_only: bool = False,
    db: Session = Depends(get_db)
):
    """List all inbound webhooks"""
    try:
        query = db.query(InboundWebhookORM)

        if active_only:
            query = query.filter(InboundWebhookORM.active == True)

        webhooks = query.all()

        return [
            InboundWebhookListResponse(
                id=w.id,
                name=w.name,
                description=w.description,
                webhook_url=f"/api/webhook/inbound/{w.webhook_token}",
                source_name=w.source_name,
                source_type=w.source_type,
                active=w.active,
                total_received=w.total_received,
                last_received_at=w.last_received_at.isoformat() if w.last_received_at else None,
                created_at=w.created_at.isoformat()
            )
            for w in webhooks
        ]

    except Exception as e:
        logger.error(f"Error listing inbound webhooks: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to list inbound webhooks: {str(e)}"
        )


@inbound_webhook_router.get("/inbound/{webhook_id}", response_model=InboundWebhookResponse)
async def get_inbound_webhook(
    webhook_id: int,
    db: Session = Depends(get_db)
):
    """Get a specific inbound webhook (auth_token is hidden)"""
    webhook = db.query(InboundWebhookORM).filter(
        InboundWebhookORM.id == webhook_id
    ).first()

    if not webhook:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Inbound webhook {webhook_id} not found"
        )

    return InboundWebhookResponse(
        id=webhook.id,
        name=webhook.name,
        description=webhook.description,
        webhook_url=f"/api/webhook/inbound/{webhook.webhook_token}",
        webhook_token=webhook.webhook_token,
        auth_token="[HIDDEN]",  # Don't expose auth token in GET
        source_name=webhook.source_name,
        source_type=webhook.source_type,
        field_mapping=webhook.field_mapping,
        default_values=webhook.default_values,
        auto_assign_to_org=webhook.auto_assign_to_org,
        allowed_ips=webhook.allowed_ips,
        active=webhook.active,
        total_received=webhook.total_received,
        last_received_at=webhook.last_received_at.isoformat() if webhook.last_received_at else None,
        created_at=webhook.created_at.isoformat(),
        updated_at=webhook.updated_at.isoformat(),
        created_by=webhook.created_by
    )


@inbound_webhook_router.put("/inbound/{webhook_id}", response_model=InboundWebhookResponse)
async def update_inbound_webhook(
    webhook_id: int,
    webhook_update: InboundWebhookUpdate,
    db: Session = Depends(get_db)
):
    """Update an inbound webhook"""
    try:
        db_webhook = db.query(InboundWebhookORM).filter(
            InboundWebhookORM.id == webhook_id
        ).first()

        if not db_webhook:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Inbound webhook {webhook_id} not found"
            )

        # Update fields
        update_data = webhook_update.model_dump(exclude_unset=True)
        for field, value in update_data.items():
            setattr(db_webhook, field, value)

        db.commit()
        db.refresh(db_webhook)

        logger.info(f"Updated inbound webhook {webhook_id}")

        return InboundWebhookResponse(
            id=db_webhook.id,
            name=db_webhook.name,
            description=db_webhook.description,
            webhook_url=f"/api/webhook/inbound/{db_webhook.webhook_token}",
            webhook_token=db_webhook.webhook_token,
            auth_token="[HIDDEN]",
            source_name=db_webhook.source_name,
            source_type=db_webhook.source_type,
            field_mapping=db_webhook.field_mapping,
            default_values=db_webhook.default_values,
            auto_assign_to_org=db_webhook.auto_assign_to_org,
            allowed_ips=db_webhook.allowed_ips,
            active=db_webhook.active,
            total_received=db_webhook.total_received,
            last_received_at=db_webhook.last_received_at.isoformat() if db_webhook.last_received_at else None,
            created_at=db_webhook.created_at.isoformat(),
            updated_at=db_webhook.updated_at.isoformat(),
            created_by=db_webhook.created_by
        )

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error updating inbound webhook: {e}", exc_info=True)
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to update inbound webhook: {str(e)}"
        )


@inbound_webhook_router.delete("/inbound/{webhook_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_inbound_webhook(
    webhook_id: int,
    db: Session = Depends(get_db)
):
    """Delete an inbound webhook"""
    try:
        db_webhook = db.query(InboundWebhookORM).filter(
            InboundWebhookORM.id == webhook_id
        ).first()

        if not db_webhook:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Inbound webhook {webhook_id} not found"
            )

        db.delete(db_webhook)
        db.commit()

        logger.info(f"Deleted inbound webhook {webhook_id}")

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error deleting inbound webhook: {e}", exc_info=True)
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to delete inbound webhook: {str(e)}"
        )


# ============================================================================
# INBOUND WEBHOOK RECEIVER
# ============================================================================

@inbound_webhook_router.post("/inbound/{webhook_token}")
async def receive_webhook(
    webhook_token: str,
    request: Request,
    authorization: Optional[str] = Header(None),
    db: Session = Depends(get_db)
):
    """Receive an external webhook and create an incident"""
    try:
        # Find webhook by token
        webhook = db.query(InboundWebhookORM).filter(
            InboundWebhookORM.webhook_token == webhook_token
        ).first()

        if not webhook:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail="Webhook not found"
            )

        if not webhook.active:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail="Webhook is inactive"
            )

        # Verify authentication
        if authorization:
            # Support Bearer token format
            auth_token = authorization.replace("Bearer ", "").strip()
            if auth_token != webhook.auth_token:
                raise HTTPException(
                    status_code=status.HTTP_401_UNAUTHORIZED,
                    detail="Invalid authentication token"
                )
        else:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Missing authentication token"
            )

        # Check IP whitelist if configured
        if webhook.allowed_ips and len(webhook.allowed_ips) > 0:
            client_ip = request.client.host
            if client_ip not in webhook.allowed_ips:
                logger.warning(f"Blocked webhook request from unauthorized IP: {client_ip}")
                raise HTTPException(
                    status_code=status.HTTP_403_FORBIDDEN,
                    detail="IP address not allowed"
                )

        # Parse incoming payload
        payload = await request.json()

        logger.info(f"Received webhook for {webhook.name}: {payload}")

        # Apply field mapping
        mapped_fields = _apply_field_mapping(payload, webhook.field_mapping or {})

        # Merge with default values
        incident_data = {**webhook.default_values, **mapped_fields}

        # Ensure required fields
        if 'title' not in incident_data or not incident_data['title']:
            incident_data['title'] = f"Incident from {webhook.source_name or 'external source'}"

        if 'description' not in incident_data or not incident_data['description']:
            incident_data['description'] = "Incident received via webhook"

        # Create incident
        incident_create = IncidentCreate(
            title=incident_data.get('title'),
            description=incident_data.get('description'),
            latitude=incident_data.get('latitude'),
            longitude=incident_data.get('longitude'),
            heading=incident_data.get('heading'),
            user_phone=incident_data.get('user_phone'),
            metadata={
                'source': 'inbound_webhook',
                'webhook_id': webhook.id,
                'webhook_name': webhook.name,
                'original_payload': payload
            }
        )

        # Generate incident ID
        import uuid
        import random
        incident_id = f"INC-{random.randint(10000000, 99999999):08X}"[:13]

        # Create incident ORM object
        from geoalchemy2.elements import WKTElement
        incident = IncidentORM(
            id=uuid.uuid4(),
            incident_id=incident_id,
            title=incident_create.title,
            description=incident_create.description,
            user_phone=incident_create.user_phone,
            latitude=incident_create.latitude,
            longitude=incident_create.longitude,
            heading=incident_create.heading,
            location=WKTElement(
                f'POINT({incident_create.longitude} {incident_create.latitude})',
                srid=4326
            ) if incident_create.latitude and incident_create.longitude else None,
            status='processing',
            priority='medium',
            category='Unclassified',
            meta_data=incident_create.metadata
        )

        db.add(incident)

        # Update webhook statistics
        webhook.total_received += 1
        webhook.last_received_at = datetime.utcnow()

        db.commit()
        db.refresh(incident)

        logger.info(f"Created incident {incident.incident_id} from webhook {webhook.name}")

        # Trigger classification (async, don't wait)
        try:
            from services.classification_service import get_classifier
            classifier = get_classifier()
            await classifier.classify_incident(db, incident)
        except Exception as e:
            logger.error(f"Error in async classification: {e}", exc_info=True)

        # Auto-assign if configured
        if webhook.auto_assign_to_org:
            try:
                from models.organization_model import OrganizationORM
                org = db.query(OrganizationORM).filter(
                    OrganizationORM.id == webhook.auto_assign_to_org
                ).first()

                if org:
                    incident.routed_to = org.id
                    incident.status = 'in_progress'
                    db.commit()

                    logger.info(f"Auto-assigned incident {incident.incident_id} to organization {org.id}")

                    # Trigger integrations
                    from services.integration_delivery_service import IntegrationDeliveryService
                    await IntegrationDeliveryService.deliver_incident(db, incident, org)

            except Exception as e:
                logger.error(f"Error in auto-assignment: {e}", exc_info=True)

        return {
            "success": True,
            "incident_id": incident.incident_id,
            "message": "Incident created successfully"
        }

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error processing webhook: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to process webhook: {str(e)}"
        )
