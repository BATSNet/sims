import 'package:equatable/equatable.dart';
import '../models/incident.dart';

enum IncidentStateStatus {
  initial,
  loading,
  loaded,
  error,
}

class IncidentState extends Equatable {
  final IncidentStateStatus status;
  final List<Incident> incidents;
  final Incident? selectedIncident;
  final String? errorMessage;

  const IncidentState({
    this.status = IncidentStateStatus.initial,
    this.incidents = const [],
    this.selectedIncident,
    this.errorMessage,
  });

  IncidentState copyWith({
    IncidentStateStatus? status,
    List<Incident>? incidents,
    Incident? selectedIncident,
    String? errorMessage,
  }) {
    return IncidentState(
      status: status ?? this.status,
      incidents: incidents ?? this.incidents,
      selectedIncident: selectedIncident ?? this.selectedIncident,
      errorMessage: errorMessage ?? this.errorMessage,
    );
  }

  List<Incident> get criticalIncidents =>
      incidents.where((i) => i.priority == IncidentPriority.critical && i.status != IncidentStatus.closed).toList();

  List<Incident> get highIncidents =>
      incidents.where((i) => i.priority == IncidentPriority.high && i.status != IncidentStatus.closed).toList();

  List<Incident> get mediumIncidents =>
      incidents.where((i) => i.priority == IncidentPriority.medium && i.status != IncidentStatus.closed).toList();

  List<Incident> get lowIncidents =>
      incidents.where((i) => i.priority == IncidentPriority.low && i.status != IncidentStatus.closed).toList();

  List<Incident> get closedIncidents =>
      incidents.where((i) => i.status == IncidentStatus.closed).toList();

  List<Incident> get activeIncidents =>
      incidents.where((i) => i.status != IncidentStatus.closed).toList();

  int get openCount => incidents.where((i) => i.status == IncidentStatus.open).length;
  int get inProgressCount => incidents.where((i) => i.status == IncidentStatus.inProgress).length;
  int get resolvedCount => incidents.where((i) => i.status == IncidentStatus.resolved).length;
  int get closedCount => incidents.where((i) => i.status == IncidentStatus.closed).length;

  @override
  List<Object?> get props => [status, incidents, selectedIncident, errorMessage];
}
