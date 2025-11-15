import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:record/record.dart';

class AudioService {
  final AudioRecorder _recorder = AudioRecorder();
  bool _isRecording = false;
  String? _recordingPath;

  bool get isRecording => _isRecording;
  String? get recordingPath => _recordingPath;

  Future<bool> initialize() async {
    try {
      // Check permission status (don't request - should be granted at app startup)
      final status = await Permission.microphone.status;
      if (!status.isGranted) {
        debugPrint('Microphone permission not granted - please grant permissions in app settings');
        return false;
      }
      return true;
    } catch (e) {
      debugPrint('Error requesting microphone permission: $e');
      return false;
    }
  }

  Future<bool> startRecording() async {
    if (_isRecording) {
      debugPrint('Already recording');
      return false;
    }

    try {
      final hasPermission = await _recorder.hasPermission();
      if (!hasPermission) {
        debugPrint('No microphone permission');
        return false;
      }

      final directory = await getApplicationDocumentsDirectory();
      final timestamp = DateTime.now().millisecondsSinceEpoch;
      _recordingPath = '${directory.path}/audio_$timestamp.m4a';

      await _recorder.start(
        RecordConfig(
          encoder: AudioEncoder.aacLc,
          bitRate: 128000,
          sampleRate: 44100,
        ),
        path: _recordingPath!,
      );

      _isRecording = true;
      return true;
    } catch (e) {
      debugPrint('Error starting recording: $e');
      return false;
    }
  }

  Future<File?> stopRecording() async {
    if (!_isRecording) {
      debugPrint('Not recording');
      return null;
    }

    try {
      final path = await _recorder.stop();
      _isRecording = false;

      if (path != null && await File(path).exists()) {
        return File(path);
      }

      return null;
    } catch (e) {
      debugPrint('Error stopping recording: $e');
      _isRecording = false;
      return null;
    }
  }

  Future<void> cancelRecording() async {
    if (!_isRecording) {
      return;
    }

    try {
      await _recorder.stop();
      _isRecording = false;

      if (_recordingPath != null) {
        final file = File(_recordingPath!);
        if (await file.exists()) {
          await file.delete();
        }
      }

      _recordingPath = null;
    } catch (e) {
      debugPrint('Error canceling recording: $e');
    }
  }

  Future<void> dispose() async {
    await _recorder.dispose();
    _isRecording = false;
    _recordingPath = null;
  }
}
