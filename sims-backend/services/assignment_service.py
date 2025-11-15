"""
Auto-assignment service for routing incidents to appropriate organizations.
Uses LLM-based semantic matching and geographic proximity.
"""
import json
import logging
from typing import Optional, List, Dict, Any, Tuple
from datetime import datetime
import httpx
from sqlalchemy.orm import Session
from sqlalchemy import func, and_
from geoalchemy2.functions import ST_Distance, ST_GeogFromText

from models.incident_model import IncidentORM
from models.organization_model import OrganizationORM
from services.classification_service import ClassificationResult
from config import Config

logger = logging.getLogger(__name__)


class AssignmentResult:
    """Result of auto-assignment."""

    def __init__(
        self,
        organization_id: Optional[int],
        organization_name: Optional[str],
        confidence: float,
        reasoning: str,
        considered_orgs: List[Dict[str, Any]] = None
    ):
        self.organization_id = organization_id
        self.organization_name = organization_name
        self.confidence = confidence
        self.reasoning = reasoning
        self.considered_orgs = considered_orgs or []

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for storage."""
        return {
            "organization_id": self.organization_id,
            "organization_name": self.organization_name,
            "confidence": self.confidence,
            "reasoning": self.reasoning,
            "considered_organizations": self.considered_orgs
        }


class AutoAssignmentService:
    """Service for automatically assigning incidents to organizations."""

    def __init__(self):
        self.api_key = Config.FEATHERLESS_API_KEY
        self.api_base = Config.FEATHERLESS_API_BASE
        self.model = Config.DEFAULT_LLM_MODEL

    async def assign_incident(
        self,
        incident: IncidentORM,
        classification: ClassificationResult,
        db: Session
    ) -> AssignmentResult:
        """
        Automatically assign incident to best-match organization.

        Args:
            incident: Incident to assign
            classification: Classification result from LLM
            db: Database session

        Returns:
            AssignmentResult with organization selection
        """
        try:
            # Get all active organizations
            active_orgs = db.query(OrganizationORM).filter(
                OrganizationORM.active == True
            ).all()

            if not active_orgs:
                logger.warning("No active organizations available for assignment")
                return self._create_no_match_result("No active organizations available")

            # Filter organizations by category mapping first
            filtered_orgs = self._filter_orgs_by_category(
                classification.category,
                active_orgs
            )

            # If no orgs match category, use all active orgs as fallback
            if not filtered_orgs:
                logger.warning(
                    f"No organizations match category '{classification.category}', "
                    f"using all active organizations"
                )
                filtered_orgs = active_orgs

            # Calculate distances if incident has location
            orgs_with_distance = self._calculate_distances(incident, filtered_orgs)

            # Use LLM to semantically match incident to organizations
            assignment = await self._llm_match_organizations(
                incident,
                classification,
                orgs_with_distance
            )

            logger.info(
                f"Assigned incident {incident.incident_id} to "
                f"{assignment.organization_name} (confidence: {assignment.confidence:.2f})"
            )

            return assignment

        except Exception as e:
            logger.error(f"Auto-assignment failed: {e}", exc_info=True)
            return self._create_no_match_result(f"Assignment error: {str(e)}")

    def _filter_orgs_by_category(
        self,
        category: str,
        organizations: List[OrganizationORM]
    ) -> List[OrganizationORM]:
        """
        Filter organizations by incident category using mapping.

        Args:
            category: Incident category
            organizations: List of organizations to filter

        Returns:
            Filtered list of organizations matching category
        """
        # Get preferred org types for this category
        preferred_types = Config.CATEGORY_TO_ORG_TYPE.get(category, [])

        if not preferred_types:
            logger.warning(f"No organization type mapping for category '{category}'")
            return organizations

        # Filter organizations by type
        filtered = [
            org for org in organizations
            if org.type in preferred_types
        ]

        logger.info(
            f"Filtered {len(filtered)} organizations for category '{category}' "
            f"(types: {', '.join(preferred_types)})"
        )

        return filtered

    def _calculate_distances(
        self,
        incident: IncidentORM,
        organizations: List[OrganizationORM]
    ) -> List[Dict[str, Any]]:
        """Calculate distances from incident location to organizations."""
        results = []

        for org in organizations:
            org_data = {
                "id": org.id,
                "name": org.name,
                "short_name": org.short_name,
                "type": org.type,
                "capabilities": org.capabilities or [],
                "response_area": org.response_area,
                "distance_km": None
            }

            # Calculate distance if both have locations
            if incident.location and org.location:
                try:
                    # Get WKT representations
                    incident_wkt = f"POINT({incident.longitude} {incident.latitude})"
                    org_wkt = f"POINT({org.location.desc})"  # This needs proper extraction

                    # For now, use simple lat/lon calculation
                    # In production, use PostGIS ST_Distance
                    if hasattr(org.location, 'coords'):
                        org_coords = org.location.coords[0]
                        distance = self._haversine_distance(
                            incident.latitude, incident.longitude,
                            org_coords[1], org_coords[0]
                        )
                        org_data["distance_km"] = round(distance, 2)
                except Exception as e:
                    logger.warning(f"Could not calculate distance for {org.name}: {e}")

            results.append(org_data)

        # Sort by distance (if available)
        results.sort(key=lambda x: x["distance_km"] if x["distance_km"] is not None else float('inf'))

        return results

    def _haversine_distance(
        self,
        lat1: float,
        lon1: float,
        lat2: float,
        lon2: float
    ) -> float:
        """Calculate distance between two points in kilometers using Haversine formula."""
        from math import radians, sin, cos, sqrt, atan2

        R = 6371  # Earth radius in kilometers

        lat1, lon1, lat2, lon2 = map(radians, [lat1, lon1, lat2, lon2])

        dlat = lat2 - lat1
        dlon = lon2 - lon1

        a = sin(dlat / 2) ** 2 + cos(lat1) * cos(lat2) * sin(dlon / 2) ** 2
        c = 2 * atan2(sqrt(a), sqrt(1 - a))

        return R * c

    async def _llm_match_organizations(
        self,
        incident: IncidentORM,
        classification: ClassificationResult,
        organizations: List[Dict[str, Any]]
    ) -> AssignmentResult:
        """Use LLM to semantically match incident to best organization."""
        if not self.api_key:
            logger.error("Cannot perform LLM matching: FEATHERLESS_API_KEY not configured")
            return self._create_no_match_result("API key not configured")

        # Create matching prompt
        prompt = self._create_matching_prompt(incident, classification, organizations)

        try:
            # Call LLM API
            response = await self._call_llm_api(prompt)

            # Parse response
            result = self._parse_matching_response(response, organizations)

            return result

        except Exception as e:
            logger.error(f"LLM matching failed: {e}", exc_info=True)
            return self._create_no_match_result(f"LLM matching error: {str(e)}")

    def _create_matching_prompt(
        self,
        incident: IncidentORM,
        classification: ClassificationResult,
        organizations: List[Dict[str, Any]]
    ) -> str:
        """Create prompt for LLM organization matching."""
        # Build incident summary
        incident_summary = f"""
Category: {classification.category}
Priority: {classification.priority}
Description: {incident.description}
Tags: {', '.join(classification.tags)}
Location: {incident.latitude}, {incident.longitude}
"""

        # Build organization list
        org_list = []
        for i, org in enumerate(organizations[:20], 1):  # Limit to top 20 to avoid token limits
            distance_info = f" (Distance: {org['distance_km']} km)" if org['distance_km'] else ""
            capabilities = ', '.join(org['capabilities']) if org['capabilities'] else 'none specified'

            org_list.append(
                f"{i}. {org['name']} ({org['short_name']})\n"
                f"   Type: {org['type']}\n"
                f"   Capabilities: {capabilities}\n"
                f"   Response Area: {org['response_area']}{distance_info}"
            )

        org_list_str = "\n\n".join(org_list)

        return f"""You are an expert emergency response coordinator for military and civilian incidents.

INCIDENT TO ASSIGN:
{incident_summary}

AVAILABLE ORGANIZATIONS:
{org_list_str}

TASK:
Select the MOST APPROPRIATE organization to handle this incident. Consider:
1. Organization capabilities matching incident category and needs
2. Geographic proximity (if distance provided)
3. Organization type appropriateness (military vs civilian vs specialized)
4. Response area coverage

RESPONSE FORMAT (JSON only, no other text):
{{
    "organization_index": 1,
    "confidence": 0.85,
    "reasoning": "Brief explanation of why this organization is best suited"
}}

The organization_index should be the number (1-{len(org_list)}) from the list above.
Confidence should be 0.0 to 1.0.

If NO organization is appropriate, respond with:
{{
    "organization_index": null,
    "confidence": 0.0,
    "reasoning": "Explanation of why no organization matches"
}}

Respond with ONLY the JSON object, no additional text."""

    async def _call_llm_api(self, prompt: str) -> str:
        """Call FeatherAI API."""
        url = f"{self.api_base}/chat/completions"

        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }

        payload = {
            "model": self.model,
            "messages": [
                {
                    "role": "system",
                    "content": "You are an expert emergency response coordinator. Always respond with valid JSON only."
                },
                {
                    "role": "user",
                    "content": prompt
                }
            ],
            "temperature": Config.LLM_TEMPERATURE,
            "max_tokens": Config.LLM_MAX_TOKENS
        }

        async with httpx.AsyncClient(timeout=90.0) as client:
            response = await client.post(url, json=payload, headers=headers)
            response.raise_for_status()

            data = response.json()
            return data["choices"][0]["message"]["content"]

    def _parse_matching_response(
        self,
        response: str,
        organizations: List[Dict[str, Any]]
    ) -> AssignmentResult:
        """Parse LLM matching response."""
        try:
            # Extract JSON from response
            response = response.strip()
            start_idx = response.find("{")
            end_idx = response.rfind("}") + 1

            if start_idx == -1 or end_idx == 0:
                raise ValueError("No JSON object found in response")

            json_str = response[start_idx:end_idx]
            data = json.loads(json_str)

            org_index = data.get("organization_index")
            confidence = float(data.get("confidence", 0.0))
            reasoning = data.get("reasoning", "No reasoning provided")

            # If no organization selected
            if org_index is None:
                return AssignmentResult(
                    organization_id=None,
                    organization_name=None,
                    confidence=confidence,
                    reasoning=reasoning,
                    considered_orgs=organizations
                )

            # Validate index
            if not (1 <= org_index <= len(organizations)):
                raise ValueError(f"Invalid organization index: {org_index}")

            # Get selected organization (convert to 0-based index)
            selected_org = organizations[org_index - 1]

            return AssignmentResult(
                organization_id=selected_org["id"],
                organization_name=selected_org["name"],
                confidence=confidence,
                reasoning=reasoning,
                considered_orgs=organizations
            )

        except Exception as e:
            logger.error(f"Failed to parse matching response: {e}\nResponse: {response}")
            return self._create_no_match_result(f"Parse error: {str(e)}")

    def _create_no_match_result(self, reason: str) -> AssignmentResult:
        """Create result when no organization can be assigned."""
        return AssignmentResult(
            organization_id=None,
            organization_name=None,
            confidence=0.0,
            reasoning=f"No assignment: {reason}",
            considered_orgs=[]
        )


# Singleton instance
_assignment_service_instance = None


def get_assignment_service() -> AutoAssignmentService:
    """Get singleton assignment service instance."""
    global _assignment_service_instance
    if _assignment_service_instance is None:
        _assignment_service_instance = AutoAssignmentService()
    return _assignment_service_instance
