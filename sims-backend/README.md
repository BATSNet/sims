# SIMS Backend

Backend server for the Situation Incident Management System (SIMS).

## Overview

This is the operator dashboard and API backend for SIMS, built with NiceGUI and FastAPI. It provides:

- Real-time incident monitoring dashboard
- Incident forwarding to responsible organizations
- Military-inspired UI design
- RESTful API endpoints

## Setup

### Requirements

- Python 3.10 or higher
- PostgreSQL with PostGIS extension (for production)

### Installation

1. Create a virtual environment:

```bash
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
```

2. Install dependencies:

```bash
pip install -r requirements.txt
```

3. Run the development server:

```bash
python main.py
```

The dashboard will be available at `http://localhost:8080`

## Project Structure

```
sims-backend/
├── main.py              # Application entry point
├── dashboard.py         # Dashboard UI components
├── theme.py            # Theme configuration and styling
├── requirements.txt    # Python dependencies
├── theme_mockup.html   # Theme design mockups
└── README.md          # This file
```

## Features

### Dashboard

- Real-time incident overview
- Incident metrics (active, high priority, total reports)
- Detailed incident table with:
  - Incident ID
  - Timestamp
  - Location
  - Description
  - Priority level
  - Forward action

### Theme

Two military-inspired theme concepts:

1. **Tactical Operations** - Terminal-style interface with grid background
2. **Strategic Command** - Modern defense tech aesthetic (currently implemented)

Navy blue (#0D2637) base color scheme with accent colors:
- Primary: #63ABFF (light blue)
- Success: #00ff88 (green)
- Critical: #ff4444 (red)
- High: #ffa600 (orange)

## Development

### Running in Development Mode

The server runs with auto-reload enabled by default:

```bash
python main.py
```

### Theme Mockups

Open `theme_mockup.html` in a browser to view the two theme design concepts.

## Next Steps

- [ ] Implement database integration (PostgreSQL + PostGIS)
- [ ] Add API endpoints for mobile app
- [ ] Implement LLM text summarization pipeline
- [ ] Add voice-to-text transcription
- [ ] Implement authentication (Keycloak)
- [ ] Add real-time updates via WebSocket
- [ ] Integrate SEDAP.Express API

## Configuration

Currently using mock data. Database configuration will be added via environment variables:

```
DATABASE_URL=postgresql://user:password@localhost/sims
```

## API Endpoints

- `GET /` - Dashboard UI
- `GET /health` - Health check endpoint

More endpoints will be added for mobile app integration.

## AI Processing Configuration

SIMS uses AI for intelligent incident processing. All AI features can be individually enabled/disabled via `config.yaml`.

### AI Features

The system provides four independent AI capabilities:

1. **Audio Transcription** - Converts voice messages to text using Whisper models
2. **Incident Categorization** - Classifies incidents using LLM (categories, priority, tags)
3. **Media Analysis** - Generates descriptions of images/videos
4. **Auto-Forwarding** - Automatically routes incidents to appropriate organizations

### Configuration File: `config.yaml`

```yaml
ai_processing:
  # Individual feature toggles
  transcription_enabled: true      # Audio-to-text transcription
  categorization_enabled: true     # LLM-based incident classification
  media_analysis_enabled: true     # Image/video analysis via vision AI
  auto_forwarding_enabled: true    # Automatic forwarding to organizations

  # Default organization fallback (used when categorization is disabled)
  default_organizations: []  # Example: [1, 2, 3]

  # Priority/category fallback values when AI classification is disabled
  default_priority: "medium"
  default_category: "Unclassified"
```

### Disabling AI Features

#### Scenario 1: Disable All AI Processing

```yaml
ai_processing:
  transcription_enabled: false
  categorization_enabled: false
  media_analysis_enabled: false
  auto_forwarding_enabled: false
  default_organizations: []
  default_priority: "medium"
  default_category: "Unclassified"
```

**Result:**
- Audio files stored but not transcribed
- Images/videos stored but not analyzed
- All incidents assigned "Unclassified" category and "medium" priority
- No automatic forwarding - incidents remain unassigned

#### Scenario 2: Disable AI but Enable Broadcast Forwarding

```yaml
ai_processing:
  transcription_enabled: false
  categorization_enabled: false
  media_analysis_enabled: false
  auto_forwarding_enabled: true
  default_organizations: [1, 2, 3]  # Forward to these org IDs
  default_priority: "high"
  default_category: "Unclassified"
```

**Result:**
- No AI processing
- All incidents automatically forwarded to organizations 1, 2, and 3
- Priority set to "high" for all incidents
- Useful for testing or emergency broadcast mode

#### Scenario 3: Partial AI (Transcription Only)

```yaml
ai_processing:
  transcription_enabled: true
  categorization_enabled: false
  media_analysis_enabled: false
  auto_forwarding_enabled: false
  default_organizations: []
  default_priority: "medium"
  default_category: "Audio Report"
```

**Result:**
- Voice messages transcribed to text
- No LLM classification or media analysis
- Incidents remain unassigned for manual review

### Configuration Validation

The system validates configuration on startup:

- `default_priority` must be one of: critical, high, medium, low
- `default_category` must be from the incident_categories list
- Warning if `categorization_enabled: false` but `default_organizations` is empty

### Default Organizations

When `categorization_enabled: false` and `auto_forwarding_enabled: true`:
- Incidents are forwarded to ALL organizations in the `default_organizations` list
- Organization IDs must exist in the database and be marked as `active: true`
- Invalid IDs are silently filtered out
- This creates a "broadcast mode" where all configured orgs receive every incident

**How to find organization IDs:**
1. Check the database: `SELECT id, name FROM organizations WHERE active = true;`
2. Or use the dashboard Organizations page to see active org IDs
