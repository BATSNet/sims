import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

import '../config/app_config.dart';
import 'meshtastic_ble_service.dart';

/// LoRa Mesh Gateway - relays HTTP API requests from the mesh to the server.
///
/// Listens for RELAY: text messages on the mesh, makes the corresponding HTTP
/// request to the backend, and sends the response back as RELAY_RESP: (or
/// chunked RELAY_RESP_C: for large responses).
class LoraGatewayService {
  static final LoraGatewayService _instance = LoraGatewayService._internal();
  factory LoraGatewayService() => _instance;
  LoraGatewayService._internal();

  final MeshtasticBleService _meshService = MeshtasticBleService();
  StreamSubscription<String>? _textSub;

  bool _enabled = false;
  bool get isEnabled => _enabled;

  int _relayedCount = 0;
  int get relayedCount => _relayedCount;

  final _logController = StreamController<String>.broadcast();
  Stream<String> get logStream => _logController.stream;

  static const String _relayPrefix = 'RELAY:';
  static const String _respPrefix = 'RELAY_RESP:';
  static const String _chunkPrefix = 'RELAY_RESP_C:';
  static const int _maxMsgSize = 230;
  static const int _chunkPayloadSize = 200;
  static const Duration _httpTimeout = Duration(seconds: 15);
  // Delay between sending chunks to avoid overwhelming the radio queue
  static const Duration _chunkDelay = Duration(milliseconds: 500);

  /// Gateway forwards to the configured backend URL.
  String get _baseUrl => AppConfig.baseUrl;

  /// Start the gateway - begin listening for RELAY: messages.
  void start() {
    if (_enabled) return;
    _enabled = true;

    _textSub?.cancel();
    _textSub = _meshService.receivedTextStream.listen(_onTextReceived);

    _log('Gateway started - forwarding to $_baseUrl');
  }

  /// Stop the gateway.
  void stop() {
    _enabled = false;
    _textSub?.cancel();
    _textSub = null;
    _log('Gateway stopped');
  }

  void _log(String msg) {
    debugPrint('[LoRa Gateway] $msg');
    _logController.add(msg);
  }

  Future<void> _onTextReceived(String text) async {
    if (!text.startsWith(_relayPrefix)) return;

    _log('Received relay request: '
        '${text.substring(0, min(80, text.length))}...');

    String reqId = 'unknown';
    String respJson;

    try {
      final jsonStr = text.substring(_relayPrefix.length);
      final request = jsonDecode(jsonStr) as Map<String, dynamic>;

      reqId = request['id'] as String? ?? 'unknown';
      final method = (request['method'] as String).toUpperCase();
      final path = request['path'] as String;
      final body = request['body'] as String?;
      final contentType = request['contentType'] as String?;

      final url = Uri.parse('$_baseUrl$path');
      _log('$reqId: $method $url');

      final headers = <String, String>{};
      if (body != null) {
        headers['Content-Type'] = contentType ?? 'application/json';
      }

      final httpRequest = http.Request(method, url);
      httpRequest.headers.addAll(headers);
      if (body != null) httpRequest.body = body;

      final client = http.Client();
      try {
        final streamedResponse = await client
            .send(httpRequest)
            .timeout(_httpTimeout);
        final respBody = await streamedResponse.stream.bytesToString();

        respJson = jsonEncode({
          'req_id': reqId,
          'status': streamedResponse.statusCode,
          'body': respBody,
        });

        _log('$reqId: ${streamedResponse.statusCode} '
            '(${respBody.length} bytes)');
      } finally {
        client.close();
      }
    } on TimeoutException {
      respJson = jsonEncode({
        'req_id': reqId,
        'status': 502,
        'body': 'Gateway error: connection timed out',
      });
      _log('$reqId: timeout');
    } on FormatException catch (e) {
      respJson = jsonEncode({
        'req_id': reqId,
        'status': 400,
        'body': 'Gateway error: malformed relay request: $e',
      });
      _log('$reqId: malformed request');
    } catch (e) {
      respJson = jsonEncode({
        'req_id': reqId,
        'status': 502,
        'body': 'Gateway error: $e',
      });
      _log('$reqId: error: $e');
    }

    await _sendResponse(respJson, reqId);
    _relayedCount++;
  }

  /// Send a relay response, chunking if necessary.
  Future<void> _sendResponse(String respJson, String reqId) async {
    final fullMsg = '$_respPrefix$respJson';

    if (utf8.encode(fullMsg).length <= _maxMsgSize) {
      // Fits in a single message
      await _meshService.sendTextMessage(fullMsg);
      _log('Sent response for $reqId (single, ${fullMsg.length} chars)');
    } else {
      // Chunk the JSON payload
      final chunks = <String>[];
      for (var i = 0; i < respJson.length; i += _chunkPayloadSize) {
        chunks.add(respJson.substring(
          i,
          min(i + _chunkPayloadSize, respJson.length),
        ));
      }

      _log('Chunking response for $reqId into ${chunks.length} parts');

      for (var idx = 0; idx < chunks.length; idx++) {
        final msg =
            '$_chunkPrefix$reqId:$idx/${chunks.length}:${chunks[idx]}';
        await _meshService.sendTextMessage(msg);

        // Delay between chunks to avoid flooding the radio
        if (idx < chunks.length - 1) {
          await Future.delayed(_chunkDelay);
        }
      }

      _log('Sent ${chunks.length} chunks for $reqId');
    }
  }

  void dispose() {
    stop();
    _logController.close();
  }
}
