import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:flutter/foundation.dart';
import '../config/app_config.dart';
import '../models/incident.dart';

class IncidentService {
  static final IncidentService _instance = IncidentService._internal();

  factory IncidentService() {
    return _instance;
  }

  IncidentService._internal();

  Future<List<Incident>> fetchIncidents({int limit = 20}) async {
    try {
      final url = '${AppConfig.baseUrl}/api/incident/?limit=$limit';
      debugPrint('=== FETCHING INCIDENTS ===');
      debugPrint('URL: $url');

      final response = await http.get(
        Uri.parse(url),
      );

      debugPrint('Response Status: ${response.statusCode}');
      debugPrint('Response Headers: ${response.headers}');
      debugPrint('Response Body (first 200 chars): ${response.body.substring(0, response.body.length > 200 ? 200 : response.body.length)}');

      if (response.statusCode == 200) {
        final data = json.decode(response.body);
        debugPrint('Decoded data type: ${data.runtimeType}');

        // Handle both array format and object format
        final incidentsList = data is List
            ? data
            : (data['incidents'] as List? ?? []);

        debugPrint('Incidents list length: ${incidentsList.length}');

        final incidents = incidentsList
            .map((json) => Incident.fromJson(json))
            .toList();

        debugPrint('Successfully parsed ${incidents.length} incidents');
        return incidents;
      } else {
        debugPrint('!!! FAILED to fetch incidents !!!');
        debugPrint('Status Code: ${response.statusCode}');
        debugPrint('Response Body: ${response.body}');
        return [];
      }
    } catch (e, stackTrace) {
      debugPrint('!!! ERROR fetching incidents !!!');
      debugPrint('Error: $e');
      debugPrint('Stack trace: $stackTrace');
      return [];
    }
  }

  Future<Incident?> fetchIncidentById(String id) async {
    try {
      final url = '${AppConfig.baseUrl}/api/incident/$id';
      debugPrint('=== FETCHING INCIDENT BY ID ===');
      debugPrint('URL: $url');

      final response = await http.get(
        Uri.parse(url),
      );

      debugPrint('Response Status: ${response.statusCode}');

      if (response.statusCode == 200) {
        final data = json.decode(response.body);
        debugPrint('Successfully fetched incident: $id');
        return Incident.fromJson(data);
      } else {
        debugPrint('!!! FAILED to fetch incident !!!');
        debugPrint('Status Code: ${response.statusCode}');
        debugPrint('Response Body: ${response.body}');
        return null;
      }
    } catch (e, stackTrace) {
      debugPrint('!!! ERROR fetching incident !!!');
      debugPrint('Error: $e');
      debugPrint('Stack trace: $stackTrace');
      return null;
    }
  }
}
