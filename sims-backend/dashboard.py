"""
SIMS Dashboard - Incident Overview and Management
"""
from datetime import datetime
from nicegui import ui
from typing import List, Dict, Optional
import httpx
import logging
from i18n import i18n

logger = logging.getLogger(__name__)
API_BASE = "http://localhost:8000/api"


def format_incident_for_dashboard(incident: Dict) -> Dict:
    """Transform API incident format to dashboard format"""
    # Parse createdAt timestamp (camelCase from API)
    created_at = incident.get('createdAt', datetime.now().isoformat())
    if 'T' in created_at:
        timestamp = created_at.replace('T', ' ').split('.')[0]
    else:
        timestamp = created_at

    # Format location
    lat = incident.get('latitude')
    lon = incident.get('longitude')
    if lat is not None and lon is not None:
        location_label = f"{lat:.3f}, {lon:.3f}"
    else:
        location_label = "No location"

    # Get category and capitalize/format it
    category = incident.get('category', 'unclassified')
    category_formatted = category.replace('_', ' ').title() if category else 'Unclassified'

    # Get assigned organization info (camelCase from API)
    assigned_org = None
    is_auto_assigned = False
    if incident.get('routedTo'):
        assigned_org = {
            'id': incident.get('routedTo'),
            'name': incident.get('routedToName', 'Unknown')
        }
        # Check if it was auto-assigned
        metadata = incident.get('metadata', {})
        assignment_history = metadata.get('assignment_history', [])
        if assignment_history and assignment_history[-1].get('auto_assigned'):
            is_auto_assigned = True

    # Extract media file URLs
    image_url = None
    audio_url = None
    video_url = None

    # Check for media files in the incident data
    media_files = incident.get('mediaFiles', [])
    if media_files:
        for media in media_files:
            media_type = media.get('type', '').lower()
            file_url = media.get('url') or media.get('fileUrl')

            if media_type in ['image', 'photo'] and not image_url:
                image_url = file_url
            elif media_type in ['audio', 'voice'] and not audio_url:
                audio_url = file_url
            elif media_type in ['video'] and not video_url:
                video_url = file_url

    # Also check for direct URL fields (camelCase from API)
    if not image_url:
        image_url = incident.get('imageUrl') or incident.get('image_url')
    if not audio_url:
        audio_url = incident.get('audioUrl') or incident.get('audio_url')
    if not video_url:
        video_url = incident.get('videoUrl') or incident.get('video_url')

    return {
        'id': incident.get('incidentId', 'UNKNOWN'),  # Formatted ID for display
        'uuid': incident.get('id', 'UNKNOWN'),  # UUID for API calls
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
        'category': category_formatted,
        'category_raw': category,
        'reporter': incident.get('userPhone', 'Unknown'),  # camelCase from API
        'title': incident.get('title', 'Incident'),
        'assigned_org': assigned_org,
        'is_auto_assigned': is_auto_assigned,
        'imageUrl': image_url,
        'audioUrl': audio_url,
        'videoUrl': video_url,
        'audioTranscript': incident.get('audioTranscript'),
    }


async def load_incidents() -> List[Dict]:
    """Load incidents from API and format for dashboard"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.get(f"{API_BASE}/incident/", params={'limit': 100})
            if response.status_code == 200:
                raw_incidents = response.json()

                # Debug: Log first incident to see what data we're getting
                if raw_incidents:
                    first_incident = raw_incidents[0]
                    logger.info(f"[DEBUG] Sample raw incident: ID={first_incident.get('incidentId')}, imageUrl={first_incident.get('imageUrl')}, audioUrl={first_incident.get('audioUrl')}, audioTranscript={first_incident.get('audioTranscript')}")

                incidents = [format_incident_for_dashboard(inc) for inc in raw_incidents]

                # Debug: Log formatted incident
                if incidents:
                    first_formatted = incidents[0]
                    logger.info(f"[DEBUG] Sample formatted incident: ID={first_formatted.get('id')}, imageUrl={first_formatted.get('imageUrl')}, audioUrl={first_formatted.get('audioUrl')}, audioTranscript={first_formatted.get('audioTranscript')}")

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
        # Use longer timeout for assignment since it includes SEDAP forwarding (up to 30s)
        async with httpx.AsyncClient(timeout=45.0) as client:
            response = await client.post(
                f"{API_BASE}/incident/{incident_id}/assign",
                json={'organization_id': organization_id, 'notes': notes}
            )
            if response.status_code == 200:
                logger.info(f"Assigned incident {incident_id} to organization {organization_id}")
                return {'success': True, 'message': i18n.t('ui.dashboard.assigned_success')}
            elif response.status_code == 404:
                logger.warning(f"Incident {incident_id} not found in database (possibly mock data)")
                return {'success': False, 'message': f'Incident {incident_id} does not exist in database. This is mock data - create a real incident from the mobile app first.'}
            else:
                logger.error(f"Failed to assign incident: {response.status_code}")
                error_detail = response.json().get('detail', 'Unknown error') if response.text else 'Unknown error'
                return {'success': False, 'message': f'Assignment failed: {error_detail}'}
    except Exception as e:
        logger.error(f"Error assigning incident: {e}", exc_info=True)
        return {'success': False, 'message': f'Error: {str(e)}'}


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
    # Debug logging
    logger.info(f"render_map called with {len(incidents)} incidents")
    valid_incidents = [inc for inc in incidents if inc['location']['lat'] is not None and inc['location']['lon'] is not None]
    logger.info(f"Found {len(valid_incidents)} incidents with valid coordinates")
    if valid_incidents:
        logger.info(f"Sample incident: {valid_incidents[0]}")

    # Add MarkerCluster CSS and JS with proper loading
    ui.add_head_html('''
        <link rel="stylesheet" href="https://unpkg.com/leaflet.markercluster@1.4.1/dist/MarkerCluster.css" />
        <link rel="stylesheet" href="https://unpkg.com/leaflet.markercluster@1.4.1/dist/MarkerCluster.Default.css" />
    ''')

    # Load MarkerCluster script and wait for it to load
    load_script_js = '''
        (function() {
            if (window.markerClusterLoaded) {
                console.log('[SIMS] MarkerCluster already loaded');
                return;
            }

            console.log('[SIMS] Loading MarkerCluster library...');
            var script = document.createElement('script');
            script.src = 'https://unpkg.com/leaflet.markercluster@1.4.1/dist/leaflet.markercluster.js';
            script.onload = function() {
                window.markerClusterLoaded = true;
                console.log('[SIMS] MarkerCluster library loaded successfully');
            };
            script.onerror = function() {
                console.error('[SIMS] Failed to load MarkerCluster library');
            };
            document.head.appendChild(script);
        })();
    '''
    ui.run_javascript(load_script_js)

    # Create leaflet map - responsive height with maxZoom for MarkerCluster
    # Default to Germany center, will be updated by geolocation
    m = ui.leaflet(center=(51.1657, 10.4515), zoom=11).classes('w-full h-[500px] sm:h-[450px] md:h-[400px] lg:h-[500px]')

    # Set maxZoom on map for MarkerCluster compatibility
    ui.run_javascript(f'''
        (function() {{
            var map = getElement({m.id}).map;
            map.options.maxZoom = 19;
            window.leafletMap = map;  // Store map reference globally
            console.log('[SIMS] Set map maxZoom to 19 and stored global reference');
        }})();
    ''')

    # Get user's current location and center map on it (50km view = zoom 11)
    geolocation_js = f'''
        (function() {{
            if (navigator.geolocation) {{
                console.log('[SIMS] Requesting geolocation...');
                navigator.geolocation.getCurrentPosition(
                    function(position) {{
                        var lat = position.coords.latitude;
                        var lon = position.coords.longitude;
                        console.log('[SIMS] Geolocation obtained:', lat, lon);

                        // Center map on user's location with zoom 11 (approximately 50km view)
                        var map = window.leafletMap || getElement({m.id}).map;
                        if (!map) {{
                            console.error('[SIMS] Map not available for geolocation');
                            return;
                        }}
                        map.setView([lat, lon], 11);

                        // Add a marker for user's current location
                        var userIcon = L.divIcon({{
                            html: '<div style="background: #00FF00; width: 10px; height: 10px; border: 2px solid #fff; border-radius: 50%; box-shadow: 0 0 10px #00FF00; cursor: pointer;"></div>',
                            className: 'user-location-marker',
                            iconSize: [14, 14],
                            iconAnchor: [7, 7]
                        }});

                        L.marker([lat, lon], {{ icon: userIcon }})
                            .bindPopup('Your Location')
                            .addTo(map);

                        // Force tile redraw after geolocation
                        setTimeout(function() {{
                            console.log('[SIMS] Redrawing tiles after geolocation');
                            map.eachLayer(function(layer) {{
                                if (layer instanceof L.TileLayer) {{
                                    layer.redraw();
                                }}
                            }});
                        }}, 200);

                        // After centering on user location, zoom to show all incidents
                        console.log('[SIMS] User location set, now zooming to show all incidents');
                        if (window.autoZoomToIncidents) {{
                            window.autoZoomToIncidents();
                        }}
                    }},
                    function(error) {{
                        if (error.message && error.message.includes('secure origins')) {{
                            console.info('[SIMS] Geolocation requires HTTPS. Map will center on incidents instead.');
                        }} else {{
                            console.warn('[SIMS] Geolocation error:', error.message);
                        }}
                        console.log('[SIMS] Auto-zooming to incidents');

                        // Trigger auto-zoom to incidents if geolocation fails
                        if (window.autoZoomToIncidents) {{
                            window.autoZoomToIncidents();
                        }}
                    }},
                    {{
                        enableHighAccuracy: true,
                        timeout: 5000,
                        maximumAge: 0
                    }}
                );
            }} else {{
                console.warn('[SIMS] Geolocation not supported by browser');
            }}
        }})();
    '''
    ui.run_javascript(geolocation_js)

    # Clear default layers and add dark tiles with cache busting
    m.clear_layers()

    # Add timestamp for cache busting to ensure fresh tiles
    import time
    cache_buster = int(time.time())

    m.tile_layer(
        url_template=f'https://{{s}}.basemaps.cartocdn.com/dark_all/{{z}}/{{x}}/{{y}}{{r}}.png?v={cache_buster}',
        options={
            'maxZoom': 19,
            'subdomains': 'abcd',
            'attribution': '',
            'noCache': True  # Prevent browser caching
        }
    )

    # Organization type icons (all gray)
    org_icons = {
        'military': {'icon': '&#9733;', 'color': '#808080', 'name': 'Military'},
        'police': {'icon': '&#128110;', 'color': '#808080', 'name': 'Police'},
        'fire': {'icon': '&#128293;', 'color': '#808080', 'name': 'Fire'},
        'medical': {'icon': '&#10010;', 'color': '#808080', 'name': 'Medical'},
        'civil_defense': {'icon': '&#9888;', 'color': '#808080', 'name': 'Civil Defense'},
        'government': {'icon': '&#127970;', 'color': '#808080', 'name': 'Government'},
        'other': {'icon': '&#9679;', 'color': '#808080', 'name': 'Other'}
    }

    # Priority colors for incidents
    colors = {
        'critical': '#FF4444',
        'high': '#ffa600',
        'medium': '#63ABFF',
        'low': 'rgba(255, 255, 255, 0.4)'
    }

    # Load and add organization markers first (so they appear under incidents)
    organizations = await load_organizations()
    for org in organizations:
        lat = org.get('latitude')
        lon = org.get('longitude')

        # Skip organizations without location
        if lat is None or lon is None:
            continue

        org_type = org.get('type', 'other')
        icon_config = org_icons.get(org_type, org_icons['other'])

        # Escape strings for JavaScript
        org_name = org.get('name', 'Unknown').replace("'", "\\'")
        org_short = org.get('short_name', '').replace("'", "\\'")
        org_type_display = icon_config['name']
        org_phone = org.get('phone', 'N/A').replace("'", "\\'")
        org_emergency = org.get('emergency_phone', 'N/A').replace("'", "\\'")
        org_contact = org.get('contact_person', 'N/A').replace("'", "\\'")
        org_city = org.get('city', 'N/A').replace("'", "\\'")
        org_address = org.get('address', 'N/A').replace("'", "\\'")
        org_capabilities = ', '.join(org.get('capabilities', [])) if org.get('capabilities') else 'None specified'
        org_capabilities = org_capabilities.replace("'", "\\'")
        org_response_area = org.get('response_area', 'Not specified').replace("'", "\\'")

        # Create organization marker with icon
        js_code = f'''
            (function() {{
                var icon = L.divIcon({{
                    html: '<div style="background: {icon_config["color"]}; width: 16px; height: 16px; border: 2px solid #fff; display: flex; align-items: center; justify-content: center; font-size: 10px; color: white; box-shadow: 0 0 8px {icon_config["color"]}; cursor: pointer;">{icon_config["icon"]}</div>',
                    className: 'custom-org-marker',
                    iconSize: [20, 20],
                    iconAnchor: [10, 10]
                }});

                var marker = L.marker([{lat}, {lon}], {{ icon: icon }});

                var popupContent = `
                    <div style="min-width: 250px; padding: 4px;">
                        <div style="font-size: 14px; font-weight: 600; color: white; margin-bottom: 8px; border-bottom: 1px solid rgba(255,255,255,0.2); padding-bottom: 6px;">
                            {org_name}
                        </div>
                        <div style="display: grid; grid-template-columns: auto 1fr; gap: 6px 12px; font-size: 11px;">
                            <div style="color: #9CA3AF; font-weight: 500;">Type:</div>
                            <div style="color: white;">{org_type_display}</div>

                            <div style="color: #9CA3AF; font-weight: 500;">City:</div>
                            <div style="color: white;">{org_city}</div>

                            <div style="color: #9CA3AF; font-weight: 500;">Contact:</div>
                            <div style="color: white;">{org_contact}</div>

                            <div style="color: #9CA3AF; font-weight: 500;">Phone:</div>
                            <div style="color: white;">{org_phone}</div>

                            <div style="color: #9CA3AF; font-weight: 500;">Emergency:</div>
                            <div style="color: white;">{org_emergency}</div>

                            <div style="color: #9CA3AF; font-weight: 500;">Address:</div>
                            <div style="color: white;">{org_address}</div>

                            <div style="color: #9CA3AF; font-weight: 500;">Capabilities:</div>
                            <div style="color: white;">{org_capabilities}</div>

                            <div style="color: #9CA3AF; font-weight: 500;">Response Area:</div>
                            <div style="color: white;">{org_response_area}</div>
                        </div>
                    </div>
                `;

                marker.bindPopup(popupContent, {{
                    className: 'custom-org-popup',
                    maxWidth: 350
                }});
                marker.addTo(getElement({m.id}).map);
            }})();
        '''

        ui.run_javascript(js_code)

    # Create marker cluster group for incidents with custom styling
    # Wait for MarkerCluster library to load before initializing
    cluster_init_js = f'''
        (function() {{
            var attempts = 0;
            var maxAttempts = 100; // 10 seconds max

            function initializeClusterGroup() {{
                attempts++;

                // Check if MarkerCluster library is loaded
                if (!window.markerClusterLoaded || typeof L === 'undefined' || typeof L.markerClusterGroup === 'undefined') {{
                    if (attempts >= maxAttempts) {{
                        console.error('[SIMS] Timeout waiting for MarkerCluster library after ' + (maxAttempts * 100) + 'ms');
                        console.error('[SIMS] L defined:', typeof L !== 'undefined');
                        console.error('[SIMS] L.markerClusterGroup defined:', typeof L !== 'undefined' && typeof L.markerClusterGroup !== 'undefined');
                        console.error('[SIMS] markerClusterLoaded flag:', window.markerClusterLoaded);
                        return;
                    }}
                    console.log('[SIMS] Waiting for MarkerCluster library... (attempt ' + attempts + '/' + maxAttempts + ')');
                    setTimeout(initializeClusterGroup, 100);
                    return;
                }}

                console.log('[SIMS] MarkerCluster library loaded, initializing cluster group');

                // Ensure map has maxZoom set before creating cluster group
                var map = getElement({m.id}).map;
                if (!map.options.maxZoom) {{
                    map.options.maxZoom = 19;
                    console.log('[SIMS] Set map.options.maxZoom to 19 before cluster initialization');
                }} else {{
                    console.log('[SIMS] Map maxZoom already set to', map.options.maxZoom);
                }}

                // Create marker cluster group with custom options
                var incidentClusterGroup = L.markerClusterGroup({{
                    showCoverageOnHover: true,
                    zoomToBoundsOnClick: true,
                    spiderfyOnMaxZoom: true,
                    removeOutsideVisibleBounds: true,
                    maxClusterRadius: 60,
                    iconCreateFunction: function(cluster) {{
                        var count = cluster.getChildCount();
                        var size = count < 10 ? 'small' : count < 50 ? 'medium' : 'large';

                        return L.divIcon({{
                            html: '<div style="background: rgba(255, 68, 68, 0.8); width: ' + (size === 'small' ? '30px' : size === 'medium' ? '40px' : '50px') + '; height: ' + (size === 'small' ? '30px' : size === 'medium' ? '40px' : '50px') + '; border: 3px solid #fff; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: ' + (size === 'small' ? '12px' : size === 'medium' ? '14px' : '16px') + '; color: white; font-weight: bold; box-shadow: 0 0 10px rgba(255, 68, 68, 0.6); cursor: pointer;"><span>' + count + '</span></div>',
                            className: 'custom-cluster-icon',
                            iconSize: L.point(size === 'small' ? 30 : size === 'medium' ? 40 : 50, size === 'small' ? 30 : size === 'medium' ? 40 : 50)
                        }});
                    }}
                }});

                // Store reference globally so we can add markers to it
                window.incidentClusterGroup = incidentClusterGroup;

                // Add cluster group to map (map variable already defined above)
                incidentClusterGroup.addTo(map);

                console.log('[SIMS] Cluster group initialized and added to map');
            }}

            // Start initialization
            initializeClusterGroup();
        }})();
    '''
    ui.run_javascript(cluster_init_js)

    # Add custom square markers for each incident using JavaScript
    valid_marker_count = 0
    for incident in incidents:
        lat = incident['location']['lat']
        lon = incident['location']['lon']

        # Skip incidents without valid coordinates
        if lat is None or lon is None:
            logger.warning(f"Skipping incident {incident['id']} - no valid coordinates")
            continue

        priority = incident['priority']
        color = colors.get(priority, '#63ABFF')
        inc_id = incident['id']
        valid_marker_count += 1

        logger.info(f"Creating marker for incident {inc_id} at ({lat}, {lon}) with priority {priority}")

        # Escape strings for JavaScript
        inc_id_escaped = inc_id.replace("'", "\\'")
        inc_desc = incident['description'].replace("'", "\\'")

        # Create click handler for this specific marker
        async def create_marker_handler(inc_id=inc_id):
            async def marker_click_handler():
                # Load organizations
                organizations = await load_organizations()
                if not organizations:
                    ui.notify(i18n.t('ui.responder.no_incidents_assigned'), type='warning')
                    return

                with ui.dialog() as dialog, ui.card().classes('w-full max-w-6xl p-4 sm:p-6'):
                    ui.label(f'Forward Incident {inc_id}').classes('text-base sm:text-lg font-bold mb-3 sm:mb-4 title-font')

                    ui.label('Select Organization:').classes('text-subtitle2 mb-4')

                    # Organization table
                    org_columns = [
                        {'name': 'name', 'label': i18n.t('ui.table.name'), 'field': 'name', 'align': 'left', 'sortable': True},
                        {'name': 'type', 'label': i18n.t('ui.table.type'), 'field': 'type', 'align': 'left', 'sortable': True},
                        {'name': 'city', 'label': i18n.t('ui.table.city'), 'field': 'city', 'align': 'left', 'sortable': True},
                        {'name': 'emergency_phone', 'label': i18n.t('ui.table.emergency_phone'), 'field': 'emergency_phone', 'align': 'left'},
                        {'name': 'action', 'label': i18n.t('ui.table.action'), 'field': 'action', 'align': 'center'},
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
                            result = await assign_incident_to_org(
                                inc_id,
                                org_id,
                                None
                            )
                            if result['success']:
                                ui.notify(f'Incident {inc_id} assigned to {org["name"]}', type='positive')
                                dialog.close()
                            else:
                                ui.notify(result['message'], type='warning')

                    org_table.on('assign', on_assign)

                    ui.separator().classes('my-4')

                    with ui.row().classes('w-full justify-end gap-2'):
                        ui.button('Cancel', on_click=dialog.close).props('flat')

                dialog.open()

            return marker_click_handler

        marker_handler = await create_marker_handler()
        marker_button = ui.button('', on_click=marker_handler).classes('hidden')
        marker_button_id = marker_button.id

        # Get translated label for Forward button
        forward_button_label = i18n.t('ui.buttons.forward')

        # Create marker with custom icon and popup with clickable button using JavaScript
        # Add to cluster group instead of directly to map
        # Wait for cluster group to be ready before adding marker
        js_code = f'''
            (function() {{
                var attempts = 0;
                var maxAttempts = 150; // 15 seconds max

                function addMarkerToCluster() {{
                    attempts++;

                    // Wait for cluster group to be ready
                    if (!window.incidentClusterGroup) {{
                        if (attempts >= maxAttempts) {{
                            console.error('[SIMS] Timeout waiting for cluster group for marker {inc_id}');
                            console.error('[SIMS] Cluster group exists:', !!window.incidentClusterGroup);
                            return;
                        }}
                        if (attempts % 20 === 0) {{
                            console.log('[SIMS] Still waiting for cluster group... (attempt ' + attempts + ')');
                        }}
                        setTimeout(addMarkerToCluster, 100);
                        return;
                    }}

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
                                     'onmouseout="this.style.background=\\'transparent\\'; this.style.borderColor=\\'white\\'; this.style.color=\\'white\\';">{forward_button_label}</button>' +
                                     '</div>';

                    marker.bindPopup(popupContent, {{
                        className: 'custom-popup-{priority}'
                    }});

                    // Add marker to cluster group
                    window.incidentClusterGroup.addLayer(marker);

                    // Store marker reference for dynamic updates
                    if (!window.incidentMarkers) {{
                        window.incidentMarkers = {{}};
                    }}
                    window.incidentMarkers['{inc_id}'] = marker;

                    console.log('[SIMS] Marker added for incident {inc_id} at ({lat}, {lon})');
                }}

                // Start adding marker
                addMarkerToCluster();
            }})();
        '''

        ui.run_javascript(js_code)

    logger.info(f"Created {valid_marker_count} incident markers")

    # Auto-zoom to fit all incidents only if geolocation is unavailable
    # Store auto-zoom function globally so it can be called if geolocation fails
    if any(inc['location']['lat'] is not None and inc['location']['lon'] is not None for inc in incidents):
        auto_zoom_js = f'''
            (function() {{
                // Store auto-zoom function globally
                window.autoZoomToIncidents = function() {{
                    // Wait for cluster group to be ready
                    if (!window.incidentClusterGroup) {{
                        console.log('[SIMS] Waiting for cluster group before auto-zoom...');
                        setTimeout(window.autoZoomToIncidents, 200);
                        return;
                    }}

                    // Wait a bit more for all markers to be added (async)
                    setTimeout(function() {{
                        var bounds = window.incidentClusterGroup.getBounds();
                        if (bounds.isValid()) {{
                            console.log('[SIMS] Auto-zooming to fit all incidents (geolocation unavailable)');
                            var map = getElement({m.id}).map;
                            map.fitBounds(bounds, {{
                                padding: [50, 50],
                                maxZoom: 15
                            }});

                            // Force tile layer redraw after zoom to prevent stale tiles
                            setTimeout(function() {{
                                console.log('[SIMS] Redrawing tiles after zoom');
                                map.eachLayer(function(layer) {{
                                    if (layer instanceof L.TileLayer) {{
                                        layer.redraw();
                                    }}
                                }});
                            }}, 300);
                        }} else {{
                            console.log('[SIMS] No valid bounds for auto-zoom');
                        }}
                    }}, 500);
                }};

                console.log('[SIMS] Auto-zoom function ready (will trigger if geolocation fails)');
            }})();
        '''
        ui.run_javascript(auto_zoom_js)
        logger.info("Auto-zoom function prepared")


async def render_stats(incidents: List[Dict]):
    """Render stats column"""
    total_incidents = len(incidents)
    critical_count = sum(1 for i in incidents if i['priority'] == 'critical')
    high_count = sum(1 for i in incidents if i['priority'] in ['critical', 'high'])
    active_count = sum(1 for i in incidents if i['status'] == 'active')

    with ui.element('div').classes('stats-column'):
        with ui.element('div').classes('metric-card'):
            ui.label(i18n.t('ui.dashboard.active_incidents')).classes('metric-label')
            ui.label(str(active_count)).classes('metric-value')

        with ui.element('div').classes('metric-card'):
            ui.label(i18n.t('ui.dashboard.high_priority')).classes('metric-label')
            ui.label(str(high_count)).classes('metric-value')

        with ui.element('div').classes('metric-card'):
            ui.label(i18n.t('ui.dashboard.total_reports')).classes('metric-label')
            ui.label(str(total_incidents)).classes('metric-value')

        with ui.element('div').classes('metric-card'):
            ui.label(i18n.t('ui.dashboard.avg_response')).classes('metric-label')
            ui.label('2.3m').classes('metric-value')


async def render_overview(incidents: List[Dict], container=None):
    """Render overview section with map and stats"""
    target = container if container else ui.element('div').classes('overview-section w-full')
    with target:
        await render_map(incidents)
        await render_stats(incidents)
    return target


async def render_incident_table(incidents: List[Dict], is_mock_data: bool = False):
    """Render the incident table with filters"""
    # Section title in container
    with ui.element('div').classes('content-container'):
        ui.label(i18n.t('ui.dashboard.active_incidents')).classes('section-title w-full')

    # Filter state
    filter_state = {
        'priority': None,
        'category': None,
        'status': None,
        'assigned_org': None
    }

    # Get unique values for filters
    priorities = sorted(list(set(inc['priority'] for inc in incidents)))
    categories = sorted(list(set(inc.get('category', 'Unclassified') for inc in incidents)))
    statuses = sorted(list(set(inc['status'] for inc in incidents)))
    organizations = sorted(list(set(
        inc['assigned_org']['name'] for inc in incidents
        if inc.get('assigned_org')
    )))

    def apply_filters(incidents_list):
        """Apply current filters to incidents list"""
        filtered = incidents_list

        if filter_state['priority']:
            filtered = [inc for inc in filtered if inc['priority'] == filter_state['priority']]

        if filter_state['category']:
            filtered = [inc for inc in filtered if inc.get('category', 'Unclassified') == filter_state['category']]

        if filter_state['status']:
            filtered = [inc for inc in filtered if inc['status'] == filter_state['status']]

        if filter_state['assigned_org']:
            filtered = [inc for inc in filtered
                       if inc.get('assigned_org') and inc['assigned_org']['name'] == filter_state['assigned_org']]

        return filtered

    # Filter section - minimal, no styling
    with ui.row().classes('gap-2 items-center mb-2'):
        search_input = ui.input(
            placeholder=i18n.t('ui.dashboard.search')
        ).classes('w-64').props('dense dark clearable borderless')

        priority_filter = ui.select(
            options=['All'] + priorities,
            value='All',
            label='Priority'
        ).classes('w-32').props('dense dark clearable borderless')

        category_filter = ui.select(
            options=['All'] + categories,
            value='All',
            label='Category'
        ).classes('w-44').props('dense dark clearable borderless')

        status_filter = ui.select(
            options=['All'] + statuses,
            value='All',
            label='Status'
        ).classes('w-32').props('dense dark clearable borderless')

        org_filter = ui.select(
            options=['All'] + organizations,
            value='All',
            label='Organization'
        ).classes('w-48').props('dense dark clearable borderless')

        clear_btn = ui.button(
            'Clear',
            on_click=lambda: clear_filters()
        ).props('flat dense')

    # Table section - full width
    with ui.element('div').classes('table-section w-full'):

        # Table columns - all sortable
        columns = [
            {'name': 'id', 'label': i18n.t('ui.table.id'), 'field': 'id', 'align': 'left', 'sortable': True},
            {'name': 'timestamp', 'label': i18n.t('ui.table.created'), 'field': 'timestamp', 'align': 'left', 'sortable': True},
            {'name': 'category', 'label': i18n.t('ui.table.category'), 'field': 'category', 'align': 'left', 'sortable': True},
            {'name': 'location', 'label': i18n.t('ui.table.location'), 'field': 'location', 'align': 'left', 'sortable': True},
            {'name': 'description', 'label': i18n.t('ui.table.description'), 'field': 'description', 'align': 'left', 'sortable': True, 'style': 'max-width: 300px; white-space: normal; word-wrap: break-word;'},
            {'name': 'priority', 'label': i18n.t('ui.table.priority'), 'field': 'priority', 'align': 'left', 'sortable': True},
            {'name': 'assigned_to', 'label': i18n.t('ui.table.assigned_to'), 'field': 'assigned_to', 'align': 'left', 'sortable': False},
            {'name': 'responder_url', 'label': i18n.t('ui.table.responder_link'), 'field': 'responder_url', 'align': 'center'},
            {'name': 'action', 'label': i18n.t('ui.table.actions'), 'field': 'action', 'align': 'center'},
        ]

        # Format incidents for table
        rows = []
        for incident in incidents:
            # Format assigned organization for display
            assigned_org_display = None
            if incident.get('assigned_org'):
                assigned_org_display = {
                    'id': incident['assigned_org']['id'],
                    'name': incident['assigned_org']['name'],
                    'is_auto': incident.get('is_auto_assigned', False)
                }

            rows.append({
                'id': incident['id'],
                'uuid': incident.get('uuid', incident.get('id', 'UNKNOWN')),
                'timestamp': incident['timestamp'],  # Full timestamp with date
                'category': incident.get('category', 'Unclassified'),
                'category_raw': incident.get('category_raw', 'unclassified'),
                'location': incident['location']['label'],
                'description': incident['description'],
                'priority': incident['priority'],
                'assigned_org': assigned_org_display,
                'action': incident['id'],
                'reporter': incident.get('reporter', 'Unknown'),
                'status': incident.get('status', 'active'),
                'type': incident.get('type', 'Incident'),
                'imageUrl': incident.get('imageUrl'),
                'audioUrl': incident.get('audioUrl'),
                'videoUrl': incident.get('videoUrl'),
                'audioTranscript': incident.get('audioTranscript'),
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
        # Get translated labels
        forward_label = i18n.t('ui.buttons.forward')
        close_label = i18n.t('ui.buttons.close')
        full_timestamp_label = i18n.t('ui.table.full_timestamp')
        full_description_label = i18n.t('ui.table.full_description')
        reporter_label = i18n.t('ui.table.reporter')

        table.add_slot('body', f'''
            <q-tr :props="props" @click="props.expand = !props.expand" style="cursor: pointer">
                <q-td auto-width>
                    <q-btn size="sm" color="white" round dense flat
                        @click.stop="props.expand = !props.expand"
                        :icon="props.expand ? 'remove' : 'add'" />
                </q-td>
                <q-td key="id" :props="props">
                    <span class="cell-id">{{{{ props.row.id }}}}</span>
                </q-td>
                <q-td key="timestamp" :props="props">
                    <span class="cell-time">{{{{ props.row.timestamp }}}}</span>
                </q-td>
                <q-td key="category" :props="props">
                    <span class="cell-category" style="font-size: 13px; color: #a0aec0;">
                        {{{{ props.row.category }}}}
                    </span>
                </q-td>
                <q-td key="location" :props="props">
                    <span class="cell-location">{{{{ props.row.location }}}}</span>
                </q-td>
                <q-td key="description" :props="props">
                    <div class="flex items-center gap-2">
                        <q-spinner-dots v-if="props.row.description && props.row.description.toLowerCase().includes('processing')" color="white" size="20px" />
                        <span class="cell-description">{{{{ props.row.description }}}}</span>
                        <div class="flex gap-1" v-if="props.row.imageUrl || props.row.audioUrl || props.row.videoUrl">
                            <a v-if="props.row.imageUrl" :href="props.row.imageUrl" target="_blank" class="text-blue-400 hover:text-blue-300" title="View Image">
                                <q-icon name="image" size="16px" />
                            </a>
                            <a v-if="props.row.audioUrl" :href="props.row.audioUrl" target="_blank" class="text-green-400 hover:text-green-300" title="Listen to Audio">
                                <q-icon name="audiotrack" size="16px" />
                            </a>
                            <a v-if="props.row.videoUrl" :href="props.row.videoUrl" target="_blank" class="text-purple-400 hover:text-purple-300" title="Watch Video">
                                <q-icon name="videocam" size="16px" />
                            </a>
                        </div>
                    </div>
                </q-td>
                <q-td key="priority" :props="props">
                    <span :class="'priority-badge priority-' + props.row.priority">
                        {{{{ props.row.priority.toUpperCase() }}}}
                    </span>
                </q-td>
                <q-td key="assigned_to" :props="props" @click.stop>
                    <q-chip
                        v-if="props.row.assigned_org"
                        :label="props.row.assigned_org.name"
                        :icon="props.row.assigned_org.is_auto ? 'auto_awesome' : 'business'"
                        color="primary"
                        text-color="white"
                        size="sm"
                        removable
                        @remove="$parent.$emit('remove_assignment', props.row.id)"
                        style="font-size: 12px;"
                    />
                    <span v-else style="color: #718096; font-size: 12px;">Unassigned</span>
                </q-td>
                <q-td key="responder_url" :props="props" @click.stop>
                    <q-btn
                        v-if="props.row.assigned_org && props.row.assigned_org.id"
                        outline
                        dense
                        size="sm"
                        label="Get Link"
                        color="teal"
                        no-caps
                        style="font-size: 12px; padding: 3px 10px"
                        @click="$parent.$emit('generate_responder_link', props.row.uuid, props.row.assigned_org.id)"
                    >
                        <q-tooltip>Generate responder portal link</q-tooltip>
                    </q-btn>
                    <span v-else style="color: #718096; font-size: 11px;">No org</span>
                </q-td>
                <q-td key="action" :props="props" @click.stop>
                    <div class="flex gap-2">
                        <q-btn
                            outline
                            dense
                            size="sm"
                            label="{forward_label}"
                            color="white"
                            no-caps
                            style="font-size: 13px; padding: 4px 12px"
                            @click="$parent.$emit('forward', props.row.id)"
                        />
                        <q-btn
                            outline
                            dense
                            size="sm"
                            label="{close_label}"
                            color="red"
                            no-caps
                            style="font-size: 13px; padding: 4px 12px"
                            @click="$parent.$emit('close', props.row.id)"
                        />
                    </div>
                </q-td>
            </q-tr>
            <q-tr v-show="props.expand" :props="props">
                <q-td colspan="100%">
                    <div class="p-3 sm:p-4">
                        <div class="grid grid-cols-1 sm:grid-cols-2 gap-3 sm:gap-4">
                            <div>
                                <div class="text-xs text-gray-400 uppercase mb-1">Incident ID</div>
                                <div class="text-white font-bold">{{{{ props.row.id }}}}</div>
                            </div>
                            <div>
                                <div class="text-xs text-gray-400 uppercase mb-1">Status</div>
                                <div class="text-white">{{{{ props.row.status || 'active' }}}}</div>
                            </div>
                            <div>
                                <div class="text-xs text-gray-400 uppercase mb-1">{full_timestamp_label}</div>
                                <div class="text-white">{{{{ props.row.timestamp }}}}</div>
                            </div>
                            <div>
                                <div class="text-xs text-gray-400 uppercase mb-1">Type</div>
                                <div class="text-white">{{{{ props.row.type }}}}</div>
                            </div>
                            <div class="col-span-1 sm:col-span-2">
                                <div class="text-xs text-gray-400 uppercase mb-1">{full_description_label}</div>
                                <div class="text-white">{{{{ props.row.description }}}}</div>
                            </div>
                            <div class="col-span-1 sm:col-span-2">
                                <div class="text-xs text-gray-400 uppercase mb-1">{reporter_label}</div>
                                <div class="text-white">{{{{ props.row.reporter || 'Unknown' }}}}</div>
                            </div>

                            <!-- MEDIA FILES SECTION - ALWAYS VISIBLE -->
                            <div class="col-span-1 sm:col-span-2" style="border-top: 2px solid #FF4444; padding-top: 16px; margin-top: 16px;">
                                <div class="text-sm text-white font-bold mb-3" style="color: #FF4444;">MEDIA FILES</div>

                                <!-- Image -->
                                <div class="mb-4">
                                    <div class="text-xs text-gray-400 uppercase mb-1">Image</div>
                                    <div v-if="props.row.imageUrl">
                                        <div class="text-xs text-gray-500 mb-2">URL: {{{{ props.row.imageUrl }}}}</div>
                                        <img :src="props.row.imageUrl" class="max-w-full h-auto rounded border-2 border-gray-700" style="max-height: 400px;" />
                                    </div>
                                    <div v-else class="text-gray-500 italic text-xs">No image attached</div>
                                </div>

                                <!-- Audio -->
                                <div class="mb-4">
                                    <div class="text-xs text-gray-400 uppercase mb-1">Audio</div>
                                    <div v-if="props.row.audioUrl">
                                        <div class="text-xs text-gray-500 mb-2">URL: {{{{ props.row.audioUrl }}}}</div>
                                        <audio controls class="w-full">
                                            <source :src="props.row.audioUrl" />
                                        </audio>
                                    </div>
                                    <div v-else class="text-gray-500 italic text-xs">No audio attached</div>
                                </div>

                                <!-- Video -->
                                <div class="mb-4">
                                    <div class="text-xs text-gray-400 uppercase mb-1">Video</div>
                                    <div v-if="props.row.videoUrl">
                                        <div class="text-xs text-gray-500 mb-2">URL: {{{{ props.row.videoUrl }}}}</div>
                                        <video controls class="max-w-full h-auto rounded border-2 border-gray-700" style="max-height: 400px;">
                                            <source :src="props.row.videoUrl" />
                                        </video>
                                    </div>
                                    <div v-else class="text-gray-500 italic text-xs">No video attached</div>
                                </div>

                                <!-- Media Analysis/Transcription -->
                                <div class="mb-4">
                                    <div class="text-xs text-gray-400 uppercase mb-1">Media Analysis / Transcription</div>
                                    <div v-if="props.row.audioTranscript" class="text-white bg-gray-800 p-3 rounded border-2 border-green-700" style="font-family: monospace; font-size: 12px; white-space: pre-wrap;">{{{{ props.row.audioTranscript }}}}</div>
                                    <div v-else-if="props.row.status === 'processing'" class="text-blue-400 italic text-xs">
                                        <i class="fas fa-spinner fa-spin mr-2"></i>Processing audio and analyzing media...
                                    </div>
                                    <div v-else>
                                        <div class="text-yellow-400 italic text-xs mb-2">No analysis available yet</div>
                                        <div class="text-xs text-gray-500">Run: POST /api/incident/{{{{ props.row.id }}}}/analyze-all-media</div>
                                    </div>
                                </div>

                                <!-- Processing Status Indicator -->
                                <div v-if="props.row.status === 'processing'" class="mb-4 p-3 bg-blue-900 border-2 border-blue-500 rounded">
                                    <div class="flex items-center">
                                        <i class="fas fa-cog fa-spin mr-2 text-blue-400"></i>
                                        <div class="text-blue-300 text-sm font-semibold">AI Processing in Progress</div>
                                    </div>
                                    <div class="text-xs text-blue-400 mt-1">Transcribing audio, analyzing media, and classifying incident...</div>
                                </div>
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
                ui.notify(i18n.t('ui.responder.no_incidents_assigned'), type='warning')
                return

            with ui.dialog() as dialog, ui.card().classes('w-full max-w-6xl p-4 sm:p-6'):
                ui.label(f'Forward Incident {incident_id}').classes('text-base sm:text-lg font-bold mb-3 sm:mb-4 title-font')

                ui.label('Select Organization:').classes('text-xs sm:text-sm mb-3 sm:mb-4')

                # Organization table
                org_columns = [
                    {'name': 'name', 'label': i18n.t('ui.table.name'), 'field': 'name', 'align': 'left', 'sortable': True},
                    {'name': 'type', 'label': i18n.t('ui.table.type'), 'field': 'type', 'align': 'left', 'sortable': True},
                    {'name': 'city', 'label': i18n.t('ui.table.city'), 'field': 'city', 'align': 'left', 'sortable': True},
                    {'name': 'emergency_phone', 'label': i18n.t('ui.table.emergency_phone'), 'field': 'emergency_phone', 'align': 'left'},
                    {'name': 'action', 'label': i18n.t('ui.table.action'), 'field': 'action', 'align': 'center'},
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
                        result = await assign_incident_to_org(
                            incident_id,
                            org_id,
                            None
                        )
                        if result['success']:
                            ui.notify(f'Incident {incident_id} assigned to {org["name"]}', type='positive')
                            dialog.close()
                        else:
                            ui.notify(result['message'], type='warning')

                org_table.on('assign', on_assign)

                ui.separator().classes('my-4')

                with ui.row().classes('w-full justify-end gap-2'):
                    ui.button('Cancel', on_click=dialog.close).props('flat')

            dialog.open()

        # Handle remove assignment action
        async def handle_remove_assignment(e):
            incident_id = e.args
            # Call API to remove assignment
            try:
                async with httpx.AsyncClient() as client:
                    response = await client.delete(
                        f"{API_BASE}/incident/{incident_id}/assignment"
                    )
                    if response.status_code == 200:
                        ui.notify(i18n.t('ui.dashboard.unassigned_success'), type='positive')
                        # Refresh the dashboard to show updated data
                        logger.info(f"Assignment removed from {incident_id}, refreshing...")
                    else:
                        logger.error(f"Failed to remove assignment: {response.status_code}")
                        ui.notify(i18n.t('ui.dashboard.unassignment_failed'), type='negative')
            except Exception as error:
                logger.error(f"Error removing assignment: {error}", exc_info=True)
                ui.notify(f'Error: {str(error)}', type='negative')

        # Handle close incident action
        async def handle_close_incident(e):
            incident_id = e.args
            try:
                async with httpx.AsyncClient() as client:
                    response = await client.put(
                        f"{API_BASE}/incident/{incident_id}",
                        json={'status': 'closed'}
                    )
                    if response.status_code == 200:
                        ui.notify(i18n.t('ui.dashboard.closed_success'), type='positive')
                        # Refresh the page to remove closed incident from table
                        ui.navigate.reload()
                    else:
                        logger.error(f"Failed to close incident: {response.status_code}")
                        ui.notify(i18n.t('ui.dashboard.close_failed'), type='negative')
            except Exception as error:
                logger.error(f"Error closing incident: {error}", exc_info=True)
                ui.notify(f'Error: {str(error)}', type='negative')

        # Handle generate responder link
        async def handle_generate_responder_link(e):
            incident_id, org_id = e.args
            try:
                # Check if org_id is valid
                if org_id is None:
                    ui.notify(i18n.t('ui.dashboard.no_org_assigned'), type='warning')
                    return

                from config import Config
                async with httpx.AsyncClient() as client:
                    response = await client.post(
                        f"{API_BASE}/organization/{org_id}/token",
                        json={
                            "organization_id": org_id,
                            "created_by": "dashboard",
                            "expires_at": None
                        }
                    )
                    if response.status_code == 201:
                        data = response.json()
                        token = data.get('token')
                        base_url = Config.PUBLIC_SERVER_URL or 'http://localhost:8000'
                        responder_url = f"{base_url}/responder/incidents/{incident_id}?token={token}"

                        with ui.dialog() as dialog, ui.card().classes('p-4'):
                            ui.label('Responder Portal Link').classes('text-lg font-bold mb-4')
                            ui.label(f'Incident: {incident_id}').classes('text-sm text-gray-400 mb-2')

                            with ui.row().classes('gap-2 w-full items-center'):
                                url_input = ui.input('URL', value=responder_url).classes('flex-1').props('readonly')
                                ui.button('Copy URL', on_click=lambda: ui.run_javascript(f'navigator.clipboard.writeText("{responder_url}")'))

                            with ui.row().classes('gap-2 mt-4'):
                                ui.button(i18n.t('ui.buttons.close'), on_click=dialog.close)

                        dialog.open()
                    else:
                        error_detail = response.text if response else 'Unknown error'
                        logger.error(f"Failed to generate token: {response.status_code} - {error_detail}")
                        ui.notify(i18n.t('ui.dashboard.token_failed'), type='negative')
            except Exception as error:
                logger.error(f"Error generating responder link: {error}", exc_info=True)
                ui.notify(f'Error: {str(error)}', type='negative')

        table.on('forward', handle_forward)
        table.on('remove_assignment', handle_remove_assignment)
        table.on('close', handle_close_incident)
        table.on('generate_responder_link', handle_generate_responder_link)

    # Store references for filter updates
    filter_refs = {
        'overview_container': None,
        'incidents': incidents
    }

    # Define filter update handlers
    async def update_filters():
        """Update table and map when filters change"""
        # Update filter state
        filter_state['priority'] = None if priority_filter.value == 'All' else priority_filter.value
        filter_state['category'] = None if category_filter.value == 'All' else category_filter.value
        filter_state['status'] = None if status_filter.value == 'All' else status_filter.value
        filter_state['assigned_org'] = None if org_filter.value == 'All' else org_filter.value

        # Apply filters
        filtered_incidents = apply_filters(filter_refs['incidents'])

        # Apply text search if provided
        search_text = (search_input.value or '').lower().strip()
        if search_text:
            filtered_incidents = [
                inc for inc in filtered_incidents
                if search_text in inc['id'].lower()
                or search_text in inc['description'].lower()
                or search_text in inc.get('category', '').lower()
                or search_text in inc['location']['label'].lower()
            ]

        # Update table rows
        new_rows = []
        for incident in filtered_incidents:
            assigned_org_display = None
            if incident.get('assigned_org'):
                assigned_org_display = {
                    'id': incident['assigned_org']['id'],
                    'name': incident['assigned_org']['name'],
                    'is_auto': incident.get('is_auto_assigned', False)
                }

            new_rows.append({
                'id': incident['id'],
                'timestamp': incident['timestamp'].split(' ')[1],
                'category': incident.get('category', 'Unclassified'),
                'category_raw': incident.get('category_raw', 'unclassified'),
                'location': incident['location']['label'],
                'description': incident['description'],
                'priority': incident['priority'],
                'assigned_org': assigned_org_display,
                'action': incident['id'],
                'reporter': incident.get('reporter', 'Unknown'),
                'status': incident.get('status', 'active'),
                'type': incident.get('type', 'Incident'),
                'imageUrl': incident.get('imageUrl'),
                'audioUrl': incident.get('audioUrl'),
                'videoUrl': incident.get('videoUrl'),
                'audioTranscript': incident.get('audioTranscript'),
            })

        table.rows = new_rows
        table.update()

        # Update map with filtered incidents
        if filter_refs['overview_container']:
            filter_refs['overview_container'].clear()
            with filter_refs['overview_container']:
                await render_overview(filtered_incidents)

        ui.notify(i18n.t('ui.filter.showing_of', count=len(filtered_incidents), total=len(filter_refs["incidents"])), type='info')

    def clear_filters():
        """Clear all filters and show all incidents"""
        search_input.value = ''
        priority_filter.value = 'All'
        category_filter.value = 'All'
        status_filter.value = 'All'
        org_filter.value = 'All'
        update_filters()

    # Connect filter change handlers
    search_input.on('update:model-value', lambda: update_filters())
    priority_filter.on('update:model-value', lambda: update_filters())
    category_filter.on('update:model-value', lambda: update_filters())
    status_filter.on('update:model-value', lambda: update_filters())
    org_filter.on('update:model-value', lambda: update_filters())

    # Return filter refs and table object so dashboard can update rows
    filter_refs['table'] = table
    filter_refs['incidents'] = incidents
    return filter_refs


async def dashboard():
    """Main dashboard page"""
    # Load initial incidents from API
    all_incidents = await load_incidents()

    # Filter out closed incidents
    incidents = [inc for inc in all_incidents if inc.get('status', 'open') != 'closed']
    logger.info(f"Filtered {len(all_incidents) - len(incidents)} closed incidents from display")

    # Use mock data as fallback if API fails
    is_mock_data = False
    if not incidents:
        logger.warning("No incidents loaded from API, using mock data as fallback")
        incidents = MOCK_INCIDENTS
        is_mock_data = True

    # Create containers that will be updated
    with ui.element('div').classes('w-full') as overview_container:
        await render_overview(incidents)

    with ui.element('div').classes('w-full') as table_container:
        filter_refs = await render_incident_table(incidents, is_mock_data=is_mock_data)
        # Set overview container reference for filter updates
        filter_refs['overview_container'] = overview_container

    # Define refresh function that updates table rows in place
    async def refresh_dashboard():
        """Reload incidents and update table rows without re-rendering"""
        nonlocal incidents, is_mock_data, filter_refs
        logger.info("Refreshing dashboard table rows in place")

        # Reload incidents from API
        all_new_incidents = await load_incidents()
        if all_new_incidents:
            # Filter out closed incidents
            incidents = [inc for inc in all_new_incidents if inc.get('status', 'open') != 'closed']
            logger.info(f"Filtered {len(all_new_incidents) - len(incidents)} closed incidents from refresh")
            is_mock_data = False
        else:
            # Still no incidents, keep using mock data
            incidents = MOCK_INCIDENTS
            is_mock_data = True

        # Update table rows in place without re-rendering (preserves expanded state)
        if 'table' in filter_refs:
            table = filter_refs['table']

            # Build new rows from updated incidents
            new_rows = []
            for incident in incidents:
                assigned_org_display = 'Unassigned'
                if incident.get('assigned_org'):
                    assigned_org_display = incident['assigned_org']['name']

                new_rows.append({
                    'id': incident['id'],
                    'uuid': incident.get('uuid', incident['id']),
                    'timestamp': incident['timestamp'],
                    'location': incident['location']['label'],
                    'latitude': incident['location']['lat'],
                    'longitude': incident['location']['lon'],
                    'description': incident['description'],
                    'priority': incident['priority'].upper(),
                    'assigned_org': assigned_org_display,
                    'action': incident['id'],
                    'reporter': incident.get('reporter', 'Unknown'),
                    'status': incident.get('status', 'active'),
                    'type': incident.get('type', 'Incident'),
                    'imageUrl': incident.get('imageUrl'),
                    'audioUrl': incident.get('audioUrl'),
                    'videoUrl': incident.get('videoUrl'),
                    'audioTranscript': incident.get('audioTranscript'),
                })

            # Update rows and refresh table
            table.rows = new_rows
            table.update()
            filter_refs['incidents'] = incidents
            logger.info(f"Updated {len(new_rows)} rows in table without re-rendering")

    # Create a button that can be triggered from JavaScript (hidden)
    refresh_button = ui.button('', on_click=refresh_dashboard).classes('hidden')

    # WebSocket client JavaScript code with dynamic marker handling
    ws_code = f'''
    (function() {{
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = protocol + '//' + window.location.host + '/ws/incidents';
        let ws = null;
        let reconnectAttempts = 0;
        const maxReconnectAttempts = 10;
        let heartbeatInterval = null;

        // Colors for different priorities
        const priorityColors = {{
            'critical': '#FF4444',
            'high': '#ffa600',
            'medium': '#63ABFF',
            'low': 'rgba(255, 255, 255, 0.4)'
        }};

        // Function to add or update a marker
        function addOrUpdateMarker(incident) {{
            // Initialize marker tracking if not exists
            if (!window.incidentMarkers) {{
                window.incidentMarkers = {{}};
                console.log('[SIMS] Initialized window.incidentMarkers');
            }}

            console.log('[SIMS] addOrUpdateMarker called with:', incident);

            // Handle different field name formats
            const lat = incident.latitude || incident.lat;
            const lon = incident.longitude || incident.lon || incident.lng;
            const incidentId = incident.incidentId || incident.incident_id || incident.id;
            const priority = incident.priority || 'medium';
            const status = incident.status || 'open';

            console.log('[SIMS] Extracted values - ID:', incidentId, 'Lat:', lat, 'Lon:', lon, 'Status:', status);

            // Skip if no coordinates or if closed
            if (!lat || !lon || status === 'closed') {{
                // Remove marker if it exists
                if (window.incidentMarkers[incidentId]) {{
                    removeMarker(incidentId);
                }}
                return;
            }}

            // Remove existing marker if updating
            if (window.incidentMarkers[incidentId]) {{
                removeMarker(incidentId);
            }}

            const color = priorityColors[priority] || '#63ABFF';
            const icon = L.divIcon({{
                html: '<div style="background: ' + color + '; width: 12px; height: 12px; border: 2px solid #fff; box-shadow: 0 0 8px ' + color + '; cursor: pointer;"></div>',
                className: 'custom-marker',
                iconSize: [16, 16],
                iconAnchor: [8, 8]
            }});

            const marker = L.marker([lat, lon], {{ icon: icon }});

            const description = incident.description || 'No description';
            const popupContent = '<div class="popup-title">' + incidentId + '</div>' +
                               '<div class="popup-description">' + description + '</div>' +
                               '<div class="popup-priority priority-' + priority + '">' + priority.toUpperCase() + '</div>';

            marker.bindPopup(popupContent, {{
                className: 'custom-popup-' + priority
            }});

            // Add to cluster group
            if (window.incidentClusterGroup) {{
                window.incidentClusterGroup.addLayer(marker);
                window.incidentMarkers[incidentId] = marker;
                console.log('[SIMS] Added/updated marker for incident', incidentId);
            }}
        }}

        // Function to remove a marker
        function removeMarker(incidentId) {{
            // Initialize marker tracking if not exists
            if (!window.incidentMarkers) {{
                window.incidentMarkers = {{}};
                console.log('[SIMS] Initialized window.incidentMarkers in removeMarker');
            }}

            const marker = window.incidentMarkers[incidentId];
            if (marker && window.incidentClusterGroup) {{
                window.incidentClusterGroup.removeLayer(marker);
                delete window.incidentMarkers[incidentId];
                console.log('[SIMS] Removed marker for incident', incidentId);
            }}
        }}

        function updateSystemStatus(message, isConnected) {{
            const statusText = document.getElementById('system-status-text');
            const statusTimestamp = document.getElementById('system-status-timestamp');
            const statusDot = document.getElementById('system-status-dot');

            if (!statusText || !statusTimestamp || !statusDot) return;

            if (isConnected) {{
                const now = new Date();
                const timeStr = now.toLocaleTimeString('en-US', {{
                    hour12: false,
                    hour: '2-digit',
                    minute: '2-digit',
                    second: '2-digit'
                }});
                statusText.textContent = 'System Operational';
                statusTimestamp.textContent = timeStr;
                statusDot.className = 'w-2 h-2 bg-[#00FF00]';
            }} else {{
                statusText.textContent = 'System Disconnected';
                statusTimestamp.textContent = '--:--:--';
                statusDot.className = 'w-2 h-2 bg-[#FF4444]';
            }}
        }}

        function sendHeartbeat() {{
            if (ws && ws.readyState === WebSocket.OPEN) {{
                ws.send(JSON.stringify({{
                    type: 'ping'
                }}));
            }}
        }}

        function connect() {{
            console.log('[SIMS WebSocket] Connecting to:', wsUrl);
            ws = new WebSocket(wsUrl);

            ws.onopen = function() {{
                console.log('[SIMS WebSocket] Connected');
                reconnectAttempts = 0;
                updateSystemStatus(null, true);

                // Subscribe to incidents channel
                ws.send(JSON.stringify({{
                    type: 'subscribe',
                    channel: 'incidents'
                }}));

                // Start heartbeat - ping every second
                heartbeatInterval = setInterval(sendHeartbeat, 1000);
            }};

            ws.onmessage = function(event) {{
                try {{
                    const message = JSON.parse(event.data);
                    console.log('[SIMS WebSocket] Message received:', message);

                    // Update system status on any message
                    updateSystemStatus(message, true);

                    if (message.type === 'incident_new' ||
                        message.type === 'incident_update' ||
                        message.type === 'incident_assigned') {{

                        console.log('[SIMS WebSocket] Incident event:', message.type);

                        // Fetch fresh incident data and update map using current origin
                        const apiUrl = window.location.protocol + '//' + window.location.host + '/api/incident/?limit=100';
                        console.log('[SIMS] Fetching incidents from:', apiUrl);
                        fetch(apiUrl)
                            .then(response => response.json())
                            .then(incidents => {{
                                console.log('[SIMS] Fetched', incidents.length, 'incidents, updating map');

                                // Clear all existing incident markers
                                if (window.incidentClusterGroup) {{
                                    window.incidentClusterGroup.clearLayers();
                                    console.log('[SIMS] Cleared all incident markers');
                                }}

                                // Re-add markers for non-closed incidents
                                let addedCount = 0;
                                incidents.forEach(incident => {{
                                    const status = incident.status || 'open';
                                    if (status !== 'closed') {{
                                        addOrUpdateMarker(incident);
                                        addedCount++;
                                    }}
                                }});

                                console.log('[SIMS] Re-added', addedCount, 'incident markers');

                                // Force map refresh after updating markers
                                if (window.leafletMap) {{
                                    // Invalidate map size to force redraw
                                    window.leafletMap.invalidateSize();

                                    // Force tile layer redraw
                                    window.leafletMap.eachLayer(function(layer) {{
                                        if (layer instanceof L.TileLayer) {{
                                            layer.redraw();
                                        }}
                                    }});

                                    console.log('[SIMS] Map refreshed after marker update');
                                }}
                            }})
                            .catch(error => {{
                                console.error('[SIMS] Error fetching incidents:', error);
                            }});

                        // Refresh table
                        getElement({refresh_button.id}).click();
                    }}
                }} catch (error) {{
                    console.error('[SIMS WebSocket] Error processing message:', error);
                }}
            }};

            ws.onerror = function(error) {{
                console.error('[SIMS WebSocket] Error:', error);
                updateSystemStatus(null, false);
            }};

            ws.onclose = function() {{
                console.log('[SIMS WebSocket] Disconnected');
                updateSystemStatus(null, false);

                // Stop heartbeat
                if (heartbeatInterval) {{
                    clearInterval(heartbeatInterval);
                    heartbeatInterval = null;
                }}

                // Attempt to reconnect
                if (reconnectAttempts < maxReconnectAttempts) {{
                    reconnectAttempts++;
                    const delay = Math.min(1000 * Math.pow(2, reconnectAttempts), 30000);
                    console.log(`[SIMS WebSocket] Reconnecting in ${{delay}}ms (attempt ${{reconnectAttempts}})`);
                    setTimeout(connect, delay);
                }} else {{
                    console.error('[SIMS WebSocket] Max reconnect attempts reached');
                }}
            }};
        }}

        connect();

        // Cleanup on page unload
        window.addEventListener('beforeunload', function() {{
            if (heartbeatInterval) {{
                clearInterval(heartbeatInterval);
            }}
            if (ws && ws.readyState === WebSocket.OPEN) {{
                ws.close();
            }}
        }});
    }})();
    '''

    ui.run_javascript(ws_code)
