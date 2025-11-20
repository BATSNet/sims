import 'package:flutter/foundation.dart';
// import 'package:workmanager/workmanager.dart';
import '../connection/websocket_service.dart';

// Background service temporarily disabled due to workmanager compatibility issues
// Will be re-enabled once workmanager is updated for current Flutter SDK

const String webSocketTaskId = 'sims_websocket_task';
const String notificationTaskId = 'sims_notification_task';

// @pragma('vm:entry-point')
// void callbackDispatcher() {
//   Workmanager().executeTask((task, inputData) async {
//     debugPrint('Background task started: $task');

//     try {
//       switch (task) {
//         case webSocketTaskId:
//           await _handleWebSocketTask(inputData);
//           break;

//         case notificationTaskId:
//           await _handleNotificationTask(inputData);
//           break;

//         default:
//           debugPrint('Unknown background task: $task');
//       }

//       return true;
//     } catch (e) {
//       debugPrint('Background task error: $e');
//       return false;
//     }
//   });
// }

Future<void> _handleWebSocketTask(Map<String, dynamic>? inputData) async {
  debugPrint('Maintaining WebSocket connection in background');

  final webSocketService = WebSocketService();

  try {
    if (!webSocketService.isConnected) {
      await webSocketService.connect();
      await Future.delayed(const Duration(seconds: 2));
    }

    webSocketService.sendMessage({'type': 'ping'});
    debugPrint('WebSocket ping sent from background');
  } catch (e) {
    debugPrint('WebSocket background task error: $e');
  }
}

Future<void> _handleNotificationTask(Map<String, dynamic>? inputData) async {
  debugPrint('Checking for high-priority incidents');

  final webSocketService = WebSocketService();

  try {
    final incidents = webSocketService.incidents;
    final criticalIncidents = incidents.where(
      (incident) => incident.priority == 'critical' || incident.priority == 'high',
    );

    if (criticalIncidents.isNotEmpty) {
      debugPrint('Found ${criticalIncidents.length} critical incidents');
      // TODO: Show notification using flutter_local_notifications
    }
  } catch (e) {
    debugPrint('Notification background task error: $e');
  }
}

class BackgroundService {
  static final BackgroundService _instance = BackgroundService._internal();

  factory BackgroundService() {
    return _instance;
  }

  BackgroundService._internal();

  bool _isInitialized = false;

  Future<void> initialize() async {
    if (_isInitialized) {
      debugPrint('BackgroundService already initialized');
      return;
    }

    debugPrint('BackgroundService temporarily disabled');
    // try {
    //   await Workmanager().initialize(
    //     callbackDispatcher,
    //     isInDebugMode: kDebugMode,
    //   );

    //   _isInitialized = true;
    //   debugPrint('BackgroundService initialized successfully');
    // } catch (e) {
    //   debugPrint('Error initializing BackgroundService: $e');
    // }
  }

  Future<void> registerTasks() async {
    if (!_isInitialized) {
      await initialize();
    }

    debugPrint('Background tasks registration temporarily disabled');
    // try {
    //   // Register WebSocket maintenance task (every 15 minutes)
    //   await Workmanager().registerPeriodicTask(
    //     webSocketTaskId,
    //     webSocketTaskId,
    //     frequency: const Duration(minutes: 15),
    //     constraints: Constraints(
    //       networkType: NetworkType.connected,
    //     ),
    //     existingWorkPolicy: ExistingWorkPolicy.replace,
    //   );

    //   // Register notification check task (every 15 minutes)
    //   await Workmanager().registerPeriodicTask(
    //     notificationTaskId,
    //     notificationTaskId,
    //     frequency: const Duration(minutes: 15),
    //     constraints: Constraints(
    //       networkType: NetworkType.connected,
    //     ),
    //     existingWorkPolicy: ExistingWorkPolicy.replace,
    //   );

    //   debugPrint('Background tasks registered successfully');
    // } catch (e) {
    //   debugPrint('Error registering background tasks: $e');
    // }
  }

  Future<void> cancelAllTasks() async {
    debugPrint('Background tasks cancellation skipped (not enabled)');
    // try {
    //   await Workmanager().cancelAll();
    //   debugPrint('All background tasks cancelled');
    // } catch (e) {
    //   debugPrint('Error cancelling background tasks: $e');
    // }
  }

  Future<void> cancelTask(String taskId) async {
    debugPrint('Background task cancellation skipped (not enabled)');
    // try {
    //   await Workmanager().cancelByUniqueName(taskId);
    //   debugPrint('Background task cancelled: $taskId');
    // } catch (e) {
    //   debugPrint('Error cancelling background task: $e');
    // }
  }
}
