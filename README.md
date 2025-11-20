<div align="center">
  <img src="resources/sims-patch.png" alt="SIMS Logo" width="200"/>
</div>

# SIMS - Situation Incident Management System

## Overview

SIMS (Situation Incident Management System) is a dual-use (military and civilian) incident alert tracking system that enables fast reporting of threats, drones, and emergency situations directly to an orchestration layer, bypassing traditional reporting chains.

## Problem Statement

Traditional reporting chains are too slow for real-time situational awareness. People often don't know how to report incidents properly, and there is a critical need for a fallback system for unknown incident types. This system provides direct reporting capability for both military and civilian use cases.

<div align="center">
  <img src="resources/screenshot.png" alt="SIMS Application Screenshot" width="600"/>
</div>

## Key Features
- **Fast Reporting**: Submit incidents in under a few seconds
- **Multi-Modal Input**: Support for photos, voice messages, and text
- **Automatic Location Capture**: GPS coordinates and device bearing/orientation
- **AI Processing**: Voice-to-text transcription and LLM-based summarization
- **Intelligent Classification**: Automatic incident categorization and routing
- **Operator Dashboard**: Real-time visualization and alert management
- **BMS Integration**: SEDAP/STANAG compatibility for military systems

## Use Cases

### Civilian Scenarios
- Armored vehicle sightings: Capture coordinates and photos for forwarding
- Airport drone detection: Quick photo and voice message reporting
- Natural disasters: Flooding, dam breaks - rapid photo submission

### Military Scenarios
- Drone detection: Soldier reports suspected drone near barracks
- Threat identification: Real-time situational awareness for patrol units

## Architecture
### Mobile App (sims-app)
Flutter-based Android application for rapid incident capture:
- Camera and voice recording
- GPS and bearing detection
- Quick submission interface
- Offline queueing capability

### Backend (sims-backend)
Python-based backend with FastAPI:
- REST API for mobile app
- Voice-to-text transcription
- LLM summarization and classification
- PostGIS database for geospatial data
- NiceGUI operator dashboard
- SEDAP.Express integration for BMS forwarding

### Infrastructure
- PostgreSQL with PostGIS extension
- Docker containerization
- External LLM APIs (FeatherAI, DeepInfra)

## Getting Started

### Prerequisites
- Docker and Docker Compose
- Python 3.11+ (for local backend development)
- Flutter SDK (for app development)
- PostgreSQL with PostGIS (or use Docker)

### Running with Docker

```bash
# Clone the repository
git clone <repository-url>
cd sims-bw

# Create environment file
cp .env.example .env
# Edit .env with your API keys

# Start all services
docker-compose up -d

# View logs
docker-compose logs -f backend
```

The backend API will be available at http://localhost:8000

### Backend Development

```bash
cd sims-backend

# Create virtual environment
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt

# Run development server
uvicorn app.main:app --reload
```

### Mobile App Development

```bash
cd sims-app

# Get dependencies
flutter pub get

# Run on connected device
flutter run

# Build APK
flutter build apk
```

## Project Structure

```
sims-bw/
├── sims-backend/       # Python backend (FastAPI, NiceGUI)
├── sims-app/          # Flutter mobile application
├── documents/         # Implementation guides and specifications
├── postgis/           # Database initialization scripts
├── docker-compose.yml # Multi-container orchestration
├── CLAUDE.md          # Development guidelines for Claude Code
└── README.md          # This file
```

## Technology Stack

**Backend:**
- Python 3.11+
- FastAPI (REST API)
- NiceGUI (Operator Dashboard)
- PostgreSQL + PostGIS (Database)
- Alembic (Database migrations)
- FeatherAI (LLM processing)
- DeepInfra (Voice-to-text)

**Mobile App:**
- Flutter
- Android (primary platform)

**Infrastructure:**
- Docker & Docker Compose
- PostGIS/PostgreSQL
- SEDAP.Express (BMS integration)

## Data Model

### Core Entities
- **Incident**: Event records with geospatial data, media, and classification
- **User**: Reporter information
- **Organization**: Responding institutions
- **Classification**: Object type, domain (land/air/sea), movement, direction, threat level

### Integration Format
JSON schema standardized against SEDAP.Express format for BMS compatibility.

## External Integrations

### SEDAP.Express (BMS Integration)

Automatically forwards classified incidents to Battle Management Systems via SEDAP-Express REST API.

**Configuration (.env):**
```bash
SEDAP_API_URL=http://<BMS_IP>:<PORT>/SEDAPEXPRESS
SEDAP_SENDER_ID=SIMS
SEDAP_CLASSIFICATION=U  # P=public, U=unclassified, R=restricted, C=confidential, S=secret, T=top secret
```

Organizations with `api_enabled=true` and `api_type='SEDAP'` will receive forwarded incidents as CONTACT and TEXT messages.

See `documents/SEDAP_DEMO_GUIDE.md` for detailed setup and testing instructions.

### AI Services

- **FeatherAI**: Text summarization, classification, object description
- **DeepInfra**: Automatic speech recognition

### Future Integrations

- **Katwarn**: To be evaluated

## Security Considerations

- Environment variables for API keys and secrets
- Input validation and sanitization
- Rate limiting on API endpoints
- HTTPS in production
- Media upload validation
- Keycloak authentication (planned)

## Contributing

We welcome contributions from the community! SIMS is an open project, and we encourage you to adopt, modify, and extend it for your own use cases.

### How to Contribute

1. **Fork the repository** and create your feature branch
2. **Make your changes** - bug fixes, new features, documentation improvements
3. **Test thoroughly** - ensure your changes work as expected
4. **Submit a pull request** - describe your changes and their purpose

### Areas for Contribution

- Feature enhancements and new capabilities
- Bug fixes and performance improvements
- Documentation and guides
- Integration with additional platforms and services
- Translations and localization
- Security improvements

### Adoption and Modification

Feel free to:
- Adapt this system for your specific use case (civilian emergency response, military operations, enterprise security, etc.)
- Modify the classification system and routing logic
- Integrate with your existing infrastructure
- Build upon the architecture for related applications

We're happy to see this project evolve and serve diverse needs across both civilian and military domains.

For questions, collaboration opportunities, or to discuss major changes, please open an issue or contact the development team.

## Contributors

This project is developed by:

- **Paul Piper** - [aivory.net](https://aivory.net) | [paul-piper.de](https://paul-piper.de) | [ilscipio.com](https://ilscipio.com)
- **Patrick Schult** - [strixx.ai](https://strixx.ai)
- **Tim Engelmann** - [adesso.de](https://adesso.de)
- **Oleksandr Serbin**

## Special Thanks

This project originated during the EDTH (European Defence Tech Hackathon) organized by the [Cyber Innovation Hub](https://cyberinnovationhub.de/).

<div align="center">
  <img src="resources/cihbw.png" alt="Cyber Innovation Hub" height="80"/>
  &nbsp;&nbsp;&nbsp;&nbsp;
  <img src="resources/edth.png" alt="EDTH" height="80"/>
</div>
