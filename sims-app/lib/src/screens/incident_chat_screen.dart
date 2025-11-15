import 'dart:io';
import 'package:flutter/material.dart';
import 'package:camera/camera.dart';
import 'package:image_picker/image_picker.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';
import '../utils/sims_colors.dart';
import '../config/app_config.dart';
import '../models/chat_message.dart';
import '../services/media_upload_service.dart';

class IncidentChatScreen extends StatefulWidget {
  final String incidentId;

  const IncidentChatScreen({
    super.key,
    required this.incidentId,
  });

  @override
  State<IncidentChatScreen> createState() => _IncidentChatScreenState();
}

class _IncidentChatScreenState extends State<IncidentChatScreen> {
  CameraController? _cameraController;
  List<CameraDescription>? _cameras;
  final TextEditingController _messageController = TextEditingController();
  final ScrollController _scrollController = ScrollController();
  final ImagePicker _imagePicker = ImagePicker();
  final MediaUploadService _uploadService = MediaUploadService();

  bool _isLoading = false;
  bool _isCameraInitialized = false;
  bool _showCamera = true;
  bool _isRecordingVideo = false;

  final List<ChatMessage> _messages = [];

  @override
  void initState() {
    super.initState();
    _initializeCamera();
    _loadChatHistory();
  }

  Future<void> _initializeCamera() async {
    try {
      _cameras = await availableCameras();
      if (_cameras != null && _cameras!.isNotEmpty) {
        _cameraController = CameraController(
          _cameras![0],
          ResolutionPreset.medium,
          enableAudio: true,
        );
        await _cameraController!.initialize();
        if (mounted) {
          setState(() {
            _isCameraInitialized = true;
          });
        }
      }
    } catch (e) {
      print('Error initializing camera: $e');
    }
  }

  Future<void> _loadChatHistory() async {
    try {
      final response = await http.get(
        Uri.parse('${AppConfig.baseUrl}/api/incidents/${widget.incidentId}/chat'),
      );

      if (response.statusCode == 200) {
        final data = json.decode(response.body);
        final messages = (data['messages'] as List)
            .map((m) => ChatMessage.fromJson(m))
            .toList();

        setState(() {
          _messages.clear();
          _messages.addAll(messages);
        });

        _scrollToBottom();
      }
    } catch (e) {
      print('Error loading chat history: $e');
    }
  }

  Future<void> _handleSubmit() async {
    if (_messageController.text.trim().isEmpty || _isLoading) return;

    final message = _messageController.text;
    _messageController.clear();

    final userMessage = ChatMessage(
      id: DateTime.now().millisecondsSinceEpoch.toString(),
      content: message,
      sender: MessageSender.user,
      timestamp: DateTime.now(),
    );

    setState(() {
      _messages.add(userMessage);
      _isLoading = true;
    });

    _scrollToBottom();

    final thinkingMessage = ChatMessage(
      id: 'thinking-${DateTime.now().millisecondsSinceEpoch}',
      content: 'Thinking...',
      sender: MessageSender.assistant,
      timestamp: DateTime.now(),
      isThinking: true,
    );

    setState(() {
      _messages.add(thinkingMessage);
    });

    _scrollToBottom();

    try {
      final response = await http.post(
        Uri.parse('${AppConfig.baseUrl}/api/incidents/${widget.incidentId}/chat'),
        headers: {'Content-Type': 'application/json'},
        body: json.encode({'message': message}),
      );

      setState(() {
        _messages.removeWhere((m) => m.isThinking);
      });

      if (response.statusCode == 200) {
        final data = json.decode(response.body);
        final aiMessage = ChatMessage(
          id: data['id'] ?? DateTime.now().millisecondsSinceEpoch.toString(),
          content: data['response'] ?? 'No response',
          sender: MessageSender.assistant,
          timestamp: DateTime.now(),
        );

        setState(() {
          _messages.add(aiMessage);
        });

        _scrollToBottom();
      }
    } catch (e) {
      print('Error sending message: $e');
      setState(() {
        _messages.removeWhere((m) => m.isThinking);
      });

      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Failed to send message'),
          duration: Duration(seconds: 2),
        ),
      );
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _handleRecordAudio() async {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text('Audio recording will be implemented'),
        duration: Duration(seconds: 2),
      ),
    );
  }

  Future<void> _takePicture() async {
    if (_cameraController == null || !_cameraController!.value.isInitialized) {
      return;
    }

    try {
      final image = await _cameraController!.takePicture();
      await _uploadAndSendImage(File(image.path));
    } catch (e) {
      print('Error taking picture: $e');
    }
  }

  Future<void> _toggleVideoRecording() async {
    if (_cameraController == null || !_cameraController!.value.isInitialized) {
      return;
    }

    if (_isRecordingVideo) {
      try {
        final video = await _cameraController!.stopVideoRecording();
        setState(() {
          _isRecordingVideo = false;
        });

        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Video saved: ${video.path}'),
            duration: const Duration(seconds: 2),
          ),
        );
      } catch (e) {
        print('Error stopping video: $e');
      }
    } else {
      try {
        await _cameraController!.startVideoRecording();
        setState(() {
          _isRecordingVideo = true;
        });
      } catch (e) {
        print('Error starting video: $e');
      }
    }
  }

  Future<void> _pickImageFromGallery() async {
    try {
      final XFile? image = await _imagePicker.pickImage(
        source: ImageSource.gallery,
      );

      if (image != null) {
        await _uploadAndSendImage(File(image.path));
      }
    } catch (e) {
      print('Error picking image: $e');
    }
  }

  Future<void> _uploadAndSendImage(File imageFile) async {
    setState(() {
      _isLoading = true;
    });

    try {
      final result = await _uploadService.uploadImage(imageFile);

      if (result.success && result.url != null) {
        final imageMessage = ChatMessage(
          id: DateTime.now().millisecondsSinceEpoch.toString(),
          content: 'Sent an image',
          sender: MessageSender.user,
          timestamp: DateTime.now(),
          imageUrl: result.url,
        );

        setState(() {
          _messages.add(imageMessage);
        });

        _scrollToBottom();

        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Image uploaded successfully'),
            duration: Duration(seconds: 2),
          ),
        );
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(result.error ?? 'Failed to upload image'),
            duration: const Duration(seconds: 2),
          ),
        );
      }
    } catch (e) {
      print('Error uploading image: $e');
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Failed to upload image'),
          duration: Duration(seconds: 2),
        ),
      );
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  void _scrollToBottom() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scrollController.hasClients) {
        _scrollController.animateTo(
          _scrollController.position.maxScrollExtent,
          duration: const Duration(milliseconds: 300),
          curve: Curves.easeOut,
        );
      }
    });
  }

  @override
  void dispose() {
    _cameraController?.dispose();
    _messageController.dispose();
    _scrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final screenHeight = MediaQuery.of(context).size.height;
    final cameraHeight = screenHeight * 0.4;

    return Scaffold(
      appBar: AppBar(
        title: Text('Incident ${widget.incidentId}'),
        backgroundColor: SimsColors.navyBlue,
        leading: IconButton(
          icon: const Icon(Icons.arrow_back),
          onPressed: () => Navigator.of(context).pop(),
        ),
        actions: [
          IconButton(
            icon: Icon(_showCamera ? Icons.videocam_off : Icons.videocam),
            onPressed: () {
              setState(() {
                _showCamera = !_showCamera;
              });
            },
          ),
        ],
      ),
      body: Column(
        children: [
          if (_showCamera)
            Container(
              height: cameraHeight,
              color: Colors.black,
              child: _isCameraInitialized && _cameraController != null
                  ? Stack(
                      children: [
                        CameraPreview(_cameraController!),
                        Positioned(
                          bottom: 16,
                          right: 16,
                          child: Column(
                            children: [
                              GestureDetector(
                                onTap: _takePicture,
                                onLongPress: _toggleVideoRecording,
                                child: FloatingActionButton(
                                  onPressed: null,
                                  backgroundColor: _isRecordingVideo
                                      ? SimsColors.criticalRed
                                      : SimsColors.accentBlue,
                                  heroTag: 'camera',
                                  child: Icon(
                                    _isRecordingVideo
                                        ? Icons.stop
                                        : Icons.camera_alt,
                                  ),
                                ),
                              ),
                              const SizedBox(height: 8),
                              FloatingActionButton(
                                onPressed: _pickImageFromGallery,
                                backgroundColor: SimsColors.navyBlueLight,
                                heroTag: 'gallery',
                                child: const Icon(Icons.photo_library),
                              ),
                            ],
                          ),
                        ),
                        if (_isRecordingVideo)
                          const Positioned(
                            top: 16,
                            left: 16,
                            child: Row(
                              children: [
                                Icon(
                                  Icons.fiber_manual_record,
                                  color: Colors.red,
                                  size: 16,
                                ),
                                SizedBox(width: 8),
                                Text(
                                  'Recording...',
                                  style: TextStyle(
                                    color: Colors.white,
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ],
                            ),
                          ),
                      ],
                    )
                  : const Center(
                      child: CircularProgressIndicator(
                        color: SimsColors.white,
                      ),
                    ),
            ),

          Expanded(
            child: ListView.builder(
              controller: _scrollController,
              padding: const EdgeInsets.all(16),
              itemCount: _messages.length,
              itemBuilder: (context, index) {
                final message = _messages[index];
                return _buildMessageBubble(message);
              },
            ),
          ),

          Container(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
            decoration: BoxDecoration(
              color: SimsColors.navyBlueLight,
              boxShadow: [
                BoxShadow(
                  color: Colors.black.withOpacity(0.2),
                  blurRadius: 4,
                  offset: const Offset(0, -2),
                ),
              ],
            ),
            child: SafeArea(
              child: Row(
                children: [
                  IconButton(
                    onPressed: _handleRecordAudio,
                    icon: const Icon(
                      Icons.mic,
                      color: SimsColors.white,
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: TextField(
                      controller: _messageController,
                      enabled: !_isLoading,
                      style: const TextStyle(color: SimsColors.white),
                      decoration: InputDecoration(
                        hintText: 'Describe the incident...',
                        hintStyle: TextStyle(
                          color: SimsColors.white.withOpacity(0.5),
                        ),
                        filled: true,
                        fillColor: SimsColors.navyBlueDark,
                        border: OutlineInputBorder(
                          borderRadius: BorderRadius.circular(24),
                          borderSide: BorderSide.none,
                        ),
                        contentPadding: const EdgeInsets.symmetric(
                          horizontal: 16,
                          vertical: 10,
                        ),
                      ),
                      maxLines: null,
                      textInputAction: TextInputAction.send,
                      onSubmitted: (_) => _handleSubmit(),
                    ),
                  ),
                  const SizedBox(width: 8),
                  IconButton(
                    onPressed: _isLoading ? null : _handleSubmit,
                    icon: _isLoading
                        ? const SizedBox(
                            width: 24,
                            height: 24,
                            child: CircularProgressIndicator(
                              color: SimsColors.accentBlue,
                              strokeWidth: 2,
                            ),
                          )
                        : const Icon(
                            Icons.send,
                            color: SimsColors.accentBlue,
                            size: 24,
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

  Widget _buildMessageBubble(ChatMessage message) {
    final isUser = message.sender == MessageSender.user;

    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Row(
        mainAxisAlignment:
            isUser ? MainAxisAlignment.end : MainAxisAlignment.start,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (!isUser)
            Container(
              width: 32,
              height: 32,
              margin: const EdgeInsets.only(right: 8),
              decoration: BoxDecoration(
                color: SimsColors.accentBlue,
                borderRadius: BorderRadius.circular(16),
              ),
              child: const Icon(
                Icons.assistant,
                size: 20,
                color: SimsColors.white,
              ),
            ),
          Flexible(
            child: Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: isUser
                    ? SimsColors.accentBlue
                    : SimsColors.navyBlueLight,
                borderRadius: BorderRadius.circular(12),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  if (message.imageUrl != null)
                    ClipRRect(
                      borderRadius: BorderRadius.circular(8),
                      child: Image.network(
                        message.imageUrl!,
                        width: 200,
                        fit: BoxFit.cover,
                        errorBuilder: (context, error, stackTrace) {
                          return Container(
                            width: 200,
                            height: 150,
                            color: SimsColors.navyBlueDark,
                            child: const Icon(
                              Icons.image_not_supported,
                              size: 48,
                              color: SimsColors.gray600,
                            ),
                          );
                        },
                      ),
                    ),
                  if (message.imageUrl != null && message.content.isNotEmpty)
                    const SizedBox(height: 8),
                  if (message.isThinking)
                    Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(
                            color: SimsColors.white,
                            strokeWidth: 2,
                          ),
                        ),
                        const SizedBox(width: 8),
                        Text(
                          message.content,
                          style: TextStyle(
                            color: SimsColors.white.withOpacity(0.8),
                            fontSize: 14,
                            fontStyle: FontStyle.italic,
                          ),
                        ),
                      ],
                    )
                  else
                    Text(
                      message.content,
                      style: const TextStyle(
                        color: SimsColors.white,
                        fontSize: 15,
                      ),
                    ),
                  const SizedBox(height: 4),
                  Text(
                    _formatTime(message.timestamp),
                    style: TextStyle(
                      color: SimsColors.white.withOpacity(0.6),
                      fontSize: 11,
                    ),
                  ),
                ],
              ),
            ),
          ),
          if (isUser)
            Container(
              width: 32,
              height: 32,
              margin: const EdgeInsets.only(left: 8),
              decoration: BoxDecoration(
                color: SimsColors.white.withOpacity(0.2),
                borderRadius: BorderRadius.circular(16),
              ),
              child: const Icon(
                Icons.person,
                size: 20,
                color: SimsColors.white,
              ),
            ),
        ],
      ),
    );
  }

  String _formatTime(DateTime time) {
    final hour = time.hour.toString().padLeft(2, '0');
    final minute = time.minute.toString().padLeft(2, '0');
    return '$hour:$minute';
  }
}
