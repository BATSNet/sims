"""
Responder Authentication Module
Handles token-based authentication for organization responders
"""
import logging
import hashlib
from datetime import datetime
from typing import Optional, Tuple
from uuid import UUID

from fastapi import HTTPException, Query, Depends, status
from sqlalchemy.orm import Session

from db.connection import get_db
from models.organization_token_model import OrganizationTokenORM
from models.organization_model import OrganizationORM
from models.incident_model import IncidentORM

logger = logging.getLogger(__name__)


def hash_token(token: str) -> str:
    """
    Hash a token using SHA-256 for secure storage and comparison.

    Args:
        token: Plain text token

    Returns:
        Hashed token (hex digest)
    """
    return hashlib.sha256(token.encode()).hexdigest()


async def validate_responder_token(
    token: str = Query(..., description="Organization access token"),
    db: Session = Depends(get_db)
) -> Tuple[OrganizationTokenORM, OrganizationORM]:
    """
    Validate responder token and return associated organization.

    Args:
        token: Access token from query parameter
        db: Database session

    Returns:
        Tuple of (token_record, organization)

    Raises:
        HTTPException: If token is invalid, expired, or inactive
    """
    # Hash the provided token
    token_hash = hash_token(token)

    # Look up token in database
    token_record = db.query(OrganizationTokenORM).filter(
        OrganizationTokenORM.token == token_hash
    ).first()

    if not token_record:
        logger.warning(f"Invalid token attempted")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid access token"
        )

    # Check if token is active
    if not token_record.active:
        logger.warning(f"Inactive token attempted for org {token_record.organization_id}")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token has been revoked"
        )

    # Check if token has expired
    if token_record.expires_at and token_record.expires_at < datetime.utcnow():
        logger.warning(f"Expired token attempted for org {token_record.organization_id}")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token has expired"
        )

    # Get associated organization
    organization = db.query(OrganizationORM).filter(
        OrganizationORM.id == token_record.organization_id
    ).first()

    if not organization:
        logger.error(f"Token {token_record.id} references non-existent org {token_record.organization_id}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Organization not found"
        )

    # Check if organization is active
    if not organization.active:
        logger.warning(f"Token used for inactive org {organization.id}")
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Organization is inactive"
        )

    # Update last_used_at timestamp
    token_record.last_used_at = datetime.utcnow()
    db.commit()

    logger.info(f"Validated token for organization: {organization.name} (ID: {organization.id})")

    return token_record, organization


async def verify_incident_access(
    incident_id: UUID,
    organization: OrganizationORM,
    db: Session
) -> IncidentORM:
    """
    Verify that the organization has access to the specified incident.

    Args:
        incident_id: UUID of the incident
        organization: Organization making the request
        db: Database session

    Returns:
        IncidentORM if access is granted

    Raises:
        HTTPException: If incident not found or access denied
    """
    # Get incident
    incident = db.query(IncidentORM).filter(
        IncidentORM.id == incident_id
    ).first()

    if not incident:
        logger.warning(f"Incident {incident_id} not found")
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Incident not found"
        )

    # Check if incident is assigned to this organization
    if incident.routed_to != organization.id:
        logger.warning(
            f"Organization {organization.id} attempted to access incident {incident_id} "
            f"which is assigned to organization {incident.routed_to}"
        )
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="You do not have access to this incident"
        )

    logger.info(f"Organization {organization.id} authorized to access incident {incident_id}")

    return incident


class ResponderAuth:
    """
    Dependency class for responder authentication with incident access verification.

    Usage:
        @router.get("/incidents/{incident_id}")
        async def get_incident(
            incident_id: UUID,
            auth: ResponderAuth = Depends()
        ):
            # auth.organization and auth.incident are available
            pass
    """

    def __init__(
        self,
        token_data: Tuple[OrganizationTokenORM, OrganizationORM] = Depends(validate_responder_token),
        db: Session = Depends(get_db)
    ):
        self.token_record = token_data[0]
        self.organization = token_data[1]
        self.db = db

    async def verify_incident(self, incident_id: UUID) -> IncidentORM:
        """Verify access to a specific incident"""
        return await verify_incident_access(incident_id, self.organization, self.db)
