import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import '../config/app_config.dart';
import '../models/incident.dart';

class WebSocketService {
  static final WebSocketService _instance = WebSocketService._internal();

  factory WebSocketService() {
    return _instance;
  }

  WebSocketService._internal();

  WebSocketChannel? _channel;
  final _messageController = StreamController<dynamic>.broadcast();
  final _connectionStateController = StreamController<bool>.broadcast();

  bool _isConnected = false;
  Timer? _reconnectTimer;
  Timer? _pingTimer;

  // State storage
  final List<Incident> _incidents = [];
  Map<String, dynamic> _stats = {};

  // Getters
  Stream<dynamic> get messageStream => _messageController.stream;
  Stream<bool> get connectionStateStream => _connectionStateController.stream;
  bool get isConnected => _isConnected;
  List<Incident> get incidents => List.unmodifiable(_incidents);
  Map<String, dynamic> get stats => Map.unmodifiable(_stats);

  // Force reconnect with new URL (call after settings change)
  Future<void> reconnect() async {
    debugPrint('Reconnecting WebSocket with new URL...');
    disconnect();
    await Future.delayed(const Duration(milliseconds: 500));
    await connect();
  }

  Future<void> connect({String? token}) async {
    if (_isConnected) {
      debugPrint('WebSocket already connected');
      return;
    }

    try {
      final wsUrl = AppConfig.baseUrl
          .replaceFirst('http://', 'ws://')
          .replaceFirst('https://', 'wss://');

      final uri = Uri.parse('$wsUrl/ws/incidents');

      debugPrint('Connecting to WebSocket: $uri');

      final headers = <String, dynamic>{
        'sims_app_id': 'sims_mobile_2024',
      };

      if (token != null) {
        headers['Authorization'] = 'Bearer $token';
      }

      _channel = IOWebSocketChannel.connect(
        uri,
        headers: headers,
        pingInterval: const Duration(seconds: 30),
        connectTimeout: const Duration(seconds: 5),
      );

      _channel?.stream.listen(
        _handleMessage,
        onError: _handleError,
        onDone: _handleDisconnect,
        cancelOnError: false,
      );

      _isConnected = true;
      _connectionStateController.add(true);
      _startPingTimer();

      debugPrint('WebSocket connected successfully');
    } catch (e) {
      debugPrint('WebSocket connection error (will retry): $e');
      _handleError(e);
    }
  }

  void _handleMessage(dynamic message) {
    try {
      final decodedMessage = jsonDecode(message);
      debugPrint('WebSocket message received: ${decodedMessage['type']}');

      // Store messages in service state
      switch (decodedMessage['type']) {
        case 'incident_new':
          final incident = Incident.fromJson(decodedMessage['data']);
          _incidents.insert(0, incident);
          break;

        case 'incident_update':
          final updatedIncident = Incident.fromJson(decodedMessage['data']);
          final index = _incidents.indexWhere((i) => i.id == updatedIncident.id);
          if (index != -1) {
            _incidents[index] = updatedIncident;
          }
          break;

        case 'incident_delete':
          final id = decodedMessage['data']['id'];
          _incidents.removeWhere((i) => i.id == id);
          break;

        case 'stats':
          _stats = Map<String, dynamic>.from(decodedMessage['data']);
          break;

        case 'chat_message':
          debugPrint('Received chat message for incident: ${decodedMessage['incident_id']}');
          // Will be handled by listeners
          break;

        case 'pong':
          debugPrint('Received pong from server');
          break;
      }

      // Broadcast to all listeners
      _messageController.add(decodedMessage);
    } catch (e) {
      debugPrint('Error handling WebSocket message: $e');
    }
  }

  void _handleError(dynamic error) {
    debugPrint('WebSocket error: $error');
    _isConnected = false;
    _connectionStateController.add(false);
    _scheduleReconnect();
  }

  void _handleDisconnect() {
    debugPrint('WebSocket disconnected');
    _isConnected = false;
    _connectionStateController.add(false);
    _pingTimer?.cancel();
    _scheduleReconnect();
  }

  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(const Duration(seconds: 30), () {
      debugPrint('Attempting to reconnect WebSocket...');
      connect();
    });
  }

  void _startPingTimer() {
    _pingTimer?.cancel();
    _pingTimer = Timer.periodic(const Duration(seconds: 30), (timer) {
      if (_isConnected) {
        sendMessage({'type': 'ping'});
      }
    });
  }

  void sendMessage(Map<String, dynamic> message) {
    if (!_isConnected || _channel == null) {
      debugPrint('Cannot send message: WebSocket not connected');
      return;
    }

    try {
      _channel?.sink.add(jsonEncode(message));
      debugPrint('WebSocket message sent: ${message['type']}');
    } catch (e) {
      debugPrint('Error sending WebSocket message: $e');
    }
  }

  void subscribeToIncidents() {
    sendMessage({
      'type': 'subscribe',
      'channel': 'incidents',
    });
  }

  void unsubscribeFromIncidents() {
    sendMessage({
      'type': 'unsubscribe',
      'channel': 'incidents',
    });
  }

  void disconnect() {
    debugPrint('Disconnecting WebSocket');
    _reconnectTimer?.cancel();
    _pingTimer?.cancel();
    _channel?.sink.close();
    _isConnected = false;
    _connectionStateController.add(false);
  }

  void clearState() {
    _incidents.clear();
    _stats.clear();
  }

  void dispose() {
    disconnect();
    _messageController.close();
    _connectionStateController.close();
  }
}
