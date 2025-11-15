"""
Quick script to generate a responder token for testing
"""
import secrets
import sys
from datetime import datetime

from db.connection import get_db
from models.organization_model import OrganizationORM
from models.organization_token_model import OrganizationTokenORM
from auth.responder_auth import hash_token


def generate_token(org_id: int = None):
    """Generate a test token for an organization"""
    db = next(get_db())

    try:
        # Get organization
        if org_id:
            org = db.query(OrganizationORM).filter(
                OrganizationORM.id == org_id
            ).first()
        else:
            # Get first organization
            org = db.query(OrganizationORM).first()

        if not org:
            print("No organizations found. Create an organization first.")
            return

        # Generate token
        plain_token = secrets.token_urlsafe(32)
        token_hash = hash_token(plain_token)

        # Create token record
        token_record = OrganizationTokenORM(
            organization_id=org.id,
            token=token_hash,
            created_by='manual_generation',
            created_at=datetime.utcnow(),
            active=True
        )

        db.add(token_record)
        db.commit()

        print("=" * 80)
        print("RESPONDER TOKEN GENERATED")
        print(f"Organization: {org.name} (ID: {org.id})")
        print(f"Token: {plain_token}")
        print(f"Access URL: http://localhost:8000/responder?token={plain_token}")
        print("=" * 80)

    except Exception as e:
        print(f"Error generating token: {e}")
    finally:
        db.close()


if __name__ == "__main__":
    org_id = int(sys.argv[1]) if len(sys.argv) > 1 else None
    generate_token(org_id)
