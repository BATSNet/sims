import 'package:equatable/equatable.dart';
import '../../models/incident.dart';

abstract class WebSocketEvent extends Equatable {
  const WebSocketEvent();

  @override
  List<Object?> get props => [];
}

class ConnectWebSocket extends WebSocketEvent {
  final String? token;

  const ConnectWebSocket({this.token});

  @override
  List<Object?> get props => [token];
}

class DisconnectWebSocket extends WebSocketEvent {
  const DisconnectWebSocket();
}

class WebSocketConnectionChanged extends WebSocketEvent {
  final bool isConnected;

  const WebSocketConnectionChanged(this.isConnected);

  @override
  List<Object?> get props => [isConnected];
}

class IncidentReceived extends WebSocketEvent {
  final Incident incident;

  const IncidentReceived(this.incident);

  @override
  List<Object?> get props => [incident];
}

class IncidentUpdated extends WebSocketEvent {
  final Incident incident;

  const IncidentUpdated(this.incident);

  @override
  List<Object?> get props => [incident];
}

class IncidentDeleted extends WebSocketEvent {
  final String incidentId;

  const IncidentDeleted(this.incidentId);

  @override
  List<Object?> get props => [incidentId];
}

class StatsReceived extends WebSocketEvent {
  final Map<String, dynamic> stats;

  const StatsReceived(this.stats);

  @override
  List<Object?> get props => [stats];
}

class SubscribeToIncidents extends WebSocketEvent {
  const SubscribeToIncidents();
}

class UnsubscribeFromIncidents extends WebSocketEvent {
  const UnsubscribeFromIncidents();
}

class LoadInitialIncidents extends WebSocketEvent {
  const LoadInitialIncidents();
}

class InitialIncidentsLoaded extends WebSocketEvent {
  final List<Incident> incidents;

  const InitialIncidentsLoaded(this.incidents);

  @override
  List<Object?> get props => [incidents];
}

class ReconnectWebSocket extends WebSocketEvent {
  const ReconnectWebSocket();
}
