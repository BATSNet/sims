"""
Configuration settings for SIMS backend application.
Loads configuration from config.yaml and environment variables.
"""
import os
import yaml
from pathlib import Path
from typing import Optional, Dict, Any, List

# Load YAML configuration
config_path = Path(__file__).parent / "config.yaml"
with open(config_path, 'r') as f:
    _yaml_config = yaml.safe_load(f)


class Config:
    """Application configuration loaded from YAML and environment variables."""

    # Database
    DATABASE_URL: str = os.getenv("DATABASE_URL", "postgresql://postgres:postgres@localhost:5432/sims")

    # Server Configuration
    PUBLIC_SERVER_URL: Optional[str] = os.getenv("PUBLIC_SERVER_URL", "http://localhost:8000")

    # Language Configuration
    LANGUAGE: str = os.getenv("LANGUAGE", _yaml_config.get('language', 'en'))

    # AI Provider Configuration
    _ai_providers = _yaml_config.get('ai_providers', {})

    # Classification Provider Config
    CLASSIFICATION_PROVIDER: str = _ai_providers.get('classification', {}).get('provider', 'anthropic')
    CLASSIFICATION_MODEL: str = _ai_providers.get('classification', {}).get('model', 'claude-3-5-sonnet-20241022')
    CLASSIFICATION_TEMPERATURE: float = _ai_providers.get('classification', {}).get('temperature', 0.3)
    CLASSIFICATION_MAX_TOKENS: int = _ai_providers.get('classification', {}).get('max_tokens', 1000)
    CLASSIFICATION_TIMEOUT: int = _ai_providers.get('classification', {}).get('timeout', 120)
    CLASSIFICATION_API_BASE: str = _ai_providers.get('classification', {}).get('api_base', None)

    # Transcription Provider Config
    TRANSCRIPTION_PROVIDER: str = _ai_providers.get('transcription', {}).get('provider', 'deepinfra')
    TRANSCRIPTION_MODEL: str = _ai_providers.get('transcription', {}).get('model', 'openai/whisper-large-v3')
    TRANSCRIPTION_TIMEOUT: int = _ai_providers.get('transcription', {}).get('timeout', 60)

    # Vision Provider Config
    VISION_PROVIDER: str = _ai_providers.get('vision', {}).get('provider', 'openai')
    VISION_MODEL: str = _ai_providers.get('vision', {}).get('model', 'gpt-4o')
    VISION_TEMPERATURE: float = _ai_providers.get('vision', {}).get('temperature', 0.7)
    VISION_MAX_TOKENS: int = _ai_providers.get('vision', {}).get('max_tokens', 500)
    VISION_TIMEOUT: int = _ai_providers.get('vision', {}).get('timeout', 60)

    # Prompts
    _prompts = _yaml_config.get('prompts', {})
    MEDIA_ANALYSIS_PROMPT: str = _prompts.get('media_analysis', '')
    CLASSIFICATION_SYSTEM_PROMPT: str = _prompts.get('classification_system', '')
    CLASSIFICATION_PROMPT_TEMPLATE: str = _prompts.get('classification_prompt', '')

    # Auto-Assignment Settings
    _auto_assignment = _yaml_config.get('auto_assignment', {})
    AUTO_ASSIGN_ENABLED: bool = _auto_assignment.get('enabled', True)
    AUTO_ASSIGN_CONFIDENCE_THRESHOLD: float = _auto_assignment.get('confidence_threshold', 0.7)

    # Classification Categories
    INCIDENT_CATEGORIES: List[str] = _yaml_config.get('incident_categories', [])

    # Priority Levels
    PRIORITY_LEVELS: List[str] = _yaml_config.get('priority_levels', [])

    # Category to Organization Type Mapping
    CATEGORY_TO_ORG_TYPE: Dict[str, List[str]] = _yaml_config.get('category_to_org_type', {})

    # AI Processing Control
    _ai_processing = _yaml_config.get('ai_processing', {})
    TRANSCRIPTION_ENABLED: bool = _ai_processing.get('transcription_enabled', True)
    CATEGORIZATION_ENABLED: bool = _ai_processing.get('categorization_enabled', True)
    MEDIA_ANALYSIS_ENABLED: bool = _ai_processing.get('media_analysis_enabled', True)
    AUTO_FORWARDING_ENABLED: bool = _ai_processing.get('auto_forwarding_enabled', True)

    DEFAULT_ORGANIZATIONS: List[int] = _ai_processing.get('default_organizations', [])
    DEFAULT_PRIORITY: str = _ai_processing.get('default_priority', 'medium')
    DEFAULT_CATEGORY: str = _ai_processing.get('default_category', 'Unclassified')

    # External API Integration Settings (kept from original config)
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

    # Legacy settings for backward compatibility
    FEATHERLESS_API_KEY: Optional[str] = os.getenv("FEATHERLESS_API_KEY")
    DEEPINFRA_API_KEY: Optional[str] = os.getenv("DEEPINFRA_API_KEY")
    FEATHERLESS_API_BASE: str = "https://api.featherless.ai/v1"
    DEFAULT_LLM_MODEL: str = CLASSIFICATION_MODEL
    LLM_TEMPERATURE: float = CLASSIFICATION_TEMPERATURE
    LLM_MAX_TOKENS: int = CLASSIFICATION_MAX_TOKENS
    LLM_TIMEOUT: int = CLASSIFICATION_TIMEOUT
    VISION_MODEL: str = VISION_MODEL

    @classmethod
    def validate(cls):
        """Validate required configuration."""
        # Validate confidence threshold
        if not (0.0 <= cls.AUTO_ASSIGN_CONFIDENCE_THRESHOLD <= 1.0):
            raise ValueError("AUTO_ASSIGN_CONFIDENCE_THRESHOLD must be between 0.0 and 1.0")

        # Check for required API keys based on provider configuration
        provider_warnings = []

        if cls.CLASSIFICATION_PROVIDER == 'openai' and not os.getenv('OPENAI_API_KEY'):
            provider_warnings.append("OPENAI_API_KEY not set (required for classification)")
        elif cls.CLASSIFICATION_PROVIDER == 'anthropic' and not os.getenv('ANTHROPIC_API_KEY'):
            provider_warnings.append("ANTHROPIC_API_KEY not set (required for classification)")
        elif cls.CLASSIFICATION_PROVIDER == 'mistral' and not os.getenv('MISTRAL_API_KEY'):
            provider_warnings.append("MISTRAL_API_KEY not set (required for classification)")
        elif cls.CLASSIFICATION_PROVIDER == 'deepinfra' and not os.getenv('DEEPINFRA_API_KEY'):
            provider_warnings.append("DEEPINFRA_API_KEY not set (required for classification)")
        elif cls.CLASSIFICATION_PROVIDER == 'featherai' and not os.getenv('FEATHERLESS_API_KEY'):
            provider_warnings.append("FEATHERLESS_API_KEY not set (required for classification)")

        if cls.TRANSCRIPTION_PROVIDER == 'openai' and not os.getenv('OPENAI_API_KEY'):
            provider_warnings.append("OPENAI_API_KEY not set (required for transcription)")
        elif cls.TRANSCRIPTION_PROVIDER == 'deepinfra' and not os.getenv('DEEPINFRA_API_KEY'):
            provider_warnings.append("DEEPINFRA_API_KEY not set (required for transcription)")

        if cls.VISION_PROVIDER == 'openai' and not os.getenv('OPENAI_API_KEY'):
            provider_warnings.append("OPENAI_API_KEY not set (required for vision)")
        elif cls.VISION_PROVIDER == 'anthropic' and not os.getenv('ANTHROPIC_API_KEY'):
            provider_warnings.append("ANTHROPIC_API_KEY not set (required for vision)")
        elif cls.VISION_PROVIDER == 'featherai' and not os.getenv('FEATHERLESS_API_KEY'):
            provider_warnings.append("FEATHERLESS_API_KEY not set (required for vision)")

        if provider_warnings:
            for warning in provider_warnings:
                print(f"Warning: {warning}")

        # Validate default priority/category values
        if cls.DEFAULT_PRIORITY not in cls.PRIORITY_LEVELS:
            raise ValueError(f"DEFAULT_PRIORITY '{cls.DEFAULT_PRIORITY}' not in PRIORITY_LEVELS")
        if cls.DEFAULT_CATEGORY not in cls.INCIDENT_CATEGORIES:
            raise ValueError(f"DEFAULT_CATEGORY '{cls.DEFAULT_CATEGORY}' not in INCIDENT_CATEGORIES")

        # Warn if categorization is disabled but default_organizations is empty
        if not cls.CATEGORIZATION_ENABLED and not cls.DEFAULT_ORGANIZATIONS:
            print("Warning: categorization_enabled is false but default_organizations is empty. "
                  "Incidents will not be auto-forwarded.")

        return True


# Validate configuration on import
Config.validate()
