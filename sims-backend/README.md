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
