import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:geolocator/geolocator.dart';

class PermissionService {
  static final PermissionService _instance = PermissionService._internal();

  factory PermissionService() {
    return _instance;
  }

  PermissionService._internal();

  Future<bool> checkAndRequestAllPermissions() async {
    final permissions = await _checkAllPermissions();

    if (permissions.every((status) => status.isGranted)) {
      return true;
    }

    return await _requestAllPermissions();
  }

  Future<List<PermissionStatus>> _checkAllPermissions() async {
    return [
      await Permission.camera.status,
      await Permission.microphone.status,
      await Permission.location.status,
      await Permission.locationWhenInUse.status,
      await Permission.bluetoothScan.status,
      await Permission.bluetoothConnect.status,
    ];
  }

  Future<bool> _requestAllPermissions() async {
    final cameraStatus = await Permission.camera.request();
    final microphoneStatus = await Permission.microphone.request();

    // Request Bluetooth permissions (needed for mesh fallback)
    final btScanStatus = await Permission.bluetoothScan.request();
    final btConnectStatus = await Permission.bluetoothConnect.request();
    if (!btScanStatus.isGranted || !btConnectStatus.isGranted) {
      debugPrint('Bluetooth permissions not granted - mesh fallback will be unavailable');
    }

    bool locationGranted = false;
    try {
      final permission = await Geolocator.checkPermission();
      if (permission == LocationPermission.denied) {
        final requested = await Geolocator.requestPermission();
        locationGranted = requested == LocationPermission.always ||
            requested == LocationPermission.whileInUse;
      } else {
        locationGranted = permission == LocationPermission.always ||
            permission == LocationPermission.whileInUse;
      }
    } catch (e) {
      debugPrint('Error requesting location permission: $e');
      locationGranted = false;
    }

    // BLE permissions are optional - don't block app if denied
    return cameraStatus.isGranted &&
        microphoneStatus.isGranted &&
        locationGranted;
  }

  Future<void> showPermissionDialog(BuildContext context) async {
    return showDialog(
      context: context,
      barrierDismissible: false,
      builder: (BuildContext context) {
        return AlertDialog(
          title: const Text('Permissions Required'),
          content: const Text(
            'SIMS requires the following permissions to function properly:\n\n'
            '- Camera - to capture incident photos and videos\n'
            '- Microphone - to record audio descriptions\n'
            '- Location - to automatically capture incident location\n'
            '- Bluetooth - for mesh network fallback (optional)\n\n'
            'Camera, microphone, and location are mandatory.',
          ),
          actions: [
            TextButton(
              onPressed: () async {
                Navigator.of(context).pop();
                final granted = await checkAndRequestAllPermissions();
                if (!granted && context.mounted) {
                  await _showSettingsDialog(context);
                }
              },
              child: const Text('Grant Permissions'),
            ),
          ],
        );
      },
    );
  }

  Future<void> _showSettingsDialog(BuildContext context) async {
    return showDialog(
      context: context,
      barrierDismissible: false,
      builder: (BuildContext context) {
        return AlertDialog(
          title: const Text('Permissions Denied'),
          content: const Text(
            'Some permissions were denied. Please grant all required permissions in Settings to use SIMS.',
          ),
          actions: [
            TextButton(
              onPressed: () async {
                await openAppSettings();
                Navigator.of(context).pop();
              },
              child: const Text('Open Settings'),
            ),
          ],
        );
      },
    );
  }

  Future<bool> hasAllPermissions() async {
    final permissions = await _checkAllPermissions();
    return permissions.every((status) => status.isGranted);
  }
}
