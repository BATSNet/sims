import 'dart:io';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import 'package:http_parser/http_parser.dart';
import 'package:mime/mime.dart';
import '../config/app_config.dart';

class UploadResult {
  final bool success;
  final String? url;
  final String? error;
  final Map<String, dynamic>? metadata;

  UploadResult({
    required this.success,
    this.url,
    this.error,
    this.metadata,
  });

  factory UploadResult.fromJson(Map<String, dynamic> json) {
    return UploadResult(
      success: json['success'] ?? false,
      url: json['url'],
      error: json['error'],
      metadata: json['metadata'],
    );
  }
}

class MediaUploadService {
  final String baseUrl = AppConfig.baseUrl;

  Future<UploadResult> uploadImage(File imageFile, {String? incidentId}) async {
    try {
      final uri = Uri.parse('$baseUrl/api/upload/image');
      final request = http.MultipartRequest('POST', uri);

      final mimeType = lookupMimeType(imageFile.path);
      final mimeTypeData = mimeType?.split('/');

      request.files.add(
        await http.MultipartFile.fromPath(
          'file',
          imageFile.path,
          contentType: mimeTypeData != null
              ? MediaType(mimeTypeData[0], mimeTypeData[1])
              : null,
        ),
      );

      // Add incident_id if provided
      if (incidentId != null) {
        request.fields['incident_id'] = incidentId;
      }

      debugPrint('Uploading image to: $uri (incident: $incidentId)');
      final streamedResponse = await request.send();
      final response = await http.Response.fromStream(streamedResponse);

      debugPrint('Image upload response: ${response.statusCode}');

      if (response.statusCode == 200) {
        final data = json.decode(response.body);
        return UploadResult.fromJson(data);
      } else {
        return UploadResult(
          success: false,
          error: 'Upload failed with status ${response.statusCode}: ${response.body}',
        );
      }
    } catch (e) {
      debugPrint('Error uploading image: $e');
      return UploadResult(
        success: false,
        error: 'Error uploading image: $e',
      );
    }
  }

  Future<UploadResult> uploadAudio(File audioFile, {String? incidentId}) async {
    try {
      final uri = Uri.parse('$baseUrl/api/upload/audio');
      final request = http.MultipartRequest('POST', uri);

      final mimeType = lookupMimeType(audioFile.path);
      final mimeTypeData = mimeType?.split('/');

      request.files.add(
        await http.MultipartFile.fromPath(
          'file',
          audioFile.path,
          contentType: mimeTypeData != null
              ? MediaType(mimeTypeData[0], mimeTypeData[1])
              : null,
        ),
      );

      // Add incident_id if provided
      if (incidentId != null) {
        request.fields['incident_id'] = incidentId;
      }

      debugPrint('Uploading audio to: $uri (incident: $incidentId)');
      final streamedResponse = await request.send();
      final response = await http.Response.fromStream(streamedResponse);

      debugPrint('Audio upload response: ${response.statusCode}');

      if (response.statusCode == 200) {
        final data = json.decode(response.body);
        return UploadResult.fromJson(data);
      } else {
        return UploadResult(
          success: false,
          error: 'Upload failed with status ${response.statusCode}: ${response.body}',
        );
      }
    } catch (e) {
      debugPrint('Error uploading audio: $e');
      return UploadResult(
        success: false,
        error: 'Error uploading audio: $e',
      );
    }
  }

  Future<UploadResult> uploadVideo(File videoFile, {String? incidentId}) async {
    try {
      final uri = Uri.parse('$baseUrl/api/upload/video');
      final request = http.MultipartRequest('POST', uri);

      final mimeType = lookupMimeType(videoFile.path);
      final mimeTypeData = mimeType?.split('/');

      request.files.add(
        await http.MultipartFile.fromPath(
          'file',
          videoFile.path,
          contentType: mimeTypeData != null
              ? MediaType(mimeTypeData[0], mimeTypeData[1])
              : null,
        ),
      );

      // Add incident_id if provided
      if (incidentId != null) {
        request.fields['incident_id'] = incidentId;
      }

      debugPrint('Uploading video to: $uri (incident: $incidentId)');
      final streamedResponse = await request.send();
      final response = await http.Response.fromStream(streamedResponse);

      debugPrint('Video upload response: ${response.statusCode}');

      if (response.statusCode == 200) {
        final data = json.decode(response.body);
        return UploadResult.fromJson(data);
      } else {
        return UploadResult(
          success: false,
          error: 'Upload failed with status ${response.statusCode}: ${response.body}',
        );
      }
    } catch (e) {
      debugPrint('Error uploading video: $e');
      return UploadResult(
        success: false,
        error: 'Error uploading video: $e',
      );
    }
  }

  Future<Map<String, dynamic>?> createIncident({
    required String title,
    required String description,
    String? imageUrl,
    String? audioUrl,
    double? latitude,
    double? longitude,
    double? heading,
    Map<String, dynamic>? metadata,
  }) async {
    try {
      final uri = Uri.parse('$baseUrl/api/incidents');
      final response = await http.post(
        uri,
        headers: {'Content-Type': 'application/json'},
        body: json.encode({
          'title': title,
          'description': description,
          'imageUrl': imageUrl,
          'audioUrl': audioUrl,
          'latitude': latitude,
          'longitude': longitude,
          'heading': heading,
          'metadata': metadata,
          'timestamp': DateTime.now().toIso8601String(),
        }),
      );

      debugPrint('Create incident response: ${response.statusCode}');

      if (response.statusCode == 200 || response.statusCode == 201) {
        return json.decode(response.body);
      } else {
        debugPrint('Error creating incident: ${response.body}');
        return null;
      }
    } catch (e) {
      debugPrint('Error creating incident: $e');
      return null;
    }
  }
}
