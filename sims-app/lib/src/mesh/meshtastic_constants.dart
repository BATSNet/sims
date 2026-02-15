/// Meshtastic BLE protocol constants.
/// UUIDs verified against working meshtastic_ble.h firmware.
class MeshtasticConstants {
  MeshtasticConstants._();

  // BLE Service UUID
  static const String serviceUuid = '6ba1b218-15a8-461f-9fa8-5dcae273eafd';

  // Characteristic UUIDs
  static const String toRadioUuid = 'f75c76d2-129e-4dad-a1dd-7866124401e7';
  static const String fromRadioUuid = '2c55e69e-4993-11ed-b878-0242ac120002';
  static const String fromNumUuid = 'ed9da18c-a800-4f66-a670-aa7547e34453';

  // Protocol constants
  static const int meshBroadcastAddr = 0xFFFFFFFF;
  static const int portNumTextMessage = 1; // TEXT_MESSAGE_APP
  static const int portNumPrivateApp = 256;
  static const int desiredMtu = 512;

  // Timeouts
  static const Duration scanTimeout = Duration(seconds: 15);
  static const Duration connectTimeout = Duration(seconds: 10);
  static const Duration configPollInterval = Duration(milliseconds: 100);
  static const Duration configTimeout = Duration(seconds: 10);

  // Reconnect backoff
  static const Duration reconnectInitial = Duration(seconds: 5);
  static const Duration reconnectMax = Duration(seconds: 60);
}
