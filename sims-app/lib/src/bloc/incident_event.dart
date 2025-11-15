import 'package:equatable/equatable.dart';
import '../models/incident.dart';

abstract class IncidentEvent extends Equatable {
  const IncidentEvent();

  @override
  List<Object?> get props => [];
}

class LoadIncidents extends IncidentEvent {
  const LoadIncidents();
}

class RefreshIncidents extends IncidentEvent {
  const RefreshIncidents();
}

class CreateIncident extends IncidentEvent {
  final String title;
  final String description;
  final IncidentPriority priority;

  const CreateIncident({
    required this.title,
    required this.description,
    this.priority = IncidentPriority.medium,
  });

  @override
  List<Object?> get props => [title, description, priority];
}

class SelectIncident extends IncidentEvent {
  final Incident incident;

  const SelectIncident(this.incident);

  @override
  List<Object?> get props => [incident];
}

class UpdateIncidentStatus extends IncidentEvent {
  final String incidentId;
  final IncidentStatus status;

  const UpdateIncidentStatus({
    required this.incidentId,
    required this.status,
  });

  @override
  List<Object?> get props => [incidentId, status];
}
