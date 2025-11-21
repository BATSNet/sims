import 'package:flutter/foundation.dart';
import 'package:geolocator/geolocator.dart';
import 'package:permission_handler/permission_handler.dart';

class LocationData {
  final double latitude;
  final double longitude;
  final double? altitude;
  final double? accuracy;
  final double? heading;
  final double? speed;
  final DateTime timestamp;

  LocationData({
    required this.latitude,
    required this.longitude,
    this.altitude,
    this.accuracy,
    this.heading,
    this.speed,
    required this.timestamp,
  });

  Map<String, dynamic> toJson() {
    return {
      'latitude': latitude,
      'longitude': longitude,
      'altitude': altitude,
      'accuracy': accuracy,
      'heading': heading,
      'speed': speed,
      'timestamp': timestamp.toIso8601String(),
    };
  }
}

class LocationService {
  Future<bool> requestPermission() async {
    try {
      // Check permission status (don't request - should be granted at app startup)
      final permission = await Geolocator.checkPermission();
      return permission == LocationPermission.always ||
          permission == LocationPermission.whileInUse;
    } catch (e) {
      debugPrint('Error checking location permission: $e');
      return false;
    }
  }

  Future<bool> isLocationServiceEnabled() async {
    return await Geolocator.isLocationServiceEnabled();
  }

  Future<LocationData?> getCurrentLocation() async {
    try {
      final serviceEnabled = await isLocationServiceEnabled();
      if (!serviceEnabled) {
        debugPrint('Location services are disabled');
        return null;
      }

      LocationPermission permission = await Geolocator.checkPermission();
      if (permission == LocationPermission.denied) {
        debugPrint('Location permission not granted - please grant permissions in app settings');
        return null;
      }

      if (permission == LocationPermission.deniedForever) {
        debugPrint('Location permission permanently denied');
        return null;
      }

      debugPrint('Attempting to get location with high accuracy...');

      Position? position;

      // Try with high accuracy first
      try {
        position = await Geolocator.getCurrentPosition(
          desiredAccuracy: LocationAccuracy.high,
          timeLimit: const Duration(seconds: 15),
        );
        debugPrint('Got location with high accuracy: ${position.latitude}, ${position.longitude}');
      } catch (e) {
        debugPrint('High accuracy location failed: $e');

        // Fallback to medium accuracy for older devices
        try {
          debugPrint('Attempting location with medium accuracy...');
          position = await Geolocator.getCurrentPosition(
            desiredAccuracy: LocationAccuracy.medium,
            timeLimit: const Duration(seconds: 20),
          );
          debugPrint('Got location with medium accuracy: ${position.latitude}, ${position.longitude}');
        } catch (e2) {
          debugPrint('Medium accuracy location failed: $e2');

          // Last resort: try low accuracy
          try {
            debugPrint('Attempting location with low accuracy...');
            position = await Geolocator.getCurrentPosition(
              desiredAccuracy: LocationAccuracy.low,
              timeLimit: const Duration(seconds: 30),
            );
            debugPrint('Got location with low accuracy: ${position.latitude}, ${position.longitude}');
          } catch (e3) {
            debugPrint('All location accuracy levels failed: $e3');

            // Final fallback: try to get last known position
            try {
              debugPrint('Attempting to get last known position...');
              position = await Geolocator.getLastKnownPosition();
              if (position != null) {
                debugPrint('Using last known position: ${position.latitude}, ${position.longitude}');
              } else {
                debugPrint('No last known position available');
              }
            } catch (e4) {
              debugPrint('Failed to get last known position: $e4');
            }
          }
        }
      }

      if (position == null) {
        debugPrint('Unable to obtain any location data');
        return null;
      }

      return LocationData(
        latitude: position.latitude,
        longitude: position.longitude,
        altitude: position.altitude,
        accuracy: position.accuracy,
        heading: position.heading,
        speed: position.speed,
        timestamp: position.timestamp ?? DateTime.now(),
      );
    } catch (e) {
      debugPrint('Error getting current location: $e');
      return null;
    }
  }

  Stream<LocationData> getLocationStream() {
    return Geolocator.getPositionStream(
      locationSettings: const LocationSettings(
        accuracy: LocationAccuracy.high,
        distanceFilter: 10,
      ),
    ).map((position) => LocationData(
          latitude: position.latitude,
          longitude: position.longitude,
          altitude: position.altitude,
          accuracy: position.accuracy,
          heading: position.heading,
          speed: position.speed,
          timestamp: position.timestamp ?? DateTime.now(),
        ));
  }

  Future<double> getDistanceBetween(
    double startLatitude,
    double startLongitude,
    double endLatitude,
    double endLongitude,
  ) async {
    return Geolocator.distanceBetween(
      startLatitude,
      startLongitude,
      endLatitude,
      endLongitude,
    );
  }
}
