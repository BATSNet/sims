"""
Organization API Endpoints
Handles organization CRUD operations
"""
import logging
import secrets
from typing import List, Optional
from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, status, Query
from sqlalchemy.orm import Session
from geoalchemy2.elements import WKTElement
from geoalchemy2 import functions as geo_func

from db.connection import get_db
from models.organization_model import (
    OrganizationORM,
    OrganizationCreate,
    OrganizationResponse,
    OrganizationUpdate
)
from models.organization_token_model import (
    OrganizationTokenORM,
    OrganizationTokenCreate,
    OrganizationTokenResponse
)
from auth.responder_auth import hash_token

logger = logging.getLogger(__name__)

organization_router = APIRouter(prefix="/api/organization", tags=["organization"])


@organization_router.post("/", response_model=OrganizationResponse, status_code=status.HTTP_201_CREATED)
async def create_organization(
    org_data: OrganizationCreate,
    db: Session = Depends(get_db)
):
    """Create a new organization"""
    try:
        # Create PostGIS point if lat/lon provided
        location_geom = None
        if org_data.latitude is not None and org_data.longitude is not None:
            location_geom = WKTElement(
                f'POINT({org_data.longitude} {org_data.latitude})',
                srid=4326
            )

        # Create organization ORM object
        db_org = OrganizationORM(
            name=org_data.name,
            short_name=org_data.short_name,
            type=org_data.type,
            contact_person=org_data.contact_person,
            phone=org_data.phone,
            email=org_data.email,
            emergency_phone=org_data.emergency_phone,
            address=org_data.address,
            city=org_data.city,
            country=org_data.country,
            location=location_geom,
            capabilities=org_data.capabilities or [],
            response_area=org_data.response_area,
            active=org_data.active,
            notes=org_data.notes
        )

        db.add(db_org)
        db.commit()
        db.refresh(db_org)

        logger.info(f"Created organization: {org_data.name}")

        return _org_to_response(db_org)

    except Exception as e:
        db.rollback()
        logger.error(f"Error creating organization: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to create organization: {str(e)}"
        )


@organization_router.get("/", response_model=List[OrganizationResponse])
async def list_organizations(
    skip: int = 0,
    limit: int = 100,
    type_filter: Optional[str] = Query(None, alias="type"),
    active_only: bool = True,
    search: Optional[str] = None,
    db: Session = Depends(get_db)
):
    """List all organizations with optional filtering"""
    try:
        query = db.query(OrganizationORM)

        if active_only:
            query = query.filter(OrganizationORM.active == True)

        if type_filter:
            query = query.filter(OrganizationORM.type == type_filter)

        if search:
            search_pattern = f"%{search}%"
            query = query.filter(
                (OrganizationORM.name.ilike(search_pattern)) |
                (OrganizationORM.short_name.ilike(search_pattern)) |
                (OrganizationORM.city.ilike(search_pattern))
            )

        orgs = query.order_by(
            OrganizationORM.name
        ).offset(skip).limit(limit).all()

        return [_org_to_response(org) for org in orgs]

    except Exception as e:
        logger.error(f"Error listing organizations: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to list organizations: {str(e)}"
        )


@organization_router.get("/{org_id}", response_model=OrganizationResponse)
async def get_organization(
    org_id: int,
    db: Session = Depends(get_db)
):
    """Get organization by ID"""
    try:
        org = db.query(OrganizationORM).filter(
            OrganizationORM.id == org_id
        ).first()

        if not org:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Organization {org_id} not found"
            )

        return _org_to_response(org)

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error retrieving organization: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to retrieve organization: {str(e)}"
        )


@organization_router.put("/{org_id}", response_model=OrganizationResponse)
async def update_organization(
    org_id: int,
    update_data: OrganizationUpdate,
    db: Session = Depends(get_db)
):
    """Update an organization"""
    try:
        org = db.query(OrganizationORM).filter(
            OrganizationORM.id == org_id
        ).first()

        if not org:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Organization {org_id} not found"
            )

        # Update fields
        update_dict = update_data.dict(exclude_unset=True)

        # Handle location separately
        if 'latitude' in update_dict or 'longitude' in update_dict:
            lat = update_dict.pop('latitude', None)
            lon = update_dict.pop('longitude', None)
            if lat is not None and lon is not None:
                org.location = WKTElement(f'POINT({lon} {lat})', srid=4326)

        for field, value in update_dict.items():
            if hasattr(org, field):
                setattr(org, field, value)

        org.updated_at = datetime.utcnow()

        db.commit()
        db.refresh(org)

        logger.info(f"Updated organization {org_id}")

        return _org_to_response(org)

    except HTTPException:
        raise
    except Exception as e:
        db.rollback()
        logger.error(f"Error updating organization: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to update organization: {str(e)}"
        )


@organization_router.delete("/{org_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_organization(
    org_id: int,
    db: Session = Depends(get_db)
):
    """Delete an organization"""
    try:
        org = db.query(OrganizationORM).filter(
            OrganizationORM.id == org_id
        ).first()

        if not org:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Organization {org_id} not found"
            )

        db.delete(org)
        db.commit()

        logger.info(f"Deleted organization {org_id}")

    except HTTPException:
        raise
    except Exception as e:
        db.rollback()
        logger.error(f"Error deleting organization: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to delete organization: {str(e)}"
        )


@organization_router.post("/{org_id}/token", response_model=OrganizationTokenResponse, status_code=status.HTTP_201_CREATED)
async def create_organization_token(
    org_id: int,
    db: Session = Depends(get_db),
    token_data: OrganizationTokenCreate = None
):
    """
    Generate a new access token for an organization.

    The token is used for responder portal access and BMS/SEDAP integration.
    Returns both the plain token (only shown once) and the hashed token record.
    """
    try:
        # Verify organization exists
        org = db.query(OrganizationORM).filter(
            OrganizationORM.id == org_id
        ).first()

        if not org:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Organization {org_id} not found"
            )

        # Generate secure random token
        plain_token = secrets.token_urlsafe(32)
        token_hash = hash_token(plain_token)

        # Get values from token_data or use defaults
        created_by = token_data.created_by if token_data else "dashboard"
        expires_at = token_data.expires_at if token_data else None

        # Create token record
        db_token = OrganizationTokenORM(
            organization_id=org_id,
            token=token_hash,
            created_by=created_by,
            expires_at=expires_at,
            created_at=datetime.utcnow(),
            active=True
        )

        db.add(db_token)
        db.commit()
        db.refresh(db_token)

        logger.info(f"Created access token for organization {org_id} ({org.name})")

        # Return response with plain token (only shown once)
        response = OrganizationTokenResponse(
            id=db_token.id,
            organization_id=db_token.organization_id,
            token=plain_token,  # Return plain token, not hash
            created_at=db_token.created_at.isoformat(),
            expires_at=db_token.expires_at.isoformat() if db_token.expires_at else None,
            created_by=db_token.created_by,
            last_used_at=db_token.last_used_at.isoformat() if db_token.last_used_at else None,
            active=db_token.active
        )

        return response

    except HTTPException:
        raise
    except Exception as e:
        db.rollback()
        logger.error(f"Error creating organization token: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to create token: {str(e)}"
        )


@organization_router.get("/{org_id}/tokens", response_model=List[OrganizationTokenResponse])
async def list_organization_tokens(
    org_id: int,
    include_inactive: bool = False,
    db: Session = Depends(get_db)
):
    """
    List all access tokens for an organization.

    Note: Only token metadata is returned, not the actual token values.
    """
    try:
        # Verify organization exists
        org = db.query(OrganizationORM).filter(
            OrganizationORM.id == org_id
        ).first()

        if not org:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Organization {org_id} not found"
            )

        # Query tokens
        query = db.query(OrganizationTokenORM).filter(
            OrganizationTokenORM.organization_id == org_id
        )

        if not include_inactive:
            query = query.filter(OrganizationTokenORM.active == True)

        tokens = query.order_by(OrganizationTokenORM.created_at.desc()).all()

        return [
            OrganizationTokenResponse(
                id=token.id,
                organization_id=token.organization_id,
                token="[REDACTED]",  # Never show the actual token
                created_at=token.created_at.isoformat(),
                expires_at=token.expires_at.isoformat() if token.expires_at else None,
                created_by=token.created_by,
                last_used_at=token.last_used_at.isoformat() if token.last_used_at else None,
                active=token.active
            )
            for token in tokens
        ]

    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Error listing organization tokens: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to list tokens: {str(e)}"
        )


@organization_router.delete("/token/{token_id}", status_code=status.HTTP_204_NO_CONTENT)
async def revoke_organization_token(
    token_id: int,
    db: Session = Depends(get_db)
):
    """
    Revoke (deactivate) an organization access token.

    The token record is not deleted, but marked as inactive.
    """
    try:
        token = db.query(OrganizationTokenORM).filter(
            OrganizationTokenORM.id == token_id
        ).first()

        if not token:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail=f"Token {token_id} not found"
            )

        # Mark as inactive instead of deleting
        token.active = False
        db.commit()

        logger.info(f"Revoked token {token_id} for organization {token.organization_id}")

    except HTTPException:
        raise
    except Exception as e:
        db.rollback()
        logger.error(f"Error revoking token: {e}", exc_info=True)
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to revoke token: {str(e)}"
        )


def _org_to_response(org: OrganizationORM) -> OrganizationResponse:
    """Convert ORM model to response model"""
    # Extract lat/lon from PostGIS point
    lat, lon = None, None
    if org.location is not None:
        try:
            # Get coordinates from geometry
            from geoalchemy2.shape import to_shape
            point = to_shape(org.location)
            lon, lat = point.x, point.y
        except Exception as e:
            logger.warning(f"Could not extract coordinates: {e}")

    return OrganizationResponse(
        id=org.id,
        name=org.name,
        short_name=org.short_name,
        type=org.type,
        contact_person=org.contact_person,
        phone=org.phone,
        email=org.email,
        emergency_phone=org.emergency_phone,
        address=org.address,
        city=org.city,
        country=org.country,
        latitude=lat,
        longitude=lon,
        capabilities=org.capabilities or [],
        response_area=org.response_area,
        active=org.active,
        notes=org.notes,
        created_at=org.created_at.isoformat() if org.created_at else datetime.utcnow().isoformat(),
        updated_at=org.updated_at.isoformat() if org.updated_at else datetime.utcnow().isoformat()
    )
