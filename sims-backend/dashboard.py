"""
SIMS Dashboard - Incident Overview and Management
"""
from datetime import datetime
from nicegui import ui
from typing import List, Dict


# Mock data for incidents (will be replaced with database queries)
MOCK_INCIDENTS = [
    {
        'id': 'INC-2847',
        'timestamp': '2025-11-14 14:25:33',
        'location': {'lat': 52.520, 'lon': 13.405, 'label': 'Berlin, Germany'},
        'description': 'UAV detection near critical facility',
        'priority': 'critical',
        'status': 'active',
        'type': 'drone',
        'reporter': 'Field Unit Alpha',
    },
    {
        'id': 'INC-2846',
        'timestamp': '2025-11-14 14:18:12',
        'location': {'lat': 48.137, 'lon': 11.576, 'label': 'Munich, Germany'},
        'description': 'Suspicious vehicle movement near checkpoint',
        'priority': 'high',
        'status': 'active',
        'type': 'vehicle',
        'reporter': 'Checkpoint 7',
    },
    {
        'id': 'INC-2845',
        'timestamp': '2025-11-14 14:02:45',
        'location': {'lat': 50.110, 'lon': 8.682, 'label': 'Frankfurt, Germany'},
        'description': 'Unidentified personnel in restricted area',
        'priority': 'medium',
        'status': 'active',
        'type': 'personnel',
        'reporter': 'Security Team 3',
    },
    {
        'id': 'INC-2844',
        'timestamp': '2025-11-14 13:55:22',
        'location': {'lat': 51.339, 'lon': 12.374, 'label': 'Leipzig, Germany'},
        'description': 'Equipment malfunction - sensor array offline',
        'priority': 'low',
        'status': 'active',
        'type': 'technical',
        'reporter': 'Maintenance Unit',
    },
    {
        'id': 'INC-2843',
        'timestamp': '2025-11-14 13:47:08',
        'location': {'lat': 53.550, 'lon': 9.993, 'label': 'Hamburg, Germany'},
        'description': 'Perimeter breach alert - fence section damaged',
        'priority': 'high',
        'status': 'active',
        'type': 'security',
        'reporter': 'Perimeter Team North',
    },
    {
        'id': 'INC-2842',
        'timestamp': '2025-11-14 13:32:15',
        'location': {'lat': 48.775, 'lon': 9.182, 'label': 'Stuttgart, Germany'},
        'description': 'Unusual communication pattern detected',
        'priority': 'medium',
        'status': 'active',
        'type': 'signals',
        'reporter': 'SIGINT Team',
    },
    {
        'id': 'INC-2841',
        'timestamp': '2025-11-14 13:18:44',
        'location': {'lat': 50.937, 'lon': 6.960, 'label': 'Cologne, Germany'},
        'description': 'Unauthorized access attempt on secure system',
        'priority': 'critical',
        'status': 'active',
        'type': 'cyber',
        'reporter': 'Cyber Defense Unit',
    },
    {
        'id': 'INC-2840',
        'timestamp': '2025-11-14 13:05:29',
        'location': {'lat': 51.050, 'lon': 13.737, 'label': 'Dresden, Germany'},
        'description': 'Thermal anomaly detected in sector 7',
        'priority': 'medium',
        'status': 'active',
        'type': 'sensor',
        'reporter': 'Surveillance Unit East',
    },
]

# Organizations for forwarding
ORGANIZATIONS = [
    {'id': 'nato', 'name': 'NATO Command'},
    {'id': 'police', 'name': 'Local Police'},
    {'id': 'military', 'name': 'Military Command'},
    {'id': 'emergency', 'name': 'Emergency Services'},
    {'id': 'aviation', 'name': 'Aviation Authority'},
]


def get_priority_class(priority: str) -> str:
    """Get CSS class for priority badge"""
    return f'priority-{priority}'


def format_coordinates(lat: float, lon: float) -> str:
    """Format coordinates for display"""
    return f'{lat:.3f}, {lon:.3f}'


async def forward_incident(incident_id: str, organization_id: str):
    """Forward an incident to a responsible organization"""
    org_name = next((org['name'] for org in ORGANIZATIONS if org['id'] == organization_id), 'Unknown')
    ui.notify(f'Incident {incident_id} forwarded to {org_name}', type='positive')




async def render_map(incidents: List[Dict]):
    """Render the incident map with markers"""
    # Create leaflet map
    m = ui.leaflet(center=(51.1657, 10.4515), zoom=6).classes('w-full h-[500px]')

    # Clear default layers and add dark tiles
    m.clear_layers()
    m.tile_layer(
        url_template='https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png',
        options={
            'maxZoom': 19,
            'subdomains': 'abcd',
            'attribution': ''
        }
    )

    # Priority colors
    colors = {
        'critical': '#FF4444',
        'high': '#ffa600',
        'medium': '#63ABFF',
        'low': 'rgba(255, 255, 255, 0.4)'
    }

    # Add custom square markers for each incident using JavaScript
    for incident in incidents:
        lat = incident['location']['lat']
        lon = incident['location']['lon']
        priority = incident['priority']
        color = colors.get(priority, '#63ABFF')

        # Escape strings for JavaScript
        inc_id = incident['id'].replace("'", "\\'")
        inc_desc = incident['description'].replace("'", "\\'")

        # Create marker with custom icon and popup using JavaScript
        js_code = f'''
            (function() {{
                var icon = L.divIcon({{
                    html: '<div style="background: {color}; width: 12px; height: 12px; border: 2px solid #fff; box-shadow: 0 0 8px {color};"></div>',
                    className: 'custom-marker',
                    iconSize: [16, 16],
                    iconAnchor: [8, 8]
                }});

                var marker = L.marker([{lat}, {lon}], {{ icon: icon }});

                var popupContent = '<div class="popup-title">{inc_id}</div>' +
                                 '<div class="popup-description">{inc_desc}</div>' +
                                 '<div class="popup-priority priority-{priority}">{priority.upper()}</div>';

                marker.bindPopup(popupContent);
                marker.addTo(getElement({m.id}).map);
            }})();
        '''

        ui.run_javascript(js_code)


async def render_stats(incidents: List[Dict]):
    """Render stats column"""
    total_incidents = len(incidents)
    critical_count = sum(1 for i in incidents if i['priority'] == 'critical')
    high_count = sum(1 for i in incidents if i['priority'] in ['critical', 'high'])
    active_count = sum(1 for i in incidents if i['status'] == 'active')

    with ui.element('div').classes('stats-column'):
        with ui.element('div').classes('metric-card'):
            ui.label('Active Incidents').classes('metric-label')
            ui.label(str(active_count)).classes('metric-value')

        with ui.element('div').classes('metric-card'):
            ui.label('High Priority').classes('metric-label')
            ui.label(str(high_count)).classes('metric-value')

        with ui.element('div').classes('metric-card'):
            ui.label('Total Reports').classes('metric-label')
            ui.label(str(total_incidents)).classes('metric-value')

        with ui.element('div').classes('metric-card'):
            ui.label('Avg Response').classes('metric-label')
            ui.label('2.3m').classes('metric-value')


async def render_overview(incidents: List[Dict]):
    """Render overview section with map and stats"""
    with ui.element('div').classes('overview-section w-full'):
        await render_map(incidents)
        await render_stats(incidents)


async def render_incident_table(incidents: List[Dict]):
    """Render the incident table with filters"""
    # Section title in container
    with ui.element('div').classes('content-container'):
        ui.label('Active Incidents').classes('section-title w-full')

    # Table section - full width
    with ui.element('div').classes('table-section w-full'):
        # Table header bar with filters
        with ui.element('div').classes('table-header-bar'):
            ui.label('Incident Log').classes('table-title')

            with ui.element('div').classes('table-actions'):
                ui.button('All', on_click=lambda: None).classes('filter-btn active')
                ui.button('Critical', on_click=lambda: None).classes('filter-btn')
                ui.button('High', on_click=lambda: None).classes('filter-btn')
                ui.button('Today', on_click=lambda: None).classes('filter-btn')

        # Table columns
        columns = [
            {'name': 'id', 'label': 'Incident ID', 'field': 'id', 'align': 'left'},
            {'name': 'timestamp', 'label': 'Time', 'field': 'timestamp', 'align': 'left'},
            {'name': 'location', 'label': 'Location', 'field': 'location', 'align': 'left'},
            {'name': 'description', 'label': 'Description', 'field': 'description', 'align': 'left'},
            {'name': 'priority', 'label': 'Priority', 'field': 'priority', 'align': 'left'},
            {'name': 'action', 'label': 'Action', 'field': 'action', 'align': 'center'},
        ]

        # Format incidents for table
        rows = []
        for incident in incidents:
            rows.append({
                'id': incident['id'],
                'timestamp': incident['timestamp'].split(' ')[1],  # Just time
                'location': incident['location']['label'],
                'description': incident['description'],
                'priority': incident['priority'],
                'action': incident['id'],
            })

        table = ui.table(
            columns=columns,
            rows=rows,
            row_key='id'
        ).classes('w-full')

        # Custom cell templates
        table.add_slot('body-cell-id', '''
            <q-td :props="props">
                <span class="cell-id">{{ props.value }}</span>
            </q-td>
        ''')

        table.add_slot('body-cell-timestamp', '''
            <q-td :props="props">
                <span class="cell-time">{{ props.value }}</span>
            </q-td>
        ''')

        table.add_slot('body-cell-location', '''
            <q-td :props="props">
                <span class="cell-location">{{ props.value }}</span>
            </q-td>
        ''')

        table.add_slot('body-cell-description', '''
            <q-td :props="props">
                <span class="cell-description">{{ props.value }}</span>
            </q-td>
        ''')

        table.add_slot('body-cell-priority', '''
            <q-td :props="props">
                <span :class="'priority-badge priority-' + props.value">
                    {{ props.value.toUpperCase() }}
                </span>
            </q-td>
        ''')

        table.add_slot('body-cell-action', '''
            <q-td :props="props">
                <q-btn
                    flat
                    dense
                    label="FORWARD"
                    class="action-button"
                    @click="$emit('forward', props.row.id)"
                />
            </q-td>
        ''')

        # Handle forward action
        async def handle_forward(e):
            incident_id = e.args
            with ui.dialog() as dialog, ui.card():
                ui.label(f'Forward incident {incident_id}').classes('text-lg mb-4')
                ui.label('Select organization:').classes('mb-2')

                for org in ORGANIZATIONS:
                    with ui.row().classes('w-full mb-2'):
                        ui.button(
                            org['name'],
                            on_click=lambda org_id=org['id']: [
                                forward_incident(incident_id, org_id),
                                dialog.close()
                            ]
                        ).classes('w-full')

                ui.button('Cancel', on_click=dialog.close).classes('mt-4')

            dialog.open()

        table.on('forward', handle_forward)


async def dashboard():
    """Main dashboard page"""
    # Overview section (Map + Stats)
    await render_overview(MOCK_INCIDENTS)

    # Incident table
    await render_incident_table(MOCK_INCIDENTS)
