"""
SIMS Backend Theme Configuration
Military-inspired design based on modern defense tech aesthetics
"""
from contextlib import asynccontextmanager
from nicegui import ui


# Color scheme
COLORS = {
    'primary': '#0D2637',        # Navy blue background
    'gradient_start': '#0D2637',
    'gradient_end': '#0a1d2a',
    'secondary': '#63ABFF',      # Light blue accent
    'accent': '#FF4444',         # Red accent
    'critical': '#FF4444',       # Critical alerts
    'high': '#ffa600',          # High priority
    'medium': '#63ABFF',        # Medium priority
    'low': 'rgba(255, 255, 255, 0.4)',  # Low priority
    'text_primary': '#ffffff',
    'text_secondary': 'rgba(255, 255, 255, 0.8)',
    'text_muted': 'rgba(255, 255, 255, 0.6)',
    'border': 'rgba(255, 255, 255, 0.1)',
    'card_bg': 'rgba(26, 31, 46, 0.6)',
    'hover_bg': 'rgba(26, 31, 46, 0.4)',
}


def apply_theme():
    """Apply the SIMS military theme colors and styles"""
    ui.colors(
        primary=COLORS['primary'],
        secondary=COLORS['secondary'],
        accent=COLORS['accent'],
        positive=COLORS['accent'],
        negative=COLORS['critical'],
        warning=COLORS['high'],
        info=COLORS['medium'],
        dark=COLORS['primary'],
        dark_page=COLORS['primary']
    )


def inject_custom_css():
    """Inject custom CSS for military-themed styling"""
    ui.add_head_html('''
        <link rel="preconnect" href="https://fonts.googleapis.com">
        <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
        <link href="https://fonts.googleapis.com/css2?family=Stack+Sans+Notch:wght@400;700&family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
        <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
        <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
        <style>
            /* Base styling */
            body {
                background: linear-gradient(180deg, #0D2637 0%, #0a1d2a 100%);
                font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
                color: #fff;
                font-size: 15px;
            }

            /* Typography - Titles */
            h1, h2, h3, .header-title, .section-title, .dashboard-title, .table-title, .title-font {
                font-family: 'Stack Sans Notch', -apple-system, BlinkMacSystemFont, sans-serif;
            }

            /* Page title in header */
            .page-title {
                font-family: 'Stack Sans Notch', -apple-system, BlinkMacSystemFont, sans-serif;
                font-weight: 900;
                font-size: 1.75rem;
                color: #ccc;
            }

            /* Logout button */
            .logout-btn {
                font-family: 'Stack Sans Notch', -apple-system, BlinkMacSystemFont, sans-serif;
            }

            /* Sidebar logo */
            .sidebar-logo {
                width: 208px;
                height: auto;
            }

            /* Sidebar and header styling */
            .q-drawer a,
            .q-header a {
                text-decoration: none !important;
            }

            /* Menu labels use title font */
            .q-drawer .q-item__label {
                font-family: 'Stack Sans Notch', -apple-system, BlinkMacSystemFont, sans-serif;
            }

            /* Custom scrollbar */
            ::-webkit-scrollbar {
                width: 8px;
                height: 8px;
            }

            ::-webkit-scrollbar-track {
                background: rgba(26, 74, 111, 0.1);
            }

            ::-webkit-scrollbar-thumb {
                background: rgba(99, 171, 255, 0.3);
                border-radius: 4px;
            }

            ::-webkit-scrollbar-thumb:hover {
                background: rgba(99, 171, 255, 0.5);
            }

            /* Overview Section - Map and Stats */
            .overview-section {
                display: grid;
                grid-template-columns: 1fr 320px;
                gap: 0;
                margin: 0 0 3rem;
                background: rgba(26, 31, 46, 0.6);
                border-left: 3px solid rgba(255, 68, 68, 0.3);
                width: 100%;
            }

            .map-container {
                height: 500px;
                min-height: 500px;
            }

            /* Make leaflet map extend */
            .overview-section .leaflet-container {
                height: 100%;
            }

            /* Container for table and other content with max width and padding */
            .content-container {
                max-width: 96rem;
                margin: 0 auto;
                padding: 0 1.5rem;
            }

            .stats-column {
                display: flex;
                flex-direction: column;
                gap: 0;
                border-left: 1px solid rgba(255, 255, 255, 0.1);
            }

            .metric-card {
                background: rgba(13, 38, 55, 0.3);
                padding: 24px;
                border-bottom: 1px solid rgba(255, 255, 255, 0.05);
                transition: background 0.2s ease;
            }

            .metric-card:hover {
                background: rgba(13, 38, 55, 0.5);
            }

            .metric-label {
                font-size: 12px;
                color: rgba(255, 255, 255, 0.4);
                text-transform: uppercase;
                letter-spacing: 1px;
                margin-bottom: 12px;
                font-weight: 500;
            }

            .metric-value {
                font-size: 42px;
                font-weight: 600;
                color: #fff;
                line-height: 1;
            }

            /* Section Title */
            .section-title {
                font-size: 32px;
                font-weight: 300;
                color: #fff;
                letter-spacing: 2px;
                margin: 0 0 1.5rem;
            }

            /* Table Section */
            .table-section {
                background: rgba(26, 31, 46, 0.6);
                margin: 0;
            }

            .table-header-bar {
                display: flex;
                justify-content: space-between;
                align-items: center;
                padding: 20px 24px;
                border-bottom: 1px solid rgba(255, 255, 255, 0.1);
                border-left: 3px solid rgba(255, 68, 68, 0.3);
            }

            .table-title {
                font-size: 16px;
                font-weight: 600;
                color: #fff;
                letter-spacing: 1px;
                text-transform: uppercase;
            }

            .table-actions {
                display: flex;
                gap: 8px;
            }

            .filter-btn {
                background: transparent;
                border: 1px solid rgba(255, 255, 255, 0.15);
                color: rgba(255, 255, 255, 0.6);
                padding: 6px 14px;
                font-size: 11px;
                text-transform: uppercase;
                letter-spacing: 0.5px;
                cursor: pointer;
                transition: all 0.2s ease;
                font-weight: 500;
            }

            .filter-btn:hover {
                border-color: rgba(255, 255, 255, 0.3);
                color: rgba(255, 255, 255, 0.9);
            }

            .filter-btn.active {
                background: rgba(255, 68, 68, 0.1);
                border-color: #FF4444;
                color: #FF4444;
            }

            /* Table styling */
            .q-table thead tr,
            .q-table tbody td {
                background: rgba(13, 38, 55, 0.2);
            }

            .q-table thead th {
                font-size: 11px;
                color: rgba(255, 255, 255, 0.5);
                text-transform: uppercase;
                letter-spacing: 1px;
                font-weight: 600;
                background: rgba(13, 38, 55, 0.4);
                border-bottom: 1px solid rgba(255, 255, 255, 0.1);
            }

            .q-table tbody tr {
                transition: all 0.15s ease;
                border-bottom: 1px solid rgba(255, 255, 255, 0.05);
                border-left: 3px solid transparent;
            }

            .q-table tbody tr:hover {
                background: rgba(26, 31, 46, 0.4);
                border-left-color: #FF4444;
            }

            .q-table tbody td {
                font-size: 14px;
                color: rgba(255, 255, 255, 0.8);
            }

            /* Priority badges */
            .priority-badge {
                display: inline-block;
                padding: 4px 10px;
                font-size: 10px;
                font-weight: 600;
                text-transform: uppercase;
                letter-spacing: 0.5px;
            }

            .priority-critical {
                color: #FF4444;
                background: rgba(255, 68, 68, 0.1);
            }

            .priority-high {
                color: #ffa600;
                background: rgba(255, 166, 0, 0.1);
            }

            .priority-medium {
                color: #63ABFF;
                background: rgba(99, 171, 255, 0.1);
            }

            .priority-low {
                color: rgba(255, 255, 255, 0.4);
                background: rgba(255, 255, 255, 0.05);
            }

            /* Action button */
            .action-button {
                background: transparent;
                border: 1px solid rgba(255, 68, 68, 0.3);
                color: #FF4444;
                padding: 6px 14px;
                font-size: 10px;
                text-transform: uppercase;
                letter-spacing: 0.5px;
                cursor: pointer;
                transition: all 0.2s ease;
                font-weight: 600;
            }

            .action-button:hover {
                background: #FF4444;
                color: #0D2637;
                border-color: #FF4444;
            }

            /* Cell styling */
            .cell-id {
                font-size: 14px;
                color: #fff;
                font-weight: 600;
                letter-spacing: 0.5px;
            }

            .cell-time {
                font-size: 13px;
                color: rgba(255, 255, 255, 0.7);
            }

            .cell-location {
                font-size: 13px;
                color: rgba(255, 255, 255, 0.6);
            }

            .cell-description {
                font-size: 14px;
                color: rgba(255, 255, 255, 0.85);
            }

            /* Leaflet Map styling */
            .custom-marker {
                cursor: pointer !important;
            }

            .custom-marker div {
                pointer-events: none;
            }

            .leaflet-popup-content-wrapper {
                background: rgba(26, 31, 46, 0.95) !important;
                color: #fff;
                border-left: 3px solid #FF4444;
            }

            .leaflet-popup-content {
                margin: 12px 16px;
                font-size: 12px;
            }

            .leaflet-popup-tip {
                background: rgba(26, 31, 46, 0.95) !important;
            }

            .popup-title {
                font-weight: 600;
                font-size: 13px;
                margin-bottom: 8px;
                color: #fff;
            }

            .popup-description {
                color: rgba(255, 255, 255, 0.8);
                margin-bottom: 6px;
            }

            .popup-priority {
                font-size: 10px;
                font-weight: 600;
                text-transform: uppercase;
                padding: 3px 8px;
                display: inline-block;
                margin-top: 4px;
            }

            /* Mobile responsive */
            @media (max-width: 1400px) {
                .overview-section {
                    grid-template-columns: 1fr;
                }

                .stats-column {
                    display: grid;
                    grid-template-columns: repeat(4, 1fr);
                }
            }

            @media (max-width: 768px) {
                .overview-section {
                    grid-template-columns: 1fr;
                }

                .stats-column {
                    grid-template-columns: repeat(2, 1fr);
                }
            }

            /* NiceGUI specific overrides */
            .q-page {
                background: linear-gradient(180deg, #0D2637 0%, #0a1d2a 100%);
            }

            .q-card {
                background: rgba(26, 31, 46, 0.6);
                border: 1px solid rgba(255, 255, 255, 0.1);
            }

            .q-table {
                background: transparent;
            }

            .q-table__card {
                background: transparent;
                border: none;
                box-shadow: none;
            }

            .q-btn {
                text-transform: none;
            }
        </style>
    ''')


@asynccontextmanager
async def frame(title: str = "SIMS Command"):
    """
    Main frame context manager for SIMS pages

    Args:
        title: Page title to display
    """
    # Apply theme
    apply_theme()
    inject_custom_css()

    # Enable dark mode
    ui.dark_mode(True)

    # Sidebar with logo
    with ui.left_drawer(top_corner=True, bottom_corner=False).classes('w-64 bg-[#0D2637]'):
        with ui.column().classes('items-center p-6'):
            ui.image('/static/sims-logo.svg').props('fit=scale-down').classes('sidebar-logo')

        with ui.column().classes('flex-1 p-6 space-y-1'):
            ui.label('Command Center').classes('text-xs font-bold text-gray-400 mb-2 title-font')

            with ui.link(target='/').classes('flex items-center gap-2 px-4 py-2 text-white hover:bg-[#FF4444] hover:bg-opacity-20 rounded no-underline'):
                ui.icon('dashboard')
                ui.label('Dashboard').classes('title-font')

        with ui.column().classes('w-full p-6 mt-auto'):
            ui.button('Logout', on_click=lambda: None).props('outline color=white').classes('w-full logout-btn')

    # Header
    with ui.header(elevated=False, bordered=False).classes('bg-[#0D2637] border-b border-[rgba(255,255,255,0.1)]'):
        with ui.row().classes('items-end justify-between w-full px-6 py-3'):
            ui.label(title).classes('page-title')
            with ui.row().classes('items-center gap-4'):
                ui.label('System Operational').classes('text-sm text-gray-400')
                ui.element('div').classes('w-2 h-2 bg-[#FF4444] rounded-full')

    # Main content area - no padding, let individual sections control their own layout
    with ui.column().classes('w-full'):
        # Yield to allow content to be added
        yield
