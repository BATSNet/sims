<div align="center">
  <img src="resources/sims_square.svg" alt="SIMS Logo" width="200"/>
  <h1>SIMS - Situation Incident Management System</h1>
  <p><strong>Fast, intelligent incident reporting for military and civilian use</strong></p>
</div>

---

## What is SIMS?

SIMS is a dual-use incident alert tracking system that enables rapid reporting of threats, drones, and emergencies directly to an orchestration layer, bypassing traditional reporting chains. Submit incidents in seconds with automatic location capture, media collection, and AI-powered classification.

## Features

- **Rapid Reporting** - Submit incidents in under a few seconds
- **Multi-Modal Input** - Photos, voice messages, and text descriptions
- **Auto Location Capture** - GPS coordinates and device bearing/orientation
- **AI Processing** - Voice-to-text transcription and LLM summarization
- **Smart Classification** - Automatic incident categorization and intelligent routing
- **Real-Time Dashboard** - Operator interface with map visualization and alert management
- **Geospatial Database** - PostGIS integration for location-based queries
- **BMS Integration** - SEDAP/STANAG compatibility for military systems
- **Offline Support** - Queue submissions when network unavailable

## Quick Start

### Using Docker (Recommended)

```bash
# Clone repository
git clone <repository-url>
cd sims-bw

# Configure environment
cp .env.example .env
# Edit .env with your API keys (FEATHER_API_KEY, DEEPINFRA_API_KEY)

# Start all services
docker-compose up -d

# Backend API: http://localhost:8000
# Operator Dashboard: http://localhost:8080
```

### Backend Installation

```bash
cd sims-backend

# Create virtual environment
python -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt

# Run development server
uvicorn app.main:app --reload --port 8000
```

**Database Setup:**
```bash
# Using Docker
docker-compose up -d db

# Or install PostgreSQL with PostGIS extension manually
```

### Mobile App Installation

**Prerequisites:** Flutter SDK, Android Studio/Xcode

```bash
cd sims-app

# Install dependencies
flutter pub get

# Configure backend URL (see below)

# Run on device/emulator
flutter run

# Build APK for Android
flutter build apk --release
```

**Configure Backend Connection:**

Edit `sims-app/lib/src/config/app_config.dart`:

```dart
// For Android Emulator
static const String devBaseUrl = 'http://10.0.2.2:8080';

// For Physical Device (use your computer's local IP)
static const String devBaseUrlPhysical = 'http://192.168.1.100:8080';
```

## Technology Stack

**Backend:** Python 3.11+, FastAPI, NiceGUI, PostgreSQL + PostGIS, Alembic

**Mobile:** Flutter (Android)

**AI/ML:** FeatherAI (LLM), DeepInfra (Speech-to-Text)

**Infrastructure:** Docker, Docker Compose

**Integration:** SEDAP.Express for BMS connectivity

## Project Structure

```
sims-bw/
├── sims-backend/       # Python backend (API + Dashboard)
├── sims-app/          # Flutter mobile application
├── research/          # API docs and integration notes
├── resources/         # Logos and assets
├── docker-compose.yml # Container orchestration
└── LICENSE            # MIT License
```

## Use Cases

**Civilian:** Armored vehicle sightings, airport drone detection, natural disasters (flooding, structural damage)

**Military:** Drone detection near installations, threat identification, real-time patrol situational awareness

## License

MIT License - see [LICENSE](LICENSE) file for details

## Security

- Store API keys in `.env` (never commit)
- Use HTTPS in production
- Validate and sanitize all inputs
- Rate limiting on API endpoints
- Media upload validation
