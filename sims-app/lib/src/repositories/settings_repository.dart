import 'package:shared_preferences/shared_preferences.dart';

class SettingsRepository {
  static const String _backendUrlKey = 'backend_url';
  static const String _useCustomUrlKey = 'use_custom_url';

  // Default URLs (match AppConfig values)
  static const String defaultDevUrl = 'http://10.0.2.2:8000';
  static const String defaultDevUrlPhysical = 'http://172.22.128.1:8000';
  static const String defaultProdUrl = 'http://91.99.179.35:8000';

  static SettingsRepository? _instance;
  final SharedPreferences _prefs;

  SettingsRepository._(this._prefs);

  // Singleton pattern (matches UserRepository)
  static Future<SettingsRepository> getInstance() async {
    if (_instance == null) {
      final prefs = await SharedPreferences.getInstance();
      _instance = SettingsRepository._(prefs);
    }
    return _instance!;
  }

  // Check if instance is initialized (public accessor)
  static bool get isInitialized => _instance != null;

  // Get instance synchronously (only if already initialized)
  static SettingsRepository? get instance => _instance;

  // Check if custom URL is enabled
  bool isCustomUrlEnabled() {
    return _prefs.getBool(_useCustomUrlKey) ?? false;
  }

  // Get custom backend URL (null if not set)
  String? getCustomBackendUrl() {
    return _prefs.getString(_backendUrlKey);
  }

  // Get the active backend URL (custom if enabled, otherwise default)
  String getBackendUrl({bool isDevelopment = false}) {
    if (isCustomUrlEnabled()) {
      return getCustomBackendUrl() ?? _getDefaultUrl(isDevelopment);
    }
    return _getDefaultUrl(isDevelopment);
  }

  // Synchronous version for AppConfig
  String getBackendUrlSync({bool isDevelopment = false}) {
    return getBackendUrl(isDevelopment: isDevelopment);
  }

  // Save custom backend URL
  Future<void> saveCustomBackendUrl(String url) async {
    final cleanedUrl = _cleanUrl(url);
    await _prefs.setString(_backendUrlKey, cleanedUrl);
    await _prefs.setBool(_useCustomUrlKey, true);
  }

  // Reset to default URL
  Future<void> resetToDefault() async {
    await _prefs.remove(_backendUrlKey);
    await _prefs.setBool(_useCustomUrlKey, false);
  }

  // Disable custom URL (keep saved but use default)
  Future<void> disableCustomUrl() async {
    await _prefs.setBool(_useCustomUrlKey, false);
  }

  // Enable custom URL
  Future<void> enableCustomUrl() async {
    await _prefs.setBool(_useCustomUrlKey, true);
  }

  // URL validation
  static bool isValidUrl(String url) {
    if (url.trim().isEmpty) return false;

    try {
      final uri = Uri.parse(url.trim());
      // Must have http or https scheme
      if (uri.scheme != 'http' && uri.scheme != 'https') {
        return false;
      }
      // Must have a host
      if (uri.host.isEmpty) {
        return false;
      }
      return true;
    } catch (e) {
      return false;
    }
  }

  // Clean URL (remove trailing slash, validate format)
  static String _cleanUrl(String url) {
    String cleaned = url.trim();
    // Remove trailing slash
    if (cleaned.endsWith('/')) {
      cleaned = cleaned.substring(0, cleaned.length - 1);
    }
    return cleaned;
  }

  static String _getDefaultUrl(bool isDevelopment) {
    if (isDevelopment) {
      return defaultDevUrl;
    }
    return defaultProdUrl;
  }

  // Get WebSocket URL from HTTP URL
  static String getWebSocketUrl(String httpUrl) {
    return httpUrl
        .replaceFirst('http://', 'ws://')
        .replaceFirst('https://', 'wss://');
  }
}
