"""
SIMS Dashboard - Incident Overview and Management
"""
from datetime import datetime
from nicegui import ui
from typing import List, Dict, Optional
import httpx
import logging

logger = logging.getLogger(__name__)
API_BASE = "http://localhost:8000/api"


def format_incident_for_dashboard(incident: Dict) -> Dict:
    """Transform API incident format to dashboard format"""
    # Parse created_at timestamp
    created_at = incident.get('created_at', datetime.now().isoformat())
    if 'T' in created_at:
        timestamp = created_at.replace('T', ' ').split('.')[0]
    else:
        timestamp = created_at

    # Format location
    lat = incident.get('latitude', 0)
    lon = incident.get('longitude', 0)
    location_label = f"{lat:.3f}, {lon:.3f}"

    return {
        'id': incident.get('incident_id', 'UNKNOWN'),
        'timestamp': timestamp,
        'location': {
            'lat': lat,
            'lon': lon,
            'label': location_label
        },
        'description': incident.get('description', 'No description'),
        'priority': incident.get('priority', 'medium'),
        'status': incident.get('status', 'open'),
        'type': incident.get('category', 'general'),
        'reporter': incident.get('user_phone', 'Unknown'),
        'title': incident.get('title', 'Incident')
    }


async def load_incidents() -> List[Dict]:
    """Load incidents from API and format for dashboard"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.get(f"{API_BASE}/incident/", params={'limit': 100})
            if response.status_code == 200:
                raw_incidents = response.json()
                incidents = [format_incident_for_dashboard(inc) for inc in raw_incidents]
                logger.info(f"Loaded {len(incidents)} incidents from API")
                return incidents
            else:
                logger.error(f"Failed to load incidents: {response.status_code}")
                return []
    except Exception as e:
        logger.error(f"Error loading incidents: {e}", exc_info=True)
        return []


async def load_organizations() -> List[Dict]:
    """Load organizations from API"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.get(f"{API_BASE}/organization/", params={'active_only': True, 'limit': 200})
            if response.status_code == 200:
                orgs = response.json()
                logger.info(f"Loaded {len(orgs)} organizations from API")
                return orgs
            else:
                logger.error(f"Failed to load organizations: {response.status_code}")
                return []
    except Exception as e:
        logger.error(f"Error loading organizations: {e}", exc_info=True)
        return []


async def assign_incident_to_org(incident_id: str, organization_id: int, notes: Optional[str] = None):
    """Assign an incident to an organization"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.post(
                f"{API_BASE}/incident/{incident_id}/assign",
                json={'organization_id': organization_id, 'notes': notes}
            )
            if response.status_code == 200:
                logger.info(f"Assigned incident {incident_id} to organization {organization_id}")
                return True
            else:
                logger.error(f"Failed to assign incident: {response.status_code}")
                return False
    except Exception as e:
        logger.error(f"Error assigning incident: {e}", exc_info=True)
        return False


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

def get_priority_class(priority: str) -> str:
    """Get CSS class for priority badge"""
    return f'priority-{priority}'


def format_coordinates(lat: float, lon: float) -> str:
    """Format coordinates for display"""
    return f'{lat:.3f}, {lon:.3f}'




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
        inc_id = incident['id']

        # Escape strings for JavaScript
        inc_id_escaped = inc_id.replace("'", "\\'")
        inc_desc = incident['description'].replace("'", "\\'")

        # Create click handler for this specific marker
        async def create_marker_handler(inc_id=inc_id):
            async def marker_click_handler():
                # Load organizations
                organizations = await load_organizations()
                if not organizations:
                    ui.notify('No organizations available', type='warning')
                    return

                with ui.dialog() as dialog, ui.card().classes('w-full max-w-6xl p-6'):
                    ui.label(f'Forward Incident {inc_id}').classes('text-h6 mb-4 title-font')

                    ui.label('Select Organization:').classes('text-subtitle2 mb-4')

                    # Organization table
                    org_columns = [
                        {'name': 'name', 'label': 'Name', 'field': 'name', 'align': 'left', 'sortable': True},
                        {'name': 'type', 'label': 'Type', 'field': 'type', 'align': 'left', 'sortable': True},
                        {'name': 'city', 'label': 'City', 'field': 'city', 'align': 'left', 'sortable': True},
                        {'name': 'emergency_phone', 'label': 'Emergency Phone', 'field': 'emergency_phone', 'align': 'left'},
                        {'name': 'action', 'label': 'Action', 'field': 'action', 'align': 'center'},
                    ]

                    org_rows = []
                    for org in organizations:
                        org_rows.append({
                            'id': org['id'],
                            'name': org['name'],
                            'type': org.get('type', 'unknown').replace('_', ' ').title(),
                            'city': org.get('city', ''),
                            'emergency_phone': org.get('emergency_phone', ''),
                        })

                    org_table = ui.table(
                        columns=org_columns,
                        rows=org_rows,
                        row_key='id',
                        pagination={'rowsPerPage': 10}
                    ).classes('w-full')

                    org_table.add_slot('body-cell-action', '''
                        <q-td :props="props">
                            <q-btn
                                outline
                                dense
                                size="sm"
                                label="Assign"
                                color="white"
                                no-caps
                                style="font-size: 13px; padding: 4px 12px"
                                @click="$parent.$emit('assign', props.row.id)"
                            />
                        </q-td>
                    ''')

                    async def on_assign(e):
                        org_id = e.args
                        org = next((o for o in organizations if o['id'] == org_id), None)
                        if org:
                            success = await assign_incident_to_org(
                                inc_id,
                                org_id,
                                None
                            )
                            if success:
                                ui.notify(f'Incident {inc_id} assigned to {org["name"]}', type='positive')
                                dialog.close()
                            else:
                                ui.notify('Failed to assign incident', type='negative')

                    org_table.on('assign', on_assign)

                    ui.separator().classes('my-4')

                    with ui.row().classes('w-full justify-end gap-2'):
                        ui.button('Cancel', on_click=dialog.close).props('flat')

                dialog.open()

            return marker_click_handler

        marker_handler = await create_marker_handler()
        marker_button = ui.button('', on_click=marker_handler).classes('hidden')
        marker_button_id = marker_button.id

        # Create marker with custom icon and popup with clickable button using JavaScript
        js_code = f'''
            (function() {{
                var markerColor = "{color}";
                var icon = L.divIcon({{
                    html: '<div style="background: ' + markerColor + '; width: 12px; height: 12px; border: 2px solid #fff; box-shadow: 0 0 8px ' + markerColor + '; cursor: pointer;"></div>',
                    className: 'custom-marker',
                    iconSize: [16, 16],
                    iconAnchor: [8, 8]
                }});

                var marker = L.marker([{lat}, {lon}], {{ icon: icon }});

                var popupContent = '<div class="popup-title">{inc_id_escaped}</div>' +
                                 '<div class="popup-description">{inc_desc}</div>' +
                                 '<div class="popup-priority priority-{priority}">{priority.upper()}</div>' +
                                 '<div style="margin-top: 12px;">' +
                                 '<button onclick="getElement({marker_button_id}).click()" ' +
                                 'style="background: transparent; color: white; border: 1px solid white; ' +
                                 'padding: 6px 14px; font-size: 10px; ' +
                                 'letter-spacing: 0.5px; cursor: pointer; font-weight: 600; ' +
                                 'transition: all 0.2s ease;" ' +
                                 'onmouseover="this.style.background=\\'#FF4444\\'; this.style.borderColor=\\'#FF4444\\'; this.style.color=\\'#0D2637\\';" ' +
                                 'onmouseout="this.style.background=\\'transparent\\'; this.style.borderColor=\\'white\\'; this.style.color=\\'white\\';">Forward</button>' +
                                 '</div>';

                marker.bindPopup(popupContent, {{
                    className: 'custom-popup-{priority}'
                }});
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

        # Table columns - all sortable
        columns = [
            {'name': 'id', 'label': 'Incident ID', 'field': 'id', 'align': 'left', 'sortable': True},
            {'name': 'timestamp', 'label': 'Time', 'field': 'timestamp', 'align': 'left', 'sortable': True},
            {'name': 'location', 'label': 'Location', 'field': 'location', 'align': 'left', 'sortable': True},
            {'name': 'description', 'label': 'Description', 'field': 'description', 'align': 'left', 'sortable': True},
            {'name': 'priority', 'label': 'Priority', 'field': 'priority', 'align': 'left', 'sortable': True},
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
            row_key='id',
            pagination={'rowsPerPage': 20, 'sortBy': 'timestamp', 'descending': True}
        ).classes('w-full')

        # Add expand column in header
        table.add_slot('header', r'''
            <q-tr :props="props">
                <q-th auto-width />
                <q-th v-for="col in props.cols" :key="col.name" :props="props">
                    {{ col.label }}
                </q-th>
            </q-tr>
        ''')

        # Add expandable rows in body
        table.add_slot('body', r'''
            <q-tr :props="props" @click="props.expand = !props.expand" style="cursor: pointer">
                <q-td auto-width>
                    <q-btn size="sm" color="white" round dense flat
                        @click.stop="props.expand = !props.expand"
                        :icon="props.expand ? 'remove' : 'add'" />
                </q-td>
                <q-td key="id" :props="props">
                    <span class="cell-id">{{ props.row.id }}</span>
                </q-td>
                <q-td key="timestamp" :props="props">
                    <span class="cell-time">{{ props.row.timestamp }}</span>
                </q-td>
                <q-td key="location" :props="props">
                    <span class="cell-location">{{ props.row.location }}</span>
                </q-td>
                <q-td key="description" :props="props">
                    <span class="cell-description">{{ props.row.description }}</span>
                </q-td>
                <q-td key="priority" :props="props">
                    <span :class="'priority-badge priority-' + props.row.priority">
                        {{ props.row.priority.toUpperCase() }}
                    </span>
                </q-td>
                <q-td key="action" :props="props" @click.stop>
                    <q-btn
                        outline
                        dense
                        size="sm"
                        label="Forward"
                        color="white"
                        no-caps
                        style="font-size: 13px; padding: 4px 12px"
                        @click="$parent.$emit('forward', props.row.id)"
                    />
                </q-td>
            </q-tr>
            <q-tr v-show="props.expand" :props="props">
                <q-td colspan="100%">
                    <div class="p-4">
                        <div class="grid grid-cols-2 gap-4">
                            <div>
                                <div class="text-xs text-gray-400 uppercase mb-1">Incident ID</div>
                                <div class="text-white font-bold">{{ props.row.id }}</div>
                            </div>
                            <div>
                                <div class="text-xs text-gray-400 uppercase mb-1">Status</div>
                                <div class="text-white">{{ props.row.status || 'active' }}</div>
                            </div>
                            <div>
                                <div class="text-xs text-gray-400 uppercase mb-1">Full Timestamp</div>
                                <div class="text-white">{{ props.row.timestamp }}</div>
                            </div>
                            <div>
                                <div class="text-xs text-gray-400 uppercase mb-1">Type</div>
                                <div class="text-white">{{ props.row.type }}</div>
                            </div>
                            <div class="col-span-2">
                                <div class="text-xs text-gray-400 uppercase mb-1">Full Description</div>
                                <div class="text-white">{{ props.row.description }}</div>
                            </div>
                            <div class="col-span-2">
                                <div class="text-xs text-gray-400 uppercase mb-1">Reporter</div>
                                <div class="text-white">{{ props.row.reporter || 'Unknown' }}</div>
                            </div>
                        </div>
                    </div>
                </q-td>
            </q-tr>
        ''')


        # Handle forward action
        async def handle_forward(e):
            incident_id = e.args
            # Load organizations
            organizations = await load_organizations()
            if not organizations:
                ui.notify('No organizations available', type='warning')
                return

            with ui.dialog() as dialog, ui.card().classes('w-full max-w-6xl p-6'):
                ui.label(f'Forward Incident {incident_id}').classes('text-h6 mb-4 title-font')

                ui.label('Select Organization:').classes('text-subtitle2 mb-4')

                # Organization table
                org_columns = [
                    {'name': 'name', 'label': 'Name', 'field': 'name', 'align': 'left', 'sortable': True},
                    {'name': 'type', 'label': 'Type', 'field': 'type', 'align': 'left', 'sortable': True},
                    {'name': 'city', 'label': 'City', 'field': 'city', 'align': 'left', 'sortable': True},
                    {'name': 'emergency_phone', 'label': 'Emergency Phone', 'field': 'emergency_phone', 'align': 'left'},
                    {'name': 'action', 'label': 'Action', 'field': 'action', 'align': 'center'},
                ]

                org_rows = []
                for org in organizations:
                    org_rows.append({
                        'id': org['id'],
                        'name': org['name'],
                        'type': org.get('type', 'unknown').replace('_', ' ').title(),
                        'city': org.get('city', ''),
                        'emergency_phone': org.get('emergency_phone', ''),
                    })

                org_table = ui.table(
                    columns=org_columns,
                    rows=org_rows,
                    row_key='id',
                    pagination={'rowsPerPage': 10}
                ).classes('w-full')

                org_table.add_slot('body-cell-action', '''
                    <q-td :props="props">
                        <q-btn
                            outline
                            dense
                            size="sm"
                            label="Assign"
                            color="white"
                            no-caps
                            style="font-size: 13px; padding: 4px 12px"
                            @click="$parent.$emit('assign', props.row.id)"
                        />
                    </q-td>
                ''')

                async def on_assign(e):
                    org_id = e.args
                    org = next((o for o in organizations if o['id'] == org_id), None)
                    if org:
                        success = await assign_incident_to_org(
                            incident_id,
                            org_id,
                            None
                        )
                        if success:
                            ui.notify(f'Incident {incident_id} assigned to {org["name"]}', type='positive')
                            dialog.close()
                        else:
                            ui.notify('Failed to assign incident', type='negative')

                org_table.on('assign', on_assign)

                ui.separator().classes('my-4')

                with ui.row().classes('w-full justify-end gap-2'):
                    ui.button('Cancel', on_click=dialog.close).props('flat')

            dialog.open()

        table.on('forward', handle_forward)


async def dashboard():
    """Main dashboard page"""
    # Load incidents from API
    incidents = await load_incidents()

    # Use mock data as fallback if API fails
    if not incidents:
        logger.warning("No incidents loaded from API, using mock data as fallback")
        incidents = MOCK_INCIDENTS

    # Overview section (Map + Stats)
    await render_overview(incidents)

    # Incident table
    await render_incident_table(incidents)
