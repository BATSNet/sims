"""
Configuration settings for SIMS backend application.
"""
import os
from typing import Optional


class Config:
    """Application configuration."""

    # Database
    DATABASE_URL: str = os.getenv("DATABASE_URL", "postgresql://postgres:postgres@localhost:5432/sims")

    # Server Configuration
    PUBLIC_SERVER_URL: Optional[str] = os.getenv("PUBLIC_SERVER_URL", "http://localhost:8000")

    # API Keys
    FEATHERLESS_API_KEY: Optional[str] = os.getenv("FEATHERLESS_API_KEY")
    DEEPINFRA_API_KEY: Optional[str] = os.getenv("DEEPINFRA_API_KEY")

    # Auto-Assignment Settings
    AUTO_ASSIGN_ENABLED: bool = os.getenv("AUTO_ASSIGN_ENABLED", "true").lower() == "true"
    AUTO_ASSIGN_CONFIDENCE_THRESHOLD: float = float(os.getenv("AUTO_ASSIGN_CONFIDENCE_THRESHOLD", "0.7"))

    # External API Integration Settings
    API_ENDPOINTS = {
        "SEDAP": {
            "url": os.getenv("SEDAP_API_URL", "http://10.3.1.127:80/SEDAPEXPRESS"),
            "port": int(os.getenv("SEDAP_API_PORT", "80")),
            "sender_id": os.getenv("SEDAP_SENDER_ID", "SIMS"),
            "classification": os.getenv("SEDAP_CLASSIFICATION", "U"),
        },
        "KATWARN": {
            "url": os.getenv("KATWARN_API_URL", ""),
            "enabled": False,
        }
    }

    # LLM Settings
    FEATHERLESS_API_BASE: str = "https://api.featherless.ai/v1"
    DEFAULT_LLM_MODEL: str = os.getenv("LLM_MODEL", "meta-llama/Meta-Llama-3.1-8B-Instruct")
    VISION_MODEL: str = os.getenv("VISION_MODEL", "llava-hf/llava-1.5-7b-hf")
    LLM_TEMPERATURE: float = 0.3  # Lower temperature for more deterministic classification
    LLM_MAX_TOKENS: int = 500
    LLM_TIMEOUT: int = 120  # Timeout in seconds

    # Classification Categories
    INCIDENT_CATEGORIES = [
        "drone_detection",
        "suspicious_vehicle",
        "suspicious_person",
        "fire_incident",
        "medical_emergency",
        "infrastructure_damage",
        "cyber_incident",
        "hazmat_incident",
        "natural_disaster",
        "airport_incident",
        "security_breach",
        "civil_unrest",
        "armed_threat",
        "explosion",
        "chemical_biological",
        "maritime_incident",
        "theft_burglary",
        "unclassified"
    ]

    # Priority Levels
    PRIORITY_LEVELS = ["critical", "high", "medium", "low"]

    # Category to Organization Type Mapping
    # Maps incident categories to preferred organization types
    CATEGORY_TO_ORG_TYPE = {
        "drone_detection": ["military", "police", "civil_defense"],
        "suspicious_vehicle": ["police", "military", "civil_defense"],
        "suspicious_person": ["police", "civil_defense"],
        "fire_incident": ["fire", "civil_defense"],
        "medical_emergency": ["medical", "fire"],
        "infrastructure_damage": ["civil_defense", "government"],
        "cyber_incident": ["military", "government", "police"],
        "hazmat_incident": ["fire", "civil_defense", "medical"],
        "natural_disaster": ["civil_defense", "fire", "government"],
        "airport_incident": ["police", "fire", "civil_defense"],
        "security_breach": ["police", "military", "civil_defense"],
        "civil_unrest": ["police", "military", "government"],
        "armed_threat": ["police", "military"],
        "explosion": ["fire", "police", "military", "civil_defense"],
        "chemical_biological": ["fire", "medical", "military", "civil_defense"],
        "maritime_incident": ["police", "military", "civil_defense"],
        "theft_burglary": ["police"],
        "unclassified": ["police", "civil_defense", "government"]
    }

    @classmethod
    def validate(cls):
        """Validate required configuration."""
        if not cls.FEATHERLESS_API_KEY:
            print("Warning: FEATHERLESS_API_KEY not set. LLM classification will not work.")

        if not (0.0 <= cls.AUTO_ASSIGN_CONFIDENCE_THRESHOLD <= 1.0):
            raise ValueError("AUTO_ASSIGN_CONFIDENCE_THRESHOLD must be between 0.0 and 1.0")

        return True


# Validate configuration on import
Config.validate()
