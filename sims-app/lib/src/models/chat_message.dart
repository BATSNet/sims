enum MessageSender {
  user,
  assistant,
  system,
}

class ChatMessage {
  final String id;
  final String content;
  final MessageSender sender;
  final DateTime timestamp;
  final String? imageUrl;
  final String? audioUrl;
  final bool isThinking;
  final bool isSending;

  ChatMessage({
    required this.id,
    required this.content,
    required this.sender,
    required this.timestamp,
    this.imageUrl,
    this.audioUrl,
    this.isThinking = false,
    this.isSending = false,
  });

  factory ChatMessage.fromJson(Map<String, dynamic> json) {
    return ChatMessage(
      id: json['id'] as String,
      content: json['content'] as String,
      sender: MessageSender.values.firstWhere(
        (e) => e.toString() == 'MessageSender.${json['sender']}',
        orElse: () => MessageSender.assistant,
      ),
      timestamp: DateTime.parse(json['timestamp'] as String),
      imageUrl: json['imageUrl'] as String?,
      audioUrl: json['audioUrl'] as String?,
      isThinking: json['isThinking'] as bool? ?? false,
      isSending: json['isSending'] as bool? ?? false,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'content': content,
      'sender': sender.toString().split('.').last,
      'timestamp': timestamp.toIso8601String(),
      'imageUrl': imageUrl,
      'audioUrl': audioUrl,
      'isThinking': isThinking,
      'isSending': isSending,
    };
  }

  ChatMessage copyWith({
    String? id,
    String? content,
    MessageSender? sender,
    DateTime? timestamp,
    String? imageUrl,
    String? audioUrl,
    bool? isThinking,
    bool? isSending,
  }) {
    return ChatMessage(
      id: id ?? this.id,
      content: content ?? this.content,
      sender: sender ?? this.sender,
      timestamp: timestamp ?? this.timestamp,
      imageUrl: imageUrl ?? this.imageUrl,
      audioUrl: audioUrl ?? this.audioUrl,
      isThinking: isThinking ?? this.isThinking,
      isSending: isSending ?? this.isSending,
    );
  }
}
