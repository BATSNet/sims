import 'dart:io';
import 'package:flutter/material.dart';
import 'package:camera/camera.dart';
import 'package:go_router/go_router.dart';
import '../services/camera_service.dart';
import '../services/audio_service.dart';
import '../services/location_service.dart';
import '../services/media_upload_service.dart';
import '../utils/sims_colors.dart';

class CameraCaptureScreen extends StatefulWidget {
  const CameraCaptureScreen({super.key});

  @override
  State<CameraCaptureScreen> createState() => _CameraCaptureScreenState();
}

class _CameraCaptureScreenState extends State<CameraCaptureScreen> {
  final CameraService _cameraService = CameraService();
  final AudioService _audioService = AudioService();
  final LocationService _locationService = LocationService();
  final MediaUploadService _uploadService = MediaUploadService();
  final TextEditingController _textController = TextEditingController();
  final ScrollController _scrollController = ScrollController();

  bool _isInitialized = false;
  bool _isProcessing = false;
  bool _showChatInput = false;
  File? _capturedImage;
  File? _capturedAudio;
  FlashMode _flashMode = FlashMode.auto;
  final List<String> _chatMessages = [];

  @override
  void initState() {
    super.initState();
    _initializeServices();
  }

  Future<void> _initializeServices() async {
    try {
      final cameraInitialized = await _cameraService.initialize();
      final audioInitialized = await _audioService.initialize();
      final locationPermission = await _locationService.requestPermission();

      if (mounted) {
        setState(() {
          _isInitialized = cameraInitialized && audioInitialized;
        });
      }

      if (!cameraInitialized) {
        _showError('Camera initialization failed');
      }
      if (!audioInitialized) {
        _showError('Microphone permission denied');
      }
      if (!locationPermission) {
        // Just log - location is optional, don't show error to user
        debugPrint('Location permission not available - incident will be submitted without location data');
      }
    } catch (e) {
      if (mounted) {
        _showError('Error initializing services: $e');
      }
    }
  }

  @override
  void dispose() {
    _cameraService.dispose();
    _audioService.dispose();
    _textController.dispose();
    _scrollController.dispose();
    super.dispose();
  }

  void _toggleChatInput() {
    setState(() {
      _showChatInput = !_showChatInput;
    });
  }

  void _sendChatMessage() {
    final message = _textController.text.trim();
    if (message.isEmpty) return;

    setState(() {
      _chatMessages.add(message);
      _textController.clear();
    });

    // Scroll to bottom
    Future.delayed(const Duration(milliseconds: 100), () {
      if (_scrollController.hasClients) {
        _scrollController.animateTo(
          _scrollController.position.maxScrollExtent,
          duration: const Duration(milliseconds: 300),
          curve: Curves.easeOut,
        );
      }
    });
  }

  Future<void> _takePicture() async {
    if (_isProcessing) return;

    setState(() {
      _isProcessing = true;
    });

    try {
      final image = await _cameraService.takePicture();
      if (image != null && mounted) {
        setState(() {
          _capturedImage = image;
        });
      }
    } catch (e) {
      _showError('Error taking picture: $e');
    } finally {
      if (mounted) {
        setState(() {
          _isProcessing = false;
        });
      }
    }
  }

  Future<void> _toggleRecording() async {
    if (_audioService.isRecording) {
      final audio = await _audioService.stopRecording();
      if (audio != null && mounted) {
        setState(() {
          _capturedAudio = audio;
        });
      }
    } else {
      final started = await _audioService.startRecording();
      if (!started) {
        _showError('Failed to start recording');
      }
      setState(() {});
    }
  }

  Future<void> _submitIncident() async {
    if (_isProcessing) return;

    setState(() {
      _isProcessing = true;
    });

    try {
      String? imageUrl;
      String? audioUrl;
      String? transcription;

      if (_capturedImage != null) {
        final imageResult = await _uploadService.uploadImage(_capturedImage!);
        if (imageResult.success) {
          imageUrl = imageResult.url;
        } else {
          _showError('Image upload failed: ${imageResult.error}');
        }
      }

      if (_capturedAudio != null) {
        final audioResult = await _uploadService.uploadAudio(_capturedAudio!);
        if (audioResult.success) {
          audioUrl = audioResult.url;
          transcription = audioResult.metadata?['transcription'];
        } else {
          _showError('Audio upload failed: ${audioResult.error}');
        }
      }

      final location = await _locationService.getCurrentLocation();

      // Build description from chat messages and transcription
      String description = '';
      if (_chatMessages.isNotEmpty) {
        description = _chatMessages.join('\n');
      }
      if (transcription != null && transcription.isNotEmpty) {
        if (description.isNotEmpty) {
          description += '\n\nVoice note: $transcription';
        } else {
          description = transcription;
        }
      }
      if (description.isEmpty) {
        description = 'Incident captured via mobile app';
      }

      final incident = await _uploadService.createIncident(
        title: 'New Incident Report',
        description: description,
        imageUrl: imageUrl,
        audioUrl: audioUrl,
        latitude: location?.latitude,
        longitude: location?.longitude,
        heading: location?.heading,
        metadata: {
          'captureTime': DateTime.now().toIso8601String(),
          'hasImage': _capturedImage != null,
          'hasAudio': _capturedAudio != null,
          'messageCount': _chatMessages.length,
        },
      );

      if (incident != null && mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Incident submitted successfully'),
            backgroundColor: SimsColors.green600,
          ),
        );

        context.pop();
      } else {
        _showError('Failed to create incident');
      }
    } catch (e) {
      _showError('Error submitting incident: $e');
    } finally {
      if (mounted) {
        setState(() {
          _isProcessing = false;
        });
      }
    }
  }

  void _showError(String message) {
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
        backgroundColor: SimsColors.criticalRed,
      ),
    );
  }

  Future<void> _toggleFlashMode() async {
    FlashMode newMode;
    switch (_flashMode) {
      case FlashMode.off:
        newMode = FlashMode.auto;
        break;
      case FlashMode.auto:
        newMode = FlashMode.always;
        break;
      case FlashMode.always:
        newMode = FlashMode.off;
        break;
      default:
        newMode = FlashMode.auto;
    }

    await _cameraService.setFlashMode(newMode);
    setState(() {
      _flashMode = newMode;
    });
  }

  String _getFlashModeIcon() {
    switch (_flashMode) {
      case FlashMode.off:
        return 'flash_off';
      case FlashMode.auto:
        return 'flash_auto';
      case FlashMode.always:
        return 'flash_on';
      default:
        return 'flash_auto';
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      resizeToAvoidBottomInset: false,
      body: SafeArea(
        child: Column(
          children: [
            // Camera preview - shrinks when chat is shown
            Flexible(
              flex: _showChatInput ? 3 : 10,
              child: Stack(
                children: [
                  if (_isInitialized && _cameraService.controller != null)
                    SizedBox.expand(
                      child: FittedBox(
                        fit: BoxFit.cover,
                        child: SizedBox(
                          width: _cameraService.controller!.value.previewSize!.height,
                          height: _cameraService.controller!.value.previewSize!.width,
                          child: CameraPreview(_cameraService.controller!),
                        ),
                      ),
                    )
                  else
                    const Center(
                      child: CircularProgressIndicator(color: SimsColors.teal),
                    ),
                  // Top controls
                  Positioned(
                    top: 16,
                    left: 16,
                    right: 16,
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        IconButton(
                          icon: const Icon(Icons.close, color: Colors.white, size: 32),
                          onPressed: () => context.pop(),
                        ),
                        IconButton(
                          icon: Icon(
                            _flashMode == FlashMode.off
                                ? Icons.flash_off
                                : _flashMode == FlashMode.auto
                                    ? Icons.flash_auto
                                    : Icons.flash_on,
                            color: Colors.white,
                            size: 32,
                          ),
                          onPressed: _toggleFlashMode,
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),

            // Chat interface - slides up when shown
            if (_showChatInput)
              Flexible(
                flex: 5,
                child: Container(
                  color: SimsColors.navyBlue,
                  child: Column(
                    children: [
                      // Chat messages
                      Expanded(
                        child: ListView.builder(
                          controller: _scrollController,
                          padding: const EdgeInsets.all(16),
                          itemCount: _chatMessages.length,
                          itemBuilder: (context, index) {
                            return Container(
                              margin: const EdgeInsets.only(bottom: 8),
                              padding: const EdgeInsets.all(12),
                              decoration: BoxDecoration(
                                color: SimsColors.accentBlue,
                                borderRadius: BorderRadius.circular(12),
                              ),
                              child: Text(
                                _chatMessages[index],
                                style: const TextStyle(color: Colors.white),
                              ),
                            );
                          },
                        ),
                      ),
                      // Text input
                      Container(
                        padding: const EdgeInsets.all(16),
                        decoration: BoxDecoration(
                          color: SimsColors.navyBlueLight,
                          boxShadow: [
                            BoxShadow(
                              color: Colors.black.withOpacity(0.2),
                              blurRadius: 8,
                              offset: const Offset(0, -2),
                            ),
                          ],
                        ),
                        child: Row(
                          children: [
                            Expanded(
                              child: TextField(
                                controller: _textController,
                                style: const TextStyle(color: Colors.white),
                                decoration: InputDecoration(
                                  hintText: 'Describe the incident...',
                                  hintStyle: TextStyle(color: Colors.white.withOpacity(0.5)),
                                  border: OutlineInputBorder(
                                    borderRadius: BorderRadius.circular(24),
                                    borderSide: BorderSide.none,
                                  ),
                                  filled: true,
                                  fillColor: SimsColors.navyBlueDark,
                                  contentPadding: const EdgeInsets.symmetric(
                                    horizontal: 16,
                                    vertical: 12,
                                  ),
                                ),
                                maxLines: null,
                                textInputAction: TextInputAction.send,
                                onSubmitted: (_) => _sendChatMessage(),
                              ),
                            ),
                            const SizedBox(width: 8),
                            IconButton(
                              icon: const Icon(Icons.send, color: SimsColors.accentBlue),
                              onPressed: _sendChatMessage,
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                ),
              ),

            // Bottom controls
            Container(
              padding: const EdgeInsets.all(24),
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                  colors: [
                    _showChatInput ? SimsColors.navyBlue : Colors.transparent,
                    _showChatInput ? SimsColors.navyBlue : Colors.black.withOpacity(0.7),
                    _showChatInput ? SimsColors.navyBlue : Colors.black,
                  ],
                ),
              ),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  if (_capturedImage != null || _capturedAudio != null || _chatMessages.isNotEmpty)
                    Container(
                      margin: const EdgeInsets.only(bottom: 16),
                      padding: const EdgeInsets.all(12),
                      decoration: BoxDecoration(
                        color: SimsColors.navyBlueLight,
                        borderRadius: BorderRadius.circular(12),
                      ),
                      child: Column(
                        children: [
                          if (_capturedImage != null)
                            Row(
                              children: [
                                const Icon(Icons.image, color: SimsColors.teal),
                                const SizedBox(width: 8),
                                const Text(
                                  'Image captured',
                                  style: TextStyle(color: Colors.white),
                                ),
                                const Spacer(),
                                IconButton(
                                  icon: const Icon(Icons.close, color: Colors.white),
                                  onPressed: () {
                                    setState(() {
                                      _capturedImage = null;
                                    });
                                  },
                                ),
                              ],
                            ),
                          if (_capturedAudio != null)
                            Row(
                              children: [
                                const Icon(Icons.mic, color: SimsColors.teal),
                                const SizedBox(width: 8),
                                const Text(
                                  'Audio recorded',
                                  style: TextStyle(color: Colors.white),
                                ),
                                const Spacer(),
                                IconButton(
                                  icon: const Icon(Icons.close, color: Colors.white),
                                  onPressed: () {
                                    setState(() {
                                      _capturedAudio = null;
                                    });
                                  },
                                ),
                              ],
                            ),
                          if (_chatMessages.isNotEmpty)
                            Row(
                              children: [
                                const Icon(Icons.chat, color: SimsColors.teal),
                                const SizedBox(width: 8),
                                Text(
                                  '${_chatMessages.length} message${_chatMessages.length > 1 ? 's' : ''}',
                                  style: const TextStyle(color: Colors.white),
                                ),
                              ],
                            ),
                        ],
                      ),
                    ),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: [
                      // Camera button
                      _buildControlButton(
                        icon: Icons.camera_alt,
                        color: _audioService.isRecording ? Colors.grey : SimsColors.white,
                        iconColor: SimsColors.blue,
                        onTap: _audioService.isRecording ? null : _takePicture,
                        isLoading: _isProcessing && !_audioService.isRecording,
                      ),
                      // Microphone button
                      _buildControlButton(
                        icon: _audioService.isRecording ? Icons.stop : Icons.mic,
                        color: _audioService.isRecording ? SimsColors.criticalRed : SimsColors.white,
                        iconColor: _audioService.isRecording ? Colors.white : SimsColors.blue,
                        onTap: _toggleRecording,
                      ),
                      // Keyboard/Text button
                      _buildControlButton(
                        icon: _showChatInput ? Icons.keyboard_hide : Icons.keyboard,
                        color: _showChatInput ? SimsColors.accentBlue : SimsColors.white,
                        iconColor: _showChatInput ? Colors.white : SimsColors.blue,
                        onTap: _toggleChatInput,
                      ),
                      // Submit button (only if something was captured)
                      if (_capturedImage != null || _capturedAudio != null || _chatMessages.isNotEmpty)
                        _buildControlButton(
                          icon: Icons.send,
                          color: _isProcessing ? Colors.grey : SimsColors.green600,
                          iconColor: Colors.white,
                          onTap: _isProcessing ? null : _submitIncident,
                          isLoading: _isProcessing && _audioService.isRecording,
                        ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildControlButton({
    required IconData icon,
    required Color color,
    required Color iconColor,
    required VoidCallback? onTap,
    bool isLoading = false,
  }) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        width: 70,
        height: 70,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color: color,
          border: Border.all(color: Colors.white, width: 4),
        ),
        child: isLoading
            ? Padding(
                padding: const EdgeInsets.all(16.0),
                child: CircularProgressIndicator(
                  strokeWidth: 3,
                  color: iconColor,
                ),
              )
            : Icon(icon, size: 32, color: iconColor),
      ),
    );
  }
}
