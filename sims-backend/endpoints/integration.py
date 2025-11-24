"""
Integration Management API Endpoints

Provides REST API for managing integration templates and organization integrations.
"""
import logging
from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session

from db.connection import get_db
from models.integration_template_model import (
    IntegrationTemplateORM,
    IntegrationTemplateCreate,
    IntegrationTemplateUpdate,
    IntegrationTemplateResponse
)
from models.organization_integration_model import (
    OrganizationIntegrationORM,
    OrganizationIntegrationCreate,
    OrganizationIntegrationUpdate,
    OrganizationIntegrationResponse
)
from models.integration_delivery_model import (
    IntegrationDeliveryORM,
    IntegrationDeliveryResponse,
    IntegrationDeliveryListResponse
)
from services.integration_delivery_service import IntegrationDeliveryService

logger = logging.getLogger(__name__)

integration_router = APIRouter(prefix="/api/integration", tags=["integration"])


# ============================================================================
# INTEGRATION TEMPLATES (Admin only)
# ============================================================================

@integration_router.post("/template", response_model=IntegrationTemplateResponse, status_code=status.HTTP_201_CREATED)
async def create_integration_template(
    template: IntegrationTemplateCreate,
    db: Session = Depends(get_db)
):
    """Create a new integration template (admin only)"""
    try:
        # Check if template with same name exists
        existing = db.query(IntegrationTemplateORM).filter(
            IntegrationTemplateORM.name == template.name
        ).first()

        if existing:
            raise HTTPException(
                status_code=status.HTTP_409_CONFLICT,
                detail=f"Integration template with name '{template.name}' already exists"
            )

        # Create template
        db_template = IntegrationTemplateORM(
            name=template.name,
            type=template.type.value if hasattr(template.type, 'value') else template.type,
            description=template.description,
            config_schema=template.config_schema,
            payload_template=template.payload_template,
            auth_type=template.auth_type.value if hasattr(template.auth_type, 'value') else template.auth_type,
            auth_schema=template.auth_schema,
            timeout_seconds=template.timeout_seconds,
            retry_enabled=template.retry_enabled,
            retry_attempts=template.retry_attempts,
            active=template.active,
            created_by=template.created_by
        )

        db.add(db_template)
        db.commit()
        db.refresh(db_template)

        logger.info(f"Created integration template: {template.name}")

        return IntegrationTemplateResponse(
            id=db_template.id,
            name=db_template.name,
            type=db_template.type,
            description=db_template.description,
            config_schema=db_template.config_schema,
            payload_template=db_template.payload_template,
            auth_type=db_template.auth_type,
            auth_schema=db_template.auth_schema,
            timeout_seconds=db_template.timeout_seconds,
            retry_enabled=db_template.retry_enabled,
            retry_attempts=db_template.retry_attempts,
            active=db_template.active,
            system_template=db_template.system_template,
            created_at=db_template.created_at.isoformat(),
            updated_at=db_template.updated_at.isoformat(),
            created_by=db_template.created_by
        )

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error creating integration template: {e}", exc_info=True)
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to create integration template: {str(e)}"
        )


@integration_router.get("/template", response_model=List[IntegrationTemplateResponse])
async def list_integration_templates(
    active_only: bool = False,
    type: Optional[str] = None,
    db: Session = Depends(get_db)
):
    """List all integration templates"""
    try:
        query = db.query(IntegrationTemplateORM)

        if active_only:
            query = query.filter(IntegrationTemplateORM.active == True)

        if type:
            query = query.filter(IntegrationTemplateORM.type == type)

        templates = query.all()

        return [
            IntegrationTemplateResponse(
                id=t.id,
                name=t.name,
                type=t.type,
                description=t.description,
                config_schema=t.config_schema,
                payload_template=t.payload_template,
                auth_type=t.auth_type,
                auth_schema=t.auth_schema,
                timeout_seconds=t.timeout_seconds,
                retry_enabled=t.retry_enabled,
                retry_attempts=t.retry_attempts,
                active=t.active,
                system_template=t.system_template,
                created_at=t.created_at.isoformat(),
                updated_at=t.updated_at.isoformat(),
                created_by=t.created_by
            )
            for t in templates
        ]

    except Exception as e:
        logger.error(f"Error listing integration templates: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to list integration templates: {str(e)}"
        )


@integration_router.get("/template/{template_id}", response_model=IntegrationTemplateResponse)
async def get_integration_template(
    template_id: int,
    db: Session = Depends(get_db)
):
    """Get a specific integration template"""
    template = db.query(IntegrationTemplateORM).filter(
        IntegrationTemplateORM.id == template_id
    ).first()

    if not template:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Integration template {template_id} not found"
        )

    return IntegrationTemplateResponse(
        id=template.id,
        name=template.name,
        type=template.type,
        description=template.description,
        config_schema=template.config_schema,
        payload_template=template.payload_template,
        auth_type=template.auth_type,
        auth_schema=template.auth_schema,
        timeout_seconds=template.timeout_seconds,
        retry_enabled=template.retry_enabled,
        retry_attempts=template.retry_attempts,
        active=template.active,
        system_template=template.system_template,
        created_at=template.created_at.isoformat(),
        updated_at=template.updated_at.isoformat(),
        created_by=template.created_by
    )


@integration_router.put("/template/{template_id}", response_model=IntegrationTemplateResponse)
async def update_integration_template(
    template_id: int,
    template_update: IntegrationTemplateUpdate,
    db: Session = Depends(get_db)
):
    """Update an integration template (admin only)"""
    try:
        db_template = db.query(IntegrationTemplateORM).filter(
            IntegrationTemplateORM.id == template_id
        ).first()

        if not db_template:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Integration template {template_id} not found"
            )

        # Prevent modifying system templates
        if db_template.system_template:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail="Cannot modify system integration templates"
            )

        # Update fields
        update_data = template_update.model_dump(exclude_unset=True)
        for field, value in update_data.items():
            if hasattr(value, 'value'):
                value = value.value
            setattr(db_template, field, value)

        db.commit()
        db.refresh(db_template)

        logger.info(f"Updated integration template {template_id}")

        return IntegrationTemplateResponse(
            id=db_template.id,
            name=db_template.name,
            type=db_template.type,
            description=db_template.description,
            config_schema=db_template.config_schema,
            payload_template=db_template.payload_template,
            auth_type=db_template.auth_type,
            auth_schema=db_template.auth_schema,
            timeout_seconds=db_template.timeout_seconds,
            retry_enabled=db_template.retry_enabled,
            retry_attempts=db_template.retry_attempts,
            active=db_template.active,
            system_template=db_template.system_template,
            created_at=db_template.created_at.isoformat(),
            updated_at=db_template.updated_at.isoformat(),
            created_by=db_template.created_by
        )

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error updating integration template: {e}", exc_info=True)
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to update integration template: {str(e)}"
        )


@integration_router.delete("/template/{template_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_integration_template(
    template_id: int,
    db: Session = Depends(get_db)
):
    """Delete an integration template (admin only)"""
    try:
        db_template = db.query(IntegrationTemplateORM).filter(
            IntegrationTemplateORM.id == template_id
        ).first()

        if not db_template:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Integration template {template_id} not found"
            )

        # Prevent deleting system templates
        if db_template.system_template:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail="Cannot delete system integration templates"
            )

        # Check if any organizations are using this template
        usage_count = db.query(OrganizationIntegrationORM).filter(
            OrganizationIntegrationORM.template_id == template_id
        ).count()

        if usage_count > 0:
            raise HTTPException(
                status_code=status.HTTP_409_CONFLICT,
                detail=f"Cannot delete template: {usage_count} organization(s) are using it"
            )

        db.delete(db_template)
        db.commit()

        logger.info(f"Deleted integration template {template_id}")

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error deleting integration template: {e}", exc_info=True)
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to delete integration template: {str(e)}"
        )


# ============================================================================
# ORGANIZATION INTEGRATIONS
# ============================================================================

@integration_router.post("/organization", response_model=OrganizationIntegrationResponse, status_code=status.HTTP_201_CREATED)
async def create_organization_integration(
    integration: OrganizationIntegrationCreate,
    db: Session = Depends(get_db)
):
    """Create a new organization integration"""
    try:
        # Validate template exists
        template = db.query(IntegrationTemplateORM).filter(
            IntegrationTemplateORM.id == integration.template_id
        ).first()

        if not template:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Integration template {integration.template_id} not found"
            )

        # Create integration
        db_integration = OrganizationIntegrationORM(
            organization_id=integration.organization_id,
            template_id=integration.template_id,
            name=integration.name,
            description=integration.description,
            config=integration.config,
            auth_credentials=integration.auth_credentials,
            custom_payload_template=integration.custom_payload_template,
            trigger_filters=integration.trigger_filters,
            active=integration.active,
            created_by=integration.created_by
        )

        db.add(db_integration)
        db.commit()
        db.refresh(db_integration)

        logger.info(f"Created organization integration: {integration.name} for org {integration.organization_id}")

        return OrganizationIntegrationResponse(
            id=db_integration.id,
            organization_id=db_integration.organization_id,
            template_id=db_integration.template_id,
            template_name=template.name,
            template_type=template.type,
            name=db_integration.name,
            description=db_integration.description,
            config=db_integration.config,
            auth_credentials_configured=bool(db_integration.auth_credentials),
            custom_payload_template=db_integration.custom_payload_template,
            trigger_filters=db_integration.trigger_filters,
            active=db_integration.active,
            last_delivery_at=db_integration.last_delivery_at.isoformat() if db_integration.last_delivery_at else None,
            last_delivery_status=db_integration.last_delivery_status,
            created_at=db_integration.created_at.isoformat(),
            updated_at=db_integration.updated_at.isoformat(),
            created_by=db_integration.created_by
        )

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error creating organization integration: {e}", exc_info=True)
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to create organization integration: {str(e)}"
        )


@integration_router.get("/organization/{org_id}", response_model=List[OrganizationIntegrationResponse])
async def list_organization_integrations(
    org_id: int,
    active_only: bool = False,
    db: Session = Depends(get_db)
):
    """List all integrations for an organization"""
    try:
        query = db.query(OrganizationIntegrationORM).filter(
            OrganizationIntegrationORM.organization_id == org_id
        )

        if active_only:
            query = query.filter(OrganizationIntegrationORM.active == True)

        integrations = query.all()

        result = []
        for integration in integrations:
            template = db.query(IntegrationTemplateORM).filter(
                IntegrationTemplateORM.id == integration.template_id
            ).first()

            result.append(OrganizationIntegrationResponse(
                id=integration.id,
                organization_id=integration.organization_id,
                template_id=integration.template_id,
                template_name=template.name if template else None,
                template_type=template.type if template else None,
                name=integration.name,
                description=integration.description,
                config=integration.config,
                auth_credentials_configured=bool(integration.auth_credentials),
                custom_payload_template=integration.custom_payload_template,
                trigger_filters=integration.trigger_filters,
                active=integration.active,
                last_delivery_at=integration.last_delivery_at.isoformat() if integration.last_delivery_at else None,
                last_delivery_status=integration.last_delivery_status,
                created_at=integration.created_at.isoformat(),
                updated_at=integration.updated_at.isoformat(),
                created_by=integration.created_by
            ))

        return result

    except Exception as e:
        logger.error(f"Error listing organization integrations: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to list organization integrations: {str(e)}"
        )


@integration_router.get("/organization/{org_id}/{integration_id}", response_model=OrganizationIntegrationResponse)
async def get_organization_integration(
    org_id: int,
    integration_id: int,
    db: Session = Depends(get_db)
):
    """Get a specific organization integration"""
    integration = db.query(OrganizationIntegrationORM).filter(
        OrganizationIntegrationORM.id == integration_id,
        OrganizationIntegrationORM.organization_id == org_id
    ).first()

    if not integration:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Integration {integration_id} not found for organization {org_id}"
        )

    template = db.query(IntegrationTemplateORM).filter(
        IntegrationTemplateORM.id == integration.template_id
    ).first()

    return OrganizationIntegrationResponse(
        id=integration.id,
        organization_id=integration.organization_id,
        template_id=integration.template_id,
        template_name=template.name if template else None,
        template_type=template.type if template else None,
        name=integration.name,
        description=integration.description,
        config=integration.config,
        auth_credentials_configured=bool(integration.auth_credentials),
        custom_payload_template=integration.custom_payload_template,
        trigger_filters=integration.trigger_filters,
        active=integration.active,
        last_delivery_at=integration.last_delivery_at.isoformat() if integration.last_delivery_at else None,
        last_delivery_status=integration.last_delivery_status,
        created_at=integration.created_at.isoformat(),
        updated_at=integration.updated_at.isoformat(),
        created_by=integration.created_by
    )


@integration_router.put("/organization/{org_id}/{integration_id}", response_model=OrganizationIntegrationResponse)
async def update_organization_integration(
    org_id: int,
    integration_id: int,
    integration_update: OrganizationIntegrationUpdate,
    db: Session = Depends(get_db)
):
    """Update an organization integration"""
    try:
        db_integration = db.query(OrganizationIntegrationORM).filter(
            OrganizationIntegrationORM.id == integration_id,
            OrganizationIntegrationORM.organization_id == org_id
        ).first()

        if not db_integration:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Integration {integration_id} not found for organization {org_id}"
            )

        # Update fields
        update_data = integration_update.model_dump(exclude_unset=True)
        for field, value in update_data.items():
            setattr(db_integration, field, value)

        db.commit()
        db.refresh(db_integration)

        template = db.query(IntegrationTemplateORM).filter(
            IntegrationTemplateORM.id == db_integration.template_id
        ).first()

        logger.info(f"Updated organization integration {integration_id}")

        return OrganizationIntegrationResponse(
            id=db_integration.id,
            organization_id=db_integration.organization_id,
            template_id=db_integration.template_id,
            template_name=template.name if template else None,
            template_type=template.type if template else None,
            name=db_integration.name,
            description=db_integration.description,
            config=db_integration.config,
            auth_credentials_configured=bool(db_integration.auth_credentials),
            custom_payload_template=db_integration.custom_payload_template,
            trigger_filters=db_integration.trigger_filters,
            active=db_integration.active,
            last_delivery_at=db_integration.last_delivery_at.isoformat() if db_integration.last_delivery_at else None,
            last_delivery_status=db_integration.last_delivery_status,
            created_at=db_integration.created_at.isoformat(),
            updated_at=db_integration.updated_at.isoformat(),
            created_by=db_integration.created_by
        )

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error updating organization integration: {e}", exc_info=True)
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to update organization integration: {str(e)}"
        )


@integration_router.delete("/organization/{org_id}/{integration_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_organization_integration(
    org_id: int,
    integration_id: int,
    db: Session = Depends(get_db)
):
    """Delete an organization integration"""
    try:
        db_integration = db.query(OrganizationIntegrationORM).filter(
            OrganizationIntegrationORM.id == integration_id,
            OrganizationIntegrationORM.organization_id == org_id
        ).first()

        if not db_integration:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Integration {integration_id} not found for organization {org_id}"
            )

        db.delete(db_integration)
        db.commit()

        logger.info(f"Deleted organization integration {integration_id}")

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error deleting organization integration: {e}", exc_info=True)
        db.rollback()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to delete organization integration: {str(e)}"
        )


@integration_router.post("/organization/{org_id}/{integration_id}/test")
async def test_organization_integration(
    org_id: int,
    integration_id: int,
    db: Session = Depends(get_db)
):
    """Test an organization integration"""
    try:
        integration = db.query(OrganizationIntegrationORM).filter(
            OrganizationIntegrationORM.id == integration_id,
            OrganizationIntegrationORM.organization_id == org_id
        ).first()

        if not integration:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Integration {integration_id} not found for organization {org_id}"
            )

        success, message = await IntegrationDeliveryService.test_integration(db, integration_id)

        return {
            "success": success,
            "message": message
        }

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error testing integration: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to test integration: {str(e)}"
        )


# ============================================================================
# DELIVERY TRACKING
# ============================================================================

@integration_router.get("/delivery/incident/{incident_id}", response_model=IntegrationDeliveryListResponse)
async def list_incident_deliveries(
    incident_id: str,
    page: int = 1,
    page_size: int = 50,
    db: Session = Depends(get_db)
):
    """List all delivery attempts for an incident"""
    try:
        query = db.query(IntegrationDeliveryORM).filter(
            IntegrationDeliveryORM.incident_id == incident_id
        ).order_by(IntegrationDeliveryORM.started_at.desc())

        total = query.count()
        deliveries = query.offset((page - 1) * page_size).limit(page_size).all()

        return IntegrationDeliveryListResponse(
            deliveries=[
                IntegrationDeliveryResponse(
                    id=d.id,
                    incident_id=str(d.incident_id),
                    organization_id=d.organization_id,
                    integration_id=d.integration_id,
                    integration_type=d.integration_type,
                    integration_name=d.integration_name,
                    status=d.status,
                    attempt_number=d.attempt_number,
                    request_url=d.request_url,
                    response_code=d.response_code,
                    error_message=d.error_message,
                    started_at=d.started_at.isoformat(),
                    completed_at=d.completed_at.isoformat() if d.completed_at else None,
                    duration_ms=d.duration_ms,
                    delivery_metadata=d.delivery_metadata or {}
                )
                for d in deliveries
            ],
            total=total,
            page=page,
            page_size=page_size
        )

    except Exception as e:
        logger.error(f"Error listing incident deliveries: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to list incident deliveries: {str(e)}"
        )


@integration_router.get("/delivery/organization/{org_id}", response_model=IntegrationDeliveryListResponse)
async def list_organization_deliveries(
    org_id: int,
    status: Optional[str] = None,
    page: int = 1,
    page_size: int = 50,
    db: Session = Depends(get_db)
):
    """List all delivery attempts for an organization"""
    try:
        query = db.query(IntegrationDeliveryORM).filter(
            IntegrationDeliveryORM.organization_id == org_id
        )

        if status:
            query = query.filter(IntegrationDeliveryORM.status == status)

        query = query.order_by(IntegrationDeliveryORM.started_at.desc())

        total = query.count()
        deliveries = query.offset((page - 1) * page_size).limit(page_size).all()

        return IntegrationDeliveryListResponse(
            deliveries=[
                IntegrationDeliveryResponse(
                    id=d.id,
                    incident_id=str(d.incident_id),
                    organization_id=d.organization_id,
                    integration_id=d.integration_id,
                    integration_type=d.integration_type,
                    integration_name=d.integration_name,
                    status=d.status,
                    attempt_number=d.attempt_number,
                    request_url=d.request_url,
                    response_code=d.response_code,
                    error_message=d.error_message,
                    started_at=d.started_at.isoformat(),
                    completed_at=d.completed_at.isoformat() if d.completed_at else None,
                    duration_ms=d.duration_ms,
                    delivery_metadata=d.delivery_metadata or {}
                )
                for d in deliveries
            ],
            total=total,
            page=page,
            page_size=page_size
        )

    except Exception as e:
        logger.error(f"Error listing organization deliveries: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to list organization deliveries: {str(e)}"
        )
