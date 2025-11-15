import 'package:equatable/equatable.dart';
import '../../models/incident.dart';

class WebSocketState extends Equatable {
  final bool isConnected;
  final List<Incident> incidents;
  final Map<String, dynamic> stats;
  final String? error;

  const WebSocketState({
    this.isConnected = false,
    this.incidents = const [],
    this.stats = const {},
    this.error,
  });

  WebSocketState copyWith({
    bool? isConnected,
    List<Incident>? incidents,
    Map<String, dynamic>? stats,
    String? error,
  }) {
    return WebSocketState(
      isConnected: isConnected ?? this.isConnected,
      incidents: incidents ?? this.incidents,
      stats: stats ?? this.stats,
      error: error,
    );
  }

  @override
  List<Object?> get props => [isConnected, incidents, stats, error];
}
