import 'dart:io';
import 'dart:convert';
import 'dart:async';
import 'package:flutter/material.dart';
import 'package:camera/camera.dart';
import 'package:go_router/go_router.dart';
import 'package:image_picker/image_picker.dart';
import 'package:http/http.dart' as http;
import 'package:uuid/uuid.dart';
import '../services/camera_service.dart';
import '../services/audio_service.dart';
import '../services/location_service.dart';
import '../services/media_upload_service.dart';
import '../repositories/user_repository.dart';
import '../config/app_config.dart';
import '../utils/sims_colors.dart';
import '../connection/websocket_service.dart';

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
  final WebSocketService _websocketService = WebSocketService();
  final ImagePicker _imagePicker = ImagePicker();
  final TextEditingController _textController = TextEditingController();
  final ScrollController _scrollController = ScrollController();
  StreamSubscription? _websocketSubscription;

  bool _isInitialized = false;
  FlashMode _flashMode = FlashMode.auto;
  final List<ChatMessage> _messages = [];
  bool _showTextInput = false;
  String? _currentIncidentId; // Formatted ID (INC-xxx) for display
  String? _currentIncidentUuid; // Database UUID for API calls
  bool _creatingIncident = false; // Flag to prevent concurrent incident creation
  late final String _sessionId; // Generate immediately on screen creation

  @override
  void initState() {
    super.initState();
    // Generate session ID immediately - don't wait for backend
    _sessionId = const Uuid().v4();
    _initializeServices();
    _initializeWebSocket();
  }

  void _initializeWebSocket() {
    // Listen for incoming WebSocket messages
    _websocketSubscription = _websocketService.messageStream.listen((message) {
      if (message['type'] == 'chat_message') {
        _handleIncomingChatMessage(message);
      }
    });

    // Ensure WebSocket is connected
    if (!_websocketService.isConnected) {
      _websocketService.connect();
    }
    _websocketService.subscribeToIncidents();
  }

  void _handleIncomingChatMessage(Map<String, dynamic> message) {
    // Only handle messages for our current incident
    if (_currentIncidentUuid != null && message['incident_id'] == _currentIncidentUuid) {
      final messageData = message['message'];
      final content = messageData['data']['content'] as String?;
      final isTyping = messageData['data']['is_typing'] as bool? ?? false;

      if (content != null) {
        setState(() {
          // Add AI response message
          _messages.add(ChatMessage(
            id: const Uuid().v4(),
            type: MessageType.text,
            text: content,
            timestamp: DateTime.now(),
          ));
        });
        _scrollToBottom();

        debugPrint('Received chat message: $content');
      }
    }
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
    _websocketSubscription?.cancel();
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

  Future<String?> _createIncident({String? description}) async {
    // Prevent concurrent incident creation
    if (_creatingIncident) {
      debugPrint('Incident creation already in progress, waiting...');
      // Wait for the current creation to complete
      int attempts = 0;
      while (_creatingIncident && attempts < 50) {
        await Future.delayed(const Duration(milliseconds: 100));
        attempts++;
      }
      // Return the existing incident ID if it was created
      if (_currentIncidentUuid != null) {
        debugPrint('Using existing incident: $_currentIncidentId (UUID: $_currentIncidentUuid)');
        return _currentIncidentUuid;
      }
    }

    // If incident already exists, return it
    if (_currentIncidentUuid != null) {
      debugPrint('Incident already exists: $_currentIncidentId (UUID: $_currentIncidentUuid)');
      return _currentIncidentUuid;
    }

    try {
      _creatingIncident = true;

      // APP generates the incident ID, not the backend!
      _currentIncidentUuid = const Uuid().v4();
      debugPrint('Creating new incident with app-generated ID: $_currentIncidentUuid');

      final location = await _locationService.getCurrentLocation();

      if (location == null) {
        debugPrint('WARNING: Location data unavailable - incident will be created without coordinates');
        debugPrint('This may be due to GPS issues on older devices or poor signal');
      } else {
        debugPrint('Location obtained: ${location.latitude}, ${location.longitude} (accuracy: ${location.accuracy}m)');
      }

      final userRepo = await UserRepository.getInstance();
      final userPhone = userRepo.getPhoneNumberSync();

      debugPrint('Creating incident with phone number: $userPhone');

      // Use provided description or generate based on existing messages
      String incidentDescription = description ?? 'Incident report';
      if (incidentDescription == 'Incident report' && _messages.isNotEmpty) {
        final firstMessage = _messages.first;
        if (firstMessage.type == MessageType.image) {
          incidentDescription = 'Incident with photo';
        } else if (firstMessage.type == MessageType.video) {
          incidentDescription = 'Incident with video';
        } else if (firstMessage.type == MessageType.audio) {
          incidentDescription = 'Incident with audio';
        } else if (firstMessage.text != null && firstMessage.text!.isNotEmpty) {
          incidentDescription = firstMessage.text!;
        }
      }

      final response = await http.post(
        Uri.parse('${AppConfig.baseUrl}/api/incidents'),
        headers: {'Content-Type': 'application/json'},
        body: json.encode({
          'id': _currentIncidentUuid, // Send app-generated ID to backend
          'session_id': _sessionId, // Session ID for the entire chat
          'title': DateTime.now().toString().substring(0, 16),
          'description': incidentDescription,
          'latitude': location?.latitude,
          'longitude': location?.longitude,
          'altitude': location?.altitude,
          'heading': location?.heading,
          'timestamp': DateTime.now().toIso8601String(),
          'user_phone': userPhone,
        }),
      );

      debugPrint('Create incident response: ${response.statusCode}');
      debugPrint('Create incident response body: ${response.body}');

      if (response.statusCode == 200 || response.statusCode == 201) {
        final data = json.decode(response.body);
        debugPrint('Incident data: $data');

        // Backend should echo back our ID and the formatted ID
        _currentIncidentId = data['incidentId'] ?? _currentIncidentUuid;

        debugPrint('Incident created successfully: $_currentIncidentId (UUID: $_currentIncidentUuid, Session: $_sessionId)');

        // Return the UUID for internal use
        return _currentIncidentUuid;
      } else {
        debugPrint('Failed to create incident: ${response.body}');
        _currentIncidentUuid = null; // Reset on failure
        return null;
      }
    } catch (e) {
      debugPrint('Error creating incident: $e');
      _currentIncidentUuid = null; // Reset on failure
      return null;
    } finally {
      _creatingIncident = false;
    }
  }

  Future<void> _sendTextMessage() async {
    final text = _textController.text.trim();
    if (text.isEmpty) return;

    // Create message ID for tracking
    final messageId = const Uuid().v4();

    // Show message immediately in chat (WhatsApp style)
    final message = ChatMessage(
      id: messageId,
      type: MessageType.text,
      text: text,
      timestamp: DateTime.now(),
      isUploading: true, // Show uploading indicator
    );

    setState(() {
      _messages.add(message);
      _textController.clear(); // Clear input immediately
    });

    _scrollToBottom();

    // Create incident if not already created
    if (_currentIncidentId == null) {
      _currentIncidentId = await _createIncident(description: text);
      if (_currentIncidentId == null) {
        _showError('Failed to create incident');
        // Mark message as failed
        _updateMessageStatus(messageId, failed: true);
        return;
      }
    }

    // Send text message to backend
    try {
      final response = await http.post(
        Uri.parse('${AppConfig.baseUrl}/api/incident/$_currentIncidentUuid/chat'),
        headers: {'Content-Type': 'application/json'},
        body: json.encode({
          'session_id': _sessionId,
          'message': text,
        }),
      );

      if (response.statusCode != 200 && response.statusCode != 201) {
        debugPrint('Failed to send chat message: ${response.body}');
        _updateMessageStatus(messageId, failed: true);
      } else {
        debugPrint('Chat message sent successfully');
        _updateMessageStatus(messageId, uploaded: true);
      }
    } catch (e) {
      debugPrint('Error sending chat message: $e');
      _updateMessageStatus(messageId, failed: true);
    }
  }

  void _updateMessageStatus(String messageId, {bool uploaded = false, bool failed = false}) {
    setState(() {
      final index = _messages.indexWhere((m) => m.id == messageId);
      if (index != -1) {
        _messages[index] = ChatMessage(
          id: _messages[index].id,
          type: _messages[index].type,
          text: _messages[index].text,
          mediaUrl: _messages[index].mediaUrl,
          localFile: _messages[index].localFile,
          timestamp: _messages[index].timestamp,
          isUploading: uploaded ? false : _messages[index].isUploading,
          failed: failed,
        );
      }
    });
  }

  Future<void> _takePicture() async {
    try {
      final image = await _cameraService.takePicture();
      if (image == null) {
        _showError('Failed to capture image');
        return;
      }

      // Show image immediately in chat (WhatsApp style)
      final messageId = const Uuid().v4();
      final message = ChatMessage(
        id: messageId,
        type: MessageType.image,
        localFile: image,
        timestamp: DateTime.now(),
        isUploading: true,
      );

      setState(() {
        _messages.add(message);
      });

      _scrollToBottom();

      // Create incident in background if needed
      if (_currentIncidentId == null) {
        _currentIncidentId = await _createIncident();
        if (_currentIncidentId == null) {
          setState(() {
            final index = _messages.indexWhere((m) => m.id == messageId);
            if (index != -1) {
              _messages[index] = _messages[index].copyWith(
                isUploading: false,
                failed: true,
              );
            }
          });
          _showError('Failed to create incident');
          return;
        }
      }

      // Upload in background
      _uploadMediaInBackground(image, MessageType.image, messageId);
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

      // Show video immediately in chat (WhatsApp style)
      final messageId = const Uuid().v4();
      final message = ChatMessage(
        id: messageId,
        type: MessageType.video,
        localFile: video,
        timestamp: DateTime.now(),
        isUploading: true,
      );

      setState(() {
        _messages.add(message);
      });

      _scrollToBottom();

      // Create incident in background if needed
      if (_currentIncidentId == null) {
        _currentIncidentId = await _createIncident();
        if (_currentIncidentId == null) {
          setState(() {
            final index = _messages.indexWhere((m) => m.id == messageId);
            if (index != -1) {
              _messages[index] = _messages[index].copyWith(
                isUploading: false,
                failed: true,
              );
            }
          });
          _showError('Failed to create incident');
          return;
        }
      }

      // Upload in background
      _uploadMediaInBackground(video, MessageType.video, messageId);
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

      // Show audio immediately in chat (WhatsApp style)
      final messageId = const Uuid().v4();
      final message = ChatMessage(
        id: messageId,
        type: MessageType.audio,
        localFile: audio,
        timestamp: DateTime.now(),
        isUploading: true,
      );

      setState(() {
        _messages.add(message);
      });

      _scrollToBottom();

      // Create incident in background if needed
      if (_currentIncidentId == null) {
        _currentIncidentId = await _createIncident();
        if (_currentIncidentId == null) {
          setState(() {
            final index = _messages.indexWhere((m) => m.id == messageId);
            if (index != -1) {
              _messages[index] = _messages[index].copyWith(
                isUploading: false,
                failed: true,
              );
            }
          });
          _showError('Failed to create incident');
          return;
        }
      }

      // Upload in background
      _uploadMediaInBackground(audio, MessageType.audio, messageId);
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

      // Show image immediately in chat (WhatsApp style)
      final messageId = const Uuid().v4();
      final message = ChatMessage(
        id: messageId,
        type: MessageType.image,
        localFile: image,
        timestamp: DateTime.now(),
        isUploading: true,
      );

      setState(() {
        _messages.add(message);
      });

      _scrollToBottom();

      // Create incident in background if needed
      if (_currentIncidentId == null) {
        _currentIncidentId = await _createIncident();
        if (_currentIncidentId == null) {
          setState(() {
            final index = _messages.indexWhere((m) => m.id == messageId);
            if (index != -1) {
              _messages[index] = _messages[index].copyWith(
                isUploading: false,
                failed: true,
              );
            }
          });
          _showError('Failed to create incident');
          return;
        }
      }

      // Upload in background
      _uploadMediaInBackground(image, MessageType.image, messageId);
    } catch (e) {
      _showError('Error picking image: $e');
    }
  }


  Future<void> _uploadMediaInBackground(File file, MessageType type, String messageId) async {
    try {
      // Upload based on type with incident UUID (not formatted ID)
      final UploadResult result;
      switch (type) {
        case MessageType.image:
          result = await _uploadService.uploadImage(file, incidentId: _currentIncidentUuid);
          break;
        case MessageType.audio:
          result = await _uploadService.uploadAudio(file, incidentId: _currentIncidentUuid);
          break;
        case MessageType.video:
          result = await _uploadService.uploadVideo(file, incidentId: _currentIncidentUuid);
          break;
        default:
          result = UploadResult(success: false, error: 'Unknown message type');
      }

      if (result.success && mounted) {
        final index = _messages.indexWhere((m) => m.id == messageId);
        if (index != -1) {
          setState(() {
            _messages[index] = _messages[index].copyWith(
              isUploading: false,
              mediaUrl: result.url,
            );
          });

          // Delete local file after successful upload to save storage
          try {
            if (_messages[index].localFile != null && await _messages[index].localFile!.exists()) {
              await _messages[index].localFile!.delete();
              debugPrint('Deleted local file after upload: ${_messages[index].localFile!.path}');
            }
          } catch (e) {
            debugPrint('Error deleting local file: $e');
          }
        }
      } else if (mounted) {
        final index = _messages.indexWhere((m) => m.id == messageId);
        if (index != -1) {
          setState(() {
            _messages[index] = _messages[index].copyWith(
              isUploading: false,
              failed: true,
            );
          });
        }
        _showError('Failed to upload ${type.name}');
      }
    } catch (e) {
      debugPrint('Error uploading media: $e');
      if (mounted) {
        final index = _messages.indexWhere((m) => m.id == messageId);
        if (index != -1) {
          setState(() {
            _messages[index] = _messages[index].copyWith(
              isUploading: false,
              failed: true,
            );
          });
        }
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
                child: CircularProgressIndicator(color: SimsColors.accentCyan),
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
                  color: SimsColors.background.withOpacity(0.90),
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
                            color: SimsColors.backgroundLight,
                            boxShadow: [
                              BoxShadow(
                                color: Colors.black.withOpacity(0.3),
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
                                      borderRadius: BorderRadius.circular(20),
                                      borderSide: BorderSide.none,
                                    ),
                                    filled: true,
                                    fillColor: SimsColors.background,
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
                                icon: const Icon(Icons.send, color: SimsColors.accentCyan),
                                onPressed: _sendTextMessage,
                              ),
                            ],
                          ),
                        ),

                      // Bottom controls
                      Container(
                        padding: const EdgeInsets.all(16),
                        color: SimsColors.background,
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
                          color: SimsColors.backgroundLight,
                          boxShadow: [
                            BoxShadow(
                              color: Colors.black.withOpacity(0.3),
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
                                    borderRadius: BorderRadius.circular(20),
                                    borderSide: BorderSide.none,
                                  ),
                                  filled: true,
                                  fillColor: SimsColors.background,
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
                              icon: const Icon(Icons.send, color: SimsColors.accentCyan),
                              onPressed: _sendTextMessage,
                            ),
                          ],
                        ),
                      ),

                    // Bottom controls
                    Container(
                      padding: const EdgeInsets.all(16),
                      color: SimsColors.background,
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
                              iconColor: _cameraService.isRecordingVideo ? Colors.white : SimsColors.accentTactical,
                            ),
                          ),
                          _buildControlButton(
                            icon: _audioService.isRecording ? Icons.stop : Icons.mic,
                            color: _audioService.isRecording ? SimsColors.criticalRed : SimsColors.white,
                            iconColor: _audioService.isRecording ? Colors.white : SimsColors.accentTactical,
                            onTap: _toggleRecording,
                          ),
                          _buildControlButton(
                            icon: Icons.photo_library,
                            color: SimsColors.white,
                            iconColor: SimsColors.accentTactical,
                            onTap: _pickFromGallery,
                          ),
                          _buildControlButton(
                            icon: _showTextInput ? Icons.keyboard_hide : Icons.keyboard,
                            color: _showTextInput ? SimsColors.accentTactical : SimsColors.white,
                            iconColor: _showTextInput ? Colors.white : SimsColors.accentTactical,
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
                color: SimsColors.accentTactical,
                borderRadius: BorderRadius.circular(8),
                boxShadow: [
                  BoxShadow(
                    color: Colors.black.withOpacity(0.2),
                    blurRadius: 3,
                    offset: const Offset(0, 2),
                  ),
                ],
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  if (message.type == MessageType.text)
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          message.text!,
                          style: const TextStyle(color: Colors.white),
                        ),
                        if (message.isUploading)
                          Padding(
                            padding: const EdgeInsets.only(top: 8),
                            child: Row(
                              children: [
                                const SizedBox(
                                  width: 12,
                                  height: 12,
                                  child: CircularProgressIndicator(
                                    strokeWidth: 2,
                                    color: Colors.white70,
                                  ),
                                ),
                                const SizedBox(width: 8),
                                const Text(
                                  'Sending...',
                                  style: TextStyle(color: Colors.white70, fontSize: 11),
                                ),
                              ],
                            ),
                          ),
                        if (message.failed)
                          Padding(
                            padding: const EdgeInsets.only(top: 8),
                            child: Row(
                              children: [
                                const Icon(Icons.error_outline, color: Colors.red, size: 14),
                                const SizedBox(width: 8),
                                const Text(
                                  'Failed to send',
                                  style: TextStyle(color: Colors.red, fontSize: 11),
                                ),
                              ],
                            ),
                          ),
                        if (!message.isUploading && !message.failed)
                          Padding(
                            padding: const EdgeInsets.only(top: 4),
                            child: Row(
                              children: [
                                const Icon(Icons.check, color: Colors.white60, size: 14),
                                const SizedBox(width: 4),
                                Text(
                                  '${message.timestamp.hour.toString().padLeft(2, '0')}:${message.timestamp.minute.toString().padLeft(2, '0')}',
                                  style: const TextStyle(color: Colors.white60, fontSize: 10),
                                ),
                              ],
                            ),
                          ),
                      ],
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
