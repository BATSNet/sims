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

    def show_batch_assign_dialog():
        """Show dialog for batch assigning organizations to an integration"""
        if not templates:
            ui.notify('No integration templates available', type='warning')
            return

        with ui.dialog() as dialog, ui.card().classes('w-full max-w-4xl bg-[#0a1929]'):
            ui.label('Batch Assign Integration').classes('text-2xl font-bold mb-6')
            ui.label('Create the same integration for multiple organizations at once').classes('text-gray-400 mb-6')

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

            # Organization selection
            ui.label('Select Organizations').classes('text-lg font-bold mt-6 mb-2')
            ui.label('Choose which organizations should receive this integration').classes('text-sm text-gray-400 mb-3')

            # Create checkboxes for organizations
            org_checkboxes = {}
            with ui.column().classes('w-full gap-2 max-h-60 overflow-y-auto'):
                for org in organizations:
                    org_checkboxes[org['id']] = ui.checkbox(org['name'], value=False)

            with ui.row().classes('w-full gap-2 mt-4'):
                ui.button('Select All', on_click=lambda: [cb.set_value(True) for cb in org_checkboxes.values()]).props('flat size=sm')
                ui.button('Deselect All', on_click=lambda: [cb.set_value(False) for cb in org_checkboxes.values()]).props('flat size=sm')

            async def batch_create():
                try:
                    # Get selected organizations
                    selected_orgs = [org_id for org_id, checkbox in org_checkboxes.items() if checkbox.value]

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

                    trigger_filters = {}
                    if priorities_input.value:
                        trigger_filters['priorities'] = [p.strip() for p in priorities_input.value.split(',')]
                    if categories_input.value:
                        trigger_filters['categories'] = [c.strip() for c in categories_input.value.split(',')]

                    # Create integration for each selected organization
                    success_count = 0
                    error_count = 0

                    for org_id in selected_orgs:
                        try:
                            data = {
                                'organization_id': org_id,
                                'template_id': template_select.value,
                                'name': name_input.value,
                                'description': desc_input.value,
                                'config': config,
                                'auth_credentials': auth_credentials,
                                'trigger_filters': trigger_filters if trigger_filters else None,
                                'active': active_switch.value,
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
        with ui.dialog() as dialog, ui.card().classes('w-full max-w-3xl bg-[#0a1929]'):
            ui.label(template['name']).classes('text-2xl font-bold mb-2')
            ui.label(f"Type: {template['type']}").classes('text-sm text-[#63ABFF] mb-6')

            if template.get('description'):
                ui.label('Description').classes('text-lg font-bold mb-2')
                ui.label(template['description']).classes('text-gray-400 mb-6')

            # Configuration schema
            ui.label('Required Configuration').classes('text-lg font-bold mb-2')
            config_schema = template.get('config_schema', {})
            if config_schema:
                with ui.column().classes('w-full gap-2 mb-6'):
                    for field, details in config_schema.items():
                        field_type = details.get('type', 'string') if isinstance(details, dict) else 'string'
                        required = details.get('required', False) if isinstance(details, dict) else False
                        desc = details.get('description', '') if isinstance(details, dict) else ''

                        with ui.element('div').classes('p-3 rounded bg-[#0d2637]'):
                            with ui.row().classes('items-center gap-2 mb-1'):
                                ui.label(field).classes('font-bold')
                                if required:
                                    ui.badge('Required', color='red').classes('text-xs')
                                ui.label(f"({field_type})").classes('text-xs text-gray-500')
                            if desc:
                                ui.label(desc).classes('text-sm text-gray-400')
            else:
                ui.label('No specific configuration required').classes('text-gray-400 mb-6')

            # Authentication
            ui.label('Authentication').classes('text-lg font-bold mb-2')
            auth_type = template.get('auth_type', 'none')
            auth_schema = template.get('auth_schema', {})

            if auth_type and auth_type != 'none':
                ui.label(f"Type: {auth_type}").classes('text-gray-400 mb-2')
                if auth_schema:
                    with ui.column().classes('w-full gap-2 mb-6'):
                        for field, details in auth_schema.items():
                            with ui.element('div').classes('p-3 rounded bg-[#0d2637]'):
                                ui.label(field).classes('font-bold mb-1')
                                if isinstance(details, dict) and details.get('description'):
                                    ui.label(details['description']).classes('text-sm text-gray-400')
            else:
                ui.label('No authentication required').classes('text-gray-400 mb-6')

            # Configuration instructions
            ui.label('How to Configure').classes('text-lg font-bold mb-2')
            template_type = template['type']

            config_instructions = {
                'webhook': {
                    'steps': [
                        '1. Get your webhook URL from your service (Zapier, n8n, Make.com, webhook.site, etc.)',
                        '2. Copy the webhook URL to the "Endpoint URL" field',
                        '3. If your service requires authentication, add the Bearer token or API key',
                        '4. Set trigger filters if you only want specific incident types',
                        '5. Click "Test Connection" to verify it works'
                    ],
                    'where': 'Works with: Zapier, Make.com, n8n, webhook.site, custom HTTP endpoints, Discord webhooks, Slack webhooks'
                },
                'sedap': {
                    'steps': [
                        '1. Obtain SEDAP.Express API endpoint from your BMS administrator',
                        '2. Get authentication credentials (username/password or API key)',
                        '3. Enter the SEDAP endpoint URL in "Endpoint URL" field',
                        '4. Add authentication credentials in the Auth section',
                        '5. Configure message format settings (CSV format, CONTACT/TEXT types)',
                        '6. Test connection before activating'
                    ],
                    'where': 'Required for: Bundeswehr BMS integration, NATO STANAG 4406 systems, military command & control centers'
                },
                'email': {
                    'steps': [
                        '1. Configure SMTP server settings (host, port)',
                        '2. For Gmail: Use smtp.gmail.com:587 and App Password (not regular password)',
                        '3. For SendGrid/Mailgun: Use their SMTP credentials',
                        '4. Enter sender email address (from_email)',
                        '5. Add recipient email addresses (comma-separated for multiple)',
                        '6. Test email delivery before activating'
                    ],
                    'where': 'Works with: Gmail, Outlook/Office365, SendGrid, Mailgun, Mailtrap, any SMTP server'
                }
            }

            if template_type in config_instructions:
                instructions = config_instructions[template_type]

                with ui.column().classes('w-full gap-2 mb-4'):
                    for step in instructions['steps']:
                        with ui.row().classes('items-start gap-2'):
                            ui.label(step.split('.')[0] + '.').classes('text-[#63ABFF] font-bold')
                            ui.label('.'.join(step.split('.')[1:])).classes('text-gray-400')

                with ui.element('div').classes('w-full p-3 rounded bg-[#0d2637] border border-[#1e3a4f] mb-6'):
                    ui.label('Where to use this:').classes('font-bold text-[#63ABFF] mb-1')
                    ui.label(instructions['where']).classes('text-sm text-gray-400')

            # Use cases
            ui.label('Common Use Cases').classes('text-lg font-bold mb-2')

            use_cases = {
                'webhook': [
                    'Send incident alerts to Zapier workflows',
                    'Forward incidents to n8n automation',
                    'Trigger custom webhooks on incident creation',
                    'Integrate with Make.com scenarios',
                    'Connect to custom HTTP endpoints'
                ],
                'sedap': [
                    'Forward incidents to military BMS (Battle Management System)',
                    'SEDAP.Express integration via CSV format',
                    'NATO STANAG 4406 compatible messaging',
                    'Bundeswehr command & control integration'
                ],
                'email': [
                    'Send email notifications to responders',
                    'Alert on-call teams via email',
                    'Forward incident reports to distribution lists',
                    'Automated incident documentation via email'
                ]
            }

            if template_type in use_cases:
                with ui.column().classes('w-full gap-2 mb-6'):
                    for use_case in use_cases[template_type]:
                        with ui.row().classes('items-start gap-2'):
                            ui.label('•').classes('text-[#63ABFF]')
                            ui.label(use_case).classes('text-gray-400')

            # Example configuration
            if template.get('payload_template'):
                ui.label('Payload Template').classes('text-lg font-bold mb-2')
                ui.label('This is the data structure sent to the endpoint:').classes('text-sm text-gray-400 mb-2')
                with ui.element('div').classes('w-full p-3 rounded bg-[#0d2637] overflow-x-auto'):
                    ui.label(template['payload_template'][:500] + ('...' if len(template['payload_template']) > 500 else '')).classes('text-xs font-mono text-gray-300')

            ui.button('Close', on_click=dialog.close).classes('mt-6').props('outline')

        dialog.open()

    def render_templates():
        """Render templates list"""
        templates_container.clear()
        with templates_container:
            if templates:
                for template in templates:
                    with ui.element('div').classes('w-full mb-3 p-4 rounded border border-[#1e3a4f] hover:bg-[#0d2637] transition-all cursor-pointer'):
                        with ui.row().classes('w-full items-center gap-4'):
                            with ui.column().classes('flex-grow'):
                                with ui.row().classes('items-center gap-2 mb-1'):
                                    ui.label(template['name']).classes('text-lg font-bold')
                                    ui.label(f"({template['type']})").classes('text-sm text-[#63ABFF]')
                                if template.get('description'):
                                    ui.label(template['description']).classes('text-sm text-gray-400')

                            ui.button(
                                'View Details',
                                on_click=lambda t=template: show_template_details(t),
                                icon='info'
                            ).props('flat color=blue')
            else:
                ui.label('No templates available').classes('text-gray-400')

    # Main UI - wrap in content container with proper width
    with ui.element('div').classes('content-container'):
        ui.label('Integration Management').classes('section-title')

        # Tabs
        with ui.tabs().classes('w-full mb-4') as tabs:
            tab_integrations = ui.tab('Push to Organizations').classes('w-full')
            tab_templates = ui.tab('View Templates').classes('w-full')
            tab_connect = ui.tab('Push to SIMS').classes('w-full')

        with ui.tab_panels(tabs, value=tab_integrations).classes('w-full'):
            # Integrations tab
            with ui.tab_panel(tab_integrations).classes('w-full'):
                # Organization selector
                with ui.row().classes('w-full items-end gap-4 mb-6'):
                    org_select = ui.select(
                        label='Organization',
                        options={},
                        on_change=on_org_selected
                    ).classes('flex-1')

                    ui.button(
                        'Batch Assign',
                        on_click=show_batch_assign_dialog,
                        icon='group_add'
                    ).props('outline')

                    ui.button(
                        'New Integration',
                        on_click=show_create_dialog,
                        icon='add'
                    ).props('color=primary')

                # Stats section
                stats_container = ui.column().classes('w-full mb-6')

                # Integrations list
                integration_container = ui.column().classes('w-full')

            # Templates tab
            with ui.tab_panel(tab_templates).classes('w-full'):
                ui.label('Available Integration Templates').classes('text-xl font-bold mb-4')
                ui.label('These are the types of integrations you can configure').classes('text-gray-400 mb-6')
                templates_container = ui.column().classes('w-full')

            # Connect to SIMS tab
            with ui.tab_panel(tab_connect).classes('w-full'):
                ui.label('How to Connect TO SIMS').classes('text-xl font-bold mb-2')
                ui.label('Ways to send incidents into the SIMS system').classes('text-gray-400 mb-6')

                with ui.row().classes('w-full gap-4'):
                    # Mobile App
                    with ui.element('div').classes('flex-1 p-4 rounded border border-[#1e3a4f]'):
                        ui.label('Mobile App').classes('text-lg font-bold mb-2')
                        ui.label('Android').classes('text-sm text-[#63ABFF] mb-2')
                        with ui.column().classes('gap-2'):
                            ui.label('• Download and install SIMS APK').classes('text-sm text-gray-400')
                            ui.label('• Register with organization code').classes('text-sm text-gray-400')
                            ui.label('• Report incidents with photos, voice, location').classes('text-sm text-gray-400')
                            ui.label('• iOS version planned for future release').classes('text-xs text-gray-500 mt-2')

                    # Inbound Webhook
                    with ui.element('div').classes('flex-1 p-4 rounded border border-[#1e3a4f]'):
                        ui.label('Inbound Webhook').classes('text-lg font-bold mb-2')
                        ui.label('Receive from external systems').classes('text-sm text-[#63ABFF] mb-2')
                        with ui.column().classes('gap-2'):
                            ui.label('• Create webhook endpoint in system').classes('text-sm text-gray-400')
                            ui.label('• Configure field mapping (JSONPath)').classes('text-sm text-gray-400')
                            ui.label('• External systems POST incidents to webhook URL').classes('text-sm text-gray-400')
                            ui.label('• Auto-routes to assigned organization').classes('text-sm text-gray-400')

                    # API Direct
                    with ui.element('div').classes('flex-1 p-4 rounded border border-[#1e3a4f]'):
                        ui.label('Direct API').classes('text-lg font-bold mb-2')
                        ui.label('POST /api/incident/create').classes('text-sm text-[#63ABFF] mb-2')
                        with ui.column().classes('gap-2'):
                            ui.label('• Send incidents programmatically').classes('text-sm text-gray-400')
                            ui.label('• Full control over incident fields').classes('text-sm text-gray-400')
                            ui.label('• Supports media file uploads').classes('text-sm text-gray-400')
                            ui.label('• Requires authentication token').classes('text-sm text-gray-400')

    # Initial data load
    async def initial_load():
        await refresh_data()
        if organizations:
            org_select.options = {org['id']: org['name'] for org in organizations}
            org_select.update()
        render_templates()

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
