import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import '../repositories/user_repository.dart';
import '../services/permission_service.dart';
import '../screens/phone_number_screen.dart';
import '../screens/incident_overview_screen.dart';
import '../screens/incident_chat_screen.dart';
import '../screens/camera_capture_screen.dart';
import '../screens/permission_screen.dart';
import '../screens/settings_screen.dart';

class AppRoutesNotifier extends ChangeNotifier {
  UserRepository? _userRepository;
  PermissionService? _permissionService;
  bool _isInitializing = true;
  bool _permissionsGranted = false;

  AppRoutesNotifier() {
    _initialize();
  }

  Future<void> _initialize() async {
    debugPrint('=== INITIALIZING APP ROUTES ===');
    _userRepository = await UserRepository.getInstance();
    _permissionService = PermissionService();
    _permissionsGranted = await _permissionService!.hasAllPermissions();
    _isInitializing = false;
    debugPrint('=== INITIALIZATION COMPLETE ===');
    debugPrint('Permissions granted: $_permissionsGranted');
    debugPrint('Phone number: ${_userRepository!.getPhoneNumberSync()}');
    notifyListeners(); // Trigger redirect re-check
  }

  void updatePermissionsGranted(bool granted) {
    _permissionsGranted = granted;
    notifyListeners();
  }

  String? redirect(BuildContext context, GoRouterState state) {
    // Don't redirect while still initializing
    if (_isInitializing || _userRepository == null) {
      debugPrint('Still initializing, no redirect');
      return null;
    }

    // Check if user has set their phone number (synchronous)
    final hasPhoneNumber = _userRepository!.getPhoneNumberSync() != null;
    debugPrint('=== REDIRECT CHECK ===');
    debugPrint('Current path: ${state.uri.path}');
    debugPrint('Has phone number: $hasPhoneNumber');
    debugPrint('Permissions granted: $_permissionsGranted');

    // Priority 1: Phone number check (must have phone number for everything)
    if (!hasPhoneNumber && state.uri.path != '/phone') {
      debugPrint('Redirecting to /phone - no phone number set');
      return '/phone';
    }

    // Don't do further checks if we're on the phone screen
    if (state.uri.path == '/phone') {
      return null;
    }

    // Priority 2: Permissions check (after phone number is set)
    if (hasPhoneNumber && !_permissionsGranted && state.uri.path != '/permissions') {
      debugPrint('Redirecting to /permissions - permissions not granted');
      return '/permissions';
    }

    // Redirect away from permissions screen if already granted
    if (_permissionsGranted && state.uri.path == '/permissions') {
      debugPrint('Redirecting to / - permissions already granted');
      return '/';
    }

    // Redirect away from phone screen if already has phone number
    if (hasPhoneNumber && state.uri.path == '/phone') {
      debugPrint('Redirecting to / - phone number already set');
      return '/';
    }

    debugPrint('No redirect needed');
    return null;
  }
}

class AppRoutes {
  static final AppRoutesNotifier _notifier = AppRoutesNotifier();

  static void updatePermissionsGranted(bool granted) {
    _notifier.updatePermissionsGranted(granted);
  }

  static GoRouter createRouter() {
    return GoRouter(
      initialLocation: '/',
      refreshListenable: _notifier,
      redirect: _notifier.redirect,
      routes: [
        GoRoute(
          path: '/',
          name: 'home',
          builder: (context, state) => const IncidentOverviewScreen(),
        ),
        GoRoute(
          path: '/permissions',
          name: 'permissions',
          builder: (context, state) => const PermissionScreen(),
        ),
        GoRoute(
          path: '/phone',
          name: 'phone',
          builder: (context, state) => const PhoneNumberScreen(),
        ),
        GoRoute(
          path: '/camera',
          name: 'camera',
          builder: (context, state) => const CameraCaptureScreen(),
        ),
        GoRoute(
          path: '/chat/:incidentId',
          name: 'chat',
          builder: (context, state) {
            final incidentId = state.pathParameters['incidentId']!;
            return IncidentChatScreen(incidentId: incidentId);
          },
        ),
        GoRoute(
          path: '/settings',
          name: 'settings',
          builder: (context, state) => const SettingsScreen(),
        ),
      ],
      errorBuilder: (context, state) => Scaffold(
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Icon(Icons.error_outline, size: 64, color: Colors.red),
              const SizedBox(height: 16),
              const Text(
                'Page not found',
                style: TextStyle(fontSize: 24),
              ),
              const SizedBox(height: 16),
              ElevatedButton(
                onPressed: () => context.go('/'),
                child: const Text('Go Home'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
