import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

import '../config/app_config.dart';
import '../mesh/meshtastic_ble_service.dart';
import '../mesh/binary_incident_encoder.dart';
import '../mesh/lora_gateway_service.dart';
import '../services/degraded_mode_service.dart';
import 'websocket_service.dart';

enum ConnectionMode { normal, meshScanning, meshConnected, disconnected }

/// Central orchestrator for transport switching between WebSocket and mesh BLE.
///
/// Monitors WebSocket health. After sustained disconnection, falls back to
/// Meshtastic BLE mesh networking. Continues attempting WS reconnection in
/// background and switches back when connectivity returns.
class ConnectionManager {
  static final ConnectionManager _instance = ConnectionManager._internal();
  factory ConnectionManager() => _instance;
  ConnectionManager._internal();

  final _modeController = StreamController<ConnectionMode>.broadcast();
  Stream<ConnectionMode> get modeStream => _modeController.stream;

  ConnectionMode _mode = ConnectionMode.normal;
  ConnectionMode get mode => _mode;

  final WebSocketService _wsService = WebSocketService();
  final MeshtasticBleService _meshService = MeshtasticBleService();
  final DegradedModeService _degradedService = DegradedModeService();
  final LoraGatewayService _gatewayService = LoraGatewayService();

  StreamSubscription<bool>? _wsSub;
  StreamSubscription<MeshConnectionState>? _meshSub;
  StreamSubscription<Uint8List>? _receivedDataSub;
  Timer? _meshFallbackTimer;

  int _consecutiveWsFailures = 0;
  DateTime? _lastWsDisconnect;
  bool _meshFallbackEnabled = true;
  bool _gatewayEnabled = false;
  bool _initialized = false;

  // Mesh fallback triggers after this duration of WS disconnection
  static const Duration _meshFallbackDelay = Duration(seconds: 60);
  static const int _maxWsFailuresBeforeMesh = 3;

  MeshtasticBleService get meshService => _meshService;
  DegradedModeService get degradedService => _degradedService;
  LoraGatewayService get gatewayService => _gatewayService;
  bool get isMeshMode => _mode == ConnectionMode.meshConnected;
  bool get isNormalMode => _mode == ConnectionMode.normal;
  bool get gatewayEnabled => _gatewayEnabled;

  void _setMode(ConnectionMode newMode) {
    if (_mode == newMode) return;
    _mode = newMode;
    _modeController.add(newMode);
    debugPrint('Connection mode: $newMode');
  }

  /// Initialize the connection manager. Call once at app startup.
  void initialize() {
    if (_initialized) return;
    _initialized = true;

    // Monitor WebSocket connection state
    _wsSub = _wsService.connectionStateStream.listen((connected) {
      if (connected) {
        _onWsConnected();
      } else {
        _onWsDisconnected();
      }
    });

    // Monitor mesh BLE connection state
    _meshSub = _meshService.stateStream.listen((meshState) {
      switch (meshState) {
        case MeshConnectionState.ready:
          if (_mode != ConnectionMode.normal) {
            _setMode(ConnectionMode.meshConnected);
          }
          // Start gateway if enabled and mesh is ready
          if (_gatewayEnabled && !_gatewayService.isEnabled) {
            _gatewayService.start();
          }
          break;
        case MeshConnectionState.scanning:
          if (_mode != ConnectionMode.normal) {
            _setMode(ConnectionMode.meshScanning);
          }
          break;
        case MeshConnectionState.disconnected:
          if (_mode == ConnectionMode.meshConnected ||
              _mode == ConnectionMode.meshScanning) {
            _setMode(ConnectionMode.disconnected);
          }
          // Stop gateway when mesh disconnects
          if (_gatewayService.isEnabled) {
            _gatewayService.stop();
          }
          break;
        default:
          break;
      }
    });

    // Listen for incoming mesh data to forward to backend (phone-as-gateway)
    _receivedDataSub = _meshService.receivedDataStream.listen((payload) {
      _forwardMeshPayloadToBackend(payload);
    });

    // Set initial mode based on WS state
    if (_wsService.isConnected) {
      _setMode(ConnectionMode.normal);
    } else {
      _setMode(ConnectionMode.disconnected);
    }
  }

  void _onWsConnected() {
    _consecutiveWsFailures = 0;
    _lastWsDisconnect = null;
    _meshFallbackTimer?.cancel();

    // Switch back to normal mode
    _setMode(ConnectionMode.normal);

    debugPrint('WebSocket reconnected - back to normal mode');
  }

  void _onWsDisconnected() {
    _consecutiveWsFailures++;
    _lastWsDisconnect ??= DateTime.now();

    debugPrint('WebSocket disconnected (failures: $_consecutiveWsFailures)');

    if (!_meshFallbackEnabled) return;

    // Trigger mesh fallback after sustained disconnection
    if (_consecutiveWsFailures >= _maxWsFailuresBeforeMesh) {
      _startMeshFallback();
    } else {
      // Start timer for delayed fallback
      _meshFallbackTimer?.cancel();
      _meshFallbackTimer = Timer(_meshFallbackDelay, () {
        if (!_wsService.isConnected) {
          _startMeshFallback();
        }
      });
    }

    if (_mode == ConnectionMode.normal) {
      _setMode(ConnectionMode.disconnected);
    }
  }

  void _startMeshFallback() {
    if (_mode == ConnectionMode.meshConnected ||
        _mode == ConnectionMode.meshScanning) {
      return; // Already in mesh mode
    }

    debugPrint('Starting mesh BLE fallback...');
    _setMode(ConnectionMode.meshScanning);
    _meshService.enableAutoReconnect();
    _meshService.scanAndConnect();

    // Pre-initialize Vosk for offline STT
    _degradedService.initializeVosk();
  }

  /// Send an incident via the appropriate transport.
  ///
  /// In normal mode: returns null (caller should use HTTP POST as usual).
  /// In mesh mode: encodes and sends binary payload via BLE.
  Future<bool?> sendIncidentViaMesh({
    required String description,
    required double latitude,
    required double longitude,
    double altitude = 0,
    String priority = 'medium',
    Uint8List? compressedImage,
    required String deviceId,
  }) async {
    if (_mode != ConnectionMode.meshConnected) {
      return null; // Not in mesh mode
    }

    final deviceMac = BinaryIncidentEncoder.generateDeviceMac(deviceId);
    final payload = BinaryIncidentEncoder.encode(
      description: description,
      latitude: latitude,
      longitude: longitude,
      altitude: altitude,
      deviceMac: deviceMac,
      priority: priority,
      imageBytes: compressedImage,
    );

    return await _meshService.sendPayload(payload);
  }

  /// Manually trigger a mesh scan.
  Future<bool> manualMeshScan() async {
    return await _meshService.scanAndConnect();
  }

  /// Toggle mesh fallback feature.
  void setMeshFallbackEnabled(bool enabled) {
    _meshFallbackEnabled = enabled;
    if (!enabled) {
      _meshFallbackTimer?.cancel();
      if (_mode == ConnectionMode.meshScanning) {
        _meshService.disconnect();
        _setMode(_wsService.isConnected
            ? ConnectionMode.normal
            : ConnectionMode.disconnected);
      }
    }
  }

  bool get meshFallbackEnabled => _meshFallbackEnabled;

  /// Toggle the LoRa gateway feature.
  /// When enabled, the phone relays RELAY: requests from the mesh to the server.
  void setGatewayEnabled(bool enabled) {
    _gatewayEnabled = enabled;
    if (enabled) {
      // Start immediately if mesh is already connected
      if (_meshService.state == MeshConnectionState.ready) {
        _gatewayService.start();
      }
    } else {
      _gatewayService.stop();
    }
  }

  /// Forward a raw binary incident payload received from mesh to the backend.
  /// Acts as a gateway: mesh device -> BLE -> phone -> HTTP -> server.
  Future<void> _forwardMeshPayloadToBackend(Uint8List payload) async {
    if (!_wsService.isConnected) {
      debugPrint('Cannot forward mesh payload: no internet connectivity');
      return;
    }

    try {
      final url = Uri.parse('${AppConfig.baseUrl}/api/lora/incident');
      debugPrint('Forwarding mesh payload to backend: ${payload.length} bytes -> $url');

      final response = await http.post(
        url,
        headers: {'Content-Type': 'application/octet-stream'},
        body: payload,
      );

      if (response.statusCode >= 200 && response.statusCode < 300) {
        debugPrint('Mesh payload forwarded successfully (${response.statusCode})');
      } else {
        debugPrint('Backend rejected mesh payload: ${response.statusCode} ${response.body}');
      }
    } catch (e) {
      debugPrint('Error forwarding mesh payload to backend: $e');
    }
  }

  void dispose() {
    _wsSub?.cancel();
    _meshSub?.cancel();
    _receivedDataSub?.cancel();
    _meshFallbackTimer?.cancel();
    _gatewayService.dispose();
    _meshService.dispose();
    _degradedService.dispose();
    _modeController.close();
    _initialized = false;
  }
}
