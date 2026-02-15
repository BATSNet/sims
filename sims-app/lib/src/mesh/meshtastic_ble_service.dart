import 'dart:async';
import 'dart:math';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'meshtastic_constants.dart';
import 'mesh_packet_builder.dart';

enum MeshConnectionState { disconnected, scanning, connecting, configuring, ready }

/// Singleton BLE service for Meshtastic mesh device communication.
///
/// Handles scanning, connecting, config exchange, and sending data packets.
/// FromRadio is READ-ONLY (no NOTIFY) - phone drives all reads.
class MeshtasticBleService {
  static final MeshtasticBleService _instance = MeshtasticBleService._internal();
  factory MeshtasticBleService() => _instance;
  MeshtasticBleService._internal();

  final _stateController = StreamController<MeshConnectionState>.broadcast();
  Stream<MeshConnectionState> get stateStream => _stateController.stream;

  final _receivedDataController = StreamController<Uint8List>.broadcast();
  /// Stream of received PRIVATE_APP payloads from the mesh network.
  /// Each emission is a raw binary incident payload ready to forward to the backend.
  Stream<Uint8List> get receivedDataStream => _receivedDataController.stream;

  final _receivedTextController = StreamController<String>.broadcast();
  /// Stream of received TEXT_MESSAGE_APP strings from the mesh network.
  Stream<String> get receivedTextStream => _receivedTextController.stream;

  MeshConnectionState _state = MeshConnectionState.disconnected;
  MeshConnectionState get state => _state;

  BluetoothDevice? _device;
  BluetoothCharacteristic? _toRadio;
  BluetoothCharacteristic? _fromRadio;
  BluetoothCharacteristic? _fromNum;
  StreamSubscription? _connectionSub;
  StreamSubscription? _fromNumSub;
  Timer? _reconnectTimer;
  int _reconnectAttempts = 0;
  bool _autoReconnect = true;
  int _packetIdCounter = 0;

  String? get connectedDeviceName => _device?.platformName;

  void _setState(MeshConnectionState newState) {
    _state = newState;
    _stateController.add(newState);
    debugPrint('Mesh BLE state: $newState');
  }

  /// Start scanning for Meshtastic devices and connect to the strongest one.
  Future<bool> scanAndConnect() async {
    if (_state == MeshConnectionState.scanning ||
        _state == MeshConnectionState.connecting) {
      debugPrint('Already scanning/connecting');
      return false;
    }

    _setState(MeshConnectionState.scanning);

    try {
      // Check if Bluetooth is on
      final adapterState = await FlutterBluePlus.adapterState.first;
      if (adapterState != BluetoothAdapterState.on) {
        debugPrint('Bluetooth is not enabled');
        _setState(MeshConnectionState.disconnected);
        return false;
      }

      // Scan for devices with Meshtastic service UUID
      BluetoothDevice? bestDevice;
      int bestRssi = -999;

      final scanSub = FlutterBluePlus.onScanResults.listen((results) {
        for (final result in results) {
          final hasService = result.advertisementData.serviceUuids.any(
            (uuid) => uuid.toString().toLowerCase() == MeshtasticConstants.serviceUuid,
          );
          if (hasService && result.rssi > bestRssi) {
            bestRssi = result.rssi;
            bestDevice = result.device;
            debugPrint('Found Meshtastic device: ${result.device.platformName} '
                'RSSI: ${result.rssi}');
          }
        }
      });

      await FlutterBluePlus.startScan(
        withServices: [Guid(MeshtasticConstants.serviceUuid)],
        timeout: MeshtasticConstants.scanTimeout,
      );

      await scanSub.cancel();

      if (bestDevice == null) {
        debugPrint('No Meshtastic devices found');
        _setState(MeshConnectionState.disconnected);
        return false;
      }

      return await _connectToDevice(bestDevice!);
    } catch (e) {
      debugPrint('Scan error: $e');
      _setState(MeshConnectionState.disconnected);
      return false;
    }
  }

  Future<bool> _connectToDevice(BluetoothDevice device) async {
    _setState(MeshConnectionState.connecting);

    try {
      // Monitor connection state for auto-reconnect
      _connectionSub?.cancel();
      _connectionSub = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          debugPrint('Mesh device disconnected');
          _onDisconnected();
        }
      });

      await device.connect(
        timeout: MeshtasticConstants.connectTimeout,
        autoConnect: false,
      );

      _device = device;

      // Request larger MTU
      await device.requestMtu(MeshtasticConstants.desiredMtu);

      // Discover services
      final services = await device.discoverServices();
      final meshService = services.firstWhere(
        (s) => s.uuid.toString().toLowerCase() == MeshtasticConstants.serviceUuid,
        orElse: () => throw Exception('Meshtastic service not found'),
      );

      // Get characteristics
      for (final c in meshService.characteristics) {
        final uuid = c.uuid.toString().toLowerCase();
        if (uuid == MeshtasticConstants.toRadioUuid) _toRadio = c;
        if (uuid == MeshtasticConstants.fromRadioUuid) _fromRadio = c;
        if (uuid == MeshtasticConstants.fromNumUuid) _fromNum = c;
      }

      if (_toRadio == null || _fromRadio == null || _fromNum == null) {
        throw Exception('Missing required Meshtastic characteristics');
      }

      // Subscribe to FromNum notifications - triggers FromRadio polling
      await _fromNum!.setNotifyValue(true);
      _fromNumSub?.cancel();
      _fromNumSub = _fromNum!.onValueReceived.listen((value) {
        debugPrint('FromNum notification: ${value.length} bytes');
        // Only poll for incoming packets when fully configured (ready state)
        if (_state == MeshConnectionState.ready) {
          _pollFromRadio();
        }
      });

      // Perform config exchange
      await _performConfigExchange();

      _reconnectAttempts = 0;
      _setState(MeshConnectionState.ready);
      debugPrint('Connected to mesh device: ${device.platformName}');
      return true;
    } catch (e) {
      debugPrint('Connection error: $e');
      _device = null;
      _toRadio = null;
      _fromRadio = null;
      _fromNum = null;
      _setState(MeshConnectionState.disconnected);
      return false;
    }
  }

  /// Config exchange: write want_config_id, then poll FromRadio for
  /// MyNodeInfo -> NodeInfo -> config_complete_id.
  Future<void> _performConfigExchange() async {
    _setState(MeshConnectionState.configuring);

    final configNonce = Random().nextInt(0x7FFFFFFF) + 1;
    debugPrint('Starting config exchange with nonce: $configNonce');

    // Write want_config_id to ToRadio
    final wantConfig = MeshPacketBuilder.buildWantConfig(configNonce);
    await _toRadio!.write(wantConfig.toList(), withoutResponse: false);

    // Poll FromRadio until config_complete_id
    final deadline = DateTime.now().add(MeshtasticConstants.configTimeout);
    var configComplete = false;

    while (!configComplete && DateTime.now().isBefore(deadline)) {
      await Future.delayed(MeshtasticConstants.configPollInterval);

      try {
        final data = await _fromRadio!.read();
        if (data.isEmpty) {
          // No more data to read
          continue;
        }

        debugPrint('FromRadio: ${data.length} bytes');

        // Check for config_complete_id (field 8, varint matching our nonce)
        // Simple heuristic: if we get an empty read after getting data, config is done
        // The firmware sends MyNodeInfo, NodeInfo, then config_complete_id
        if (_isConfigComplete(Uint8List.fromList(data), configNonce)) {
          configComplete = true;
          debugPrint('Config exchange complete');
        }
      } catch (e) {
        debugPrint('FromRadio read error: $e');
      }
    }

    if (!configComplete) {
      debugPrint('Config exchange timed out - proceeding anyway');
    }
  }

  /// Check if a FromRadio message contains config_complete_id matching our nonce.
  /// FromRadio.config_complete_id is field 8, varint.
  bool _isConfigComplete(Uint8List data, int nonce) {
    // Parse protobuf looking for field 8 varint
    var pos = 0;
    while (pos < data.length) {
      if (pos >= data.length) break;

      // Read tag
      var tag = 0;
      var shift = 0;
      while (pos < data.length) {
        final b = data[pos++];
        tag |= (b & 0x7F) << shift;
        if ((b & 0x80) == 0) break;
        shift += 7;
      }

      final fieldNumber = tag >> 3;
      final wireType = tag & 0x07;

      if (wireType == 0) {
        // Varint
        var value = 0;
        shift = 0;
        while (pos < data.length) {
          final b = data[pos++];
          value |= (b & 0x7F) << shift;
          if ((b & 0x80) == 0) break;
          shift += 7;
        }
        if (fieldNumber == 8 && value == nonce) {
          return true;
        }
      } else if (wireType == 2) {
        // Length-delimited - skip
        var len = 0;
        shift = 0;
        while (pos < data.length) {
          final b = data[pos++];
          len |= (b & 0x7F) << shift;
          if ((b & 0x80) == 0) break;
          shift += 7;
        }
        pos += len;
      } else if (wireType == 5) {
        pos += 4; // fixed32
      } else if (wireType == 1) {
        pos += 8; // fixed64
      } else {
        break; // Unknown wire type
      }
    }
    return false;
  }

  /// Poll FromRadio for incoming packets after a FromNum notification.
  /// Reads repeatedly until an empty response, dispatching by portnum.
  Future<void> _pollFromRadio() async {
    if (_fromRadio == null) return;

    try {
      // Read all available FromRadio messages
      while (true) {
        final data = await _fromRadio!.read();
        if (data.isEmpty) break; // No more data

        debugPrint('FromRadio received: ${data.length} bytes');

        final message = MeshPacketBuilder.parseFromRadio(
          Uint8List.fromList(data),
        );
        if (message == null) continue;

        if (message.isPrivateApp) {
          debugPrint('Received PRIVATE_APP payload: ${message.payload.length} bytes');
          _receivedDataController.add(message.payload);
        } else if (message.isTextMessage) {
          final text = message.textContent;
          if (text != null) {
            debugPrint('Received text message (${text.length} chars)'
                '${text.length > 60 ? ': ${text.substring(0, 60)}...' : ': $text'}');
            _receivedTextController.add(text);
          }
        }
      }
    } catch (e) {
      debugPrint('Error polling FromRadio: $e');
    }
  }

  void _onDisconnected() {
    _toRadio = null;
    _fromRadio = null;
    _fromNum = null;
    _fromNumSub?.cancel();
    _setState(MeshConnectionState.disconnected);

    if (_autoReconnect) {
      _scheduleReconnect();
    }
  }

  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    _reconnectAttempts++;

    final delay = Duration(
      milliseconds: min(
        MeshtasticConstants.reconnectInitial.inMilliseconds *
            pow(2, _reconnectAttempts - 1).toInt(),
        MeshtasticConstants.reconnectMax.inMilliseconds,
      ),
    );

    debugPrint('Scheduling mesh reconnect in ${delay.inSeconds}s '
        '(attempt $_reconnectAttempts)');

    _reconnectTimer = Timer(delay, () {
      scanAndConnect();
    });
  }

  /// Send a text message as a TEXT_MESSAGE_APP mesh packet (broadcast).
  Future<bool> sendTextMessage(String text) async {
    if (_state != MeshConnectionState.ready || _toRadio == null) {
      debugPrint('Cannot send text: mesh not ready (state: $_state)');
      return false;
    }

    try {
      _packetIdCounter++;
      final packet = MeshPacketBuilder.buildTextPacket(
        text,
        packetId: _packetIdCounter,
      );

      debugPrint('Sending text message: ${packet.length} bytes '
          '(${text.length} chars, id: $_packetIdCounter)');

      await _toRadio!.write(packet.toList(), withoutResponse: false);
      debugPrint('Text message sent successfully');
      return true;
    } catch (e) {
      debugPrint('Error sending text message: $e');
      return false;
    }
  }

  /// Send a binary payload as a PRIVATE_APP mesh packet (broadcast).
  Future<bool> sendPayload(Uint8List payload) async {
    if (_state != MeshConnectionState.ready || _toRadio == null) {
      debugPrint('Cannot send: mesh not ready (state: $_state)');
      return false;
    }

    try {
      _packetIdCounter++;
      final packet = MeshPacketBuilder.buildDataPacket(
        payload,
        packetId: _packetIdCounter,
      );

      debugPrint('Sending mesh packet: ${packet.length} bytes '
          '(payload: ${payload.length} bytes, id: $_packetIdCounter)');

      // flutter_blue_plus handles chunking for large writes
      await _toRadio!.write(
        packet.toList(),
        withoutResponse: false,
      );

      debugPrint('Mesh packet sent successfully');
      return true;
    } catch (e) {
      debugPrint('Error sending mesh packet: $e');
      return false;
    }
  }

  /// Disconnect and stop auto-reconnect.
  Future<void> disconnect() async {
    _autoReconnect = false;
    _reconnectTimer?.cancel();
    _connectionSub?.cancel();
    _fromNumSub?.cancel();

    try {
      await _device?.disconnect();
    } catch (e) {
      debugPrint('Disconnect error: $e');
    }

    _device = null;
    _toRadio = null;
    _fromRadio = null;
    _fromNum = null;
    _setState(MeshConnectionState.disconnected);
  }

  /// Enable auto-reconnect and start scanning.
  void enableAutoReconnect() {
    _autoReconnect = true;
    _reconnectAttempts = 0;
  }

  void dispose() {
    disconnect();
    _stateController.close();
    _receivedDataController.close();
    _receivedTextController.close();
  }
}
