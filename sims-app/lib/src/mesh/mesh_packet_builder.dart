import 'dart:convert';
import 'dart:typed_data';
import 'meshtastic_constants.dart';

/// A parsed mesh message with portnum, payload, and sender info.
class ParsedMeshMessage {
  final int portnum;
  final Uint8List payload;
  final int? fromNode;

  ParsedMeshMessage({
    required this.portnum,
    required this.payload,
    this.fromNode,
  });

  bool get isTextMessage => portnum == MeshtasticConstants.portNumTextMessage;
  bool get isPrivateApp => portnum == MeshtasticConstants.portNumPrivateApp;

  /// Decode payload as UTF-8 text. Returns null if not a text message.
  String? get textContent {
    if (!isTextMessage) return null;
    return utf8.decode(payload, allowMalformed: true);
  }
}

/// Minimal hand-encoded Meshtastic protobuf builder and parser.
///
/// Encodes/decodes the subset needed for data packets:
/// ToRadio { want_config_id | packet: MeshPacket { to, decoded: Data { portnum, payload } } }
/// FromRadio { packet: MeshPacket { from, to, decoded: Data { portnum, payload } } }
///
/// Protobuf wire format reference:
///   - Varint: type 0, field = (field_number << 3) | 0
///   - Length-delimited: type 2, field = (field_number << 3) | 2
///   - Fixed32: type 5, field = (field_number << 3) | 5
class MeshPacketBuilder {
  MeshPacketBuilder._();

  /// Build a ToRadio message requesting config with the given nonce.
  /// ToRadio.want_config_id = field 8, varint.
  static Uint8List buildWantConfig(int configNonce) {
    final writer = _ProtoWriter();
    writer.writeVarint(8, configNonce); // field 8 = want_config_id
    return writer.toBytes();
  }

  /// Build a ToRadio message wrapping a MeshPacket with a TEXT_MESSAGE_APP payload.
  /// Broadcasts to all nodes by default.
  static Uint8List buildTextPacket(String text, {int? packetId, int? destination}) {
    final textBytes = Uint8List.fromList(utf8.encode(text));

    // Inner: Data message
    // Data.portnum = field 1, varint (TEXT_MESSAGE_APP = 1)
    // Data.payload = field 2, bytes
    final dataWriter = _ProtoWriter();
    dataWriter.writeVarint(1, MeshtasticConstants.portNumTextMessage);
    dataWriter.writeBytes(2, textBytes);
    final dataBytes = dataWriter.toBytes();

    // Middle: MeshPacket
    final packetWriter = _ProtoWriter();
    packetWriter.writeFixed32(
      2,
      destination ?? MeshtasticConstants.meshBroadcastAddr,
    );
    if (packetId != null) {
      packetWriter.writeFixed32(6, packetId);
    }
    packetWriter.writeBytes(4, dataBytes);
    final packetBytes = packetWriter.toBytes();

    // Outer: ToRadio
    final toRadioWriter = _ProtoWriter();
    toRadioWriter.writeBytes(1, packetBytes);
    return toRadioWriter.toBytes();
  }

  /// Build a ToRadio message wrapping a MeshPacket with a PRIVATE_APP payload.
  /// Broadcasts to all nodes.
  static Uint8List buildDataPacket(Uint8List payload, {int? packetId}) {
    // Inner: Data message
    // Data.portnum = field 1, varint (PRIVATE_APP = 256)
    // Data.payload = field 2, bytes
    final dataWriter = _ProtoWriter();
    dataWriter.writeVarint(1, MeshtasticConstants.portNumPrivateApp);
    dataWriter.writeBytes(2, payload);
    final dataBytes = dataWriter.toBytes();

    // Middle: MeshPacket
    // MeshPacket.to = field 2, fixed32
    // MeshPacket.decoded = field 4, bytes (Data submessage)
    // MeshPacket.id = field 6, fixed32
    final packetWriter = _ProtoWriter();
    packetWriter.writeFixed32(2, MeshtasticConstants.meshBroadcastAddr);
    if (packetId != null) {
      packetWriter.writeFixed32(6, packetId);
    }
    packetWriter.writeBytes(4, dataBytes);
    final packetBytes = packetWriter.toBytes();

    // Outer: ToRadio
    // ToRadio.packet = field 1, bytes (MeshPacket submessage)
    final toRadioWriter = _ProtoWriter();
    toRadioWriter.writeBytes(1, packetBytes);
    return toRadioWriter.toBytes();
  }

  /// Parse a FromRadio message and extract PRIVATE_APP payload if present.
  ///
  /// FromRadio.packet = field 2, length-delimited (MeshPacket)
  /// MeshPacket.from = field 1, fixed32
  /// MeshPacket.to = field 2, fixed32
  /// MeshPacket.decoded = field 4, length-delimited (Data)
  /// Data.portnum = field 1, varint
  /// Data.payload = field 2, length-delimited
  ///
  /// Returns the raw payload bytes if portnum == PRIVATE_APP, null otherwise.
  static Uint8List? parseFromRadioPayload(Uint8List data) {
    // Extract MeshPacket from FromRadio (field 2, length-delimited)
    final meshPacketBytes = _ProtoReader.extractLengthDelimited(data, 2);
    if (meshPacketBytes == null) return null;

    // Extract Data submessage from MeshPacket (field 4, length-delimited)
    final dataBytes = _ProtoReader.extractLengthDelimited(meshPacketBytes, 4);
    if (dataBytes == null) return null;

    // Check portnum (field 1, varint) - must be PRIVATE_APP (256)
    final portnum = _ProtoReader.extractVarint(dataBytes, 1);
    if (portnum != MeshtasticConstants.portNumPrivateApp) return null;

    // Extract payload (field 2, length-delimited)
    return _ProtoReader.extractLengthDelimited(dataBytes, 2);
  }

  /// Generic FromRadio parser - extracts any mesh packet with portnum and payload.
  /// Returns a ParsedMeshMessage with portnum, payload bytes, and sender node ID.
  static ParsedMeshMessage? parseFromRadio(Uint8List data) {
    // Extract MeshPacket from FromRadio (field 2, length-delimited)
    final meshPacketBytes = _ProtoReader.extractLengthDelimited(data, 2);
    if (meshPacketBytes == null) return null;

    // Extract sender node ID (field 1, fixed32)
    final fromNode = _ProtoReader.extractFixed32(meshPacketBytes, 1);

    // Extract Data submessage from MeshPacket (field 4, length-delimited)
    final dataBytes = _ProtoReader.extractLengthDelimited(meshPacketBytes, 4);
    if (dataBytes == null) return null;

    // Get portnum (field 1, varint)
    final portnum = _ProtoReader.extractVarint(dataBytes, 1);
    if (portnum == null) return null;

    // Get payload (field 2, length-delimited)
    final payload = _ProtoReader.extractLengthDelimited(dataBytes, 2);
    if (payload == null) return null;

    return ParsedMeshMessage(
      portnum: portnum,
      payload: payload,
      fromNode: fromNode,
    );
  }
}

/// Minimal protobuf wire format writer.
class _ProtoWriter {
  final _buffer = BytesBuilder(copy: false);

  void writeVarint(int fieldNumber, int value) {
    _writeTag(fieldNumber, 0);
    _writeRawVarint(value);
  }

  void writeFixed32(int fieldNumber, int value) {
    _writeTag(fieldNumber, 5);
    final bytes = Uint8List(4);
    final bd = ByteData.sublistView(bytes);
    bd.setUint32(0, value & 0xFFFFFFFF, Endian.little);
    _buffer.add(bytes);
  }

  void writeBytes(int fieldNumber, Uint8List data) {
    _writeTag(fieldNumber, 2);
    _writeRawVarint(data.length);
    _buffer.add(data);
  }

  void _writeTag(int fieldNumber, int wireType) {
    _writeRawVarint((fieldNumber << 3) | wireType);
  }

  void _writeRawVarint(int value) {
    // Handle unsigned 64-bit values encoded as varint
    var v = value;
    while (v > 0x7F) {
      _buffer.addByte((v & 0x7F) | 0x80);
      v = v >> 7;
    }
    _buffer.addByte(v & 0x7F);
  }

  Uint8List toBytes() => _buffer.toBytes();
}

/// Minimal protobuf wire format reader for extracting specific fields.
class _ProtoReader {
  /// Extract a varint field value by field number. Returns null if not found.
  static int? extractVarint(Uint8List data, int targetField) {
    var pos = 0;
    while (pos < data.length) {
      final tagResult = _readRawVarint(data, pos);
      if (tagResult == null) break;
      pos = tagResult.nextPos;

      final fieldNumber = tagResult.value >> 3;
      final wireType = tagResult.value & 0x07;

      if (wireType == 0) {
        // Varint
        final valResult = _readRawVarint(data, pos);
        if (valResult == null) break;
        pos = valResult.nextPos;
        if (fieldNumber == targetField) return valResult.value;
      } else if (wireType == 2) {
        // Length-delimited - skip
        final lenResult = _readRawVarint(data, pos);
        if (lenResult == null) break;
        pos = lenResult.nextPos + lenResult.value;
      } else if (wireType == 5) {
        if (fieldNumber == targetField) break; // Can't return fixed32 as varint
        pos += 4;
      } else if (wireType == 1) {
        pos += 8;
      } else {
        break;
      }
    }
    return null;
  }

  /// Extract a fixed32 field value by field number. Returns null if not found.
  static int? extractFixed32(Uint8List data, int targetField) {
    var pos = 0;
    while (pos < data.length) {
      final tagResult = _readRawVarint(data, pos);
      if (tagResult == null) break;
      pos = tagResult.nextPos;

      final fieldNumber = tagResult.value >> 3;
      final wireType = tagResult.value & 0x07;

      if (wireType == 0) {
        // Varint - skip
        final valResult = _readRawVarint(data, pos);
        if (valResult == null) break;
        pos = valResult.nextPos;
      } else if (wireType == 2) {
        // Length-delimited - skip
        final lenResult = _readRawVarint(data, pos);
        if (lenResult == null) break;
        pos = lenResult.nextPos + lenResult.value;
      } else if (wireType == 5) {
        // Fixed32
        if (pos + 4 > data.length) break;
        if (fieldNumber == targetField) {
          final bd = ByteData.sublistView(data, pos, pos + 4);
          return bd.getUint32(0, Endian.little);
        }
        pos += 4;
      } else if (wireType == 1) {
        pos += 8;
      } else {
        break;
      }
    }
    return null;
  }

  /// Extract a length-delimited field (bytes/submessage) by field number.
  static Uint8List? extractLengthDelimited(Uint8List data, int targetField) {
    var pos = 0;
    while (pos < data.length) {
      final tagResult = _readRawVarint(data, pos);
      if (tagResult == null) break;
      pos = tagResult.nextPos;

      final fieldNumber = tagResult.value >> 3;
      final wireType = tagResult.value & 0x07;

      if (wireType == 0) {
        // Varint - skip
        final valResult = _readRawVarint(data, pos);
        if (valResult == null) break;
        pos = valResult.nextPos;
      } else if (wireType == 2) {
        // Length-delimited
        final lenResult = _readRawVarint(data, pos);
        if (lenResult == null) break;
        pos = lenResult.nextPos;
        final len = lenResult.value;
        if (pos + len > data.length) break;
        if (fieldNumber == targetField) {
          return Uint8List.sublistView(data, pos, pos + len);
        }
        pos += len;
      } else if (wireType == 5) {
        pos += 4;
      } else if (wireType == 1) {
        pos += 8;
      } else {
        break;
      }
    }
    return null;
  }

  static _VarintResult? _readRawVarint(Uint8List data, int pos) {
    var value = 0;
    var shift = 0;
    while (pos < data.length) {
      final b = data[pos++];
      value |= (b & 0x7F) << shift;
      if ((b & 0x80) == 0) {
        return _VarintResult(value, pos);
      }
      shift += 7;
      if (shift >= 64) break;
    }
    return null;
  }
}

class _VarintResult {
  final int value;
  final int nextPos;
  _VarintResult(this.value, this.nextPos);
}
