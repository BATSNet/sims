"""
SIMS Incident Chat Interface - Simplified
Simple chat interface for incident reporting via Flutter WebView
"""
import json
import uuid
from datetime import datetime
from nicegui import ui, app
from typing import Optional
from sqlalchemy.orm import Session

from db.connection import get_db
from services.chat_history import ChatHistory, create_chat_session
from models.incident_model import IncidentORM
from geoalchemy2.elements import WKTElement


class SimpleIncidentChat:
    """Simplified chat handler for incident reporting"""

    def __init__(self, chat_container, user_input, db: Session):
        self.chat_container = chat_container
        self.user_input = user_input
        self.db = db
        self.location = None
        self.user_phone = None
        self.incident_id = None
        self.incident_uuid = None
        self.session_id = None

    async def create_incident(self, initial_message: str):
        """Create incident on first message"""
        incident_uuid = uuid.uuid4()
        incident_id = f"INC-{uuid.uuid4().hex[:8].upper()}"

        # Create PostGIS point if location available
        location_geom = None
        if self.location and 'lat' in self.location and 'lon' in self.location:
            location_geom = WKTElement(
                f'POINT({self.location["lon"]} {self.location["lat"]})',
                srid=4326
            )

        # Create incident
        db_incident = IncidentORM(
            id=incident_uuid,
            incident_id=incident_id,
            user_phone=self.user_phone,
            location=location_geom,
            latitude=self.location.get('lat') if self.location else None,
            longitude=self.location.get('lon') if self.location else None,
            title=f"Incident {incident_id}",
            description=initial_message,
            status='open',
            priority='medium',
            category='Unclassified',
            tags=[],
            metadata={'source': 'mobile_chat'}
        )

        self.db.add(db_incident)
        self.db.flush()

        # Create chat session
        self.session_id = create_chat_session(self.db, str(incident_uuid), self.user_phone)

        # Add initial message
        chat = ChatHistory(self.db, self.session_id)
        chat.add_message("user", initial_message)

        self.db.commit()

        self.incident_id = incident_id
        self.incident_uuid = str(incident_uuid)

        return incident_id

    async def add_message(self, message: str):
        """Add user message to chat"""
        if not self.session_id:
            return

        chat = ChatHistory(self.db, self.session_id)
        chat.add_message("user", message)

        # Update incident description with all messages
        messages = chat.get_messages()
        user_messages = [msg['content'] for msg in messages if msg['role'] == 'user']

        if len(user_messages) > 1:
            # Update incident with aggregated messages
            incident = self.db.query(IncidentORM).filter(
                IncidentORM.incident_id == self.incident_id
            ).first()

            if incident:
                incident.description = '\n\n'.join(user_messages)
                incident.updated_at = datetime.utcnow()
                self.db.commit()

    def refresh_chat_display(self):
        """Refresh chat display from database"""
        self.chat_container.clear()

        if not self.session_id:
            # Show welcome message
            with self.chat_container:
                with ui.row().classes('w-full justify-center mb-3 sm:mb-4'):
                    with ui.card().classes('bg-gray-800 text-center p-3 sm:p-4'):
                        ui.label('Describe what happened').classes('text-xs sm:text-sm text-gray-300')
            return

        # Load all messages from database
        try:
            chat = ChatHistory(self.db, self.session_id)
            messages = chat.get_messages()

            with self.chat_container:
                for msg in messages:
                    role = msg.get('role', 'user')
                    content = msg.get('content', '')

                    if role == 'user':
                        with ui.row().classes('w-full justify-end mb-2 sm:mb-3 px-2 sm:px-0'):
                            with ui.card().classes('bg-blue-500 text-white max-w-[85%] sm:max-w-[75%] p-2 sm:p-3'):
                                ui.label(content).classes('text-xs sm:text-sm break-words')
                    else:
                        with ui.row().classes('w-full justify-start mb-2 sm:mb-3 px-2 sm:px-0'):
                            with ui.card().classes('bg-gray-700 text-white max-w-[85%] sm:max-w-[75%] p-2 sm:p-3'):
                                ui.label(content).classes('text-xs sm:text-sm break-words')
        except Exception as e:
            print(f"Error refreshing chat: {e}")

    async def send_message(self):
        """Handle user message submission"""
        message = self.user_input.value.strip()
        if not message:
            return

        self.user_input.value = ''

        # Create incident on first message
        if not self.incident_id:
            try:
                incident_id = await self.create_incident(message)
                response = f"Incident {incident_id} created."
            except Exception as e:
                print(f"Error creating incident: {e}")
                response = "Error creating incident."
        else:
            # Add follow-up message
            try:
                await self.add_message(message)
                response = "Message saved."
            except Exception as e:
                print(f"Error adding message: {e}")
                response = "Error saving message."

        # Store assistant response
        if self.session_id:
            try:
                chat = ChatHistory(self.db, self.session_id)
                chat.add_message("assistant", response)
            except Exception as e:
                print(f"Error saving response: {e}")

        # Refresh entire chat display from database
        self.refresh_chat_display()

        # Auto-scroll
        await ui.run_javascript('''
            const container = document.querySelector('.chat-container');
            if (container) {
                container.scrollTop = container.scrollHeight;
            }
        ''')


async def render_simple_chat(db: Session):
    """Render simplified chat interface"""

    # Get location from storage
    location_str = app.storage.user.get('location', '')
    location = None
    if location_str:
        try:
            lat, lon = location_str.split(';')
            location = {'lat': float(lat), 'lon': float(lon)}
        except:
            pass

    # Get user phone from storage
    user_phone = app.storage.user.get('user_phone', None)

    # Main container
    with ui.column().classes('w-full h-screen bg-gray-900'):
        # Header - responsive padding and text
        with ui.row().classes('w-full p-3 sm:p-4 bg-gray-800 border-b border-gray-700 items-center'):
            ui.label('Incident Report').classes('text-base sm:text-lg font-bold text-white')
            if location:
                ui.label(f'📍 {location["lat"]:.4f}, {location["lon"]:.4f}').classes('text-xs text-gray-400 ml-auto hidden sm:block')

        # Messages container - responsive padding
        chat_container = ui.column().classes('w-full flex-1 overflow-y-auto p-2 sm:p-4 chat-container')

        # Input area - responsive layout
        with ui.row().classes('w-full p-2 sm:p-4 bg-gray-800 border-t border-gray-700 gap-2'):
            user_input = ui.input(placeholder='Type your message...').classes(
                'flex-1 bg-gray-700 text-white border-gray-600 text-sm sm:text-base'
            )
            send_btn = ui.button('Send').classes('bg-blue-600 text-white text-xs sm:text-sm px-3 sm:px-4')

            # Create chat instance
            chat_instance = SimpleIncidentChat(chat_container, user_input, db)
            chat_instance.location = location
            chat_instance.user_phone = user_phone

            # Load initial chat state
            chat_instance.refresh_chat_display()

            # Wire up send button
            send_btn.on_click(chat_instance.send_message)
            user_input.on('keydown.enter', chat_instance.send_message)


async def incident_page():
    """Main incident chat page"""
    db_gen = get_db()
    db = next(db_gen)

    try:
        await render_simple_chat(db)
    finally:
        db.close()
