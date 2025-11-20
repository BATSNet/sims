import 'package:equatable/equatable.dart';

enum IncidentPriority {
  critical,
  high,
  medium,
  low;

  String get displayName {
    switch (this) {
      case IncidentPriority.critical:
        return 'Critical';
      case IncidentPriority.high:
        return 'High';
      case IncidentPriority.medium:
        return 'Medium';
      case IncidentPriority.low:
        return 'Low';
    }
  }
}

enum IncidentStatus {
  open,
  inProgress,
  resolved,
  closed;

  String get displayName {
    switch (this) {
      case IncidentStatus.open:
        return 'Open';
      case IncidentStatus.inProgress:
        return 'In Progress';
      case IncidentStatus.resolved:
        return 'Resolved';
      case IncidentStatus.closed:
        return 'Closed';
    }
  }
}

class Incident extends Equatable {
  final String id;
  final String? incidentId; // The formatted incident ID like "INC-xxx"
  final String title;
  final String description;
  final IncidentPriority priority;
  final IncidentStatus status;
  final DateTime createdAt;
  final DateTime updatedAt;
  final String? location;
  final double? latitude;
  final double? longitude;
  final String? imageUrl;
  final String? audioUrl;
  final double? heading;
  final String? userPhone; // Reporter phone number
  final String? category; // Incident category

  const Incident({
    required this.id,
    this.incidentId,
    required this.title,
    required this.description,
    required this.priority,
    required this.status,
    required this.createdAt,
    required this.updatedAt,
    this.location,
    this.latitude,
    this.longitude,
    this.imageUrl,
    this.audioUrl,
    this.heading,
    this.userPhone,
    this.category,
  });

  Incident copyWith({
    String? id,
    String? incidentId,
    String? title,
    String? description,
    IncidentPriority? priority,
    IncidentStatus? status,
    DateTime? createdAt,
    DateTime? updatedAt,
    String? location,
    double? latitude,
    double? longitude,
    String? imageUrl,
    String? audioUrl,
    double? heading,
    String? userPhone,
    String? category,
  }) {
    return Incident(
      id: id ?? this.id,
      incidentId: incidentId ?? this.incidentId,
      title: title ?? this.title,
      description: description ?? this.description,
      priority: priority ?? this.priority,
      status: status ?? this.status,
      createdAt: createdAt ?? this.createdAt,
      updatedAt: updatedAt ?? this.updatedAt,
      location: location ?? this.location,
      latitude: latitude ?? this.latitude,
      longitude: longitude ?? this.longitude,
      imageUrl: imageUrl ?? this.imageUrl,
      audioUrl: audioUrl ?? this.audioUrl,
      heading: heading ?? this.heading,
      userPhone: userPhone ?? this.userPhone,
      category: category ?? this.category,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'incidentId': incidentId,
      'title': title,
      'description': description,
      'priority': priority.name,
      'status': status.name,
      'createdAt': createdAt.toIso8601String(),
      'updatedAt': updatedAt.toIso8601String(),
      'location': location,
      'latitude': latitude,
      'longitude': longitude,
      'imageUrl': imageUrl,
      'audioUrl': audioUrl,
      'heading': heading,
      'userPhone': userPhone,
      'category': category,
    };
  }

  factory Incident.fromJson(Map<String, dynamic> json) {
    return Incident(
      id: (json['id'] ?? 'UNKNOWN') as String,
      incidentId: json['incidentId'] as String?,
      title: (json['title'] ?? 'Untitled') as String,
      description: (json['description'] ?? 'No description') as String,
      priority: IncidentPriority.values.firstWhere(
        (e) => e.name.toLowerCase() == (json['priority'] as String?)?.toLowerCase(),
        orElse: () => IncidentPriority.medium,
      ),
      status: IncidentStatus.values.firstWhere(
        (e) => e.name.toLowerCase() == (json['status'] as String?)?.toLowerCase(),
        orElse: () => IncidentStatus.open,
      ),
      createdAt: json['createdAt'] != null
          ? DateTime.parse(json['createdAt'] as String)
          : DateTime.now(),
      updatedAt: json['updatedAt'] != null
          ? DateTime.parse(json['updatedAt'] as String)
          : DateTime.now(),
      location: json['location'] as String?,
      latitude: (json['latitude'] as num?)?.toDouble(),
      longitude: (json['longitude'] as num?)?.toDouble(),
      imageUrl: json['imageUrl'] as String?,
      audioUrl: json['audioUrl'] as String?,
      heading: (json['heading'] as num?)?.toDouble(),
      userPhone: json['userPhone'] as String?,
      category: json['category'] as String?,
    );
  }

  @override
  List<Object?> get props => [
        id,
        incidentId,
        title,
        description,
        priority,
        status,
        createdAt,
        updatedAt,
        location,
        latitude,
        longitude,
        imageUrl,
        audioUrl,
        heading,
        userPhone,
        category,
      ];
}
