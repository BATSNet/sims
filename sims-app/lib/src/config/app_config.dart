import '../repositories/settings_repository.dart';

class AppConfig {
  // Change this to switch between dev and production
  static const bool isDevelopment = false;

  // Development settings (kept as defaults)
  static const String devBaseUrl = 'http://10.0.2.2:8000';
  static const String devBaseUrlPhysical = 'http://172.22.128.1:8000';

  // Production settings
  static const String prodBaseUrl = 'http://91.99.179.35:8000';

  // Get current base URL based on environment
  // This now checks SettingsRepository first for custom URL
  static String get baseUrl {
    // Try to get from settings repository if available
    if (SettingsRepository.isInitialized) {
      return SettingsRepository.instance!.getBackendUrlSync(
        isDevelopment: isDevelopment,
      );
    }

    // Fallback to static defaults
    if (isDevelopment) {
      return devBaseUrl;
    }
    return prodBaseUrl;
  }

  // Chat endpoint
  static String get chatUrl => '$baseUrl/incident';

  // Helper to check if running on emulator or physical device
  static bool get isEmulator {
    return true; // Default to emulator
  }

  // Get the appropriate dev URL
  static String get devUrl => isEmulator ? devBaseUrl : devBaseUrlPhysical;

  // Force refresh of base URL (call after updating settings)
  static void refresh() {
    // This method can be called to ensure the next baseUrl access
    // gets the latest value from SettingsRepository
    // Currently a no-op since getter always checks fresh
  }
}
