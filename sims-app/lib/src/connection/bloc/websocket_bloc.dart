import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../websocket_service.dart';
import '../../models/incident.dart';
import '../../services/incident_service.dart';
import 'websocket_event.dart';
import 'websocket_state.dart';

class WebSocketBloc extends Bloc<WebSocketEvent, WebSocketState> {
  final WebSocketService _webSocketService;
  final IncidentService _incidentService = IncidentService();
  StreamSubscription<dynamic>? _messageSubscription;
  StreamSubscription<bool>? _connectionSubscription;

  WebSocketBloc(this._webSocketService) : super(const WebSocketState()) {
    on<ConnectWebSocket>(_onConnect);
    on<DisconnectWebSocket>(_onDisconnect);
    on<WebSocketConnectionChanged>(_onConnectionChanged);
    on<IncidentReceived>(_onIncidentReceived);
    on<IncidentUpdated>(_onIncidentUpdated);
    on<IncidentDeleted>(_onIncidentDeleted);
    on<StatsReceived>(_onStatsReceived);
    on<SubscribeToIncidents>(_onSubscribeToIncidents);
    on<UnsubscribeFromIncidents>(_onUnsubscribeFromIncidents);
    on<LoadInitialIncidents>(_onLoadInitialIncidents);
    on<InitialIncidentsLoaded>(_onInitialIncidentsLoaded);
    on<ReconnectWebSocket>(_onReconnect);

    _setupListeners();
  }

  void _setupListeners() {
    // Listen to connection state changes
    _connectionSubscription = _webSocketService.connectionStateStream.listen(
      (isConnected) {
        add(WebSocketConnectionChanged(isConnected));
      },
    );

    // Listen to WebSocket messages and convert to events
    _messageSubscription = _webSocketService.messageStream.listen(
      (message) {
        try {
          final messageType = message['type'] as String?;
          final data = message['data'];

          switch (messageType) {
            case 'incident_new':
              if (data != null) {
                final incident = Incident.fromJson(data);
                add(IncidentReceived(incident));
              }
              break;

            case 'incident_update':
              if (data != null) {
                final incident = Incident.fromJson(data);
                add(IncidentUpdated(incident));
              }
              break;

            case 'incident_delete':
              if (data != null && data['id'] != null) {
                add(IncidentDeleted(data['id']));
              }
              break;

            case 'stats':
              if (data != null) {
                add(StatsReceived(Map<String, dynamic>.from(data)));
              }
              break;

            case 'subscribed':
            case 'unsubscribed':
            case 'pong':
              // Informational messages, no action needed
              break;

            default:
              debugPrint('Unknown WebSocket message type: $messageType');
          }
        } catch (e) {
          debugPrint('Error processing WebSocket message: $e');
        }
      },
      onError: (error) {
        debugPrint('WebSocket stream error: $error');
      },
    );
  }

  Future<void> _onConnect(
    ConnectWebSocket event,
    Emitter<WebSocketState> emit,
  ) async {
    try {
      await _webSocketService.connect(token: event.token);
    } catch (e) {
      debugPrint('Error connecting to WebSocket: $e');
      emit(state.copyWith(error: e.toString()));
    }
  }

  Future<void> _onDisconnect(
    DisconnectWebSocket event,
    Emitter<WebSocketState> emit,
  ) async {
    _webSocketService.disconnect();
    emit(state.copyWith(
      isConnected: false,
      incidents: [],
      stats: {},
    ));
  }

  void _onConnectionChanged(
    WebSocketConnectionChanged event,
    Emitter<WebSocketState> emit,
  ) {
    emit(state.copyWith(isConnected: event.isConnected));

    // Subscribe to incidents and load initial data when connected
    if (event.isConnected) {
      _webSocketService.subscribeToIncidents();
      add(const LoadInitialIncidents());
    }
  }

  Future<void> _onLoadInitialIncidents(
    LoadInitialIncidents event,
    Emitter<WebSocketState> emit,
  ) async {
    try {
      debugPrint('Loading initial incidents...');
      final incidents = await _incidentService.fetchIncidents(limit: 20);
      debugPrint('Loaded ${incidents.length} initial incidents');
      add(InitialIncidentsLoaded(incidents));
    } catch (e) {
      debugPrint('Error loading initial incidents: $e');
    }
  }

  void _onInitialIncidentsLoaded(
    InitialIncidentsLoaded event,
    Emitter<WebSocketState> emit,
  ) {
    debugPrint('Setting initial incidents: ${event.incidents.length}');
    emit(state.copyWith(incidents: event.incidents));
  }

  void _onIncidentReceived(
    IncidentReceived event,
    Emitter<WebSocketState> emit,
  ) {
    debugPrint('New incident received: ${event.incident.id} - ${event.incident.title}');
    final updatedIncidents = [event.incident, ...state.incidents];
    debugPrint('Total incidents after adding: ${updatedIncidents.length}');
    emit(state.copyWith(incidents: updatedIncidents));
  }

  void _onIncidentUpdated(
    IncidentUpdated event,
    Emitter<WebSocketState> emit,
  ) {
    debugPrint('Incident updated: ${event.incident.id}');
    final updatedIncidents = state.incidents.map((incident) {
      return incident.id == event.incident.id ? event.incident : incident;
    }).toList();
    emit(state.copyWith(incidents: updatedIncidents));
  }

  void _onIncidentDeleted(
    IncidentDeleted event,
    Emitter<WebSocketState> emit,
  ) {
    debugPrint('Incident deleted: ${event.incidentId}');
    final updatedIncidents = state.incidents
        .where((incident) => incident.id != event.incidentId)
        .toList();
    debugPrint('Total incidents after deletion: ${updatedIncidents.length}');
    emit(state.copyWith(incidents: updatedIncidents));
  }

  void _onStatsReceived(
    StatsReceived event,
    Emitter<WebSocketState> emit,
  ) {
    emit(state.copyWith(stats: event.stats));
  }

  void _onSubscribeToIncidents(
    SubscribeToIncidents event,
    Emitter<WebSocketState> emit,
  ) {
    _webSocketService.subscribeToIncidents();
  }

  void _onUnsubscribeFromIncidents(
    UnsubscribeFromIncidents event,
    Emitter<WebSocketState> emit,
  ) {
    _webSocketService.unsubscribeFromIncidents();
  }

  Future<void> _onReconnect(
    ReconnectWebSocket event,
    Emitter<WebSocketState> emit,
  ) async {
    debugPrint('Reconnecting WebSocket with new URL...');
    try {
      await _webSocketService.reconnect();
    } catch (e) {
      debugPrint('Error reconnecting to WebSocket: $e');
      emit(state.copyWith(error: e.toString()));
    }
  }

  @override
  Future<void> close() {
    _messageSubscription?.cancel();
    _connectionSubscription?.cancel();
    return super.close();
  }
}
