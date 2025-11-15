class AppConfig {
  // Change this to switch between dev and production
  static const bool isDevelopment = true;

  // Development settings
  static const String devBaseUrl = 'http://10.0.2.2:8000'; // Host machine IP for emulator
  static const String devBaseUrlPhysical = 'http://172.22.128.1:8000'; // Host machine IP for physical device

  // Production settings
  static const String prodBaseUrl = 'https://api.sims.example.com';

  // Get current base URL based on environment
  static String get baseUrl {
    if (isDevelopment) {
      // For physical devices, use devBaseUrlPhysical
      // For emulator, use devBaseUrl
      return devBaseUrl;
    }
    return prodBaseUrl;
  }

  // Chat endpoint
  static String get chatUrl => '$baseUrl/incident';

  // Helper to check if running on emulator or physical device
  static bool get isEmulator {
    // This can be enhanced with platform checks if needed
    return true; // Default to emulator
  }

  // Get the appropriate dev URL
  static String get devUrl => isEmulator ? devBaseUrl : devBaseUrlPhysical;
}
