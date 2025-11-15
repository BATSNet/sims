import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import '../repositories/user_repository.dart';
import '../services/permission_service.dart';
import '../screens/phone_number_screen.dart';
import '../screens/incident_overview_screen.dart';
import '../screens/incident_chat_screen.dart';
import '../screens/camera_capture_screen.dart';
import '../screens/permission_screen.dart';

class AppRoutes {
  static GoRouter createRouter() {
    return GoRouter(
      initialLocation: '/',
      // NOTE: Permissions are checked when user tries to use camera/mic (not at startup)
      redirect: (BuildContext context, GoRouterState state) async {
        try {
          // Check if user has set their phone number
          final userRepo = await UserRepository.getInstance();
          final hasPhoneNumber = await userRepo.hasPhoneNumber();

          if (!hasPhoneNumber && state.uri.path != '/phone') {
            return '/phone';
          }

          if (hasPhoneNumber && state.uri.path == '/phone') {
            return '/';
          }

          return null;
        } catch (e) {
          debugPrint('!!! ERROR in redirect logic: $e');
          return null;
        }
      },
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
