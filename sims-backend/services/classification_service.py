"""
LLM-based incident classification service using FeatherAI.
"""
import json
import logging
from typing import Optional, Dict, Any, List
import httpx
from config import Config

logger = logging.getLogger(__name__)


class ClassificationResult:
    """Result of incident classification."""

    def __init__(
        self,
        category: str,
        priority: str,
        tags: List[str],
        confidence: float,
        reasoning: str,
        raw_response: Optional[str] = None
    ):
        self.category = category
        self.priority = priority
        self.tags = tags
        self.confidence = confidence
        self.reasoning = reasoning
        self.raw_response = raw_response

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for storage."""
        return {
            "category": self.category,
            "priority": self.priority,
            "tags": self.tags,
            "confidence": self.confidence,
            "reasoning": self.reasoning
        }


class IncidentClassifier:
    """Classifies incidents using LLM."""

    def __init__(self):
        self.api_key = Config.FEATHERLESS_API_KEY
        self.api_base = Config.FEATHERLESS_API_BASE
        self.model = Config.DEFAULT_LLM_MODEL
        self.temperature = Config.LLM_TEMPERATURE
        self.max_tokens = Config.LLM_MAX_TOKENS

        if not self.api_key:
            logger.warning("FEATHERLESS_API_KEY not configured. Classification will fail.")

    async def classify_incident(
        self,
        description: str,
        transcription: Optional[str] = None,
        latitude: Optional[float] = None,
        longitude: Optional[float] = None,
        heading: Optional[float] = None
    ) -> ClassificationResult:
        """
        Classify an incident using LLM.

        Args:
            description: Text description of the incident
            transcription: Voice transcription (if available)
            latitude: Location latitude
            longitude: Location longitude
            heading: Device heading/bearing

        Returns:
            ClassificationResult with category, priority, tags, and confidence
        """
        if not self.api_key:
            logger.error("Cannot classify: FEATHERLESS_API_KEY not configured")
            return self._create_fallback_result("API key not configured")

        # Build combined incident text
        incident_text = self._build_incident_text(
            description, transcription, latitude, longitude, heading
        )

        # Create classification prompt
        prompt = self._create_classification_prompt(incident_text)

        try:
            # Call LLM API
            result = await self._call_llm_api(prompt)

            # Parse and validate result
            classification = self._parse_llm_response(result)

            logger.info(
                f"Classified incident as '{classification.category}' "
                f"with confidence {classification.confidence:.2f}"
            )

            return classification

        except Exception as e:
            logger.error(f"Classification failed: {e}", exc_info=True)
            return self._create_fallback_result(f"Classification error: {str(e)}")

    def _build_incident_text(
        self,
        description: str,
        transcription: Optional[str],
        latitude: Optional[float],
        longitude: Optional[float],
        heading: Optional[float]
    ) -> str:
        """Build combined text for classification."""
        parts = []

        if description:
            parts.append(f"Description: {description}")

        if transcription:
            parts.append(f"Voice Message: {transcription}")

        if latitude and longitude:
            parts.append(f"Location: {latitude:.6f}, {longitude:.6f}")

        if heading is not None:
            parts.append(f"Heading: {heading} degrees")

        return "\n".join(parts)

    def _create_classification_prompt(self, incident_text: str) -> str:
        """Create prompt for LLM classification."""
        categories_list = "\n".join([f"- {cat}" for cat in Config.INCIDENT_CATEGORIES])
        priorities_list = ", ".join(Config.PRIORITY_LEVELS)

        return f"""You are an expert incident classification system for military and civilian emergency response.

Analyze the following incident report and classify it.

INCIDENT REPORT:
{incident_text}

TASK:
Classify this incident by providing:
1. Category (choose the most appropriate from the list below)
2. Priority level (critical, high, medium, or low)
3. Tags (3-5 relevant keywords for filtering/search)
4. Confidence (0.0 to 1.0, how confident are you in this classification?)
5. Reasoning (brief explanation of your classification)

AVAILABLE CATEGORIES:
{categories_list}

PRIORITY GUIDELINES:
- critical: Immediate threat to life, active attack, mass casualties
- high: Serious threat, potential casualties, time-sensitive
- medium: Notable incident, limited immediate danger
- low: Minor incident, informational

RESPONSE FORMAT (JSON only, no other text):
{{
    "category": "category_name",
    "priority": "priority_level",
    "tags": ["tag1", "tag2", "tag3"],
    "confidence": 0.85,
    "reasoning": "Brief explanation of classification"
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
                    "content": "You are an expert incident classification system. Always respond with valid JSON only."
                },
                {
                    "role": "user",
                    "content": prompt
                }
            ],
            "temperature": self.temperature,
            "max_tokens": self.max_tokens
        }

        timeout = httpx.Timeout(Config.LLM_TIMEOUT, connect=10.0)
        async with httpx.AsyncClient(timeout=timeout) as client:
            response = await client.post(url, json=payload, headers=headers)
            response.raise_for_status()

            data = response.json()
            return data["choices"][0]["message"]["content"]

    def _parse_llm_response(self, response: str) -> ClassificationResult:
        """Parse LLM response into ClassificationResult."""
        try:
            # Try to extract JSON from response (in case LLM added extra text)
            response = response.strip()

            # Find JSON object in response
            start_idx = response.find("{")
            end_idx = response.rfind("}") + 1

            if start_idx == -1 or end_idx == 0:
                raise ValueError("No JSON object found in response")

            json_str = response[start_idx:end_idx]
            data = json.loads(json_str)

            # Validate required fields
            category = data.get("category", "unclassified")
            priority = data.get("priority", "medium")
            tags = data.get("tags", [])
            confidence = float(data.get("confidence", 0.5))
            reasoning = data.get("reasoning", "No reasoning provided")

            # Validate category
            if category not in Config.INCIDENT_CATEGORIES:
                logger.warning(f"Unknown category '{category}', defaulting to 'unclassified'")
                category = "unclassified"
                confidence = min(confidence, 0.5)

            # Validate priority
            if priority not in Config.PRIORITY_LEVELS:
                logger.warning(f"Unknown priority '{priority}', defaulting to 'medium'")
                priority = "medium"

            # Ensure confidence is in valid range
            confidence = max(0.0, min(1.0, confidence))

            return ClassificationResult(
                category=category,
                priority=priority,
                tags=tags,
                confidence=confidence,
                reasoning=reasoning,
                raw_response=response
            )

        except Exception as e:
            logger.error(f"Failed to parse LLM response: {e}\nResponse: {response}")
            return self._create_fallback_result(f"Parse error: {str(e)}")

    def _create_fallback_result(self, error_msg: str) -> ClassificationResult:
        """Create fallback classification when service fails."""
        return ClassificationResult(
            category="unclassified",
            priority="medium",
            tags=["unclassified", "needs_review"],
            confidence=0.0,
            reasoning=f"Auto-classification failed: {error_msg}"
        )


# Singleton instance
_classifier_instance = None


def get_classifier() -> IncidentClassifier:
    """Get singleton classifier instance."""
    global _classifier_instance
    if _classifier_instance is None:
        _classifier_instance = IncidentClassifier()
    return _classifier_instance
