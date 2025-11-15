import 'package:bloc/bloc.dart';
import '../models/incident.dart';
import 'incident_event.dart';
import 'incident_state.dart';

class IncidentBloc extends Bloc<IncidentEvent, IncidentState> {
  IncidentBloc() : super(const IncidentState()) {
    on<LoadIncidents>(_onLoadIncidents);
    on<RefreshIncidents>(_onRefreshIncidents);
    on<CreateIncident>(_onCreateIncident);
    on<SelectIncident>(_onSelectIncident);
    on<UpdateIncidentStatus>(_onUpdateIncidentStatus);
  }

  Future<void> _onLoadIncidents(LoadIncidents event, Emitter<IncidentState> emit) async {
    emit(state.copyWith(status: IncidentStateStatus.loading));

    try {
      await Future.delayed(const Duration(milliseconds: 500));
      final mockIncidents = _generateMockIncidents();
      emit(state.copyWith(
        status: IncidentStateStatus.loaded,
        incidents: mockIncidents,
      ));
    } catch (e) {
      emit(state.copyWith(
        status: IncidentStateStatus.error,
        errorMessage: e.toString(),
      ));
    }
  }

  Future<void> _onRefreshIncidents(RefreshIncidents event, Emitter<IncidentState> emit) async {
    try {
      await Future.delayed(const Duration(milliseconds: 300));
      final mockIncidents = _generateMockIncidents();
      emit(state.copyWith(
        status: IncidentStateStatus.loaded,
        incidents: mockIncidents,
      ));
    } catch (e) {
      emit(state.copyWith(
        status: IncidentStateStatus.error,
        errorMessage: e.toString(),
      ));
    }
  }

  Future<void> _onCreateIncident(CreateIncident event, Emitter<IncidentState> emit) async {
    final newIncident = Incident(
      id: DateTime.now().millisecondsSinceEpoch.toString(),
      title: event.title,
      description: event.description,
      priority: event.priority,
      status: IncidentStatus.open,
      createdAt: DateTime.now(),
      updatedAt: DateTime.now(),
    );

    final updatedIncidents = [newIncident, ...state.incidents];
    emit(state.copyWith(
      incidents: updatedIncidents,
      selectedIncident: newIncident,
    ));
  }

  void _onSelectIncident(SelectIncident event, Emitter<IncidentState> emit) {
    emit(state.copyWith(selectedIncident: event.incident));
  }

  Future<void> _onUpdateIncidentStatus(UpdateIncidentStatus event, Emitter<IncidentState> emit) async {
    final updatedIncidents = state.incidents.map((incident) {
      if (incident.id == event.incidentId) {
        return incident.copyWith(
          status: event.status,
          updatedAt: DateTime.now(),
        );
      }
      return incident;
    }).toList();

    emit(state.copyWith(incidents: updatedIncidents));
  }

  List<Incident> _generateMockIncidents() {
    final now = DateTime.now();

    return [
      Incident(
        id: '1',
        title: 'Suspected drone near airport',
        description: 'Unidentified drone spotted hovering near runway 27L',
        priority: IncidentPriority.critical,
        status: IncidentStatus.open,
        createdAt: now.subtract(const Duration(minutes: 5)),
        updatedAt: now.subtract(const Duration(minutes: 5)),
        location: 'Vilnius Airport',
        latitude: 54.6872,
        longitude: 25.2797,
        imageUrl: 'https://images.unsplash.com/photo-1473968512647-3e447244af8f?w=800',
      ),
      Incident(
        id: '2',
        title: 'Armored vehicles approaching border',
        description: 'Three armored vehicles spotted moving towards eastern border checkpoint',
        priority: IncidentPriority.high,
        status: IncidentStatus.inProgress,
        createdAt: now.subtract(const Duration(hours: 1, minutes: 30)),
        updatedAt: now.subtract(const Duration(minutes: 20)),
        location: 'Medininkai Border Checkpoint',
        latitude: 54.5544,
        longitude: 25.3033,
        imageUrl: 'https://images.unsplash.com/photo-1587407627257-27672ab5c2c0?w=800',
      ),
      Incident(
        id: '3',
        title: 'Flooding in residential area',
        description: 'Heavy rain causing flooding, several homes affected',
        priority: IncidentPriority.high,
        status: IncidentStatus.open,
        createdAt: now.subtract(const Duration(hours: 2)),
        updatedAt: now.subtract(const Duration(hours: 2)),
        location: 'Kaunas Old Town',
        latitude: 54.8985,
        longitude: 23.9036,
        imageUrl: 'https://images.unsplash.com/photo-1547683905-f686c993aae5?w=800',
      ),
      Incident(
        id: '4',
        title: 'Suspicious activity near military base',
        description: 'Person with camera taking photos of military facility perimeter',
        priority: IncidentPriority.medium,
        status: IncidentStatus.resolved,
        createdAt: now.subtract(const Duration(hours: 4)),
        updatedAt: now.subtract(const Duration(hours: 1)),
        location: 'Rukla Military Base',
        latitude: 55.0331,
        longitude: 24.2667,
      ),
      Incident(
        id: '5',
        title: 'Traffic accident on main highway',
        description: 'Two vehicle collision, road partially blocked',
        priority: IncidentPriority.medium,
        status: IncidentStatus.inProgress,
        createdAt: now.subtract(const Duration(hours: 3)),
        updatedAt: now.subtract(const Duration(hours: 2, minutes: 30)),
        location: 'A1 Highway km 45',
        latitude: 54.7500,
        longitude: 25.3000,
      ),
    ];
  }
}
