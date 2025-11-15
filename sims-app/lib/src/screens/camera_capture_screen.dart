import 'dart:io';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:camera/camera.dart';
import 'package:go_router/go_router.dart';
import 'package:image_picker/image_picker.dart';
import 'package:http/http.dart' as http;
import '../services/camera_service.dart';
import '../services/audio_service.dart';
import '../services/location_service.dart';
import '../services/media_upload_service.dart';
import '../repositories/user_repository.dart';
import '../config/app_config.dart';
import '../utils/sims_colors.dart';

export '../services/media_upload_service.dart' show UploadResult;

// Chat message types
enum MessageType { text, image, audio, video }

class ChatMessage {
  final String id;
  final MessageType type;
  final String? text;
  final String? mediaUrl;
  final File? localFile;
  final DateTime timestamp;
  final bool isUploading;
  final bool failed;

  ChatMessage({
    required this.id,
    required this.type,
    this.text,
    this.mediaUrl,
    this.localFile,
    required this.timestamp,
    this.isUploading = false,
    this.failed = false,
  });

  ChatMessage copyWith({
    bool? isUploading,
    bool? failed,
    String? mediaUrl,
  }) {
    return ChatMessage(
      id: id,
      type: type,
      text: text,
      mediaUrl: mediaUrl ?? this.mediaUrl,
      localFile: localFile,
      timestamp: timestamp,
      isUploading: isUploading ?? this.isUploading,
      failed: failed ?? this.failed,
    );
  }
}

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
  final ImagePicker _imagePicker = ImagePicker();
  final TextEditingController _textController = TextEditingController();
  final ScrollController _scrollController = ScrollController();

  bool _isInitialized = false;
  FlashMode _flashMode = FlashMode.auto;
  final List<ChatMessage> _messages = [];
  bool _showTextInput = false;
  String? _currentIncidentId;

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

  void _scrollToBottom() {
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

  void _toggleTextInput() {
    setState(() {
      _showTextInput = !_showTextInput;
    });
  }

  Future<String?> _createIncident() async {
    try {
      final location = await _locationService.getCurrentLocation();
      final userRepo = await UserRepository.getInstance();
      final userPhone = userRepo.getPhoneNumberSync();

      final response = await http.post(
        Uri.parse('${AppConfig.baseUrl}/api/incidents'),
        headers: {'Content-Type': 'application/json'},
        body: json.encode({
          'title': 'Incident Report',
          'description': 'Incident reported from mobile app',
          'latitude': location?.latitude,
          'longitude': location?.longitude,
          'heading': location?.heading,
          'timestamp': DateTime.now().toIso8601String(),
          'user_phone': userPhone,
        }),
      );

      debugPrint('Create incident response: ${response.statusCode}');

      if (response.statusCode == 200 || response.statusCode == 201) {
        final data = json.decode(response.body);
        debugPrint('Incident created: ${data['id']}');
        return data['id'];
      } else {
        debugPrint('Failed to create incident: ${response.body}');
        return null;
      }
    } catch (e) {
      debugPrint('Error creating incident: $e');
      return null;
    }
  }

  Future<void> _sendTextMessage() async {
    final text = _textController.text.trim();
    if (text.isEmpty) return;

    // Create incident if not already created
    if (_currentIncidentId == null) {
      _currentIncidentId = await _createIncident();
      if (_currentIncidentId == null) {
        _showError('Failed to create incident');
        return;
      }
    }

    final messageId = DateTime.now().millisecondsSinceEpoch.toString();
    final message = ChatMessage(
      id: messageId,
      type: MessageType.text,
      text: text,
      timestamp: DateTime.now(),
    );

    setState(() {
      _messages.add(message);
      _textController.clear();
    });

    _scrollToBottom();
  }

  Future<void> _takePicture() async {
    try {
      final image = await _cameraService.takePicture();
      if (image == null) {
        _showError('Failed to capture image');
        return;
      }

      await _uploadMedia(image, MessageType.image);
    } catch (e) {
      _showError('Error taking picture: $e');
    }
  }

  Future<void> _startVideoRecording() async {
    try {
      final started = await _cameraService.startVideoRecording();
      if (!started) {
        _showError('Failed to start video recording');
        return;
      }
      setState(() {});
    } catch (e) {
      _showError('Error starting video recording: $e');
    }
  }

  Future<void> _stopVideoRecording() async {
    try {
      final video = await _cameraService.stopVideoRecording();
      if (video == null) {
        _showError('Failed to save video');
        return;
      }

      setState(() {});
      await _uploadMedia(video, MessageType.video);
    } catch (e) {
      _showError('Error stopping video recording: $e');
      setState(() {});
    }
  }

  Future<void> _toggleRecording() async {
    if (_audioService.isRecording) {
      final audio = await _audioService.stopRecording();
      if (audio == null) {
        _showError('Failed to save recording');
        return;
      }

      await _uploadMedia(audio, MessageType.audio);
    } else {
      final started = await _audioService.startRecording();
      if (!started) {
        _showError('Failed to start recording');
      }
      setState(() {});
    }
  }

  Future<void> _pickFromGallery() async {
    try {
      final XFile? pickedFile = await _imagePicker.pickImage(
        source: ImageSource.gallery,
      );

      if (pickedFile == null) return;

      final image = File(pickedFile.path);
      await _uploadMedia(image, MessageType.image);
    } catch (e) {
      _showError('Error picking image: $e');
    }
  }

  Future<void> _uploadMedia(File file, MessageType type) async {
    // Create incident if not already created
    if (_currentIncidentId == null) {
      _currentIncidentId = await _createIncident();
      if (_currentIncidentId == null) {
        _showError('Failed to create incident');
        return;
      }
    }

    final messageId = DateTime.now().millisecondsSinceEpoch.toString();
    final message = ChatMessage(
      id: messageId,
      type: type,
      localFile: file,
      timestamp: DateTime.now(),
      isUploading: true,
    );

    setState(() {
      _messages.add(message);
    });

    _scrollToBottom();

    // Upload based on type with incident_id
    final UploadResult result;
    switch (type) {
      case MessageType.image:
        result = await _uploadService.uploadImage(file, incidentId: _currentIncidentId);
        break;
      case MessageType.audio:
        result = await _uploadService.uploadAudio(file, incidentId: _currentIncidentId);
        break;
      case MessageType.video:
        result = await _uploadService.uploadVideo(file, incidentId: _currentIncidentId);
        break;
      default:
        result = UploadResult(success: false, error: 'Unknown message type');
    }

    if (result.success && mounted) {
      final index = _messages.indexWhere((m) => m.id == messageId);
      if (index != -1) {
        setState(() {
          _messages[index] = message.copyWith(
            isUploading: false,
            mediaUrl: result.url,
          );
        });
      }
    } else if (mounted) {
      final index = _messages.indexWhere((m) => m.id == messageId);
      if (index != -1) {
        setState(() {
          _messages[index] = message.copyWith(
            isUploading: false,
            failed: true,
          );
        });
      }
      _showError('Failed to upload ${type.name}');
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

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      resizeToAvoidBottomInset: true,
      body: SafeArea(
        child: Stack(
          children: [
            // Camera preview - full screen background
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

            // Chat area (only when there are messages) - positioned at bottom 5/8 of screen
            if (_messages.isNotEmpty)
              Positioned(
                left: 0,
                right: 0,
                bottom: 0,
                top: MediaQuery.of(context).size.height * 0.375, // Start at 3/8 from top (leaves 5/8 for chat)
                child: Container(
                  color: SimsColors.navyBlue.withOpacity(0.85),
                  child: Column(
                    children: [
                      // Chat messages list
                      Expanded(
                        child: ListView.builder(
                          controller: _scrollController,
                          padding: const EdgeInsets.all(16),
                          itemCount: _messages.length,
                          itemBuilder: (context, index) {
                            return _buildMessageBubble(_messages[index]);
                          },
                        ),
                      ),

                      // Text input (only show when keyboard button is pressed)
                      if (_showTextInput)
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
                                  onSubmitted: (_) => _sendTextMessage(),
                                ),
                              ),
                              const SizedBox(width: 8),
                              IconButton(
                                icon: const Icon(Icons.send, color: SimsColors.accentBlue),
                                onPressed: _sendTextMessage,
                              ),
                            ],
                          ),
                        ),

                      // Bottom controls
                      Container(
                        padding: const EdgeInsets.all(16),
                        color: SimsColors.navyBlue,
                        child: Row(
                          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                          children: [
                            GestureDetector(
                              onTap: _takePicture,
                              onLongPress: _startVideoRecording,
                              onLongPressUp: _stopVideoRecording,
                              child: _buildControlButtonWidget(
                                icon: _cameraService.isRecordingVideo ? Icons.videocam : Icons.camera_alt,
                                color: _cameraService.isRecordingVideo ? SimsColors.criticalRed : SimsColors.white,
                                iconColor: _cameraService.isRecordingVideo ? Colors.white : SimsColors.blue,
                              ),
                            ),
                            _buildControlButton(
                              icon: _audioService.isRecording ? Icons.stop : Icons.mic,
                              color: _audioService.isRecording ? SimsColors.criticalRed : SimsColors.white,
                              iconColor: _audioService.isRecording ? Colors.white : SimsColors.blue,
                              onTap: _toggleRecording,
                            ),
                            _buildControlButton(
                              icon: Icons.photo_library,
                              color: SimsColors.white,
                              iconColor: SimsColors.blue,
                              onTap: _pickFromGallery,
                            ),
                            _buildControlButton(
                              icon: _showTextInput ? Icons.keyboard_hide : Icons.keyboard,
                              color: _showTextInput ? SimsColors.accentBlue : SimsColors.white,
                              iconColor: _showTextInput ? Colors.white : SimsColors.blue,
                              onTap: _toggleTextInput,
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                ),
              ),

            // Bottom controls (when no messages) - positioned at bottom
            if (_messages.isEmpty)
              Positioned(
                left: 0,
                right: 0,
                bottom: 0,
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    // Text input (only show when keyboard button is pressed)
                    if (_showTextInput)
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
                                onSubmitted: (_) => _sendTextMessage(),
                              ),
                            ),
                            const SizedBox(width: 8),
                            IconButton(
                              icon: const Icon(Icons.send, color: SimsColors.accentBlue),
                              onPressed: _sendTextMessage,
                            ),
                          ],
                        ),
                      ),

                    // Bottom controls
                    Container(
                      padding: const EdgeInsets.all(16),
                      color: SimsColors.navyBlue,
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                        children: [
                          GestureDetector(
                            onTap: _takePicture,
                            onLongPress: _startVideoRecording,
                            onLongPressUp: _stopVideoRecording,
                            child: _buildControlButtonWidget(
                              icon: _cameraService.isRecordingVideo ? Icons.videocam : Icons.camera_alt,
                              color: _cameraService.isRecordingVideo ? SimsColors.criticalRed : SimsColors.white,
                              iconColor: _cameraService.isRecordingVideo ? Colors.white : SimsColors.blue,
                            ),
                          ),
                          _buildControlButton(
                            icon: _audioService.isRecording ? Icons.stop : Icons.mic,
                            color: _audioService.isRecording ? SimsColors.criticalRed : SimsColors.white,
                            iconColor: _audioService.isRecording ? Colors.white : SimsColors.blue,
                            onTap: _toggleRecording,
                          ),
                          _buildControlButton(
                            icon: Icons.photo_library,
                            color: SimsColors.white,
                            iconColor: SimsColors.blue,
                            onTap: _pickFromGallery,
                          ),
                          _buildControlButton(
                            icon: _showTextInput ? Icons.keyboard_hide : Icons.keyboard,
                            color: _showTextInput ? SimsColors.accentBlue : SimsColors.white,
                            iconColor: _showTextInput ? Colors.white : SimsColors.blue,
                            onTap: _toggleTextInput,
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
          ],
        ),
      ),
    );
  }

  Widget _buildMessageBubble(ChatMessage message) {
    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Message content
          Expanded(
            child: Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: SimsColors.accentBlue,
                borderRadius: BorderRadius.circular(12),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  if (message.type == MessageType.text)
                    Text(
                      message.text!,
                      style: const TextStyle(color: Colors.white),
                    )
                  else if (message.type == MessageType.image)
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        if (message.localFile != null)
                          ClipRRect(
                            borderRadius: BorderRadius.circular(8),
                            child: Image.file(
                              message.localFile!,
                              height: 200,
                              width: double.infinity,
                              fit: BoxFit.cover,
                            ),
                          ),
                        if (message.isUploading)
                          const Padding(
                            padding: EdgeInsets.only(top: 8),
                            child: Row(
                              children: [
                                SizedBox(
                                  width: 16,
                                  height: 16,
                                  child: CircularProgressIndicator(
                                    strokeWidth: 2,
                                    color: Colors.white,
                                  ),
                                ),
                                SizedBox(width: 8),
                                Text(
                                  'Uploading...',
                                  style: TextStyle(color: Colors.white70, fontSize: 12),
                                ),
                              ],
                            ),
                          ),
                        if (message.failed)
                          const Padding(
                            padding: EdgeInsets.only(top: 8),
                            child: Row(
                              children: [
                                Icon(Icons.error, color: Colors.red, size: 16),
                                SizedBox(width: 8),
                                Text(
                                  'Upload failed',
                                  style: TextStyle(color: Colors.red, fontSize: 12),
                                ),
                              ],
                            ),
                          ),
                      ],
                    )
                  else if (message.type == MessageType.audio)
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Row(
                          children: [
                            Icon(Icons.mic, color: Colors.white),
                            SizedBox(width: 8),
                            Text(
                              'Voice message',
                              style: TextStyle(color: Colors.white),
                            ),
                          ],
                        ),
                        if (message.isUploading)
                          const Padding(
                            padding: EdgeInsets.only(top: 8),
                            child: Row(
                              children: [
                                SizedBox(
                                  width: 16,
                                  height: 16,
                                  child: CircularProgressIndicator(
                                    strokeWidth: 2,
                                    color: Colors.white,
                                  ),
                                ),
                                SizedBox(width: 8),
                                Text(
                                  'Uploading...',
                                  style: TextStyle(color: Colors.white70, fontSize: 12),
                                ),
                              ],
                            ),
                          ),
                        if (message.failed)
                          const Padding(
                            padding: EdgeInsets.only(top: 8),
                            child: Row(
                              children: [
                                Icon(Icons.error, color: Colors.red, size: 16),
                                SizedBox(width: 8),
                                Text(
                                  'Upload failed',
                                  style: TextStyle(color: Colors.red, fontSize: 12),
                                ),
                              ],
                            ),
                          ),
                      ],
                    )
                  else if (message.type == MessageType.video)
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Row(
                          children: [
                            Icon(Icons.videocam, color: Colors.white),
                            SizedBox(width: 8),
                            Text(
                              'Video message',
                              style: TextStyle(color: Colors.white),
                            ),
                          ],
                        ),
                        if (message.isUploading)
                          const Padding(
                            padding: EdgeInsets.only(top: 8),
                            child: Row(
                              children: [
                                SizedBox(
                                  width: 16,
                                  height: 16,
                                  child: CircularProgressIndicator(
                                    strokeWidth: 2,
                                    color: Colors.white,
                                  ),
                                ),
                                SizedBox(width: 8),
                                Text(
                                  'Uploading...',
                                  style: TextStyle(color: Colors.white70, fontSize: 12),
                                ),
                              ],
                            ),
                          ),
                        if (message.failed)
                          const Padding(
                            padding: EdgeInsets.only(top: 8),
                            child: Row(
                              children: [
                                Icon(Icons.error, color: Colors.red, size: 16),
                                SizedBox(width: 8),
                                Text(
                                  'Upload failed',
                                  style: TextStyle(color: Colors.red, fontSize: 12),
                                ),
                              ],
                            ),
                          ),
                      ],
                    ),
                  // Timestamp
                  Padding(
                    padding: const EdgeInsets.only(top: 4),
                    child: Text(
                      '${message.timestamp.hour}:${message.timestamp.minute.toString().padLeft(2, '0')}',
                      style: const TextStyle(
                        color: Colors.white60,
                        fontSize: 10,
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildControlButtonWidget({
    required IconData icon,
    required Color color,
    required Color iconColor,
  }) {
    return Container(
      width: 60,
      height: 60,
      decoration: BoxDecoration(
        shape: BoxShape.circle,
        color: color,
        border: Border.all(color: Colors.white, width: 3),
      ),
      child: Icon(icon, size: 28, color: iconColor),
    );
  }

  Widget _buildControlButton({
    required IconData icon,
    required Color color,
    required Color iconColor,
    required VoidCallback? onTap,
  }) {
    return GestureDetector(
      onTap: onTap,
      child: _buildControlButtonWidget(
        icon: icon,
        color: color,
        iconColor: iconColor,
      ),
    );
  }
}
