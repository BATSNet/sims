"""
SIMS Incident Chat Interface
Chat-based incident reporting with Flutter integration
"""
import json
from datetime import datetime
from nicegui import ui, app
from typing import Optional


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


async def process_incident_message(message: str, location: Optional[dict] = None) -> dict:
    """
    Process incident report message

    Args:
        message: User's incident description
        location: Location data from Flutter (lat, lon)

    Returns:
        dict with response and incident data
    """
    # TODO: Integrate with LLM for classification and summarization
    # TODO: Store in database

    # Mock response for now
    incident_id = f"INC-{datetime.now().strftime('%Y%m%d%H%M%S')}"

    return {
        'incident_id': incident_id,
        'message': message,
        'location': location,
        'timestamp': datetime.now().isoformat(),
        'response': f"Incident {incident_id} recorded. Please provide any additional details or media."
    }


class IncidentChat:
    """Handles incident reporting chat interface"""

    def __init__(self, chat_container, user_input):
        self.chat_container = chat_container
        self.user_input = user_input
        self.location = None
        self.conversation_history = []

    async def send_message(self):
        """Process and send user message"""
        message = self.user_input.value.strip()

        if not message:
            return

        # Clear input and show loading
        self.user_input.value = ''
        await ui.run_javascript(send_flutter_message('loading', {'value': True}))

        # Add user message to chat
        with self.chat_container:
            with ui.row().classes('w-full justify-end mb-4'):
                with ui.card().classes('bg-[#63ABFF] text-white max-w-[80%]'):
                    ui.label(message).classes('text-sm')

        # Show thinking status
        await ui.run_javascript(send_flutter_message('thought', {'value': 'Processing incident report...'}))

        # Process message
        result = await process_incident_message(message, self.location)
        self.conversation_history.append({
            'role': 'user',
            'content': message,
            'timestamp': datetime.now().isoformat()
        })

        # Add system response
        with self.chat_container:
            with ui.row().classes('w-full justify-start mb-4'):
                with ui.card().classes('bg-[rgba(26,31,46,0.6)] text-white max-w-[80%]'):
                    ui.label(result['response']).classes('text-sm')

        self.conversation_history.append({
            'role': 'system',
            'content': result['response'],
            'incident_id': result['incident_id'],
            'timestamp': result['timestamp']
        })

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


async def render_incident_chat():
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
            # Create chat instance
            user_input = ui.input(placeholder='').classes('hidden flapp-input').props('sims-chat-input')
            incident_chat = IncidentChat(chat_container, user_input)
            incident_chat.location = location

            # Hidden submit button with special attribute for Flutter to target
            ui.button(on_click=incident_chat.send_message).classes('hidden sims-submit-btn').props('sims-chat-submit')


async def incident_page():
    """Main incident chat page"""
    # Render chat interface
    await render_incident_chat()
