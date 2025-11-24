"""
Integration plugins package
"""
from plugins.base_integration import IntegrationPlugin, IntegrationResult
from plugins.integration_registry import IntegrationRegistry

__all__ = ['IntegrationPlugin', 'IntegrationResult', 'IntegrationRegistry']
