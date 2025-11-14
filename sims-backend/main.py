"""
SIMS Backend - Main Application Entry Point
Situation Incident Management System - Operator Dashboard
"""
import logging
from pathlib import Path
from nicegui import app, ui
from dashboard import dashboard
import theme

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Configure static files
RESOURCES_PATH = Path(__file__).parent.parent / 'resources'
app.add_static_files('/static', str(RESOURCES_PATH))


@ui.page('/')
async def index():
    """Main dashboard page"""
    async with theme.frame('SIMS Command'):
        await dashboard()


@ui.page('/health')
def health():
    """Health check endpoint"""
    return {'status': 'ok', 'service': 'sims-backend'}


def main():
    """Run the SIMS backend application"""
    try:
        logger.info('Starting SIMS Backend...')

        ui.run(
            host='0.0.0.0',
            port=8080,
            title='SIMS - Situation Incident Management System',
            favicon=str(RESOURCES_PATH / 'sims-icon.svg'),
            dark=True,
            reload=False,
            show=False,
            show_welcome_message=False,
        )

    except Exception as e:
        logger.error(f'Failed to start SIMS Backend: {e}', exc_info=True)
        raise


if __name__ in {"__main__", "__mp_main__"}:
    main()
