"""
Integration Management Dashboard - NiceGUI Interface

Provides UI for managing organization integrations, templates, and viewing delivery logs.
"""
from datetime import datetime
from nicegui import ui
from typing import List, Dict, Optional
import httpx
import logging
import json

logger = logging.getLogger(__name__)
API_BASE = "http://localhost:8000/api"


async def fetch_integration_templates() -> List[Dict]:
    """Fetch all integration templates"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.get(f"{API_BASE}/integration/template")
            response.raise_for_status()
            return response.json()
    except Exception as e:
        logger.error(f"Error fetching integration templates: {e}")
        return []


async def fetch_organizations() -> List[Dict]:
    """Fetch all organizations"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.get(f"{API_BASE}/organization/")
            response.raise_for_status()
            return response.json()
    except Exception as e:
        logger.error(f"Error fetching organizations: {e}")
        return []


async def fetch_organization_integrations(org_id: Optional[int] = None) -> List[Dict]:
    """Fetch integrations for a specific organization or all integrations"""
    try:
        async with httpx.AsyncClient() as client:
            if org_id:
                response = await client.get(f"{API_BASE}/integration/organization/{org_id}")
            else:
                response = await client.get(f"{API_BASE}/integration/organization")
            response.raise_for_status()
            return response.json()
    except Exception as e:
        logger.error(f"Error fetching organization integrations: {e}")
        return []


async def fetch_all_organizations_with_integrations() -> List[Dict]:
    """Fetch all organizations with their integrations"""
    try:
        orgs = await fetch_organizations()
        result = []
        for org in orgs:
            org_integrations = await fetch_organization_integrations(org['id'])
            result.append({
                'id': org['id'],
                'name': org['name'],
                'type': org['type'],
                'integrations': org_integrations
            })
        return result
    except Exception as e:
        logger.error(f"Error fetching organization integrations: {e}")
        return []


async def create_organization_integration(data: Dict) -> Optional[Dict]:
    """Create a new organization integration"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.post(
                f"{API_BASE}/integration/organization",
                json=data,
                timeout=30.0
            )
            response.raise_for_status()
            return response.json()
    except httpx.HTTPStatusError as e:
        logger.error(f"HTTP error creating integration: {e.response.status_code} - {e.response.text}")
        raise
    except Exception as e:
        logger.error(f"Error creating organization integration: {e}")
        raise


async def update_organization_integration(org_id: int, integration_id: int, data: Dict) -> Optional[Dict]:
    """Update an existing organization integration"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.put(
                f"{API_BASE}/integration/organization/{org_id}/{integration_id}",
                json=data,
                timeout=30.0
            )
            response.raise_for_status()
            return response.json()
    except Exception as e:
        logger.error(f"Error updating organization integration: {e}")
        raise


async def delete_organization_integration(org_id: int, integration_id: int):
    """Delete an organization integration"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.delete(
                f"{API_BASE}/integration/organization/{org_id}/{integration_id}",
                timeout=30.0
            )
            response.raise_for_status()
    except Exception as e:
        logger.error(f"Error deleting organization integration: {e}")
        raise


async def test_integration_connection(org_id: int, integration_id: int) -> Dict:
    """Test an integration connection"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.post(
                f"{API_BASE}/integration/organization/{org_id}/{integration_id}/test",
                timeout=30.0
            )
            response.raise_for_status()
            return response.json()
    except Exception as e:
        logger.error(f"Error testing integration: {e}")
        return {"success": False, "message": str(e)}


async def fetch_delivery_logs(org_id: int, limit: int = 50) -> List[Dict]:
    """Fetch delivery logs for an organization"""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.get(
                f"{API_BASE}/integration/delivery/organization/{org_id}?limit={limit}"
            )
            response.raise_for_status()
            return response.json()
    except Exception as e:
        logger.error(f"Error fetching delivery logs: {e}")
        return []


def integration_dashboard_page():
    """Main integration management dashboard"""

    # State
    templates = []
    organizations = []
    integrations = []
    selected_org_id = None
    selected_org_name = ""

    # Containers
    integration_container = None
    stats_container = None

    async def refresh_data():
        """Refresh all data"""
        nonlocal templates, organizations
        templates = await fetch_integration_templates()
        organizations = await fetch_organizations()

        if selected_org_id:
            await refresh_integrations()

    async def refresh_integrations():
        """Refresh integrations for selected organization"""
        nonlocal integrations
        if not selected_org_id:
            integrations = []
            render_integrations()
            return

        integrations = await fetch_organization_integrations(selected_org_id)
        render_integrations()
        render_stats()

    def render_stats():
        """Render integration statistics"""
        stats_container.clear()
        with stats_container:
            active_count = sum(1 for i in integrations if i.get('active'))
            inactive_count = len(integrations) - active_count

            with ui.row().classes('w-full gap-4'):
                with ui.element('div').classes('metric-card flex-1'):
                    ui.label('Total Integrations').classes('metric-label')
                    ui.label(str(len(integrations))).classes('metric-value')

                with ui.element('div').classes('metric-card flex-1'):
                    ui.label('Active').classes('metric-label')
                    ui.label(str(active_count)).classes('metric-value text-[#63ABFF]')

                with ui.element('div').classes('metric-card flex-1'):
                    ui.label('Inactive').classes('metric-label')
                    ui.label(str(inactive_count)).classes('metric-value text-gray-500')

    def render_integrations():
        """Render integration list"""
        integration_container.clear()
        with integration_container:
            if not selected_org_id:
                with ui.element('div').classes('w-full p-8 text-center'):
                    ui.label('Select an organization to view integrations').classes('text-gray-400')
                return

            if not integrations:
                with ui.element('div').classes('w-full p-8 text-center'):
                    ui.label('No integrations configured').classes('text-gray-400 mb-4')
                    ui.button(
                        'Create First Integration',
                        on_click=show_create_dialog,
                        icon='add'
                    ).props('color=primary')
                return

            for integration in integrations:
                render_integration_card(integration)

    def render_integration_card(integration: Dict):
        """Render a single integration card"""
        # Status indicator
        is_active = integration.get('active', False)
        status_color = '#63ABFF' if is_active else '#666'

        with ui.element('div').classes('w-full mb-4 p-4 rounded border border-[#1e3a4f] bg-[#0a1929] hover:bg-[#0d2637] transition-all'):
            with ui.row().classes('w-full items-start gap-4'):
                # Status dot
                with ui.element('div').classes('mt-1'):
                    ui.html(f'<div style="width: 12px; height: 12px; border-radius: 50%; background: {status_color}; box-shadow: 0 0 8px {status_color};"></div>')

                # Content
                with ui.column().classes('flex-grow gap-2'):
                    with ui.row().classes('w-full items-center gap-2'):
                        ui.label(integration['name']).classes('text-lg font-bold text-white')
                        ui.label(f"({integration['template_type']})").classes('text-sm text-gray-400')

                    ui.label(integration['template_name']).classes('text-sm text-[#63ABFF]')

                    if integration.get('description'):
                        ui.label(integration['description']).classes('text-sm text-gray-400')

                # Actions
                with ui.column().classes('gap-2'):
                    with ui.row().classes('gap-2'):
                        ui.button(
                            icon='play_arrow',
                            on_click=lambda i=integration: test_integration(i['organization_id'], i['id'])
                        ).props('flat dense color=blue').classes('text-xs').tooltip('Test Connection')

                        ui.button(
                            icon='edit',
                            on_click=lambda i=integration: show_edit_dialog(i)
                        ).props('flat dense color=orange').classes('text-xs').tooltip('Edit')

                        ui.button(
                            icon='bar_chart',
                            on_click=lambda i=integration: show_delivery_logs(i['organization_id'])
                        ).props('flat dense').classes('text-xs').tooltip('View Logs')

                        ui.button(
                            icon='delete',
                            on_click=lambda i=integration: confirm_delete(i['organization_id'], i['id'], i['name'])
                        ).props('flat dense color=red').classes('text-xs').tooltip('Delete')

    async def test_integration(org_id: int, integration_id: int):
        """Test an integration connection"""
        ui.notify('Testing connection...', type='info')
        result = await test_integration_connection(org_id, integration_id)
        if result.get('success'):
            ui.notify(result.get('message', 'Connection successful'), type='positive')
        else:
            ui.notify(result.get('message', 'Connection failed'), type='negative')

    def confirm_delete(org_id: int, integration_id: int, name: str):
        """Confirm deletion dialog"""
        with ui.dialog() as dialog, ui.card().classes('bg-[#0a1929]'):
            ui.label(f'Delete Integration').classes('text-xl font-bold mb-4')
            ui.label(f'Are you sure you want to delete "{name}"?').classes('text-gray-400 mb-4')
            ui.label('This action cannot be undone.').classes('text-sm text-red-400 mb-4')

            with ui.row().classes('gap-2'):
                ui.button('Cancel', on_click=dialog.close).props('outline')
                ui.button(
                    'Delete',
                    on_click=lambda: (dialog.close(), delete_integration(org_id, integration_id))
                ).props('color=red')

        dialog.open()

    async def delete_integration(org_id: int, integration_id: int):
        """Delete an integration"""
        try:
            await delete_organization_integration(org_id, integration_id)
            ui.notify('Integration deleted', type='positive')
            await refresh_integrations()
        except Exception as e:
            ui.notify(f'Error deleting integration: {str(e)}', type='negative')

    def show_edit_dialog(integration: Dict):
        """Show dialog to edit an integration"""
        with ui.dialog() as dialog, ui.card().classes('w-full max-w-2xl bg-[#0a1929]'):
            ui.label(f"Edit: {integration['name']}").classes('text-xl font-bold mb-4')

            name_input = ui.input('Name', value=integration['name']).classes('w-full')
            desc_input = ui.textarea('Description', value=integration.get('description', '')).classes('w-full')
            active_switch = ui.switch('Active', value=integration.get('active', True))

            async def save_changes():
                try:
                    update_data = {
                        'name': name_input.value,
                        'description': desc_input.value,
                        'active': active_switch.value
                    }
                    await update_organization_integration(
                        integration['organization_id'],
                        integration['id'],
                        update_data
                    )
                    ui.notify('Integration updated', type='positive')
                    dialog.close()
                    await refresh_integrations()
                except Exception as e:
                    ui.notify(f'Error: {str(e)}', type='negative')

            with ui.row().classes('mt-4 gap-2'):
                ui.button('Save', on_click=save_changes).props('color=primary')
                ui.button('Cancel', on_click=dialog.close).props('outline')

        dialog.open()

    def show_create_dialog():
        """Show dialog to create a new integration"""
        if not selected_org_id:
            ui.notify('Please select an organization first', type='warning')
            return

        with ui.dialog() as dialog, ui.card().classes('w-full max-w-3xl bg-[#0a1929]'):
            ui.label('Create Integration').classes('text-2xl font-bold mb-6')

            # Template selection
            template_select = ui.select(
                label='Integration Type',
                options={t['id']: f"{t['name']}" for t in templates},
                value=templates[0]['id'] if templates else None
            ).classes('w-full mb-4')

            # Basic info
            ui.label('Basic Information').classes('text-lg font-bold mt-4 mb-2')
            name_input = ui.input('Name', placeholder='My Integration').classes('w-full')
            desc_input = ui.textarea('Description (optional)', placeholder='Describe this integration').classes('w-full')

            # Config
            ui.label('Configuration').classes('text-lg font-bold mt-4 mb-2')
            endpoint_input = ui.input('Endpoint URL', placeholder='https://webhook.site/...').classes('w-full')
            timeout_input = ui.number('Timeout (seconds)', value=30, min=5, max=300).classes('w-full')

            # Auth
            ui.label('Authentication').classes('text-lg font-bold mt-4 mb-2')
            auth_token_input = ui.input('Bearer Token / API Key (optional)', password=True, password_toggle_button=True).classes('w-full')

            # Filters
            ui.label('Trigger Filters (optional)').classes('text-lg font-bold mt-4 mb-2')
            with ui.row().classes('w-full gap-4'):
                priorities_input = ui.input('Priorities', placeholder='critical,high').classes('flex-1')
                categories_input = ui.input('Categories', placeholder='Security,Military').classes('flex-1')

            active_switch = ui.switch('Active', value=True).classes('mt-4')

            async def create_integration():
                try:
                    config = {
                        'endpoint_url': endpoint_input.value,
                        'timeout': timeout_input.value
                    }

                    auth_credentials = {}
                    if auth_token_input.value:
                        auth_credentials['token'] = auth_token_input.value

                    trigger_filters = {}
                    if priorities_input.value:
                        trigger_filters['priorities'] = [p.strip() for p in priorities_input.value.split(',')]
                    if categories_input.value:
                        trigger_filters['categories'] = [c.strip() for c in categories_input.value.split(',')]

                    data = {
                        'organization_id': selected_org_id,
                        'template_id': template_select.value,
                        'name': name_input.value,
                        'description': desc_input.value,
                        'config': config,
                        'auth_credentials': auth_credentials,
                        'trigger_filters': trigger_filters if trigger_filters else None,
                        'active': active_switch.value,
                        'created_by': 'dashboard'
                    }

                    await create_organization_integration(data)
                    ui.notify('Integration created', type='positive')
                    dialog.close()
                    await refresh_integrations()
                except Exception as e:
                    ui.notify(f'Error: {str(e)}', type='negative')

            with ui.row().classes('mt-6 gap-2'):
                ui.button('Create', on_click=create_integration, icon='add').props('color=primary')
                ui.button('Cancel', on_click=dialog.close).props('outline')

        dialog.open()

    async def show_delivery_logs(org_id: int):
        """Show delivery logs dialog"""
        logs = await fetch_delivery_logs(org_id, limit=50)

        with ui.dialog() as dialog, ui.card().classes('w-full max-w-5xl bg-[#0a1929]'):
            ui.label('Integration Delivery Logs').classes('text-2xl font-bold mb-4')

            if logs:
                for log in logs:
                    status = log.get('status', 'unknown')
                    status_colors = {
                        'success': '#63ABFF',
                        'failed': '#FF4444',
                        'pending': '#ffa600',
                        'timeout': '#FF4444'
                    }
                    status_color = status_colors.get(status, '#666')

                    with ui.element('div').classes('w-full mb-2 p-3 rounded bg-[#0d2637] border border-[#1e3a4f]'):
                        with ui.row().classes('w-full items-center gap-3'):
                            ui.html(f'<div style="width: 8px; height: 8px; border-radius: 50%; background: {status_color}; box-shadow: 0 0 6px {status_color};"></div>')

                            with ui.column().classes('flex-grow gap-1'):
                                with ui.row().classes('items-center gap-2'):
                                    ui.label(log.get('integration_name', 'Unknown')).classes('font-bold text-white')
                                    ui.label(f"({log.get('integration_type', 'unknown')})").classes('text-xs text-gray-400')
                                    ui.label(status.upper()).classes(f'text-xs px-2 py-1 rounded').style(f'background: {status_color}20; color: {status_color}')

                                with ui.row().classes('text-xs text-gray-400 gap-4'):
                                    ui.label(f"Incident: {log.get('incident_id', 'N/A')}")
                                    ui.label(f"Response: {log.get('response_code', 'N/A')}")
                                    ui.label(f"Duration: {log.get('duration_ms', 0)}ms")
                                    ui.label(f"{log.get('started_at', 'unknown')}")

                                if log.get('error_message'):
                                    ui.label(f"Error: {log['error_message']}").classes('text-xs text-red-400 mt-1')
            else:
                with ui.element('div').classes('w-full p-8 text-center'):
                    ui.label('No delivery logs found').classes('text-gray-400')

            ui.button('Close', on_click=dialog.close).classes('mt-4').props('outline')

        dialog.open()

    async def on_org_selected(e):
        nonlocal selected_org_id, selected_org_name
        selected_org_id = e.value
        org = next((o for o in organizations if o['id'] == selected_org_id), None)
        selected_org_name = org['name'] if org else ''
        await refresh_integrations()

    async def show_batch_assign_dialog():
        """Show dialog for batch assigning organizations to an integration"""
        if not templates:
            ui.notify('No integration templates available', type='warning')
            return

        # Fetch all organizations with their integrations
        orgs_with_integrations = await fetch_all_organizations_with_integrations()

        with ui.dialog() as dialog, ui.card().classes('w-full max-w-6xl bg-[#0a1929]'):
            ui.label('Batch Assign Integration').classes('text-2xl font-bold mb-4')
            ui.label('Create the same integration for multiple organizations at once').classes('text-gray-400 mb-6')

            # Template selection
            with ui.row().classes('w-full gap-4 mb-6'):
                template_select = ui.select(
                    label='Integration Type',
                    options={t['id']: f"{t['name']}" for t in templates},
                    value=templates[0]['id'] if templates else None
                ).classes('flex-1')

            # Basic info
            ui.label('Integration Configuration').classes('text-lg font-bold mb-2')
            with ui.row().classes('w-full gap-4'):
                name_input = ui.input('Name', placeholder='My Integration').classes('flex-1')
                endpoint_input = ui.input('Endpoint URL', placeholder='https://webhook.site/...').classes('flex-1')

            with ui.row().classes('w-full gap-4 mb-4'):
                timeout_input = ui.number('Timeout (seconds)', value=30, min=5, max=300).classes('flex-1')
                auth_token_input = ui.input('Bearer Token / API Key (optional)', password=True, password_toggle_button=True).classes('flex-1')

            # Organization selection with table
            ui.label('Select Organizations').classes('text-lg font-bold mt-6 mb-2')

            # Search/filter
            search_input = ui.input('Search organizations', placeholder='Filter by name or type...').classes('w-full mb-4')

            # Table container
            table_container = ui.column().classes('w-full')

            selected_orgs = set()

            def render_org_table():
                table_container.clear()

                # Filter organizations
                search_term = search_input.value.lower() if search_input.value else ''
                filtered_orgs = [
                    org for org in orgs_with_integrations
                    if search_term in org['name'].lower() or search_term in org['type'].lower()
                ]

                with table_container:
                    # Table header
                    with ui.element('div').classes('w-full border border-[#1e3a4f] rounded'):
                        with ui.row().classes('w-full bg-[#0d2637] p-3 font-bold border-b border-[#1e3a4f]'):
                            ui.label('Select').classes('w-20')
                            ui.label('Organization').classes('flex-1')
                            ui.label('Type').classes('w-32')
                            ui.label('Current Integrations').classes('flex-1')

                        # Table rows
                        for org in filtered_orgs:
                            with ui.row().classes('w-full p-3 border-b border-[#1e3a4f] items-center hover:bg-[#0d2637]'):
                                # Checkbox
                                checkbox = ui.checkbox('', value=org['id'] in selected_orgs)
                                checkbox.classes('w-20')

                                def make_toggle(org_id, cb):
                                    def toggle():
                                        if cb.value:
                                            selected_orgs.add(org_id)
                                        else:
                                            selected_orgs.discard(org_id)
                                    return toggle

                                checkbox.on_value_change(make_toggle(org['id'], checkbox))

                                # Organization name
                                ui.label(org['name']).classes('flex-1')

                                # Type
                                type_colors = {
                                    'military': 'bg-blue-900 text-blue-300',
                                    'police': 'bg-indigo-900 text-indigo-300',
                                    'fire': 'bg-red-900 text-red-300',
                                    'medical': 'bg-green-900 text-green-300',
                                    'civil_defense': 'bg-yellow-900 text-yellow-300',
                                }
                                color_class = type_colors.get(org['type'], 'bg-gray-800 text-gray-300')
                                ui.label(org['type'].replace('_', ' ').title()).classes(f'w-32 px-2 py-1 rounded text-xs {color_class}')

                                # Current integrations
                                if org['integrations']:
                                    integration_names = ', '.join([i['name'] for i in org['integrations']])
                                    ui.label(integration_names).classes('flex-1 text-sm text-[#63ABFF]')
                                else:
                                    ui.label('None').classes('flex-1 text-sm text-gray-500')

            # Update table on search
            search_input.on_value_change(lambda: render_org_table())

            # Button actions
            def select_all_orgs():
                selected_orgs.clear()
                selected_orgs.update([org['id'] for org in orgs_with_integrations])
                render_org_table()

            def deselect_all_orgs():
                selected_orgs.clear()
                render_org_table()

            def select_military():
                selected_orgs.clear()
                selected_orgs.update([org['id'] for org in orgs_with_integrations if org['type'] == 'military'])
                render_org_table()

            # Selection controls (must be after function definitions)
            with ui.row().classes('w-full gap-2 mb-4'):
                ui.button('Select All', icon='check_box', on_click=select_all_orgs).props('flat size=sm')
                ui.button('Deselect All', icon='check_box_outline_blank', on_click=deselect_all_orgs).props('flat size=sm')
                ui.button('Select Military', icon='shield', on_click=select_military).props('flat size=sm color=blue')

            # Render initial table
            render_org_table()

            async def batch_create():
                try:
                    if not selected_orgs:
                        ui.notify('Please select at least one organization', type='warning')
                        return

                    # Build config
                    config = {
                        'endpoint_url': endpoint_input.value,
                        'timeout': timeout_input.value
                    }

                    auth_credentials = {}
                    if auth_token_input.value:
                        auth_credentials['token'] = auth_token_input.value

                    # Create integration for each selected organization
                    success_count = 0
                    error_count = 0

                    for org_id in selected_orgs:
                        try:
                            data = {
                                'organization_id': org_id,
                                'template_id': template_select.value,
                                'name': name_input.value,
                                'description': '',
                                'config': config,
                                'auth_credentials': auth_credentials,
                                'trigger_filters': None,
                                'active': True,
                                'created_by': 'dashboard_batch'
                            }
                            await create_organization_integration(data)
                            success_count += 1
                        except Exception as e:
                            logger.error(f"Error creating integration for org {org_id}: {e}")
                            error_count += 1

                    if error_count > 0:
                        ui.notify(f'Created {success_count} integrations, {error_count} failed', type='warning')
                    else:
                        ui.notify(f'Successfully created {success_count} integrations', type='positive')

                    dialog.close()
                    await refresh_integrations()
                except Exception as e:
                    ui.notify(f'Error: {str(e)}', type='negative')

            with ui.row().classes('mt-6 gap-2'):
                ui.button('Create for Selected Organizations', on_click=batch_create, icon='add_circle').props('color=primary')
                ui.button('Cancel', on_click=dialog.close).props('outline')

        dialog.open()

    def show_template_details(template: Dict):
        """Show detailed template information dialog"""
        with ui.dialog() as dialog, ui.card().classes('w-full max-w-4xl'):
            with ui.row().classes('w-full items-center justify-between mb-4'):
                ui.label(template['name']).classes('text-2xl font-bold')
                ui.badge(template['type'].upper()).classes('text-sm')

            if template.get('description'):
                ui.label(template['description']).classes('text-gray-500 mb-6')

            template_type = template['type']

            # What it does
            ui.label('What it does').classes('text-lg font-semibold mb-2')
            overviews = {
                'webhook': 'Sends incident data in real-time to external services via HTTP POST.',
                'sedap': 'Formats incident data as CSV and sends it to military Battle Management Systems.',
                'email': 'Sends incident alerts via email to configured recipients.'
            }
            ui.label(overviews.get(template_type, template.get('description', ''))).classes('text-sm mb-6')

            # How to configure
            ui.label('How to configure').classes('text-lg font-semibold mb-2')

            with ui.expansion('Step 1: Batch Assign', icon='people').classes('w-full mb-2'):
                ui.label('Select multiple organizations and click "Assign to Selected" to create integrations for all at once.').classes('text-sm')

            with ui.expansion('Step 2: Configure Each Organization', icon='settings').classes('w-full mb-2'):
                config_help = {
                    'webhook': 'Each organization needs its own webhook URL. Go to "Manage Integrations", click EDIT, and enter the webhook endpoint for that organization (e.g., Zapier webhook, n8n endpoint).',
                    'sedap': 'All organizations share the same SEDAP endpoint (configured in .env). No per-organization configuration needed.',
                    'email': 'Each organization needs recipient email addresses. Go to "Manage Integrations", click EDIT, and enter the email addresses.'
                }
                ui.label(config_help.get(template_type, 'Configure in Manage Integrations section.')).classes('text-sm')

            with ui.expansion('Step 3: Share Config Across Organizations', icon='share').classes('w-full mb-6'):
                ui.label('If multiple organizations use the same endpoint (e.g., same Zapier account), you can:').classes('text-sm mb-2')
                ui.label('1. Configure one organization completely').classes('text-sm ml-4')
                ui.label('2. Copy the config JSON from that organization').classes('text-sm ml-4')
                ui.label('3. Paste it into other organizations').classes('text-sm ml-4')
                ui.label('This makes batch configuration fast and consistent.').classes('text-sm ml-4 font-semibold mt-2')

            # Required fields
            ui.label('Required configuration fields').classes('text-lg font-semibold mb-2')
            config_schema = template.get('config_schema', {})
            if config_schema:
                with ui.column().classes('w-full gap-2 mb-6'):
                    for field, details in config_schema.items():
                        with ui.card().classes('w-full'):
                            with ui.row().classes('items-center gap-2'):
                                ui.label(field).classes('font-semibold')
                                if isinstance(details, dict):
                                    if details.get('required'):
                                        ui.badge('Required', color='red')
                                    ui.label(f"({details.get('type', 'string')})").classes('text-sm text-gray-500')
                            if isinstance(details, dict) and details.get('description'):
                                ui.label(details['description']).classes('text-sm text-gray-600')
            else:
                ui.label('No specific configuration required').classes('text-sm text-gray-500 mb-6')

            # Compatible systems
            compatible_systems = {
                'webhook': 'Zapier, Make.com, n8n, webhook.site, Discord, Slack, custom endpoints',
                'sedap': 'Bundeswehr BMS, NATO STANAG 4406, military command centers',
                'email': 'Gmail, Outlook, SendGrid, Mailgun, any SMTP server'
            }
            if template_type in compatible_systems:
                ui.label('Works with').classes('text-lg font-semibold mb-2')
                ui.label(compatible_systems[template_type]).classes('text-sm mb-6')

            with ui.row().classes('w-full gap-2'):
                ui.button('Close', on_click=dialog.close).props('flat')

        dialog.open()

    def render_templates():
        """Render templates list - tactical style"""
        templates_container.clear()
        with templates_container:
            if templates:
                for template in templates:
                    # Tactical template card
                    with ui.element('div').classes('w-full mb-2 p-3').style('background: rgba(13, 38, 55, 0.3); border-left: 2px solid #1e3a4f; transition: all 0.2s;').on('mouseenter', lambda e: e.sender.style('background: rgba(13, 38, 55, 0.5); border-left: 2px solid #63ABFF;')).on('mouseleave', lambda e: e.sender.style('background: rgba(13, 38, 55, 0.3); border-left: 2px solid #1e3a4f;')):
                        with ui.row().classes('w-full items-center gap-4'):
                            with ui.column().classes('flex-grow gap-1'):
                                ui.label(template['name'].upper()).classes('text-sm font-mono font-bold')
                                ui.label(f"TYPE: {template['type'].upper()}").classes('text-xs font-mono text-[#63ABFF]')
                                if template.get('description'):
                                    ui.label(template['description']).classes('text-xs font-mono text-gray-400')

                            ui.button(
                                'INFO',
                                on_click=lambda t=template: show_template_details(t),
                                icon='info'
                            ).props('outline').classes('text-white')
            else:
                ui.label('NO TEMPLATES AVAILABLE').classes('text-xs font-mono text-gray-600')

    async def fetch_all_integrations():
        """Fetch all organization integrations"""
        try:
            async with httpx.AsyncClient() as client:
                # Get all organizations
                orgs_response = await client.get(f'{API_BASE}/organization/')
                orgs = orgs_response.json()

                all_integrations = []
                for org in orgs:
                    # Get integrations for this org
                    int_response = await client.get(f'{API_BASE}/integration/organization/{org["id"]}')
                    integrations = int_response.json()

                    for integration in integrations:
                        integration['organization_name'] = org['name']
                        integration['organization_short'] = org.get('short_name', org['name'])
                        all_integrations.append(integration)

                return all_integrations
        except Exception as e:
            logger.error(f"Error fetching integrations: {e}")
            return []

    def show_integration_config(integration):
        """Show integration configuration dialog with edit capability and shareable config"""
        import json

        with ui.dialog() as dialog, ui.card().classes('w-full max-w-4xl'):
            ui.label(f'{integration["name"]} - {integration["organization_name"]}').classes('text-xl font-bold mb-2')
            ui.label(integration['template_type'].upper()).classes('text-sm text-gray-500 mb-4')

            # Configuration fields based on type
            if integration['template_type'] == 'webhook':
                ui.label('Webhook URL').classes('text-sm font-semibold mb-1')
                endpoint_input = ui.input(placeholder='https://webhook.site/...', value=integration['config'].get('endpoint_url', '')).classes('w-full mb-3')

                ui.label('Bearer Token (optional)').classes('text-sm font-semibold mb-1')
                token_input = ui.input(placeholder='Your authentication token', value=integration['auth_credentials'].get('token', ''), password=True, password_toggle_button=True).classes('w-full mb-4')

            elif integration['template_type'] == 'sedap':
                with ui.card().classes('w-full mb-4'):
                    ui.label('SEDAP uses shared environment configuration').classes('text-sm mb-1')
                    ui.label('No per-organization configuration needed').classes('text-sm text-gray-500')
                endpoint_input = None
                token_input = None

            elif integration['template_type'] == 'email':
                ui.label('Recipient Email(s)').classes('text-sm font-semibold mb-1')
                endpoint_input = ui.input(placeholder='email@example.com, another@example.com', value=integration['config'].get('to_email', '')).classes('w-full mb-4')
                token_input = None

            # Shareable config section
            if integration['template_type'] != 'sedap':
                with ui.expansion('Share Configuration', icon='share').classes('w-full mb-4'):
                    ui.label('Copy this JSON to apply the same config to other organizations:').classes('text-sm mb-2')

                    # Create shareable config (without org-specific data)
                    shareable_config = {
                        'config': integration['config'].copy(),
                        'auth_credentials': integration['auth_credentials'].copy() if integration['auth_credentials'] else {}
                    }
                    config_json = json.dumps(shareable_config, indent=2)

                    config_textarea = ui.textarea(value=config_json).classes('w-full font-mono text-sm').props('rows=8')

                    with ui.row().classes('w-full gap-2'):
                        def copy_config():
                            ui.run_javascript(f'navigator.clipboard.writeText({json.dumps(config_json)})')
                            ui.notify('Config copied to clipboard', type='positive')

                        ui.button('Copy to Clipboard', on_click=copy_config, icon='content_copy').props('size=sm outline')

                        def paste_config():
                            try:
                                pasted = json.loads(config_textarea.value)
                                if endpoint_input and pasted.get('config', {}).get('endpoint_url'):
                                    endpoint_input.value = pasted['config']['endpoint_url']
                                if endpoint_input and integration['template_type'] == 'email' and pasted.get('config', {}).get('to_email'):
                                    endpoint_input.value = pasted['config']['to_email']
                                if token_input and pasted.get('auth_credentials', {}).get('token'):
                                    token_input.value = pasted['auth_credentials']['token']
                                ui.notify('Config pasted successfully', type='positive')
                            except Exception as e:
                                ui.notify(f'Invalid JSON: {str(e)}', type='negative')

                        ui.button('Paste from JSON', on_click=paste_config, icon='content_paste').props('size=sm outline')

            # Status
            with ui.card().classes('w-full mb-4'):
                with ui.row().classes('items-center gap-2'):
                    ui.label('Status:').classes('font-semibold')
                    if integration['active']:
                        ui.badge('ACTIVE', color='positive')
                    else:
                        ui.badge('INACTIVE', color='negative')

                if integration.get('last_delivery_at'):
                    ui.label(f"Last delivery: {integration['last_delivery_at']}").classes('text-sm text-gray-500')
                    if integration.get('last_delivery_status'):
                        ui.label(f"Status: {integration['last_delivery_status']}").classes('text-sm text-gray-500')

            # Actions
            with ui.row().classes('w-full justify-end gap-2 mt-4'):
                ui.button('Close', on_click=dialog.close).props('flat')

                async def save_config():
                    try:
                        # Build update payload
                        update_data = {
                            'name': integration['name'],
                            'description': integration.get('description', ''),
                            'config': integration['config'].copy(),
                            'auth_credentials': integration['auth_credentials'].copy() if integration['auth_credentials'] else {},
                            'trigger_filters': integration.get('trigger_filters', {}),
                            'active': integration['active']
                        }

                        # Update config based on type
                        if endpoint_input:
                            if integration['template_type'] == 'webhook':
                                update_data['config']['endpoint_url'] = endpoint_input.value
                            elif integration['template_type'] == 'email':
                                update_data['config']['to_email'] = endpoint_input.value

                        if token_input and token_input.value:
                            update_data['auth_credentials']['token'] = token_input.value
                            update_data['auth_credentials']['auth_type'] = 'bearer_token'

                        # Save via API
                        async with httpx.AsyncClient() as client:
                            response = await client.put(
                                f'{API_BASE}/integration/organization/{integration["id"]}',
                                json=update_data
                            )
                            response.raise_for_status()

                        ui.notify('Configuration saved successfully', type='positive')
                        dialog.close()
                        await render_manage_integrations()
                    except Exception as e:
                        logger.error(f"Error saving integration config: {e}")
                        ui.notify(f'Error saving configuration: {str(e)}', type='negative')

                if integration['template_type'] != 'sedap':
                    ui.button('Save Configuration', on_click=save_config, icon='save').props('color=primary')

        dialog.open()

    async def render_manage_integrations():
        """Render all integrations with edit/delete actions"""
        manage_integrations_container.clear()

        all_integrations = await fetch_all_integrations()

        with manage_integrations_container:
            if all_integrations:
                for integration in all_integrations:
                    # Tactical integration card
                    with ui.element('div').classes('w-full mb-2 p-3').style('background: rgba(13, 38, 55, 0.3); border-left: 2px solid #1e3a4f;'):
                        with ui.row().classes('w-full items-center gap-4'):
                            # Integration info
                            with ui.column().classes('flex-grow gap-1'):
                                ui.label(f"{integration['organization_short']} - {integration['name']}").classes('text-sm font-mono font-bold')
                                ui.label(f"TYPE: {integration['template_type'].upper()}").classes('text-xs font-mono text-[#63ABFF]')

                                # Show config status
                                if integration['template_type'] == 'webhook':
                                    has_url = bool(integration['config'].get('endpoint_url'))
                                    status = 'CONFIGURED' if has_url else 'NEEDS CONFIGURATION'
                                    color = '#34d399' if has_url else '#ffa600'
                                    ui.label(status).classes('text-xs font-mono').style(f'color: {color};')
                                elif integration['template_type'] == 'email':
                                    has_email = bool(integration['config'].get('to_email'))
                                    status = 'CONFIGURED' if has_email else 'NEEDS CONFIGURATION'
                                    color = '#34d399' if has_email else '#ffa600'
                                    ui.label(status).classes('text-xs font-mono').style(f'color: {color};')
                                else:
                                    ui.label('READY').classes('text-xs font-mono text-green-400')

                            # Actions
                            with ui.row().classes('gap-1'):
                                ui.button(
                                    'EDIT',
                                    on_click=lambda i=integration: show_integration_config(i),
                                    icon='edit'
                                ).props('flat dense').classes('text-xs font-mono')

                                ui.button(
                                    'DELETE',
                                    on_click=lambda i=integration: delete_integration(i),
                                    icon='delete'
                                ).props('flat dense').classes('text-xs font-mono')
            else:
                ui.label('NO INTEGRATIONS CONFIGURED').classes('text-xs font-mono text-gray-600')

    async def delete_integration(integration):
        """Delete an integration"""
        try:
            async with httpx.AsyncClient() as client:
                response = await client.delete(f'{API_BASE}/integration/organization/{integration["id"]}')
                response.raise_for_status()

            ui.notify(f'Deleted integration for {integration["organization_short"]}', type='positive')
            await render_manage_integrations()
            await load_orgs_table()
        except Exception as e:
            logger.error(f"Error deleting integration: {e}")
            ui.notify(f'Error deleting integration: {str(e)}', type='negative')

    # Main UI - centered, full width, clean layout like reddit-observer
    with ui.column().classes('w-full my-6 gap-8'):
        # Push to Organizations section
        ui.label('PUSH TO ORGANIZATIONS').classes('text-md font-bold mb-4 text-gray-400 tracking-wider')

        # Command bar
        with ui.row().classes('w-full gap-4 items-center mb-4'):
            search_input = ui.input('Search', placeholder='Filter organizations...').classes('flex-1')

            type_filter = ui.select(
                label='Type',
                options={
                    'all': 'All',
                    'military': 'Military',
                    'police': 'Police',
                    'fire': 'Fire',
                    'medical': 'Medical',
                    'civil_defense': 'Civil Defense',
                    'government': 'Government',
                    'other': 'Other'
                },
                value='all'
            ).classes('w-48')

            selection_buttons_row = ui.row().classes('gap-2')

            template_select = ui.select(
                label='Integration Template',
                options={},
            ).classes('w-64')

            assign_button_row = ui.row().classes('gap-2')

        # Organizations table
        orgs_table_container = ui.column().classes('w-full')

        # Manage Integrations section
        ui.label('MANAGE INTEGRATIONS').classes('text-md font-bold mb-4 mt-8 text-gray-400 tracking-wider')
        ui.label('Configure, test, and manage all active integrations. Click EDIT to set webhook URLs, email recipients, or other required settings.').classes('text-xs font-mono text-gray-400 mb-4')
        manage_integrations_container = ui.column().classes('w-full')

        # Templates section
        ui.label('AVAILABLE TEMPLATES').classes('text-md font-bold mb-4 mt-8 text-gray-400 tracking-wider')
        templates_container = ui.column().classes('w-full')

        # Push to SIMS section
        ui.label('PUSH TO SIMS').classes('text-md font-bold mb-4 mt-8 text-gray-400 tracking-wider')

        # Information cards
        with ui.row().classes('w-full gap-4'):
            # Mobile App
            with ui.card().classes('flex-1'):
                with ui.row().classes('w-full items-center gap-2 mb-2'):
                    ui.icon('smartphone', size='md')
                    ui.label('Mobile App').classes('text-lg font-semibold')
                ui.label('Android').classes('text-sm text-gray-500 mb-2')
                with ui.column().classes('gap-1 text-sm'):
                    ui.label('• Download and install SIMS APK')
                    ui.label('• Register with organization code')
                    ui.label('• Report incidents with photos, voice, location')
                    ui.label('• iOS version planned').classes('text-gray-500 italic')

            # Inbound Webhook
            with ui.card().classes('flex-1'):
                with ui.row().classes('w-full items-center gap-2 mb-2'):
                    ui.icon('webhook', size='md')
                    ui.label('Inbound Webhook').classes('text-lg font-semibold')
                ui.label('External Systems').classes('text-sm text-gray-500 mb-2')
                with ui.column().classes('gap-1 text-sm'):
                    ui.label('• Create webhook endpoint in system')
                    ui.label('• Configure field mapping (JSONPath)')
                    ui.label('• External systems POST to webhook URL')
                    ui.label('• Auto-routes to assigned organization')

            # API Direct
            with ui.card().classes('flex-1'):
                with ui.row().classes('w-full items-center gap-2 mb-2'):
                    ui.icon('code', size='md')
                    ui.label('Direct API').classes('text-lg font-semibold')
                ui.label('POST /api/incident/create').classes('text-sm text-gray-500 mb-2')
                with ui.column().classes('gap-1 text-sm'):
                    ui.label('• Send incidents programmatically')
                    ui.label('• Full control over incident fields')
                    ui.label('• Supports media file uploads')
                    ui.label('• Requires authentication token')

    # Organizations table logic
    selected_orgs = set()
    orgs_with_integrations = []

    async def load_orgs_table():
        nonlocal orgs_with_integrations
        orgs_with_integrations = await fetch_all_organizations_with_integrations()
        render_orgs_table()

    def render_orgs_table():
        orgs_table_container.clear()

        # Filter organizations
        search_term = search_input.value.lower() if search_input.value else ''
        type_filter_value = type_filter.value if type_filter.value else 'all'

        filtered_orgs = [
            org for org in orgs_with_integrations
            if (search_term in org['name'].lower() or search_term in org['type'].lower())
            and (type_filter_value == 'all' or org['type'] == type_filter_value)
        ]

        with orgs_table_container:
            # Prepare table data - NO selection column, we'll use NiceGUI built-in
            columns = [
                {'name': 'name', 'label': 'Organization', 'field': 'name', 'align': 'left', 'sortable': True},
                {'name': 'type', 'label': 'Type', 'field': 'type', 'align': 'left', 'sortable': True},
                {'name': 'integrations', 'label': 'Assigned Integrations', 'field': 'integrations', 'align': 'left', 'sortable': False},
            ]

            rows = []
            for org in filtered_orgs:
                # Format integrations
                if org['integrations']:
                    integration_names = ', '.join([i['name'] for i in org['integrations']])
                else:
                    integration_names = 'None'

                rows.append({
                    'id': org['id'],
                    'name': org['name'],
                    'type': org['type'].replace('_', ' ').title(),
                    'integrations': integration_names,
                })

            # Create table - selection='multiple' adds ONE checkbox column automatically
            table = ui.table(
                columns=columns,
                rows=rows,
                row_key='id',
                selection='multiple'
            ).props('flat dense').classes('w-full')

            # Bind selection to our selected_orgs set
            def update_selection(e):
                selected_orgs.clear()
                for row in table.selected:
                    selected_orgs.add(row['id'])

            table.on('update:selected', update_selection)

    # Button handlers
    def select_all_orgs():
        selected_orgs.clear()
        selected_orgs.update([org['id'] for org in orgs_with_integrations])
        render_orgs_table()

    def deselect_all_orgs():
        selected_orgs.clear()
        render_orgs_table()

    def select_filtered():
        # Select only the currently filtered organizations
        search_term = search_input.value.lower() if search_input.value else ''
        type_filter_value = type_filter.value if type_filter.value else 'all'

        filtered_org_ids = [
            org['id'] for org in orgs_with_integrations
            if (search_term in org['name'].lower() or search_term in org['type'].lower())
            and (type_filter_value == 'all' or org['type'] == type_filter_value)
        ]

        selected_orgs.clear()
        selected_orgs.update(filtered_org_ids)
        render_orgs_table()

    async def assign_to_selected():
        if not selected_orgs:
            ui.notify('Please select at least one organization', type='warning')
            return

        if not template_select.value:
            ui.notify('Please select an integration type', type='warning')
            return

        # Get template info
        template = next((t for t in templates if t['id'] == template_select.value), None)
        if not template:
            ui.notify('Invalid integration template', type='negative')
            return

        # Create integration for each selected organization
        success_count = 0
        error_count = 0

        for org_id in selected_orgs:
            try:
                # Get the template config - use defaults for now
                # Webhook/SEDAP/Email configs are handled by the template
                data = {
                    'organization_id': org_id,
                    'template_id': template_select.value,
                    'name': f'{template["name"]}',
                    'description': f'Auto-assigned {template["name"]}',
                    'config': {},  # Empty config, uses template defaults
                    'auth_credentials': {},
                    'trigger_filters': {},  # Empty dict, not None
                    'active': True,
                    'created_by': 'dashboard_batch'
                }
                await create_organization_integration(data)
                success_count += 1
            except Exception as e:
                logger.error(f"Error creating integration for org {org_id}: {e}")
                error_count += 1

        if error_count > 0:
            ui.notify(f'Assigned {success_count} integrations, {error_count} failed', type='warning')
        else:
            ui.notify(f'Successfully assigned {success_count} integrations', type='positive')

        # Reload tables
        await render_manage_integrations()
        await load_orgs_table()

    # Create buttons now that handlers are defined - tactical style
    with selection_buttons_row:
        ui.button('ALL', on_click=select_all_orgs).props('flat dense').classes('text-xs font-mono')
        ui.button('FLTRD', on_click=select_filtered).props('flat dense').classes('text-xs font-mono')
        ui.button('NONE', on_click=deselect_all_orgs).props('flat dense').classes('text-xs font-mono')

    # Add the assign button to top row - styled like logout button
    with assign_button_row:
        ui.button('Assign Selected', icon='send', on_click=assign_to_selected).props('outline color=white').classes('logout-btn')

    # Wire up search and type filter
    search_input.on_value_change(lambda: render_orgs_table())
    type_filter.on_value_change(lambda: render_orgs_table())

    # Initial data load
    async def initial_load():
        await refresh_data()
        if templates:
            template_select.options = {t['id']: t['name'] for t in templates}
            template_select.update()
        render_templates()
        await render_manage_integrations()
        await load_orgs_table()

    ui.timer(0.1, initial_load, once=True)

    # Status update script (same as dashboard)
    status_update_js = '''
        (function() {
            function updateStatus() {
                const statusTimestamp = document.getElementById('system-status-timestamp');
                const statusDot = document.getElementById('system-status-dot');
                const statusText = document.getElementById('system-status-text');

                if (statusTimestamp && statusDot && statusText) {
                    const now = new Date();
                    const timeStr = now.toLocaleTimeString('en-US', {
                        hour12: false,
                        hour: '2-digit',
                        minute: '2-digit',
                        second: '2-digit'
                    });
                    statusTimestamp.textContent = timeStr;
                    statusDot.className = 'w-2 h-2 bg-[#00FF00]';
                    statusText.textContent = 'System Operational';
                }
            }

            updateStatus();
            setInterval(updateStatus, 1000);
        })();
    '''
    ui.run_javascript(status_update_js)
