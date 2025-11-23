"""
SIMS Responder Portal
Organization responder interface for incident management
"""
import logging
from datetime import datetime
from typing import Optional
from uuid import UUID
import httpx

from nicegui import ui, app
from sqlalchemy.orm import Session

from db.connection import get_db, session
from theme_responder import frame as responder_frame, COLORS
from models.incident_model import IncidentORM
from models.organization_model import OrganizationORM
from services.chat_history import ChatHistory, get_session_by_incident
from i18n import i18n

logger = logging.getLogger(__name__)


@ui.page('/responder')
async def responder_login():
    """Token-based login page for organization responders"""

    # Apply theme but no frame - custom full-screen design
    from theme_responder import apply_theme, inject_custom_css
    apply_theme()
    inject_custom_css()
    ui.dark_mode(True)

    with ui.element('div').classes('w-full h-screen').style('background: linear-gradient(180deg, #0A1929 0%, #071220 100%);'):
        # Header bar
        with ui.element('div').classes('w-full border-b').style('border-color: rgba(255, 255, 255, 0.1); background: #071220;'):
            with ui.element('div').classes('max-w-7xl mx-auto px-6 py-4'):
                with ui.row().classes('items-center gap-4'):
                    ui.image('/static/sims-logo.svg').style('width: 120px; height: auto;')
                    ui.label(f'/ {i18n.t("ui.responder.title")}').classes('text-xl').style('font-family: "Stack Sans Notch"; color: rgba(255, 255, 255, 0.4);')

        # Main content
        with ui.element('div').classes('flex items-center justify-center').style('height: calc(100vh - 80px); padding: 16px;'):
            with ui.element('div').classes('w-full max-w-md'):
                # Access panel
                with ui.element('div').style('background: #0d1f2d; border: 1px solid rgba(38, 198, 218, 0.2); border-left: 3px solid #00897B;'):
                    # Header
                    with ui.element('div').classes('px-8 py-6 flex justify-center').style('border-bottom: 1px solid rgba(255, 255, 255, 0.1);'):
                        ui.image('/static/sims-icon.svg').style('width: 80px; height: auto;')

                    # Form
                    with ui.element('div').classes('p-8'):
                        ui.label(i18n.t('ui.responder.token_label')).classes('text-xs mb-2').style(
                            'color: rgba(255, 255, 255, 0.5); letter-spacing: 1px;'
                        )

                        token_input = ui.input(
                            placeholder=i18n.t('ui.responder.token_placeholder')
                        ).props('dark outlined dense').classes('w-full').style(
                            'border-color: rgba(38, 198, 218, 0.3);'
                        )

                        error_label = ui.label('').classes('text-sm mt-3').style('color: #FF4444;')
                        error_label.visible = False

                        async def validate_token():
                            token = token_input.value.strip()

                            if not token:
                                error_label.text = f'> {i18n.t("ui.responder.access_denied_token")}'
                                error_label.visible = True
                                return

                            error_label.visible = False

                            # Validate token by making API call
                            try:
                                async with httpx.AsyncClient() as client:
                                    response = await client.get(
                                        f'http://localhost:8000/api/responder/incidents?token={token}',
                                        timeout=10.0
                                    )

                                    if response.status_code == 200:
                                        # Token valid, redirect to incidents page
                                        ui.navigate.to(f'/responder/incidents?token={token}')
                                    else:
                                        error_label.text = f'> {i18n.t("ui.responder.access_denied_invalid")}'
                                        error_label.visible = True

                            except Exception as e:
                                logger.error(f"Error validating token: {e}")
                                error_label.text = f'> {i18n.t("ui.responder.connection_error")}'
                                error_label.visible = True

                        ui.button(i18n.t('ui.responder.access_portal'), on_click=validate_token).props(
                            'outline'
                        ).classes('w-full mt-6').style(
                            'border-color: #00897B; color: #00897B; font-family: "Stack Sans Notch"; letter-spacing: 1px; padding: 12px;'
                        )

                        # Info text
                        with ui.element('div').classes('mt-8 pt-6').style('border-top: 1px solid rgba(255, 255, 255, 0.1);'):
                            ui.label('Authorized organizations only').classes('text-xs').style(
                                'color: rgba(255, 255, 255, 0.4);'
                            )
                            ui.label('Contact your system administrator for access').classes('text-xs mt-1').style(
                                'color: rgba(255, 255, 255, 0.3);'
                            )


@ui.page('/responder/incidents')
async def responder_incidents_list():
    """List of incidents assigned to the organization"""

    # Get token from query params
    token = app.storage.user.get('token')
    if 'token' in ui.context.client.request.query_params:
        token = ui.context.client.request.query_params['token']
        app.storage.user['token'] = token

    if not token:
        ui.navigate.to('/responder')
        return

    async with responder_frame(i18n.t('ui.responder.assigned_incidents')):
        # Fetch incidents from API
        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(
                    f'http://localhost:8000/api/responder/incidents?token={token}',
                    timeout=10.0
                )

                if response.status_code != 200:
                    with ui.column().classes('w-full items-center justify-center p-12'):
                        ui.label(i18n.t('ui.responder.unable_to_load')).classes('text-xl text-red-500')
                        ui.label('Please check your access token').classes('text-sm text-gray-400 mt-2')
                    return

                incidents = response.json()

        except Exception as e:
            logger.error(f"Error fetching incidents: {e}")
            with ui.column().classes('w-full items-center justify-center p-12'):
                ui.label(i18n.t('ui.responder.unable_to_load')).classes('text-xl text-red-500')
            return

        # Display incidents in a table
        with ui.element('div').classes('w-full max-w-6xl mx-auto p-6'):
            if not incidents:
                with ui.column().classes('w-full items-center justify-center p-12'):
                    ui.label(i18n.t('ui.responder.no_incidents_assigned')).classes('text-xl text-gray-400')
                    ui.label('Incidents will appear here when assigned to your organization').classes(
                        'text-sm text-gray-400 mt-2'
                    )
            else:
                ui.label(f'{len(incidents)} {i18n.t("ui.responder.assigned_incidents")}').classes('text-2xl font-bold mb-6 title-font').style('color: #fff;')

                # Create table data
                columns = [
                    {'name': 'incident_id', 'label': i18n.t('ui.table.id'), 'field': 'incident_id', 'align': 'left', 'sortable': True},
                    {'name': 'title', 'label': i18n.t('ui.table.title'), 'field': 'title', 'align': 'left'},
                    {'name': 'priority', 'label': i18n.t('ui.table.priority'), 'field': 'priority', 'align': 'left', 'sortable': True},
                    {'name': 'status', 'label': i18n.t('ui.table.status'), 'field': 'status', 'align': 'left', 'sortable': True},
                    {'name': 'created_at', 'label': i18n.t('ui.table.created'), 'field': 'created_at', 'align': 'left', 'sortable': True},
                    {'name': 'actions', 'label': i18n.t('ui.table.actions'), 'field': 'actions', 'align': 'center'},
                ]

                rows = []
                for inc in incidents:
                    # API returns camelCase keys
                    created_at_str = inc.get('createdAt', inc.get('created_at', datetime.now().isoformat()))
                    rows.append({
                        'id': inc.get('id'),
                        'incident_id': inc.get('incidentId', inc.get('incident_id', 'UNKNOWN')),
                        'title': inc.get('title', 'Untitled') or 'Untitled',
                        'priority': inc.get('priority', 'medium'),
                        'status': inc.get('status', 'open'),
                        'created_at': datetime.fromisoformat(created_at_str).strftime('%Y-%m-%d %H:%M'),
                    })

                table = ui.table(
                    columns=columns,
                    rows=rows,
                    row_key='id',
                    pagination=10
                ).props('dark flat bordered').classes('w-full')

                # Custom cell rendering
                table.add_slot('body-cell-priority', '''
                    <q-td :props="props">
                        <span :class="'priority-badge priority-' + props.row.priority">
                            {{ props.row.priority }}
                        </span>
                    </q-td>
                ''')

                table.add_slot('body-cell-actions', f'''
                    <q-td :props="props">
                        <q-btn
                            label="Open"
                            size="sm"
                            color="teal"
                            outline
                            @click="$parent.$emit('openIncident', props.row.id)"
                        />
                    </q-td>
                ''')

                # Handle row click
                table.on('openIncident', lambda e: ui.navigate.to(
                    f'/responder/incidents/{e.args}?token={token}'
                ))


@ui.page('/responder/incidents/{incident_id}')
async def responder_incident_chat(incident_id: str):
    """Chat interface for a specific incident"""

    # Get token from storage or query params
    token = app.storage.user.get('token')
    if 'token' in ui.context.client.request.query_params:
        token = ui.context.client.request.query_params['token']
        app.storage.user['token'] = token

    if not token:
        ui.navigate.to('/responder')
        return

    # Fetch incident and chat data
    try:
        async with httpx.AsyncClient() as client:
            # Get incident details
            inc_response = await client.get(
                f'http://localhost:8000/api/responder/incidents/{incident_id}?token={token}',
                timeout=10.0
            )

            if inc_response.status_code != 200:
                with ui.column().classes('w-full items-center justify-center p-12'):
                    ui.label('Unable to load incident').classes('text-xl text-red-500')
                return

            incident = inc_response.json()

            # Get chat messages
            chat_response = await client.get(
                f'http://localhost:8000/api/responder/incidents/{incident_id}/chat?token={token}',
                timeout=10.0
            )

            messages = chat_response.json() if chat_response.status_code == 200 else []

            # Get internal notes
            notes_response = await client.get(
                f'http://localhost:8000/api/responder/incidents/{incident_id}/notes?token={token}',
                timeout=10.0
            )

            notes = notes_response.json() if notes_response.status_code == 200 else []

    except Exception as e:
        logger.error(f"Error loading incident: {e}")
        with ui.column().classes('w-full items-center justify-center p-12'):
            ui.label('Error loading incident').classes('text-xl text-red-500')
        return

    # Handle both camelCase and snake_case keys from API
    incident_id_str = incident.get('incidentId', incident.get('incident_id', 'UNKNOWN'))
    status = incident.get('status', 'open')
    priority = incident.get('priority', 'medium')
    title = incident.get('title') or 'Untitled Incident'
    created_at_str = incident.get('createdAt', incident.get('created_at', datetime.now().isoformat()))

    async with responder_frame(f'Incident {incident_id_str}'):
        # Chat container layout
        with ui.element('div').classes('w-full max-w-6xl mx-auto'):
            with ui.element('div').classes('chat-container'):
                # Main chat area
                with ui.element('div').classes('chat-main'):
                    # Incident header
                    with ui.element('div').classes('incident-header'):
                        ui.label(incident_id_str).classes('incident-header-id')
                        ui.label(title).classes('incident-header-title')
                        with ui.row().classes('gap-4'):
                            ui.label(f"Status: {status}").classes('incident-header-meta')
                            ui.label(f"Priority: {priority}").classes('incident-header-meta')
                            ui.label(f"Created: {datetime.fromisoformat(created_at_str).strftime('%Y-%m-%d %H:%M')}").classes('incident-header-meta')

                    # Media files section
                    media_files = incident.get('mediaFiles', [])
                    image_url = incident.get('imageUrl') or incident.get('image_url')
                    audio_url = incident.get('audioUrl') or incident.get('audio_url')
                    video_url = incident.get('videoUrl') or incident.get('video_url')

                    # Extract from media_files if available
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

                    if image_url or audio_url or video_url:
                        with ui.element('div').classes('p-4').style('background: rgba(0, 0, 0, 0.3); border-radius: 8px; margin-bottom: 16px;'):
                            ui.label('Attached Media').classes('text-sm text-gray-400 mb-2')
                            with ui.row().classes('gap-4'):
                                if image_url:
                                    with ui.column().classes('gap-2'):
                                        ui.label('Image').classes('text-xs text-gray-400')
                                        ui.image(image_url).style('max-width: 300px; max-height: 200px; border-radius: 4px;')
                                if audio_url:
                                    with ui.column().classes('gap-2'):
                                        ui.label('Audio').classes('text-xs text-gray-400')
                                        ui.html(f'<audio controls style="max-width: 300px;"><source src="{audio_url}" /></audio>', sanitize=False)
                                if video_url:
                                    with ui.column().classes('gap-2'):
                                        ui.label('Video').classes('text-xs text-gray-400')
                                        ui.html(f'<video controls style="max-width: 300px; max-height: 200px; border-radius: 4px;"><source src="{video_url}" /></video>', sanitize=False)

                    # Chat messages
                    chat_container = ui.column().classes('chat-messages')

                    with chat_container:
                        if not messages:
                            with ui.column().classes('w-full items-center justify-center p-12'):
                                ui.label('No messages yet').classes('text-gray-400')
                        else:
                            for msg in messages:
                                role = msg['role']
                                content = msg['content']
                                timestamp = datetime.fromisoformat(msg['timestamp']).strftime('%H:%M')

                                msg_class = 'message-user' if role == 'user' else 'message-assistant'

                                with ui.element('div').classes(f'message-bubble {msg_class}'):
                                    ui.label(content).classes('break-words')
                                    ui.label(timestamp).classes('message-timestamp')

                    # Chat input area
                    with ui.element('div').classes('chat-input-area'):
                        with ui.row().classes('w-full gap-2'):
                            message_input = ui.input(
                                placeholder='Type your message...'
                            ).props('dark filled').classes('flex-1')

                            async def send_message():
                                content = message_input.value.strip()
                                if not content:
                                    return

                                try:
                                    async with httpx.AsyncClient() as client:
                                        response = await client.post(
                                            f'http://localhost:8000/api/responder/incidents/{incident_id}/chat?token={token}',
                                            json={'content': content, 'role': 'assistant'},
                                            timeout=10.0
                                        )

                                        if response.status_code == 201:
                                            message_input.value = ''
                                            ui.navigate.reload()
                                        else:
                                            ui.notify('Failed to send message', color='red')

                                except Exception as e:
                                    logger.error(f"Error sending message: {e}")
                                    ui.notify('Error sending message', color='red')

                            ui.button(icon='send', on_click=send_message).props('color=teal flat')

                # Sidebar with actions and notes
                with ui.element('div').classes('chat-sidebar'):
                    # Status update
                    with ui.element('div').classes('action-panel'):
                        ui.label('Change Status').classes('action-panel-title')

                        async def update_status(new_status: str):
                            try:
                                async with httpx.AsyncClient() as client:
                                    response = await client.put(
                                        f'http://localhost:8000/api/responder/incidents/{incident_id}/status?token={token}',
                                        json={'status': new_status},
                                        timeout=10.0
                                    )

                                    if response.status_code == 200:
                                        ui.notify(f'Status updated to {new_status}', color='teal')
                                        ui.navigate.reload()
                                    else:
                                        ui.notify('Failed to update status', color='red')

                            except Exception as e:
                                logger.error(f"Error updating status: {e}")
                                ui.notify('Error updating status', color='red')

                        ui.select(
                            ['open', 'in_progress', 'resolved', 'closed'],
                            value=status,
                            on_change=lambda e: update_status(e.value)
                        ).props('dark filled').classes('w-full')

                    # Priority update
                    with ui.element('div').classes('action-panel'):
                        ui.label('Change Priority').classes('action-panel-title')

                        async def update_priority(new_priority: str):
                            try:
                                async with httpx.AsyncClient() as client:
                                    response = await client.put(
                                        f'http://localhost:8000/api/responder/incidents/{incident_id}/priority?token={token}',
                                        json={'priority': new_priority},
                                        timeout=10.0
                                    )

                                    if response.status_code == 200:
                                        ui.notify(f'Priority updated to {new_priority}', color='teal')
                                        ui.navigate.reload()
                                    else:
                                        ui.notify('Failed to update priority', color='red')

                            except Exception as e:
                                logger.error(f"Error updating priority: {e}")
                                ui.notify('Error updating priority', color='red')

                        ui.select(
                            ['critical', 'high', 'medium', 'low'],
                            value=priority,
                            on_change=lambda e: update_priority(e.value)
                        ).props('dark filled').classes('w-full')

                    # Add internal note
                    with ui.element('div').classes('action-panel'):
                        ui.label('Internal Notes').classes('action-panel-title')

                        note_input = ui.textarea(
                            placeholder='Add an internal note (not visible to reporter)...'
                        ).props('dark filled').classes('w-full')

                        async def add_note():
                            note_text = note_input.value.strip()
                            if not note_text:
                                return

                            try:
                                async with httpx.AsyncClient() as client:
                                    response = await client.post(
                                        f'http://localhost:8000/api/responder/incidents/{incident_id}/notes?token={token}',
                                        json={
                                            'incident_id': incident_id,
                                            'organization_id': 0,  # Will be set by backend
                                            'note_text': note_text,
                                            'created_by': 'responder'
                                        },
                                        timeout=10.0
                                    )

                                    if response.status_code == 201:
                                        note_input.value = ''
                                        ui.notify('Note added', color='teal')
                                        ui.navigate.reload()
                                    else:
                                        ui.notify('Failed to add note', color='red')

                            except Exception as e:
                                logger.error(f"Error adding note: {e}")
                                ui.notify('Error adding note', color='red')

                        ui.button('Add Note', on_click=add_note).props('color=teal outline').classes('w-full mt-2')

                    # Display existing notes
                    with ui.element('div').classes('notes-section'):
                        if notes:
                            for note in notes:
                                with ui.element('div').classes('note-item'):
                                    ui.label(note['note_text']).classes('break-words')
                                    ui.label(
                                        f"{datetime.fromisoformat(note['created_at']).strftime('%Y-%m-%d %H:%M')} - {note.get('created_by', 'Unknown')}"
                                    ).classes('note-meta')
                        else:
                            ui.label('No internal notes yet').classes('text-sm text-gray-400 text-center')

                    # Map with incident location (if available)
                    lat = incident.get('latitude')
                    lon = incident.get('longitude')
                    if lat and lon:
                        with ui.element('div').classes('action-panel'):
                            ui.label('Location').classes('action-panel-title')

                            # Create unique map ID
                            map_id = f'map-{incident_id}'

                            # Add map container
                            ui.html(f'<div id="{map_id}" style="height: 200px; width: 100%;"></div>', sanitize=False)

                            # Add map initialization script
                            ui.run_javascript(f'''
                                var map = L.map('{map_id}').setView([{lat}, {lon}], 18);
                                L.tileLayer('https://{{s}}.basemaps.cartocdn.com/dark_all/{{z}}/{{x}}/{{y}}{{r}}.png', {{
                                    attribution: '&copy; OpenStreetMap contributors &copy; CARTO',
                                    maxZoom: 20
                                }}).addTo(map);

                                var marker = L.marker([{lat}, {lon}]).addTo(map);
                                marker.bindPopup('<b>Incident Location</b><br>Lat: {lat:.6f}<br>Lon: {lon:.6f}').openPopup();
                            ''')


def init_responder_portal():
    """Initialize the responder portal routes"""
    logger.info("Responder portal routes initialized")
