"""
LLM-based incident classification service using configurable AI providers.
"""
import json
import logging
from typing import Optional, Dict, Any, List
from config import Config
from services.ai_providers.factory import ProviderFactory
from services.ai_providers.base import Message
from i18n import i18n

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
    """Classifies incidents using configurable LLM provider."""

    def __init__(self):
        """Initialize classifier with configured provider."""
        self.provider = ProviderFactory.create_llm_provider(
            provider_name=Config.CLASSIFICATION_PROVIDER,
            model=Config.CLASSIFICATION_MODEL,
            temperature=Config.CLASSIFICATION_TEMPERATURE,
            max_tokens=Config.CLASSIFICATION_MAX_TOKENS,
            timeout=Config.CLASSIFICATION_TIMEOUT,
            api_base=Config.CLASSIFICATION_API_BASE
        )

        if not self.provider:
            logger.warning(
                f"Failed to initialize classification provider: {Config.CLASSIFICATION_PROVIDER}"
            )

        # Format system prompt with language
        self.system_prompt = Config.CLASSIFICATION_SYSTEM_PROMPT.format(
            language=i18n.get_language_name()
        )

    async def classify_incident(
        self,
        description: str,
        transcription: Optional[str] = None,
        image_url: Optional[str] = None,
        latitude: Optional[float] = None,
        longitude: Optional[float] = None,
        heading: Optional[float] = None,
        category_hint: Optional[str] = None
    ) -> ClassificationResult:
        """
        Classify an incident using LLM with optional image analysis.

        Args:
            description: Text description of the incident
            transcription: Voice transcription (if available)
            image_url: URL to incident image for visual analysis
            latitude: Location latitude
            longitude: Location longitude
            heading: Device heading/bearing
            category_hint: Previous category to consider

        Returns:
            ClassificationResult with category, priority, tags, and confidence
        """
        if not self.provider:
            logger.error("Cannot classify: provider not initialized")
            return self._create_fallback_result("Provider not initialized")

        # Build combined incident text
        incident_text = self._build_incident_text(
            description, transcription, latitude, longitude, heading
        )

        # Create classification prompt
        prompt = self._create_classification_prompt(incident_text, category_hint)

        try:
            # Create messages
            messages = [
                Message(role="system", content=self.system_prompt),
                Message(role="user", content=prompt)
            ]

            # Call LLM API (with vision if image provided)
            if image_url:
                result = await self.provider.chat_completion_with_vision(
                    messages=messages,
                    image_url=image_url
                )
            else:
                result = await self.provider.chat_completion(messages=messages)

            # Parse and validate result
            classification = self._parse_llm_response(result.content)

            logger.info(
                f"Classified incident as '{classification.category}' "
                f"with confidence {classification.confidence:.2f} using {Config.CLASSIFICATION_PROVIDER}"
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

    def _create_classification_prompt(self, incident_text: str, category_hint: Optional[str] = None) -> str:
        """Create prompt for LLM classification using template from config."""
        categories_list = "\n".join([f"- {cat}" for cat in Config.INCIDENT_CATEGORIES])

        category_context = ""
        if category_hint:
            category_context = f"\n\nNOTE: This incident was previously classified as '{category_hint}'. Consider this context but re-evaluate based on new information."

        # Use template from config.yaml with language injection
        prompt = Config.CLASSIFICATION_PROMPT_TEMPLATE.format(
            category_context=category_context,
            incident_text=incident_text,
            categories_list=categories_list,
            language=i18n.get_language_name()
        )

        return prompt

    def _parse_llm_response(self, response: str) -> ClassificationResult:
        """Parse LLM response into ClassificationResult."""
        try:
            # Try to extract JSON from response (in case LLM added extra text)
            response = response.strip()

            # Remove markdown code blocks if present
            if response.startswith("```"):
                # Find the JSON content between code blocks
                lines = response.split("\n")
                response = "\n".join(lines[1:-1]) if len(lines) > 2 else response

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
