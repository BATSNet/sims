"""
Internationalization (i18n) module for SIMS backend.
Provides translation support for UI strings and messages.
"""
import yaml
from pathlib import Path
from typing import Dict, Any
from config import Config


class I18n:
    """Translation manager for SIMS backend."""

    def __init__(self, language: str = None):
        """
        Initialize i18n with specified language.

        Args:
            language: Language code (en, de). Falls back to Config.LANGUAGE if not provided.
        """
        self.language = language or Config.LANGUAGE
        self.translations = self._load_translations()
        self._language_names = {
            'en': 'English',
            'de': 'German'
        }

    def _load_translations(self) -> Dict[str, Any]:
        """Load translation file for current language."""
        locale_file = Path(__file__).parent / f"locales/{self.language}.yaml"

        if not locale_file.exists():
            # Fallback to English if translation file doesn't exist
            print(f"Warning: Translation file for '{self.language}' not found, falling back to English")
            self.language = 'en'
            locale_file = Path(__file__).parent / "locales/en.yaml"

        with open(locale_file, 'r', encoding='utf-8') as f:
            return yaml.safe_load(f)

    def t(self, key: str, **kwargs) -> str:
        """
        Translate a key with optional variable interpolation.

        Args:
            key: Dot-separated translation key (e.g., 'ui.dashboard.title')
            **kwargs: Variables to interpolate in the translation

        Returns:
            Translated string with interpolated variables

        Examples:
            i18n.t('ui.dashboard.title')
            i18n.t('messages.auto_response.org_notified', org_name='Police Department')
        """
        keys = key.split('.')
        value = self.translations

        # Navigate nested dictionary
        for k in keys:
            if isinstance(value, dict) and k in value:
                value = value[k]
            else:
                # Key not found, return the key itself as fallback
                return key

        # Interpolate variables if any
        if kwargs and isinstance(value, str):
            try:
                return value.format(**kwargs)
            except KeyError as e:
                print(f"Warning: Variable {e} not provided for translation key '{key}'")
                return value

        return value if isinstance(value, str) else key

    def get_language_name(self) -> str:
        """Get the full name of the current language."""
        return self._language_names.get(self.language, 'English')

    def get_language_code(self) -> str:
        """Get the ISO 639-1 language code."""
        return self.language

    def get_all_categories(self) -> Dict[str, str]:
        """Get all translated category names."""
        return self.translations.get('categories', {})

    def get_all_priorities(self) -> Dict[str, str]:
        """Get all translated priority names."""
        return self.translations.get('priorities', {})


# Global i18n instance
i18n = I18n()


# Helper function for easy access
def t(key: str, **kwargs) -> str:
    """
    Shorthand for i18n.t()

    Args:
        key: Dot-separated translation key
        **kwargs: Variables to interpolate

    Returns:
        Translated string
    """
    return i18n.t(key, **kwargs)
