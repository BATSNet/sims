import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import '../services/permission_service.dart';
import '../utils/sims_colors.dart';

class PermissionScreen extends StatefulWidget {
  const PermissionScreen({super.key});

  @override
  State<PermissionScreen> createState() => _PermissionScreenState();
}

class _PermissionScreenState extends State<PermissionScreen> {
  final PermissionService _permissionService = PermissionService();
  bool _isRequesting = false;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Icon(
                Icons.security,
                size: 100,
                color: SimsColors.accentBlue,
              ),
              const SizedBox(height: 32),
              const Text(
                'Permissions Required',
                style: TextStyle(
                  fontSize: 28,
                  fontWeight: FontWeight.bold,
                  color: SimsColors.white,
                ),
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 16),
              Text(
                'SIMS requires the following permissions to function properly:',
                style: TextStyle(
                  fontSize: 16,
                  color: SimsColors.white.withOpacity(0.8),
                ),
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 32),
              _buildPermissionItem(
                Icons.camera_alt,
                'Camera',
                'To capture incident photos and videos',
              ),
              const SizedBox(height: 16),
              _buildPermissionItem(
                Icons.mic,
                'Microphone',
                'To record audio descriptions',
              ),
              const SizedBox(height: 16),
              _buildPermissionItem(
                Icons.location_on,
                'Location',
                'To automatically capture incident location',
              ),
              const SizedBox(height: 48),
              SizedBox(
                width: double.infinity,
                height: 56,
                child: ElevatedButton(
                  onPressed: _isRequesting ? null : _handleGrantPermissions,
                  style: ElevatedButton.styleFrom(
                    backgroundColor: SimsColors.accentBlue,
                    foregroundColor: SimsColors.white,
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(12),
                    ),
                  ),
                  child: _isRequesting
                      ? const SizedBox(
                          width: 24,
                          height: 24,
                          child: CircularProgressIndicator(
                            color: SimsColors.white,
                            strokeWidth: 2,
                          ),
                        )
                      : const Text(
                          'Grant Permissions',
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                ),
              ),
              const SizedBox(height: 16),
              Text(
                'These permissions are mandatory for using the app',
                style: TextStyle(
                  fontSize: 13,
                  color: SimsColors.white.withOpacity(0.6),
                ),
                textAlign: TextAlign.center,
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildPermissionItem(IconData icon, String title, String description) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: SimsColors.navyBlueLight,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        children: [
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: SimsColors.accentBlue.withOpacity(0.2),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Icon(
              icon,
              color: SimsColors.accentBlue,
              size: 28,
            ),
          ),
          const SizedBox(width: 16),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: const TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                    color: SimsColors.white,
                  ),
                ),
                const SizedBox(height: 4),
                Text(
                  description,
                  style: TextStyle(
                    fontSize: 13,
                    color: SimsColors.white.withOpacity(0.7),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _handleGrantPermissions() async {
    setState(() {
      _isRequesting = true;
    });

    final granted = await _permissionService.checkAndRequestAllPermissions();

    if (mounted) {
      if (granted) {
        context.go('/phone');
      } else {
        setState(() {
          _isRequesting = false;
        });
        await _showSettingsDialog();
      }
    }
  }

  Future<void> _showSettingsDialog() async {
    return showDialog(
      context: context,
      barrierDismissible: false,
      builder: (BuildContext context) {
        return AlertDialog(
          backgroundColor: SimsColors.navyBlueLight,
          title: const Text(
            'Permissions Denied',
            style: TextStyle(color: SimsColors.white),
          ),
          content: Text(
            'Some permissions were denied. Please grant all required permissions in Settings to use SIMS.',
            style: TextStyle(color: SimsColors.white.withOpacity(0.9)),
          ),
          actions: [
            TextButton(
              onPressed: () {
                Navigator.of(context).pop();
              },
              child: const Text('Cancel'),
            ),
            ElevatedButton(
              onPressed: () async {
                Navigator.of(context).pop();
                await _permissionService.checkAndRequestAllPermissions();
              },
              child: const Text('Retry'),
            ),
          ],
        );
      },
    );
  }
}
