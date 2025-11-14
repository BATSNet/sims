# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Writing Style Guidelines

**CRITICAL**: When working in this repository:
- NEVER use emojis in any code, documentation, commits, or communication
- NEVER use em dashes (—) - use regular hyphens (-) instead
- Keep all writing professional, clear, and straightforward

## Project Overview

**SIMS (Situation Incident Management System)** is a dual-use (military & civilian) incident alert tracking system that enables fast reporting of threats, drones, and emergency situations directly to an orchestration layer, bypassing traditional reporting chains.

The system consists of:
- **sims-app**: Flutter mobile app (Android) for rapid incident reporting
- **sims-backend**: Python backend with API, LLM processing, and operator dashboard
- **research**: Markdown files with API documentation and research notes

## Problem Statement

Traditional reporting chains are too slow for real-time situational awareness. People don't always know how to report incidents properly. This system provides fast (<few seconds), simplified incident reporting with automatic location capture, media collection, and intelligent routing.

## Use Cases

1. **Civilian armored vehicle sighting**: Capture coordinates and photos, forward information
2. **Military drone detection**: Soldier reports suspected drone, forwarded to appropriate channels
3. **Natural disaster**: Flooding/dam breaks - civilian takes photo and sends through system
4. **Civilian airport drone**: Person sees drone near airport, takes picture + voice message, sent to orchestration layer within seconds

## Technical Stack

### Backend (sims-backend)
- **Language**: Python
- **API Framework**: FastAPI (recommended)
- **Dashboard**: NiceGUI for operator dashboard
- **Database**: PostgreSQL with PostGIS extension for geospatial data
- **LLM Integration**: FeatherAI API for text summarization, classification, object description
- **Voice-to-Text**: Model from https://deepinfra.com/models/automatic-speech-recognition/
- **Authentication**: Keycloak (if time allows)

### Mobile App (sims-app)
- **Framework**: Flutter
- **Platform**: Android (primary target)
- **Capabilities**: Camera capture, voice recording, GPS location, bearing/orientation

### Infrastructure
- **Containerization**: Docker & Docker Compose
- **Database**: PostGIS/PostgreSQL
- **Integration Tools**: n8n or similar (maybe)

### External Integrations
- **SEDAP.Express API**: Connects to BMS (Battle Management Systems) - CSV over REST API
- **Katwarn**: To be evaluated

## Architecture

### Data Flow
1. User opens app and captures incident (photo/voice/text)
2. App sends location, media, and description to backend API
3. Backend processes:
   - Transcribes voice to text (if voice message)
   - Summarizes and classifies incident using LLM
   - Routes based on classification
   - Stores in PostGIS database
4. Operator dashboard displays real-time alerts
5. Operator can forward to responsible organizations via SEDAP/STANAG integration

### Core Components

1. **Database (PostGIS/PostgreSQL)**
   - Incident reports with geospatial data
   - User information
   - Organization data
   - Classification metadata

2. **Backend API Services**
   - REST endpoints for mobile app
   - LLM text summarization pipeline
   - Classification engine
   - Voice-to-text transcription service
   - Intelligent routing based on classification

3. **Operator Dashboard (NiceGUI)**
   - Real-time incident overview
   - Map visualization with PostGIS integration
   - Alert management
   - Possible chat interface for follow-up questions

4. **Mobile App**
   - Quick capture interface
   - Camera + voice recording
   - Automatic GPS/bearing capture
   - Offline queueing capability

## Data Model

### Key Entities
- **Incident**: Core event record with geospatial data, media, classification
- **User**: Reporter information
- **Organization**: Responding institutions
- **Classification Tags**: Object type, domain (land/air/sea), movement, direction, threat level

### JSON Schema
Must be standardized against SEDAP.Express format for BMS integration.

## Development Commands

### Backend (sims-backend)

```bash
# Setup virtual environment
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt

# Run development server
python -m uvicorn main:app --reload

# Run tests
pytest

# Database migrations (if using Alembic)
alembic upgrade head
```

### Mobile App (sims-app)

```bash
# Get dependencies
flutter pub get

# Run on connected device/emulator
flutter run

# Build APK
flutter build apk

# Run tests
flutter test

# Check for issues
flutter doctor
```

### App Configuration

The Flutter app uses `lib/src/config/app_config.dart` to manage backend URLs:

**For Development:**
1. Edit `lib/src/config/app_config.dart`
2. Set `isDevelopment = true`
3. Update URLs:
   - `devBaseUrl`: For Android emulator (default: `http://10.0.2.2:8080`)
   - `devBaseUrlPhysical`: For physical devices (use your computer's IP)

**Important URLs:**
- Android emulator uses `10.0.2.2` to access host machine's localhost
- Physical devices need your computer's local IP (e.g., `192.168.1.100:8080`)

**Example config for localhost:8080:**
```dart
static const String devBaseUrl = 'http://10.0.2.2:8080'; // Emulator
static const String devBaseUrlPhysical = 'http://192.168.1.100:8080'; // Physical device
```

### Docker

```bash
# Build and start all services
docker-compose up --build

# Start in detached mode
docker-compose up -d

# View logs
docker-compose logs -f

# Stop all services
docker-compose down

# Rebuild specific service
docker-compose build backend
```

### Database

```bash
# Access PostgreSQL container
docker-compose exec db psql -U postgres -d sims

# Run migrations
docker-compose exec backend alembic upgrade head

# Backup database
docker-compose exec db pg_dump -U postgres sims > backup.sql
```

## Key Requirements

1. **Speed**: Reporting must complete in <few seconds
2. **Location**: Automatic GPS capture with bearing/orientation
3. **Media**: Photo and voice recording support
4. **Processing**: Voice-to-text transcription + LLM summarization
5. **Classification**: Automatic incident categorization and routing
6. **Notifications**: Operator alerts in real-time
7. **Integration**: SEDAP/STANAG compatibility for military BMS

## Classification System

Demo classification tags (to be expanded):
- **Object type**: Vehicle, drone, person, natural disaster, etc.
- **Domain**: land/air/sea
- **Movement**: yes/no
- **Direction/bearing**: Captured from device sensors
- **Threat level**: To be defined based on classification

## Security Considerations

- Never commit `.env` files (use `.env.example` as template)
- Store API keys and secrets in environment variables
- Use Keycloak for authentication when implemented
- Validate and sanitize all user inputs
- Implement rate limiting on API endpoints
- Use HTTPS in production
- Sanitize media uploads (check file types, scan for malware)

## Development Timeline

### Friday 14.11
- Design data model
- Set up PostGIS
- Write API endpoints

### Saturday 15.11
- To be planned

### Sunday 16.11
- Refine implementation
- Prepare pitch

## Important Notes

- Backend must intelligently route incidents based on classification
- Must standardize JSON object format for incident information (check against SEDAP)
- Integration with military BMS via SEDAP.Express is critical requirement
- System serves both military and civilian use cases
- Minimize operator reaction time through effective dashboard design
- Consider offline capability for app (queue submissions when no network)
- Research folder contains markdown files with API documentation and integration details
