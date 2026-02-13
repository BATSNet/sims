"""
SIMS Responder Theme Configuration
Teal/turquoise variant for organization responders
"""
from contextlib import asynccontextmanager
from nicegui import ui


# Color scheme - Modern tactical teal variant
COLORS = {
    # Base - Deep tactical slate/charcoal
    'background': '#0A0E12',           # Darker, more sophisticated
    'background_light': '#121820',     # Cards, elevated surfaces
    'background_elevated': '#1A2028',  # Modals, overlays
    'background_card': 'rgba(18, 24, 32, 0.8)',  # Semi-transparent cards

    # Teal accents - Muted, professional
    'accent_teal': '#4A7C59',          # Muted teal-green (primary CTA)
    'accent_cyan': '#2DD4BF',          # Status indicators, live updates
    'accent_amber': '#F59E0B',         # Warnings

    # Priority colors - Muted tones
    'critical': '#B91C1C',             # Deep red
    'high': '#D97706',                 # Burnt orange
    'medium': '#2DD4BF',               # Tactical cyan
    'low': '#4A7C59',                  # Muted green

    # Text colors
    'text_primary': '#FFFFFF',
    'text_secondary': 'rgba(255, 255, 255, 0.85)',
    'text_muted': 'rgba(255, 255, 255, 0.6)',

    # Borders and dividers
    'border': 'rgba(255, 255, 255, 0.08)',
    'border_accent': 'rgba(74, 124, 89, 0.3)',
}


def apply_theme():
    """Apply the SIMS responder theme colors and styles"""
    ui.colors(
        primary='#4A7C59',              # Tactical teal-green
        secondary='#2DD4BF',            # Cyan
        accent='#2DD4BF',               # Cyan
        dark='#0A0E12',                 # Deep background
        positive='#4A7C59',             # Success green
        negative='#B91C1C',             # Error red
        info='#2DD4BF',                 # Info cyan
        warning='#F59E0B',              # Warning amber
    )


def inject_custom_css():
    """Inject custom CSS for teal-themed responder styling"""
    ui.add_head_html('''
        <link rel="preconnect" href="https://fonts.googleapis.com">
        <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
        <link href="https://fonts.googleapis.com/css2?family=Stack+Sans+Notch:wght@400;700&family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
        <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
        <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
        <link rel="stylesheet" href="https://unpkg.com/leaflet.markercluster@1.5.3/dist/MarkerCluster.css" />
        <link rel="stylesheet" href="https://unpkg.com/leaflet.markercluster@1.5.3/dist/MarkerCluster.Default.css" />
        <script src="https://unpkg.com/leaflet.markercluster@1.5.3/dist/leaflet.markercluster.js"></script>
        <style>
            /* Base styling - Modern tactical teal theme */
            body {
                background: linear-gradient(180deg, #0A0E12 0%, #121820 100%);
                font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
                color: #fff;
                font-size: 15px;
                font-weight: 400;
                line-height: 1.6;
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
                color: #26C6DA;
            }

            /* Custom scrollbar - Teal */
            ::-webkit-scrollbar {
                width: 8px;
                height: 8px;
            }

            ::-webkit-scrollbar-track {
                background: rgba(0, 137, 123, 0.1);
            }

            ::-webkit-scrollbar-thumb {
                background: rgba(38, 198, 218, 0.3);
            }

            ::-webkit-scrollbar-thumb:hover {
                background: rgba(38, 198, 218, 0.5);
            }

            /* Chat container */
            .chat-container {
                display: grid;
                grid-template-columns: 1fr 320px;
                gap: 0;
                height: calc(100vh - 80px);
                background: #0A1929;
            }

            .chat-main {
                display: flex;
                flex-direction: column;
                height: 100%;
                border-right: 1px solid rgba(255, 255, 255, 0.1);
            }

            .chat-sidebar {
                background: #071220;
                border-left: 3px solid rgba(0, 137, 123, 0.3);
                display: flex;
                flex-direction: column;
                overflow-y: auto;
            }

            /* Chat messages area */
            .chat-messages {
                flex: 1;
                overflow-y: auto;
                padding: 24px;
                background: #0A1929;
            }

            .message-bubble {
                max-width: 70%;
                padding: 12px 16px;
                margin-bottom: 16px;
                border-radius: 8px;
                font-size: 14px;
                line-height: 1.5;
            }

            .message-user {
                margin-left: auto;
                background: rgba(99, 171, 255, 0.2);
                border-left: 3px solid #63ABFF;
                color: rgba(255, 255, 255, 0.9);
            }

            .message-assistant {
                margin-right: auto;
                background: rgba(38, 198, 218, 0.15);
                border-left: 3px solid #26C6DA;
                color: rgba(255, 255, 255, 0.9);
            }

            .message-timestamp {
                font-size: 11px;
                color: rgba(255, 255, 255, 0.4);
                margin-top: 4px;
            }

            /* Chat input area */
            .chat-input-area {
                background: #071220;
                border-top: 1px solid rgba(255, 255, 255, 0.1);
                padding: 16px 24px;
            }

            /* Incident list */
            .incident-list {
                background: #0A1929;
                padding: 0;
            }

            .incident-card {
                background: #071220;
                padding: 16px;
                border-bottom: 1px solid rgba(255, 255, 255, 0.05);
                border-left: 3px solid transparent;
                cursor: pointer;
                transition: all 0.2s ease;
            }

            .incident-card:hover {
                background: rgba(0, 137, 123, 0.1);
                border-left-color: #00897B;
            }

            .incident-card.active {
                background: rgba(0, 137, 123, 0.15);
                border-left-color: #26C6DA;
            }

            .incident-card-id {
                font-size: 13px;
                font-weight: 600;
                color: #26C6DA;
                letter-spacing: 0.5px;
                margin-bottom: 6px;
            }

            .incident-card-title {
                font-size: 14px;
                color: rgba(255, 255, 255, 0.9);
                margin-bottom: 4px;
                font-weight: 500;
            }

            .incident-card-time {
                font-size: 11px;
                color: rgba(255, 255, 255, 0.4);
            }

            /* Priority badges - Modern tactical theme */
            .priority-badge {
                display: inline-block;
                padding: 4px 10px;
                font-size: 11px;
                font-weight: 600;
                text-transform: uppercase;
                letter-spacing: 0.5px;
                border-radius: 2px;
            }

            .priority-critical {
                background: rgba(185, 28, 28, 0.15);
                color: #EF4444;
                border: 1px solid rgba(185, 28, 28, 0.3);
            }

            .priority-high {
                background: rgba(217, 119, 6, 0.15);
                color: #F59E0B;
                border: 1px solid rgba(217, 119, 6, 0.3);
            }

            .priority-medium {
                background: rgba(45, 212, 191, 0.15);
                color: #2DD4BF;
                border: 1px solid rgba(45, 212, 191, 0.3);
            }

            .priority-low {
                background: rgba(74, 124, 89, 0.15);
                color: #4A7C59;
                border: 1px solid rgba(74, 124, 89, 0.3);
            }

            /* Action panel */
            .action-panel {
                padding: 20px;
                border-bottom: 1px solid rgba(255, 255, 255, 0.1);
            }

            .action-panel-title {
                font-size: 12px;
                color: rgba(255, 255, 255, 0.4);
                text-transform: uppercase;
                letter-spacing: 1px;
                margin-bottom: 12px;
                font-weight: 500;
            }

            .action-button {
                background: #4A7C59;
                border: none;
                border-radius: 4px;
                color: white;
                padding: 10px 16px;
                font-size: 11px;
                text-transform: uppercase;
                letter-spacing: 0.5px;
                cursor: pointer;
                transition: all 0.2s ease;
                font-weight: 600;
                width: 100%;
                margin-bottom: 8px;
                box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
            }

            .action-button:hover {
                background: #5a8c69;
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.3);
                transform: translateY(-1px);
            }

            /* Notes section */
            .notes-section {
                padding: 20px;
                max-height: 300px;
                overflow-y: auto;
            }

            .note-item {
                background: rgba(0, 137, 123, 0.1);
                padding: 12px;
                margin-bottom: 8px;
                border-left: 2px solid #00897B;
                font-size: 13px;
                color: rgba(255, 255, 255, 0.8);
            }

            .note-meta {
                font-size: 11px;
                color: rgba(255, 255, 255, 0.4);
                margin-top: 6px;
            }

            /* Login page */
            .login-container {
                display: flex;
                align-items: center;
                justify-content: center;
                min-height: 100vh;
                background: linear-gradient(180deg, #0A1929 0%, #071220 100%);
                padding: 16px;
            }

            .login-card {
                background: #0d1f2d;
                border: 1px solid rgba(38, 198, 218, 0.2);
                border-left: 3px solid #00897B;
                padding: 48px;
                max-width: 400px;
                width: 100%;
                box-shadow: none !important;
            }

            .login-logo {
                width: 180px;
                height: auto;
                margin: 0 auto 32px;
                display: block;
            }

            .login-title {
                font-family: 'Stack Sans Notch', -apple-system, BlinkMacSystemFont, sans-serif;
                font-size: 28px;
                font-weight: 700;
                color: #26C6DA;
                margin-bottom: 8px;
                text-align: center;
            }

            .login-subtitle {
                font-size: 14px;
                color: rgba(255, 255, 255, 0.6);
                margin-bottom: 32px;
                text-align: center;
            }

            /* Responsive login */
            @media (max-width: 640px) {
                .login-card {
                    padding: 32px 24px;
                }

                .login-logo {
                    width: 140px;
                    margin-bottom: 24px;
                }

                .login-title {
                    font-size: 24px;
                }

                .login-subtitle {
                    font-size: 13px;
                    margin-bottom: 24px;
                }
            }

            @media (max-width: 480px) {
                .login-card {
                    padding: 24px 16px;
                }

                .login-logo {
                    width: 120px;
                }

                .login-title {
                    font-size: 20px;
                }
            }

            /* Incident header */
            .incident-header {
                background: #071220;
                padding: 20px 24px;
                border-bottom: 1px solid rgba(255, 255, 255, 0.1);
                border-left: 3px solid rgba(0, 137, 123, 0.3);
            }

            .incident-header-id {
                font-size: 16px;
                font-weight: 600;
                color: #26C6DA;
                letter-spacing: 1px;
                margin-bottom: 4px;
            }

            .incident-header-title {
                font-size: 20px;
                color: #fff;
                font-weight: 500;
                margin-bottom: 8px;
            }

            .incident-header-meta {
                font-size: 13px;
                color: rgba(255, 255, 255, 0.5);
            }

            /* Map container for incident location */
            .map-container {
                height: 200px;
                margin: 0;
            }

            .leaflet-container {
                height: 100%;
                background: #0A1929;
            }

            /* Mobile responsive */
            @media (max-width: 1024px) {
                .chat-container {
                    grid-template-columns: 1fr;
                }

                .chat-sidebar {
                    display: none;
                }

                .message-bubble {
                    max-width: 85%;
                }
            }

            /* NiceGUI specific overrides */
            .q-page {
                background: linear-gradient(180deg, #0A0E12 0%, #121820 100%);
            }

            .q-card {
                background: rgba(18, 24, 32, 0.8);
                border: 1px solid rgba(255, 255, 255, 0.08);
                border-radius: 4px !important;
                box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3) !important;
            }

            .q-btn {
                text-transform: none;
                border-radius: 4px !important;
            }

            .q-input {
                border-radius: 4px !important;
            }

            /* Table styling for incident list */
            .q-table thead tr,
            .q-table tbody td {
                background: #0A1929;
            }

            .q-table thead th {
                font-size: 11px;
                color: rgba(255, 255, 255, 0.5);
                text-transform: uppercase;
                letter-spacing: 1px;
                font-weight: 600;
                background: #071220;
                border-bottom: 1px solid rgba(255, 255, 255, 0.1);
            }

            .q-table tbody tr {
                transition: all 0.15s ease;
                border-bottom: 1px solid rgba(255, 255, 255, 0.05);
                border-left: 3px solid transparent;
            }

            .q-table tbody tr:hover {
                background: rgba(0, 137, 123, 0.1);
                border-left-color: #00897B;
            }

            .q-table tbody td {
                font-size: 14px;
                color: rgba(255, 255, 255, 0.8);
            }
        </style>
    ''')


@asynccontextmanager
async def frame(title: str = "SIMS Responder"):
    """
    Main frame context manager for SIMS responder pages

    Args:
        title: Page title to display
    """
    # Apply theme
    apply_theme()
    inject_custom_css()

    # Enable dark mode
    ui.dark_mode(True)

    # Header only (no sidebar for responder portal)
    header = ui.header(elevated=False, bordered=False).classes('border-b border-[rgba(255,255,255,0.08)]')
    header.style('background: linear-gradient(90deg, #0A0E12 0%, #121820 100%); box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);')
    with header:
        with ui.element('div').classes('w-full max-w-6xl mx-auto px-4'):
            with ui.row().classes('items-center justify-between w-full py-3'):
                # Logo/Title
                with ui.row().classes('items-center gap-4'):
                    ui.image('/static/sims-logo.svg').style('width: 120px; height: auto;')
                    ui.label(title).classes('page-title').style('color: #fff;')

                # Status section
                with ui.row().classes('items-center gap-3'):
                    ui.label('Responder Portal').classes('text-sm text-gray-400')
                    status_dot = ui.element('div').classes('w-2 h-2 status-dot')
                    status_dot.style('background: #2DD4BF; box-shadow: 0 0 8px rgba(45, 212, 191, 0.6);')

    # Main content area
    with ui.column().classes('w-full'):
        # Yield to allow content to be added
        yield
