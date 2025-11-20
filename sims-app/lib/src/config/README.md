# App Configuration

## Quick Start: Changing IP/Port

**To change backend IP and port, edit this file:**
```
sims-app/lib/src/config/app_config.dart
```

### For Android Emulator (localhost)

Change the port number in `devBaseUrl`:
```dart
static const String devBaseUrl = 'http://10.0.2.2:8080';  // Change :8080 to your port
```

**Examples:**
- Backend on port 3000: `'http://10.0.2.2:3000'`
- Backend on port 5000: `'http://10.0.2.2:5000'`
- Backend on port 8000: `'http://10.0.2.2:8000'`

**Important:** Always use `10.0.2.2` for emulator (not `localhost` or `127.0.0.1`)

### For Physical Android Device

1. Find your computer's IP address:
   - Windows: Open cmd and run `ipconfig`, look for "IPv4 Address"
   - Mac: Open terminal and run `ifconfig`, look for `en0` inet address
   - Linux: Run `ip addr` or `ifconfig`

2. Update `devBaseUrlPhysical`:
```dart
static const String devBaseUrlPhysical = 'http://YOUR_IP:PORT';
```

**Examples:**
- `'http://192.168.1.100:8080'`
- `'http://10.0.0.5:3000'`
- `'http://172.16.0.10:5000'`

**Note:** Your phone and computer must be on the same WiFi network!

### Switching to Production

```dart
static const bool isDevelopment = false;  // Change to false
static const String prodBaseUrl = 'https://your-server.com';  // Your production URL
```

## Configuration File Structure

The `app_config.dart` file contains:

```dart
class AppConfig {
  // Toggle between dev and production
  static const bool isDevelopment = true;

  // Development URLs
  static const String devBaseUrl = 'http://10.0.2.2:8080';              // For emulator
  static const String devBaseUrlPhysical = 'http://192.168.1.100:8080'; // For physical device

  // Production URL
  static const String prodBaseUrl = 'https://api.sims.example.com';

  // Computed properties (don't change these)
  static String get baseUrl {
    if (isDevelopment) {
      return devBaseUrl;  // Or devBaseUrlPhysical for physical device
    }
    return prodBaseUrl;
  }

  static String get chatUrl => '$baseUrl/incident';
}
```

## Backend Requirements

The backend should expose:
- `/incident` endpoint for the chat interface
- Accepts `location` header with format: `latitude;longitude` (e.g., `54.6872;25.2797`)
- Provides hidden elements: `[sims-chat-input]` and `[sims-chat-submit]`
- Sends messages via JavaScript: `window.Flutter.postMessage({type: 'loading', value: true})`

## After Changing Configuration

1. Save the `app_config.dart` file
2. Restart the Flutter app (hot reload may not apply config changes)
3. Check console logs for connection errors if WebView doesn't load

## Troubleshooting

**Problem:** WebView shows blank screen
- Check if backend is running on the configured port
- Verify emulator uses `10.0.2.2`, not `localhost`
- Check console for "WebView error:" messages

**Problem:** Physical device can't connect
- Verify both devices on same WiFi
- Check firewall isn't blocking the port
- Ping your computer's IP from phone to test connectivity

**Problem:** Still seeing old URL
- Do a full app restart (not hot reload)
- Try `flutter clean` then `flutter run`
