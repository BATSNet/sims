import 'dart:io';
import 'package:camera/camera.dart';
import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:video_compress/video_compress.dart';

class CameraService {
  CameraController? _controller;
  List<CameraDescription>? _cameras;
  bool _isInitialized = false;

  bool get isInitialized => _isInitialized;
  CameraController? get controller => _controller;

  Future<bool> initialize() async {
    try {
      // Check permission status (don't request - should be granted at app startup)
      final status = await Permission.camera.status;
      if (!status.isGranted) {
        debugPrint('Camera permission not granted - please grant permissions in app settings');
        return false;
      }

      _cameras = await availableCameras();
      if (_cameras == null || _cameras!.isEmpty) {
        debugPrint('No cameras available');
        return false;
      }

      final camera = _cameras!.first;
      _controller = CameraController(
        camera,
        ResolutionPreset.high,
        enableAudio: false,
        imageFormatGroup: ImageFormatGroup.jpeg,
      );

      await _controller!.initialize();
      _isInitialized = true;
      return true;
    } catch (e) {
      debugPrint('Error initializing camera: $e');
      return false;
    }
  }

  Future<File?> takePicture() async {
    if (!_isInitialized || _controller == null) {
      debugPrint('Camera not initialized');
      return null;
    }

    try {
      final image = await _controller!.takePicture();
      return File(image.path);
    } catch (e) {
      debugPrint('Error taking picture: $e');
      return null;
    }
  }

  Future<void> dispose() async {
    await _controller?.dispose();
    _controller = null;
    _isInitialized = false;
  }

  Future<void> switchCamera() async {
    if (_cameras == null || _cameras!.length < 2) {
      return;
    }

    final currentCameraIndex = _cameras!.indexOf(_controller!.description);
    final newCameraIndex = (currentCameraIndex + 1) % _cameras!.length;
    final newCamera = _cameras![newCameraIndex];

    await _controller?.dispose();

    _controller = CameraController(
      newCamera,
      ResolutionPreset.high,
      enableAudio: false,
      imageFormatGroup: ImageFormatGroup.jpeg,
    );

    await _controller!.initialize();
  }

  Future<void> setFlashMode(FlashMode mode) async {
    if (_controller == null) return;
    await _controller!.setFlashMode(mode);
  }

  Future<bool> startVideoRecording() async {
    if (!_isInitialized || _controller == null) {
      debugPrint('Camera not initialized');
      return false;
    }

    if (_controller!.value.isRecordingVideo) {
      debugPrint('Already recording video');
      return false;
    }

    try {
      await _controller!.startVideoRecording();
      return true;
    } catch (e) {
      debugPrint('Error starting video recording: $e');
      return false;
    }
  }

  Future<File?> stopVideoRecording() async {
    if (!_isInitialized || _controller == null) {
      debugPrint('Camera not initialized');
      return null;
    }

    if (!_controller!.value.isRecordingVideo) {
      debugPrint('Not recording video');
      return null;
    }

    try {
      final video = await _controller!.stopVideoRecording();
      final videoFile = File(video.path);

      // Compress video and save as MP4
      debugPrint('Compressing video: ${video.path}');
      final info = await VideoCompress.compressVideo(
        video.path,
        quality: VideoQuality.MediumQuality,
        deleteOrigin: true, // Delete the original uncompressed file
        includeAudio: true,
      );

      if (info == null || info.file == null) {
        debugPrint('Video compression failed, using original file');
        // If compression fails, rename to .mp4
        final directory = await getTemporaryDirectory();
        final timestamp = DateTime.now().millisecondsSinceEpoch;
        final mp4Path = '${directory.path}/VID_$timestamp.mp4';
        final mp4File = await videoFile.copy(mp4Path);
        await videoFile.delete();
        return mp4File;
      }

      debugPrint('Video compressed: ${info.file!.path} (${info.filesize} bytes)');
      return info.file;
    } catch (e) {
      debugPrint('Error stopping video recording: $e');
      return null;
    }
  }

  bool get isRecordingVideo {
    return _controller?.value.isRecordingVideo ?? false;
  }
}
