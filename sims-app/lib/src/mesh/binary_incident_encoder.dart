import 'dart:convert';
import 'dart:typed_data';

/// Encodes an incident into the compact binary format expected by
/// POST /api/lora/incident (see sims-backend/endpoints/lora.py).
///
/// Format (little-endian):
///   [0]     uint8   version (0x01)
///   [1]     uint8   flags (bit0=has_image, bit1=has_audio, bits2-3=priority)
///   [2-5]   int32   latitude * 1e7
///   [6-9]   int32   longitude * 1e7
///   [10-11] int16   altitude (meters)
///   [12-17] uint8[6] device MAC
///   [18]    uint8   description length
///   [19..]  UTF-8   description (desc_len bytes)
///   [..]    uint16  image length (LE) + image bytes (if present)
class BinaryIncidentEncoder {
  BinaryIncidentEncoder._();

  static const int _version = 0x01;

  static const Map<String, int> _priorityValues = {
    'critical': 0,
    'high': 1,
    'medium': 2,
    'low': 3,
  };

  /// Encode an incident to binary payload.
  ///
  /// [description] - text description (max 255 bytes UTF-8)
  /// [latitude], [longitude] - GPS coordinates
  /// [altitude] - meters above sea level
  /// [deviceMac] - 6-byte device identifier
  /// [priority] - 'critical', 'high', 'medium', or 'low'
  /// [imageBytes] - optional compressed image data
  static Uint8List encode({
    required String description,
    required double latitude,
    required double longitude,
    double altitude = 0,
    required Uint8List deviceMac,
    String priority = 'medium',
    Uint8List? imageBytes,
  }) {
    assert(deviceMac.length == 6, 'Device MAC must be exactly 6 bytes');

    final descBytes = utf8.encode(description);
    final descLen = descBytes.length.clamp(0, 255);

    final hasImage = imageBytes != null && imageBytes.isNotEmpty;
    final priorityVal = _priorityValues[priority] ?? 2;
    final flags = (hasImage ? 0x01 : 0) | (priorityVal << 2);

    // Calculate total size
    var totalSize = 19 + descLen; // header + description
    if (hasImage) {
      totalSize += 2 + imageBytes.length; // uint16 len + data
    }

    final buffer = ByteData(totalSize);
    var pos = 0;

    // Version
    buffer.setUint8(pos, _version);
    pos += 1;

    // Flags
    buffer.setUint8(pos, flags);
    pos += 1;

    // Latitude as int32 * 1e7
    buffer.setInt32(pos, (latitude * 1e7).round(), Endian.little);
    pos += 4;

    // Longitude as int32 * 1e7
    buffer.setInt32(pos, (longitude * 1e7).round(), Endian.little);
    pos += 4;

    // Altitude as int16
    buffer.setInt16(pos, altitude.round().clamp(-32768, 32767), Endian.little);
    pos += 2;

    // Device MAC (6 bytes)
    final bytes = buffer.buffer.asUint8List();
    bytes.setRange(pos, pos + 6, deviceMac);
    pos += 6;

    // Description length + data
    buffer.setUint8(pos, descLen);
    pos += 1;
    bytes.setRange(pos, pos + descLen, descBytes.sublist(0, descLen));
    pos += descLen;

    // Image (length-prefixed)
    if (hasImage) {
      buffer.setUint16(pos, imageBytes.length, Endian.little);
      pos += 2;
      bytes.setRange(pos, pos + imageBytes.length, imageBytes);
      pos += imageBytes.length;
    }

    return bytes;
  }

  /// Generate a stable 6-byte device identifier from a string (e.g. phone ID).
  static Uint8List generateDeviceMac(String identifier) {
    // Simple hash-based MAC generation
    final hash = identifier.codeUnits.fold<int>(
      0x5A5A5A5A,
      (acc, c) => ((acc << 5) + acc + c) & 0xFFFFFFFF,
    );
    final mac = Uint8List(6);
    mac[0] = 0x4D; // 'M' for mobile
    mac[1] = 0x4F; // 'O' for phone origin
    mac[2] = (hash >> 24) & 0xFF;
    mac[3] = (hash >> 16) & 0xFF;
    mac[4] = (hash >> 8) & 0xFF;
    mac[5] = hash & 0xFF;
    return mac;
  }
}
