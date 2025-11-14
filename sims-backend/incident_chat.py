"""
SIMS Incident Chat Interface
Chat-based incident reporting with Flutter integration and persistent history
"""
import json
import uuid
from datetime import datetime
from nicegui import ui, app
from typing import Optional
from sqlalchemy.orm import Session

from db.connection import get_db
from services.chat_history import ChatHistory, create_chat_session, get_session_by_incident
from models.incident_model import IncidentORM
from geoalchemy2.elements import WKTElement


def send_flutter_message(message_type: str, data: dict = None) -> str:
    """
    Generate JavaScript to send messages to Flutter app via postMessage bridge

    Args:
        message_type: Type of message (loading, thought, upload_start, etc.)
        data: Additional data to send with the message

    Returns:
        JavaScript code string to execute
    """
    message = {"type": message_type}

    if message_type == 'loading':
        message['value'] = bool(data.get('value', False))
    else:
        message.update(data or {})

    return f"""
        let message = {json.dumps(message)};

        function sendMessage() {{
            if (window.Flutter) {{
                window.Flutter.postMessage(JSON.stringify(message));
                return true;
            }} else if (window.flutter_inappwebview) {{
                window.flutter_inappwebview.callHandler('Flutter', JSON.stringify(message));
                return true;
            }} else {{
                return false;
            }}
        }}

        // Retry mechanism with 100 attempts every 10ms
        if (!sendMessage()) {{
            let attempts = 0;
            const maxAttempts = 100;
            const checkInterval = setInterval(() => {{
                attempts++;
                if (sendMessage()) {{
                    clearInterval(checkInterval);
                }} else if (attempts >= maxAttempts) {{
                    console.error('Failed to find Flutter channel after ' + maxAttempts + ' attempts');
                    clearInterval(checkInterval);
                }}
            }}, 10);
        }}
    """


async def create_or_get_incident(db: Session, message: str, location: Optional[dict] = None, user_phone: Optional[str] = None) -> tuple:
    """
    Create a new incident or get existing one

    Returns:
        (incident_id, incident_uuid, session_id)
    """
    # Generate IDs
    incident_uuid = uuid.uuid4()
    incident_id = f"INC-{uuid.uuid4().hex[:8].upper()}"

    # Create PostGIS point if lat/lon provided
    location_geom = None
    if location and 'lat' in location and 'lon' in location:
        # PostGIS uses (lon, lat) order for POINT
        location_geom = WKTElement(
            f'POINT({location["lon"]} {location["lat"]})',
            srid=4326
        )

    # Create incident
    db_incident = IncidentORM(
        id=incident_uuid,
        incident_id=incident_id,
        user_phone=user_phone,
        location=location_geom,
        latitude=location.get('lat') if location else None,
        longitude=location.get('lon') if location else None,
        title=f"Incident reported at {datetime.now().strftime('%Y-%m-%d %H:%M')}",
        description=message,
        status='open',
        priority='medium',
        category='Unclassified',
        tags=[],
        metadata={}
    )

    db.add(db_incident)
    db.flush()

    # Create chat session
    session_id = create_chat_session(db, str(incident_uuid), user_phone)

    # Add initial message
    chat = ChatHistory(db, session_id)
    chat.add_message("user", message)

    db.commit()

    return incident_id, str(incident_uuid), session_id


class IncidentChat:
    """Handles incident reporting chat interface with persistent storage"""

    def __init__(self, chat_container, user_input, db: Session):
        self.chat_container = chat_container
        self.user_input = user_input
        self.db = db
        self.location = None
        self.user_phone = None
        self.incident_id = None
        self.incident_uuid = None
        self.session_id = None

    async def load_chat_history(self):
        """Load existing chat history from database"""
        if not self.session_id:
            return

        try:
            chat = ChatHistory(self.db, self.session_id)
            messages = chat.get_messages()

            # Display historical messages
            for msg in messages:
                if msg['role'] == 'user':
                    with self.chat_container:
                        with ui.row().classes('w-full justify-end mb-4'):
                            with ui.card().classes('bg-[#63ABFF] text-white max-w-[80%]'):
                                ui.label(msg['content']).classes('text-sm')
                else:
                    with self.chat_container:
                        with ui.row().classes('w-full justify-start mb-4'):
                            with ui.card().classes('bg-[rgba(26,31,46,0.6)] text-white max-w-[80%]'):
                                ui.label(msg['content']).classes('text-sm')

        except Exception as e:
            print(f"Error loading chat history: {e}")

    async def send_message(self):
        """Process and send user message with database persistence"""
        message = self.user_input.value.strip()

        if not message:
            return

        # Clear input and show loading
        self.user_input.value = ''
        await ui.run_javascript(send_flutter_message('loading', {'value': True}))

        # Create incident if first message
        if not self.incident_id:
            try:
                self.incident_id, self.incident_uuid, self.session_id = await create_or_get_incident(
                    self.db,
                    message,
                    self.location,
                    self.user_phone
                )
            except Exception as e:
                print(f"Error creating incident: {e}")
                await ui.run_javascript(send_flutter_message('loading', {'value': False}))
                return

        # Add user message to chat
        with self.chat_container:
            with ui.row().classes('w-full justify-end mb-4'):
                with ui.card().classes('bg-[#63ABFF] text-white max-w-[80%]'):
                    ui.label(message).classes('text-sm')

        # If not first message, store in database
        if self.session_id:
            try:
                chat = ChatHistory(self.db, self.session_id)
                if len(chat.get_messages()) > 1:  # Not the initial message
                    chat.add_message("user", message)
            except Exception as e:
                print(f"Error saving message: {e}")

        # Show thinking status
        await ui.run_javascript(send_flutter_message('thought', {'value': 'Processing incident report...'}))

        # Generate response (TODO: integrate with LLM)
        response = f"Incident {self.incident_id} recorded. Please provide any additional details or media."

        # Add system response to chat
        with self.chat_container:
            with ui.row().classes('w-full justify-start mb-4'):
                with ui.card().classes('bg-[rgba(26,31,46,0.6)] text-white max-w-[80%]'):
                    ui.label(response).classes('text-sm')

        # Store assistant response in database
        if self.session_id:
            try:
                chat = ChatHistory(self.db, self.session_id)
                chat.add_message("assistant", response)
            except Exception as e:
                print(f"Error saving response: {e}")

        # Clear loading and thought
        await ui.run_javascript(send_flutter_message('loading', {'value': False}))
        await ui.run_javascript(send_flutter_message('thought', {'value': ''}))

        # Scroll to bottom
        await ui.run_javascript('''
            const container = document.querySelector('.chat-messages');
            if (container) {
                container.scrollTop = container.scrollHeight;
            }
        ''')


async def render_incident_chat(db: Session):
    """Render incident chat interface"""

    # Get location from request headers (sent by Flutter)
    location_str = app.storage.user.get('location', '')
    location = None

    if location_str:
        try:
            lat, lon = location_str.split(';')
            location = {'lat': float(lat), 'lon': float(lon)}
        except:
            pass

    # Full height container for chat
    with ui.column().classes('w-full h-screen'):
        # Chat messages container
        chat_container = ui.column().classes('w-full flex-1 overflow-y-auto p-4 chat-messages')

        # Welcome message
        with chat_container:
            with ui.row().classes('w-full justify-center mb-6'):
                with ui.card().classes('bg-[rgba(26,31,46,0.6)] text-center'):
                    ui.label('Incident Reporting').classes('text-lg font-bold mb-2 title-font')
                    ui.label('Describe the incident you want to report.').classes('text-sm text-gray-400')
                    if location:
                        ui.label(f'Location: {location["lat"]:.4f}, {location["lon"]:.4f}').classes('text-xs text-gray-500 mt-2')

        # For Flutter app, create hidden elements
        with ui.element('div').classes('hidden'):
            # Create chat instance with database connection
            user_input = ui.input(placeholder='').classes('hidden flapp-input').props('sims-chat-input')
            incident_chat = IncidentChat(chat_container, user_input, db)
            incident_chat.location = location

            # Load existing chat history if incident_id provided
            # TODO: Get incident_id from URL parameter or app.storage
            # await incident_chat.load_chat_history()

            # Hidden submit button with special attribute for Flutter to target
            ui.button(on_click=incident_chat.send_message).classes('hidden sims-submit-btn').props('sims-chat-submit')


async def incident_page():
    """Main incident chat page with database connection"""
    # Get database session
    db_gen = get_db()
    db = next(db_gen)

    try:
        # Render chat interface
        await render_incident_chat(db)
    finally:
        # Close database session
        db.close()
