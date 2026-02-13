"""
SIMS Backend Theme Configuration
Military-inspired design based on modern defense tech aesthetics
"""
from contextlib import asynccontextmanager
from nicegui import ui


# Color scheme - Modern 2026 tactical palette
COLORS = {
    # Base - Deep tactical slate/charcoal
    'background': '#0A0E12',           # Darker, more sophisticated
    'background_light': '#121820',     # Cards, elevated surfaces
    'background_elevated': '#1A2028',  # Modals, overlays
    'background_card': 'rgba(18, 24, 32, 0.8)',  # Semi-transparent cards

    # Tactical accents - Muted, professional
    'accent_tactical': '#4A7C59',      # Muted olive-green (primary CTA)
    'accent_cyan': '#2DD4BF',          # Status indicators, live updates
    'accent_amber': '#F59E0B',         # Warnings

    # Slate neutrals for depth
    'slate_900': '#0F172A',
    'slate_800': '#1E293B',
    'slate_700': '#334155',
    'slate_600': '#475569',
    'slate_500': '#64748B',

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
    """Apply the SIMS military theme colors and styles"""
    ui.colors(
        primary='#4A7C59',              # Tactical green
        secondary='#2DD4BF',            # Cyan
        accent='#2DD4BF',               # Cyan
        dark='#0A0E12',                 # Deep background
        positive='#4A7C59',             # Success green
        negative='#B91C1C',             # Error red
        info='#2DD4BF',                 # Info cyan
        warning='#F59E0B',              # Warning amber
    )


def inject_custom_css():
    """Inject custom CSS for military-themed styling"""
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
            /* Base styling */
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
                font-weight: 700;
                letter-spacing: 0.5px;
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

            /* Remove tab panel background */
            .q-tab-panel {
                background: transparent !important;
                padding: 0 !important;
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
                background: rgba(18, 24, 32, 0.3);
            }

            ::-webkit-scrollbar-thumb {
                background: rgba(74, 124, 89, 0.4);
                border-radius: 4px;
            }

            ::-webkit-scrollbar-thumb:hover {
                background: rgba(74, 124, 89, 0.6);
            }

            /* Overview Section - Map and Stats */
            .overview-section {
                display: grid;
                grid-template-columns: 1fr 320px;
                gap: 0;
                margin: 0 0 3rem;
                margin-left: 0;
                background: #121820;
                border-left: 3px solid rgba(74, 124, 89, 0.3);
                border-radius: 4px;
                overflow: hidden;
                box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
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
                min-width: 72rem;
                max-width: 96rem;
                margin-left: 0;
                margin-right: auto;
                padding-left: 0;
                padding-right: 0;
            }

            .stats-column {
                display: flex;
                flex-direction: column;
                gap: 0;
                border-left: 1px solid rgba(255, 255, 255, 0.1);
            }

            .metric-card {
                background: rgba(18, 24, 32, 0.8);
                padding: 24px;
                border-bottom: 1px solid rgba(255, 255, 255, 0.08);
                transition: all 0.2s ease;
            }

            .metric-card:hover {
                background: rgba(26, 32, 40, 0.9);
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.4);
                transform: translateY(-2px);
            }

            .metric-label {
                font-size: 11px;
                color: rgba(255, 255, 255, 0.6);
                text-transform: uppercase;
                letter-spacing: 0.8px;
                margin-bottom: 12px;
                font-weight: 600;
            }

            .metric-value {
                font-size: 42px;
                font-weight: 600;
                color: #fff;
                line-height: 1;
            }

            /* Section Title */
            .section-title {
                font-family: 'Stack Sans Notch', -apple-system, BlinkMacSystemFont, sans-serif;
                font-size: 32px;
                font-weight: 700;
                color: #fff;
                letter-spacing: 0.5px;
                margin: 0 0 1.5rem;
                text-transform: uppercase;
            }

            /* Table Section */
            .table-section {
                background: #121820;
                margin: 0;
                margin-left: 0;
                width: 100%;
                border-radius: 4px;
                overflow: hidden;
                box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
            }

            .table-header-bar {
                display: flex;
                justify-content: space-between;
                align-items: center;
                padding: 20px 24px;
                border-bottom: 1px solid rgba(255, 255, 255, 0.08);
                border-left: 3px solid rgba(74, 124, 89, 0.3);
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
                border: 1px solid rgba(255, 255, 255, 0.2);
                color: rgba(255, 255, 255, 0.7);
                padding: 6px 14px;
                font-size: 11px;
                text-transform: uppercase;
                letter-spacing: 0.5px;
                cursor: pointer;
                transition: all 0.2s ease;
                font-weight: 600;
                border-radius: 4px;
            }

            .filter-btn:hover {
                border-color: rgba(74, 124, 89, 0.5);
                color: rgba(255, 255, 255, 0.9);
                background: rgba(74, 124, 89, 0.1);
            }

            .filter-btn.active {
                background: rgba(74, 124, 89, 0.15);
                border-color: #4A7C59;
                color: #4A7C59;
            }

            /* Table styling */
            .q-table {
                background: #0A0E12;
            }

            .q-table thead tr,
            .q-table tbody td {
                background: rgba(18, 24, 32, 0.6);
            }

            .q-table thead th {
                font-size: 11px;
                color: rgba(255, 255, 255, 0.6);
                text-transform: uppercase;
                letter-spacing: 0.8px;
                font-weight: 600;
                background: #121820;
                border-bottom: 1px solid rgba(255, 255, 255, 0.08);
                border-right: 1px solid rgba(255, 255, 255, 0.03);
            }

            .q-table tbody tr {
                transition: all 0.2s ease;
                border-bottom: 1px solid rgba(255, 255, 255, 0.05);
                border-left: 3px solid transparent;
            }

            .q-table tbody tr:hover {
                background: rgba(26, 32, 40, 0.8);
                border-left-color: #4A7C59;
            }

            .q-table tbody td {
                font-size: 14px;
                color: rgba(255, 255, 255, 0.85);
                border-right: 1px solid rgba(255, 255, 255, 0.03);
            }

            /* Priority badges */
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

            /* Action button */
            .action-button {
                background: #4A7C59;
                border: none;
                border-radius: 4px;
                color: white;
                padding: 10px 20px;
                font-size: 11px;
                text-transform: uppercase;
                letter-spacing: 0.5px;
                cursor: pointer;
                transition: all 0.2s ease;
                font-weight: 600;
                box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
            }

            .action-button:hover {
                background: #5a8c69;
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.3);
                transform: translateY(-1px);
            }

            .action-button-secondary {
                background: transparent;
                border: 1px solid rgba(255, 255, 255, 0.2);
                color: white;
                box-shadow: none;
            }

            .action-button-secondary:hover {
                border-color: rgba(74, 124, 89, 0.5);
                background: rgba(74, 124, 89, 0.1);
                transform: translateY(-1px);
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
                background: rgba(18, 24, 32, 0.95) !important;
                backdrop-filter: blur(10px);
                border: 1px solid rgba(255, 255, 255, 0.08) !important;
                border-radius: 4px !important;
                box-shadow: 0 4px 12px rgba(0, 0, 0, 0.5) !important;
                color: #fff;
            }

            .custom-popup-critical .leaflet-popup-content-wrapper {
                border-left: 3px solid #B91C1C !important;
            }

            .custom-popup-high .leaflet-popup-content-wrapper {
                border-left: 3px solid #D97706 !important;
            }

            .custom-popup-medium .leaflet-popup-content-wrapper {
                border-left: 3px solid #2DD4BF !important;
            }

            .custom-popup-low .leaflet-popup-content-wrapper {
                border-left: 3px solid #4A7C59 !important;
            }

            .leaflet-popup-content {
                margin: 12px 16px;
                font-size: 12px;
            }

            .leaflet-popup-tip {
                background: rgba(18, 24, 32, 0.95) !important;
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

                .map-container {
                    height: 400px;
                    min-height: 400px;
                }
            }

            @media (max-width: 1024px) {
                .content-container {
                    padding-left: 1rem;
                    padding-right: 1rem;
                }

                .table-header-bar {
                    padding: 16px 20px;
                }

                .section-title {
                    font-size: 28px;
                    margin: 0 0 1rem;
                }

                .metric-card {
                    padding: 20px;
                }

                .metric-value {
                    font-size: 36px;
                }
            }

            @media (max-width: 768px) {
                .overview-section {
                    grid-template-columns: 1fr;
                    margin: 0 0 2rem;
                }

                .stats-column {
                    grid-template-columns: repeat(2, 1fr);
                }

                .map-container {
                    height: 350px;
                    min-height: 350px;
                }

                .content-container {
                    padding-left: 0.5rem;
                    padding-right: 0.5rem;
                }

                .section-title {
                    font-size: 24px;
                    letter-spacing: 1px;
                }

                .table-header-bar {
                    flex-direction: column;
                    align-items: flex-start;
                    gap: 12px;
                    padding: 12px 16px;
                }

                .table-actions {
                    width: 100%;
                    overflow-x: auto;
                    flex-wrap: wrap;
                }

                .filter-btn {
                    white-space: nowrap;
                }

                .metric-card {
                    padding: 16px;
                }

                .metric-value {
                    font-size: 32px;
                }

                .metric-label {
                    font-size: 11px;
                }

                /* Make table scrollable on mobile */
                .q-table__container {
                    overflow-x: auto;
                }

                .q-table tbody td,
                .q-table thead th {
                    padding: 8px 12px;
                    font-size: 13px;
                }

                .priority-badge {
                    padding: 3px 8px;
                    font-size: 9px;
                }

                .action-button {
                    padding: 5px 12px;
                    font-size: 9px;
                }
            }

            @media (max-width: 640px) {
                .stats-column {
                    grid-template-columns: 1fr;
                }

                .map-container {
                    height: 300px;
                    min-height: 300px;
                }

                .section-title {
                    font-size: 20px;
                }

                .table-title {
                    font-size: 14px;
                }

                .metric-value {
                    font-size: 28px;
                }

                .q-table tbody td,
                .q-table thead th {
                    padding: 6px 8px;
                    font-size: 12px;
                }

                /* Stack table cells on very small screens */
                .cell-description {
                    font-size: 13px;
                    max-width: 200px;
                    overflow: hidden;
                    text-overflow: ellipsis;
                }
            }

            /* NiceGUI specific overrides */
            .q-page {
                background: linear-gradient(180deg, #0A0E12 0%, #121820 100%);
            }

            .q-card {
                background: rgba(18, 24, 32, 0.8);
                border: 1px solid rgba(255, 255, 255, 0.08);
                border-radius: 4px;
                box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
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
                border-radius: 4px !important;
            }

            /* Glassmorphic effects */
            .glass-effect {
                background: rgba(18, 24, 32, 0.7);
                backdrop-filter: blur(10px);
                border: 1px solid rgba(255, 255, 255, 0.08);
                border-radius: 4px;
            }

            .status-indicator {
                background: rgba(74, 124, 89, 0.15);
                backdrop-filter: blur(8px);
                border: 1px solid rgba(74, 124, 89, 0.3);
                border-radius: 12px;
                padding: 6px 12px;
            }

            .status-dot {
                width: 6px;
                height: 6px;
                border-radius: 50%;
                background: #2DD4BF;
                box-shadow: 0 0 8px rgba(45, 212, 191, 0.6);
                animation: pulse 2s infinite;
            }

            @keyframes pulse {
                0%, 100% { opacity: 1; }
                50% { opacity: 0.5; }
            }

            /* Chat interface styling */
            .chat-messages {
                max-height: calc(100vh - 200px);
            }

            .chat-messages .q-card {
                padding: 12px 16px;
            }

            /* Hidden elements for Flutter bridge */
            .hidden {
                display: none !important;
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

    # Sidebar with logo - responsive drawer
    # On desktop (>1024px): persistent drawer, open by default
    # On mobile (<1024px): overlay drawer, closed by default
    drawer = ui.left_drawer(top_corner=True, bottom_corner=False).classes('w-64').props('breakpoint=1024')
    drawer.style('background: linear-gradient(180deg, #0A0E12 0%, #121820 100%); border-right: 1px solid rgba(255, 255, 255, 0.08);')

    with drawer:
        # Logo
        with ui.column().classes('items-center p-6'):
            ui.image('/static/sims-logo.svg').props('fit=scale-down').classes('sidebar-logo')

        with ui.column().classes('flex-1 p-6 space-y-1 w-full'):
            ui.label('Command Center').classes('text-xs font-bold text-gray-400 mb-2 title-font')

            # Dashboard link
            nav_link = ui.link(target='/').classes('flex items-center gap-2 px-4 py-2 text-white no-underline w-full')
            nav_link.style('border-radius: 4px; transition: all 0.2s ease;')
            nav_link.classes('hover:bg-[rgba(74,124,89,0.1)]')
            with nav_link:
                ui.icon('dashboard', size='md')
                ui.label('Dashboard').classes('title-font')

            # Organizations link
            nav_link = ui.link(target='/organizations').classes('flex items-center gap-2 px-4 py-2 text-white no-underline w-full')
            nav_link.style('border-radius: 4px; transition: all 0.2s ease;')
            nav_link.classes('hover:bg-[rgba(74,124,89,0.1)]')
            with nav_link:
                ui.icon('corporate_fare', size='md')
                ui.label('Organizations').classes('title-font')

            # Integrations link
            nav_link = ui.link(target='/integrations').classes('flex items-center gap-2 px-4 py-2 text-white no-underline w-full')
            nav_link.style('border-radius: 4px; transition: all 0.2s ease;')
            nav_link.classes('hover:bg-[rgba(74,124,89,0.1)]')
            with nav_link:
                ui.icon('hub', size='md')
                ui.label('Integrations').classes('title-font')

        # Logout button at bottom
        with ui.column().classes('w-full p-6 mt-auto'):
            ui.button('Logout', on_click=lambda: None).props('outline color=white').classes('w-full logout-btn')

    # Header with hamburger menu for mobile
    header = ui.header(elevated=False, bordered=False).classes('border-b border-[rgba(255,255,255,0.08)]')
    header.style('background: linear-gradient(90deg, #0A0E12 0%, #121820 100%); box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);')
    with header:
        with ui.element('div').classes('w-full max-w-[96rem] mx-auto px-2 sm:px-4'):
            with ui.row().classes('items-center justify-between w-full py-3 gap-2'):
                # Hamburger menu for mobile
                with ui.row().classes('items-center gap-2'):
                    ui.button(icon='menu', on_click=drawer.toggle).props('flat color=white dense').classes('lg:hidden')
                    ui.label(title).classes('page-title text-lg sm:text-xl lg:text-2xl')

                # Status section - responsive
                with ui.row().classes('items-center gap-2 sm:gap-3'):
                    # System status text (hidden on very small screens)
                    status_text = ui.label('System Operational').classes('text-xs sm:text-sm text-gray-400 hidden sm:block').props('id=system-status-text')
                    # Timestamp
                    status_timestamp = ui.label('--:--:--').classes('text-xs sm:text-sm font-bold title-font text-[#ccc]').props('id=system-status-timestamp').style('min-width: 50px; font-variant-numeric: tabular-nums;')
                    # Status indicator dot - with glassmorphic effect
                    status_dot = ui.element('div').classes('w-2 h-2 status-dot').props('id=system-status-dot')
                    status_dot.style('background: #2DD4BF; box-shadow: 0 0 8px rgba(45, 212, 191, 0.6);')

    # Main content area - no padding, let individual sections control their own layout
    with ui.column().classes('w-full'):
        # Yield to allow content to be added
        yield
