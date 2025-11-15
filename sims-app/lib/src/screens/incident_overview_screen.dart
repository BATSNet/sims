import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:go_router/go_router.dart';
import '../connection/bloc/websocket_bloc.dart';
import '../connection/bloc/websocket_state.dart';
import '../models/incident.dart';
import '../utils/sims_colors.dart';
import '../widgets/orbit_spinner.dart';

class IncidentOverviewScreen extends StatelessWidget {
  const IncidentOverviewScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text(
          'SIMS',
          style: TextStyle(
            color: SimsColors.white,
            fontWeight: FontWeight.bold,
          ),
        ),
        backgroundColor: SimsColors.blue,
        actions: [
          BlocBuilder<WebSocketBloc, WebSocketState>(
            builder: (context, state) {
              return Padding(
                padding: const EdgeInsets.symmetric(horizontal: 16),
                child: Center(
                  child: Row(
                    children: [
                      Container(
                        width: 8,
                        height: 8,
                        decoration: BoxDecoration(
                          color: state.isConnected
                              ? SimsColors.lowGreen
                              : SimsColors.criticalRed,
                          shape: BoxShape.circle,
                        ),
                      ),
                      const SizedBox(width: 8),
                      Text(
                        state.isConnected ? 'Live' : 'Offline',
                        style: const TextStyle(
                          fontSize: 12,
                          color: SimsColors.white,
                        ),
                      ),
                    ],
                  ),
                ),
              );
            },
          ),
        ],
      ),
      body: BlocBuilder<WebSocketBloc, WebSocketState>(
        builder: (context, state) {
          final recentIncidents = state.incidents.take(5).toList();

          return RefreshIndicator(
            onRefresh: () async {
              // WebSocket automatically updates, no manual refresh needed
            },
            child: CustomScrollView(
              slivers: [
                if (!state.isConnected)
                  SliverToBoxAdapter(
                    child: Container(
                      margin: const EdgeInsets.all(16),
                      padding: const EdgeInsets.all(12),
                      decoration: BoxDecoration(
                        color: SimsColors.navyBlueLight,
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(
                          color: SimsColors.highOrange.withOpacity(0.5),
                          width: 1,
                        ),
                      ),
                      child: Row(
                        children: [
                          const Icon(
                            Icons.info_outline,
                            color: SimsColors.highOrange,
                            size: 20,
                          ),
                          const SizedBox(width: 12),
                          Expanded(
                            child: Text(
                              'Offline mode - You can still report incidents',
                              style: TextStyle(
                                fontSize: 13,
                                color: SimsColors.white.withOpacity(0.9),
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                SliverToBoxAdapter(
                  child: SizedBox(
                    height: MediaQuery.of(context).size.height * 0.25,
                  ),
                ),
                SliverToBoxAdapter(
                  child: _buildReportButton(context),
                ),
                if (recentIncidents.isEmpty)
                  SliverToBoxAdapter(
                    child: _buildWelcomeSection(context),
                  ),
                if (recentIncidents.isNotEmpty)
                  SliverToBoxAdapter(
                    child: Padding(
                      padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
                      child: Text(
                        'Recent Incidents',
                        style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                          color: SimsColors.white,
                        ),
                      ),
                    ),
                  ),
                SliverList(
                  delegate: SliverChildBuilderDelegate(
                    (context, index) {
                      final incident = recentIncidents[index];
                      return Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 16),
                        child: _buildCompactIncidentCard(context, incident),
                      );
                    },
                    childCount: recentIncidents.length,
                  ),
                ),
                SliverToBoxAdapter(
                  child: _buildEmergencyContactSection(),
                ),
                const SliverToBoxAdapter(
                  child: SizedBox(height: 80),
                ),
              ],
            ),
          );
        },
      ),
    );
  }

  Widget _buildReportButton(BuildContext context) {
    return GestureDetector(
      onTap: () {
        context.push('/camera');
      },
      child: Container(
        height: 200,
        margin: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [
              SimsColors.blue.withOpacity(0.9),
              SimsColors.blue.withOpacity(0.7),
            ],
          ),
          borderRadius: BorderRadius.circular(16),
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Container(
              width: 100,
              height: 100,
              decoration: BoxDecoration(
                color: SimsColors.white.withOpacity(0.2),
                shape: BoxShape.circle,
              ),
              child: const Icon(
                Icons.camera_alt,
                size: 60,
                color: SimsColors.white,
              ),
            ),
            const SizedBox(height: 16),
            const Text(
              'Start Recording',
              style: TextStyle(
                fontSize: 24,
                fontWeight: FontWeight.bold,
                color: SimsColors.white,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              'Tap to report a new incident',
              style: TextStyle(
                fontSize: 16,
                color: SimsColors.white.withOpacity(0.9),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildWelcomeSection(BuildContext context) {
    return Container(
      margin: const EdgeInsets.fromLTRB(16, 32, 16, 8),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: SimsColors.navyBlueLight,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: SimsColors.teal.withOpacity(0.3),
          width: 1,
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(
                Icons.info_outline,
                color: SimsColors.teal,
                size: 20,
              ),
              const SizedBox(width: 8),
              Text(
                'Welcome to SIMS',
                style: TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.bold,
                  color: SimsColors.white,
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Text(
            'Tap the button above to capture your first incident report.',
            style: TextStyle(
              fontSize: 13,
              color: SimsColors.white.withOpacity(0.9),
              height: 1.4,
            ),
          ),
          const SizedBox(height: 12),
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: SimsColors.navyBlueDark.withOpacity(0.5),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    const Icon(
                      Icons.phone,
                      color: SimsColors.criticalRed,
                      size: 18,
                    ),
                    const SizedBox(width: 8),
                    Text(
                      'Emergency Numbers',
                      style: TextStyle(
                        fontSize: 14,
                        fontWeight: FontWeight.bold,
                        color: SimsColors.white,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                _buildEmergencyNumber('Police', '110'),
                const SizedBox(height: 6),
                _buildEmergencyNumber('Fire/Medical', '112'),
                const SizedBox(height: 6),
                _buildEmergencyNumber('Command Center', '+49 123 456 789'),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildTipItem({
    required IconData icon,
    required String title,
    required String description,
  }) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          padding: const EdgeInsets.all(8),
          decoration: BoxDecoration(
            color: SimsColors.teal.withOpacity(0.2),
            borderRadius: BorderRadius.circular(8),
          ),
          child: Icon(
            icon,
            color: SimsColors.teal,
            size: 20,
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                title,
                style: TextStyle(
                  fontSize: 14,
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
                  height: 1.4,
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _buildEmergencyNumber(String label, String number) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Text(
          label,
          style: TextStyle(
            fontSize: 13,
            color: SimsColors.white.withOpacity(0.8),
          ),
        ),
        Text(
          number,
          style: const TextStyle(
            fontSize: 14,
            fontWeight: FontWeight.bold,
            color: SimsColors.white,
            letterSpacing: 0.5,
          ),
        ),
      ],
    );
  }

  Widget _buildEmergencyContactSection() {
    return Container(
      margin: const EdgeInsets.fromLTRB(16, 24, 16, 8),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: SimsColors.navyBlueLight,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: SimsColors.criticalRed.withOpacity(0.3),
          width: 1,
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(
                Icons.phone,
                color: SimsColors.criticalRed,
                size: 20,
              ),
              const SizedBox(width: 8),
              Text(
                'Emergency Contact',
                style: TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.bold,
                  color: SimsColors.white,
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          _buildEmergencyNumber('Police', '110'),
          const SizedBox(height: 8),
          _buildEmergencyNumber('Fire/Medical', '112'),
          const SizedBox(height: 8),
          _buildEmergencyNumber('Command Center', '+49 123 456 789'),
        ],
      ),
    );
  }

  Widget _buildCompactIncidentCard(BuildContext context, Incident incident) {
    final priorityColor = _getPriorityColor(incident.priority);

    return GestureDetector(
      onTap: () {
        context.push('/chat/${incident.id}');
      },
      child: Container(
        margin: const EdgeInsets.only(bottom: 12),
        constraints: const BoxConstraints(minHeight: 80),
        decoration: BoxDecoration(
          color: SimsColors.navyBlueLight,
          borderRadius: BorderRadius.circular(12),
        ),
        child: IntrinsicHeight(
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              if (incident.imageUrl != null)
                ClipRRect(
                  borderRadius: const BorderRadius.horizontal(left: Radius.circular(12)),
                  child: Image.network(
                    incident.imageUrl!,
                    width: 80,
                    fit: BoxFit.cover,
                    errorBuilder: (context, error, stackTrace) {
                      return Container(
                        width: 80,
                        color: SimsColors.navyBlueDark,
                        child: const Icon(
                          Icons.image_not_supported,
                          size: 32,
                          color: SimsColors.gray600,
                        ),
                      );
                    },
                  ),
                ),
              Expanded(
                child: Padding(
                  padding: const EdgeInsets.all(12),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    mainAxisAlignment: MainAxisAlignment.center,
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Row(
                        children: [
                          Container(
                            width: 3,
                            height: 16,
                            decoration: BoxDecoration(
                              color: priorityColor,
                              borderRadius: BorderRadius.circular(2),
                            ),
                          ),
                          const SizedBox(width: 8),
                          Expanded(
                            child: Text(
                              incident.title,
                              style: const TextStyle(
                                fontSize: 15,
                                fontWeight: FontWeight.bold,
                                color: SimsColors.white,
                              ),
                              maxLines: 1,
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 4),
                      Text(
                        incident.description,
                        style: TextStyle(
                          fontSize: 13,
                          color: SimsColors.white.withOpacity(0.7),
                        ),
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                      ),
                      const SizedBox(height: 4),
                      Row(
                        children: [
                          Icon(
                            Icons.access_time,
                            size: 12,
                            color: SimsColors.gray600,
                          ),
                          const SizedBox(width: 4),
                          Text(
                            _formatDateTime(incident.createdAt),
                            style: TextStyle(
                              fontSize: 12,
                              color: SimsColors.gray600,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Color _getPriorityColor(IncidentPriority priority) {
    switch (priority) {
      case IncidentPriority.critical:
        return SimsColors.criticalRed;
      case IncidentPriority.high:
        return SimsColors.highOrange;
      case IncidentPriority.medium:
        return SimsColors.mediumBlue;
      case IncidentPriority.low:
        return SimsColors.lowGreen;
    }
  }

  String _formatDateTime(DateTime dateTime) {
    final now = DateTime.now();
    final difference = now.difference(dateTime);

    if (difference.inMinutes < 1) {
      return 'Just now';
    } else if (difference.inMinutes < 60) {
      return '${difference.inMinutes}m ago';
    } else if (difference.inHours < 24) {
      return '${difference.inHours}h ago';
    } else {
      return '${difference.inDays}d ago';
    }
  }
}
