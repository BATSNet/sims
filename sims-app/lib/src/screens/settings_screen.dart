import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:go_router/go_router.dart';
import '../repositories/settings_repository.dart';
import '../config/app_config.dart';
import '../utils/sims_colors.dart';
import '../connection/bloc/websocket_bloc.dart';
import '../connection/bloc/websocket_event.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late SettingsRepository _settingsRepo;
  final TextEditingController _urlController = TextEditingController();

  bool _isLoading = true;
  bool _useCustomUrl = false;
  String? _validationError;
  bool _isSaving = false;

  @override
  void initState() {
    super.initState();
    _loadSettings();
  }

  Future<void> _loadSettings() async {
    _settingsRepo = await SettingsRepository.getInstance();
    setState(() {
      _useCustomUrl = _settingsRepo.isCustomUrlEnabled();
      _urlController.text = _settingsRepo.getCustomBackendUrl() ?? '';
      _isLoading = false;
    });
  }

  Future<void> _saveUrl() async {
    setState(() {
      _validationError = null;
      _isSaving = true;
    });

    final url = _urlController.text.trim();

    if (!SettingsRepository.isValidUrl(url)) {
      setState(() {
        _validationError = 'Invalid URL format. Must start with http:// or https://';
        _isSaving = false;
      });
      return;
    }

    try {
      await _settingsRepo.saveCustomBackendUrl(url);
      if (mounted) {
        // Trigger WebSocket reconnection
        context.read<WebSocketBloc>().add(const ReconnectWebSocket());

        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Backend URL saved. Reconnecting...'),
            backgroundColor: SimsColors.lowGreen,
            duration: Duration(seconds: 2),
          ),
        );
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _validationError = 'Error saving URL: $e';
        });
      }
    } finally {
      if (mounted) {
        setState(() {
          _isSaving = false;
        });
      }
    }
  }

  Future<void> _resetToDefault() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: SimsColors.navyBlueLight,
        title: const Text('Reset to Default', style: TextStyle(color: SimsColors.white)),
        content: const Text(
          'This will reset the backend URL to the default value. Continue?',
          style: TextStyle(color: SimsColors.white),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () => Navigator.of(context).pop(true),
            style: ElevatedButton.styleFrom(
              backgroundColor: SimsColors.accentBlue,
            ),
            child: const Text('Reset'),
          ),
        ],
      ),
    );

    if (confirmed == true) {
      await _settingsRepo.resetToDefault();
      // Trigger WebSocket reconnection
      if (mounted) {
        context.read<WebSocketBloc>().add(const ReconnectWebSocket());
      }
      setState(() {
        _useCustomUrl = false;
        _urlController.clear();
        _validationError = null;
      });
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Reset to default URL'),
            backgroundColor: SimsColors.mediumBlue,
          ),
        );
      }
    }
  }

  Future<void> _toggleCustomUrl(bool value) async {
    if (value) {
      await _settingsRepo.enableCustomUrl();
    } else {
      await _settingsRepo.disableCustomUrl();
    }
    setState(() {
      _useCustomUrl = value;
    });
  }

  void _copyToClipboard(String text) {
    Clipboard.setData(ClipboardData(text: text));
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text('URL copied to clipboard'),
        duration: Duration(seconds: 1),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    if (_isLoading) {
      return const Scaffold(
        backgroundColor: SimsColors.navyBlue,
        body: Center(
          child: CircularProgressIndicator(
            color: SimsColors.accentBlue,
          ),
        ),
      );
    }

    final currentUrl = _settingsRepo.getBackendUrl(
      isDevelopment: AppConfig.isDevelopment,
    );

    return Scaffold(
      backgroundColor: SimsColors.navyBlue,
      appBar: AppBar(
        title: const Text('Settings'),
        backgroundColor: SimsColors.blue,
        leading: IconButton(
          icon: const Icon(Icons.arrow_back, color: SimsColors.white),
          onPressed: () => context.pop(),
        ),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Current URL Section
            _buildSectionTitle('Current Backend URL'),
            const SizedBox(height: 8),
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: SimsColors.navyBlueLight,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(
                  color: SimsColors.accentBlue.withOpacity(0.5),
                ),
              ),
              child: Row(
                children: [
                  const Icon(
                    Icons.link,
                    color: SimsColors.accentBlue,
                    size: 20,
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      currentUrl,
                      style: const TextStyle(
                        color: SimsColors.accentBlue,
                        fontWeight: FontWeight.bold,
                        fontSize: 14,
                      ),
                    ),
                  ),
                ],
              ),
            ),

            const SizedBox(height: 24),

            // Custom URL Toggle
            _buildSectionTitle('Backend Configuration'),
            const SizedBox(height: 12),
            Container(
              decoration: BoxDecoration(
                color: SimsColors.navyBlueLight,
                borderRadius: BorderRadius.circular(8),
              ),
              child: SwitchListTile(
                title: const Text(
                  'Use Custom URL',
                  style: TextStyle(color: SimsColors.white),
                ),
                subtitle: Text(
                  'Override default backend URL',
                  style: TextStyle(
                    color: SimsColors.white.withOpacity(0.7),
                    fontSize: 12,
                  ),
                ),
                value: _useCustomUrl,
                onChanged: _toggleCustomUrl,
                activeColor: SimsColors.accentBlue,
              ),
            ),

            if (_useCustomUrl) ...[
              const SizedBox(height: 16),
              TextField(
                controller: _urlController,
                style: const TextStyle(color: SimsColors.white),
                decoration: InputDecoration(
                  labelText: 'Backend URL',
                  labelStyle: TextStyle(color: SimsColors.white.withOpacity(0.7)),
                  hintText: 'http://192.168.1.100:8000',
                  hintStyle: TextStyle(color: SimsColors.white.withOpacity(0.3)),
                  errorText: _validationError,
                  prefixIcon: const Icon(Icons.link, color: SimsColors.accentBlue),
                  border: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                    borderSide: BorderSide(color: SimsColors.white.withOpacity(0.3)),
                  ),
                  enabledBorder: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                    borderSide: BorderSide(color: SimsColors.white.withOpacity(0.3)),
                  ),
                  focusedBorder: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                    borderSide: const BorderSide(color: SimsColors.accentBlue),
                  ),
                  filled: true,
                  fillColor: SimsColors.navyBlueLight,
                ),
                keyboardType: TextInputType.url,
                enabled: !_isSaving,
              ),

              const SizedBox(height: 16),

              Row(
                children: [
                  Expanded(
                    child: ElevatedButton(
                      onPressed: _isSaving ? null : _saveUrl,
                      style: ElevatedButton.styleFrom(
                        padding: const EdgeInsets.symmetric(vertical: 16),
                        backgroundColor: SimsColors.accentBlue,
                      ),
                      child: _isSaving
                          ? const SizedBox(
                              width: 20,
                              height: 20,
                              child: CircularProgressIndicator(
                                strokeWidth: 2,
                                color: SimsColors.white,
                              ),
                            )
                          : const Text('Save URL'),
                    ),
                  ),
                  const SizedBox(width: 12),
                  TextButton(
                    onPressed: _isSaving ? null : _resetToDefault,
                    child: const Text(
                      'Reset',
                      style: TextStyle(color: SimsColors.highOrange),
                    ),
                  ),
                ],
              ),
            ],

            const SizedBox(height: 32),

            // Examples Section
            _buildSectionTitle('URL Examples'),
            const SizedBox(height: 12),
            _buildUrlExample(
              'Android Emulator',
              'http://10.0.2.2:8000',
              Icons.phone_android,
            ),
            const SizedBox(height: 8),
            _buildUrlExample(
              'Physical Device (Local Network)',
              'http://192.168.1.100:8000',
              Icons.smartphone,
            ),
            const SizedBox(height: 8),
            _buildUrlExample(
              'Production Server',
              'http://91.99.179.35:8000',
              Icons.cloud,
            ),

            const SizedBox(height: 24),

            // Info Section
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: SimsColors.navyBlueLight,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(
                  color: SimsColors.teal.withOpacity(0.3),
                ),
              ),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Icon(
                    Icons.info_outline,
                    color: SimsColors.teal,
                    size: 20,
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      'Changing the backend URL will require reconnecting to the server. The app will attempt to reconnect automatically.',
                      style: TextStyle(
                        fontSize: 13,
                        color: SimsColors.white.withOpacity(0.9),
                        height: 1.4,
                      ),
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

  Widget _buildSectionTitle(String title) {
    return Text(
      title,
      style: const TextStyle(
        fontSize: 18,
        fontWeight: FontWeight.bold,
        color: SimsColors.white,
      ),
    );
  }

  Widget _buildUrlExample(String label, String url, IconData icon) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: SimsColors.navyBlueLight,
        borderRadius: BorderRadius.circular(8),
      ),
      child: Row(
        children: [
          Icon(icon, color: SimsColors.accentBlue, size: 20),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  label,
                  style: const TextStyle(
                    fontSize: 12,
                    color: SimsColors.gray600,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  url,
                  style: const TextStyle(
                    fontSize: 14,
                    color: SimsColors.white,
                    fontFamily: 'monospace',
                  ),
                ),
              ],
            ),
          ),
          IconButton(
            icon: const Icon(Icons.copy, size: 18),
            color: SimsColors.accentBlue,
            onPressed: () => _copyToClipboard(url),
          ),
        ],
      ),
    );
  }

  @override
  void dispose() {
    _urlController.dispose();
    super.dispose();
  }
}
