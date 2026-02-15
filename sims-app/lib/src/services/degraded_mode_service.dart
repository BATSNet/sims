import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:image/image.dart' as img;
import 'package:vosk_flutter_service/vosk_flutter.dart';
import 'package:path_provider/path_provider.dart';

/// Provides offline speech-to-text (Vosk) and image compression for mesh mode.
class DegradedModeService {
  static final DegradedModeService _instance = DegradedModeService._internal();
  factory DegradedModeService() => _instance;
  DegradedModeService._internal();

  VoskFlutterPlugin? _vosk;
  Model? _model;
  Recognizer? _recognizer;
  SpeechService? _speechService;
  bool _modelLoaded = false;
  bool _isListening = false;

  final _transcriptionController = StreamController<String>.broadcast();
  Stream<String> get transcriptionStream => _transcriptionController.stream;

  bool get isModelLoaded => _modelLoaded;
  bool get isListening => _isListening;

  // Image compression constants matching sims-smart-espidf
  static const int meshImageWidth = 320;
  static const int meshImageHeight = 240;
  static const int meshJpegQuality = 35;

  /// Initialize the Vosk offline speech recognition model.
  /// Should be called once at app startup or when entering mesh mode.
  Future<bool> initializeVosk() async {
    if (_modelLoaded) return true;

    try {
      _vosk = VoskFlutterPlugin.instance();

      // Try loading model from app documents directory first
      final appDir = await getApplicationDocumentsDirectory();
      final modelDir = Directory('${appDir.path}/vosk-model-small-en-us');
      String modelPath;

      if (await modelDir.exists()) {
        modelPath = modelDir.path;
      } else {
        // Extract model from bundled assets
        final loader = ModelLoader();
        modelPath = await loader.loadFromAssets('assets/models/vosk-model-small-en-us');
      }

      _model = await _vosk!.createModel(modelPath);
      _recognizer = await _vosk!.createRecognizer(
        model: _model!,
        sampleRate: 16000,
      );

      _modelLoaded = true;
      debugPrint('Vosk model loaded successfully');
      return true;
    } catch (e) {
      debugPrint('Failed to initialize Vosk: $e');
      return false;
    }
  }

  /// Start listening for speech and streaming transcription results.
  Future<bool> startListening() async {
    if (!_modelLoaded || _recognizer == null) {
      debugPrint('Vosk model not loaded');
      return false;
    }

    if (_isListening) return true;

    try {
      _isListening = true;
      debugPrint('Vosk listening started');

      // The SpeechService handles audio input and feeds to recognizer
      _speechService = await _vosk!.initSpeechService(_recognizer!);
      _speechService!.onPartial().listen((partial) {
        _transcriptionController.add(partial);
      });
      _speechService!.onResult().listen((result) {
        _transcriptionController.add(result);
      });
      await _speechService!.start();

      return true;
    } catch (e) {
      debugPrint('Error starting Vosk listener: $e');
      _isListening = false;
      return false;
    }
  }

  /// Stop listening and return the final transcription result.
  Future<String?> stopListening() async {
    if (!_isListening) return null;

    try {
      await _speechService?.stop();
      _isListening = false;
      final result = await _recognizer?.getFinalResult();
      debugPrint('Vosk final result: $result');
      return result;
    } catch (e) {
      debugPrint('Error stopping Vosk listener: $e');
      _isListening = false;
      return null;
    }
  }

  /// Compress an image file for mesh transmission.
  /// Resizes to 320x240, converts to grayscale, encodes as JPEG Q35.
  /// Returns compressed bytes (typically 3-8KB).
  static Future<Uint8List?> compressImageForMesh(File imageFile) async {
    try {
      final bytes = await imageFile.readAsBytes();
      return await compute(_compressImageIsolate, bytes);
    } catch (e) {
      debugPrint('Error compressing image: $e');
      return null;
    }
  }

  /// Compress raw image bytes for mesh (runs in isolate for performance).
  static Uint8List? _compressImageIsolate(Uint8List inputBytes) {
    try {
      final decoded = img.decodeImage(inputBytes);
      if (decoded == null) return null;

      // Resize to QVGA (320x240)
      final resized = img.copyResize(
        decoded,
        width: meshImageWidth,
        height: meshImageHeight,
        interpolation: img.Interpolation.linear,
      );

      // Convert to grayscale
      final grayscale = img.grayscale(resized);

      // Encode as JPEG at low quality for minimal size
      final compressed = img.encodeJpg(grayscale, quality: meshJpegQuality);
      return Uint8List.fromList(compressed);
    } catch (e) {
      return null;
    }
  }

  /// Compress raw bytes directly (for when you already have the image data).
  static Future<Uint8List?> compressImageBytes(Uint8List imageBytes) async {
    try {
      return await compute(_compressImageIsolate, imageBytes);
    } catch (e) {
      debugPrint('Error compressing image bytes: $e');
      return null;
    }
  }

  void dispose() {
    _transcriptionController.close();
    _speechService = null;
    _recognizer = null;
    _model = null;
    _isListening = false;
    _modelLoaded = false;
  }
}
