"""
Organization Management Page
Manage organizations that can respond to incidents
"""
import logging
from typing import List, Dict, Optional
import httpx
from nicegui import ui
import theme

logger = logging.getLogger(__name__)

# Organization types
ORG_TYPES = [
    {'value': 'military', 'label': 'Military'},
    {'value': 'police', 'label': 'Police'},
    {'value': 'fire', 'label': 'Fire Services'},
    {'value': 'medical', 'label': 'Medical Services'},
    {'value': 'civil_defense', 'label': 'Civil Defense'},
    {'value': 'government', 'label': 'Government'},
    {'value': 'other', 'label': 'Other'},
]

API_BASE = "http://localhost:8000/api"


async def organizations_page():
    """Organization management page"""

    # State
    organizations = []
    search_term = ''
    type_filter = None
    selected_org = None

    async def load_organizations():
        """Fetch organizations from API"""
        nonlocal organizations
        try:
            params = {'limit': 200, 'active_only': False}
            if search_term:
                params['search'] = search_term
            if type_filter:
                params['type'] = type_filter

            async with httpx.AsyncClient() as client:
                response = await client.get(f"{API_BASE}/organization/", params=params)
                if response.status_code == 200:
                    organizations = response.json()
                    org_table.refresh()
                else:
                    ui.notify(f'Failed to load organizations: {response.status_code}', type='negative')
        except Exception as e:
            logger.error(f"Error loading organizations: {e}", exc_info=True)
            ui.notify(f'Error loading organizations: {str(e)}', type='negative')

    async def delete_organization(org_id: int):
        """Delete an organization"""
        try:
            async with httpx.AsyncClient() as client:
                response = await client.delete(f"{API_BASE}/organization/{org_id}")
                if response.status_code == 204:
                    ui.notify('Organization deleted successfully', type='positive')
                    await load_organizations()
                else:
                    ui.notify(f'Failed to delete organization: {response.status_code}', type='negative')
        except Exception as e:
            logger.error(f"Error deleting organization: {e}", exc_info=True)
            ui.notify(f'Error deleting organization: {str(e)}', type='negative')

    async def save_organization(org_data: Dict, org_id: Optional[int] = None):
        """Create or update an organization"""
        try:
            async with httpx.AsyncClient() as client:
                if org_id:
                    # Update existing
                    response = await client.put(f"{API_BASE}/organization/{org_id}", json=org_data)
                else:
                    # Create new
                    response = await client.post(f"{API_BASE}/organization/", json=org_data)

                if response.status_code in [200, 201]:
                    ui.notify('Organization saved successfully', type='positive')
                    await load_organizations()
                    return True
                else:
                    ui.notify(f'Failed to save organization: {response.status_code}', type='negative')
                    return False
        except Exception as e:
            logger.error(f"Error saving organization: {e}", exc_info=True)
            ui.notify(f'Error saving organization: {str(e)}', type='negative')
            return False

    def show_org_dialog(org: Optional[Dict] = None):
        """Show dialog for creating/editing organization"""
        is_edit = org is not None

        with ui.dialog() as dialog, ui.card().classes('w-full max-w-3xl p-6'):
            ui.label(f"{'Edit' if is_edit else 'Add'} Organization").classes('text-h6 title-font mb-4')

            with ui.grid(columns=2).classes('w-full gap-4'):
                name_input = ui.input('Name *', value=org.get('name') if org else '').classes('col-span-2')
                short_name_input = ui.input('Short Name', value=org.get('short_name') if org else '')

                type_select = ui.select(
                    label='Type *',
                    options={t['value']: t['label'] for t in ORG_TYPES},
                    value=org.get('type') if org else None
                ).classes('w-full')

                contact_person_input = ui.input('Contact Person', value=org.get('contact_person') if org else '')
                phone_input = ui.input('Phone', value=org.get('phone') if org else '')
                email_input = ui.input('Email', value=org.get('email') if org else '')
                emergency_phone_input = ui.input('Emergency Phone', value=org.get('emergency_phone') if org else '')

                address_input = ui.input('Address', value=org.get('address') if org else '').classes('col-span-2')
                city_input = ui.input('City', value=org.get('city') if org else '')
                country_input = ui.input('Country', value=org.get('country', 'Germany') if org else 'Germany')

                response_area_input = ui.textarea('Response Area', value=org.get('response_area') if org else '').classes('col-span-2')
                notes_input = ui.textarea('Notes', value=org.get('notes') if org else '').classes('col-span-2')

                active_switch = ui.switch('Active', value=org.get('active', True) if org else True)

            ui.separator().classes('my-4')

            with ui.row().classes('w-full justify-end gap-2'):
                ui.button('Cancel', on_click=dialog.close).props('flat')

                async def on_save():
                    # Validate required fields
                    if not name_input.value or not type_select.value:
                        ui.notify('Name and Type are required', type='warning')
                        return

                    org_data = {
                        'name': name_input.value,
                        'short_name': short_name_input.value or None,
                        'type': type_select.value,
                        'contact_person': contact_person_input.value or None,
                        'phone': phone_input.value or None,
                        'email': email_input.value or None,
                        'emergency_phone': emergency_phone_input.value or None,
                        'address': address_input.value or None,
                        'city': city_input.value or None,
                        'country': country_input.value or 'Germany',
                        'response_area': response_area_input.value or None,
                        'notes': notes_input.value or None,
                        'active': active_switch.value,
                        'capabilities': []
                    }

                    success = await save_organization(org_data, org.get('id') if org else None)
                    if success:
                        dialog.close()

                ui.button('Save', on_click=on_save).props('color=primary')

        dialog.open()

    # Content container
    with ui.element('div').classes('content-container'):
        # Header with Add button
        with ui.row().classes('w-full items-center justify-between mb-6'):
            ui.button('Add Organization', icon='add', on_click=lambda: show_org_dialog()).props('outline color=white')

        # Filters
        with ui.row().classes('w-full gap-4 items-end mb-6'):
            async def on_search_change(e):
                nonlocal search_term
                search_term = e.value
                await load_organizations()

            ui.input(
                'Search',
                placeholder='Search by name, short name, or city',
                on_change=on_search_change
            ).classes('flex-grow').props('clearable')

            async def on_type_filter_change(e):
                nonlocal type_filter
                type_filter = e.value
                await load_organizations()

            ui.select(
                label='Filter by Type',
                options={None: 'All Types', **{t['value']: t['label'] for t in ORG_TYPES}},
                value=None,
                on_change=on_type_filter_change
            ).classes('w-64')

            ui.button('Refresh', icon='refresh', on_click=load_organizations)

        # Organizations Table
        with ui.element('div').classes('table-section w-full'):
            def create_org_table():
                columns = [
                    {'name': 'name', 'label': 'Name', 'field': 'name', 'align': 'left', 'sortable': True},
                    {'name': 'short_name', 'label': 'Short Name', 'field': 'short_name', 'align': 'left'},
                    {'name': 'type', 'label': 'Type', 'field': 'type', 'align': 'left', 'sortable': True},
                    {'name': 'city', 'label': 'City', 'field': 'city', 'align': 'left'},
                    {'name': 'phone', 'label': 'Phone', 'field': 'phone', 'align': 'left'},
                    {'name': 'emergency_phone', 'label': 'Emergency', 'field': 'emergency_phone', 'align': 'left'},
                    {'name': 'active', 'label': 'Active', 'field': 'active', 'align': 'center'},
                    {'name': 'actions', 'label': 'Actions', 'field': 'actions', 'align': 'center'},
                ]

                rows = []
                for org in organizations:
                    # Format type display
                    type_label = next((t['label'] for t in ORG_TYPES if t['value'] == org['type']), org['type'])

                    rows.append({
                        'id': org['id'],
                        'name': org['name'],
                        'short_name': org.get('short_name', ''),
                        'type': type_label,
                        'city': org.get('city', ''),
                        'phone': org.get('phone', ''),
                        'emergency_phone': org.get('emergency_phone', ''),
                        'active': 'Yes' if org.get('active') else 'No',
                        'contact_person': org.get('contact_person', ''),
                        'email': org.get('email', ''),
                        'address': org.get('address', ''),
                        'response_area': org.get('response_area', ''),
                        'notes': org.get('notes', ''),
                        '_org': org
                    })

                table = ui.table(
                    columns=columns,
                    rows=rows,
                    row_key='id',
                    pagination={'rowsPerPage': 10, 'sortBy': 'name'}
                ).classes('w-full')

                table.add_slot('body-cell-actions', '''
                    <q-td :props="props">
                        <q-btn flat dense icon="edit" color="primary" size="sm" @click="$parent.$emit('edit-org', props.row.id)">
                            <q-tooltip>Edit</q-tooltip>
                        </q-btn>
                        <q-btn flat dense icon="delete" color="negative" size="sm" @click="$parent.$emit('delete-org', props.row.id)">
                            <q-tooltip>Delete</q-tooltip>
                        </q-btn>
                    </q-td>
                ''')

                async def on_edit(e):
                    org_id = e.args
                    org = next((o for o in organizations if o['id'] == org_id), None)
                    if org:
                        show_org_dialog(org)

                async def on_delete(e):
                    org_id = e.args
                    await delete_organization(org_id)

                table.on('edit-org', on_edit)
                table.on('delete-org', on_delete)

                return table

            org_table = ui.refreshable(create_org_table)
            org_table()

    # Initial load
    await load_organizations()
