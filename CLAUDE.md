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

#### Configuring Backend IP and Port

The Flutter app connects to the backend server. To change the IP address and port:

**Configuration File Location:**
```
sims-app/lib/src/config/app_config.dart
```

**Step-by-Step Instructions:**

1. Open `sims-app/lib/src/config/app_config.dart` in your editor

2. Find these configuration constants:
   ```dart
   static const bool isDevelopment = true;
   static const String devBaseUrl = 'http://10.0.2.2:8080';
   static const String devBaseUrlPhysical = 'http://192.168.1.100:8080';
   static const String prodBaseUrl = 'https://api.sims.example.com';
   ```

3. **For Android Emulator:**
   - Change the port in `devBaseUrl` (e.g., from `:8080` to `:3000`)
   - Use `10.0.2.2` as the IP (this is the emulator's special IP to access host machine)
   - Example: `'http://10.0.2.2:3000'`

4. **For Physical Android Device:**
   - Find your computer's local IP address:
     - Windows: Run `ipconfig` in cmd, look for "IPv4 Address"
     - Mac/Linux: Run `ifconfig` or `ip addr`, look for local network IP
   - Change `devBaseUrlPhysical` to your computer's IP and port
   - Example: `'http://192.168.1.100:8080'`

5. **For Production:**
   - Set `isDevelopment = false`
   - Update `prodBaseUrl` with your production server URL

**Common Scenarios:**

| Scenario | Configuration |
|----------|--------------|
| Backend on localhost:8080 (emulator) | `devBaseUrl = 'http://10.0.2.2:8080'` |
| Backend on localhost:3000 (emulator) | `devBaseUrl = 'http://10.0.2.2:3000'` |
| Backend on different port (emulator) | `devBaseUrl = 'http://10.0.2.2:PORT'` |
| Physical device, PC IP 192.168.1.100 | `devBaseUrlPhysical = 'http://192.168.1.100:8080'` |
| Production server | Set `isDevelopment = false`, update `prodBaseUrl` |

**Important Notes:**
- Android emulator MUST use `10.0.2.2` (not `localhost` or `127.0.0.1`)
- Physical devices must use your computer's actual network IP address
- Both devices must be on the same WiFi network
- Restart the Flutter app after changing configuration

#### Backend AI Configuration

The backend uses AI for incident processing. Configure AI features in `sims-backend/config.yaml`:

**Important Configuration Rules:**
- All AI features default to `true` (enabled)
- Each feature can be independently toggled
- When categorization is disabled, use `default_organizations` for broadcast mode
- `default_priority` and `default_category` must be valid values from the categories/priority lists

**Common Configuration Scenarios:**

1. **Full AI Mode** (default):
   - All features enabled
   - Smart incident routing based on AI classification

2. **No AI Mode**:
   - All features disabled
   - Manual categorization and routing

3. **Broadcast Mode**:
   - Categorization disabled
   - Auto-forwarding enabled with default_organizations list
   - All incidents forwarded to configured orgs

See `sims-backend/README.md` for detailed configuration examples.

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

## SSH Server Access

### Production Server
- **Host**: 91.99.179.35
- **Hostname**: sims-demo
- **User**: root
- **SSH Keys Available**:
  - `C:\Users\pp\.ssh\paul_2023.ppk` (PuTTY format, password-protected)
  - `~/.ssh/claude` (OpenSSH format, no passphrase - for Claude Code automation)

### Connection Instructions

**Using OpenSSH (WSL/Linux/Mac) - Recommended for Claude Code:**
```bash
ssh -i ~/.ssh/claude root@91.99.179.35
```

**Using PuTTY (Windows):**
```
Host: 91.99.179.35
Port: 22
Username: root
Private key: C:\Users\pp\.ssh\paul_2023.ppk
```

**Using plink (Windows command line):**
```bash
plink -i "C:\Users\pp\.ssh\paul_2023.ppk" root@91.99.179.35
```

**Notes**:
- The `claude` key is specifically for automated SSH access from WSL/Linux environments
- The `paul_2023.ppk` key requires a passphrase for authentication
- Server fingerprint has been verified and added to known_hosts

### WireGuard VPN for SEDAP/Bundeswehr Network

The server has a WireGuard VPN tunnel configured for connecting to the Bundeswehr SEDAP network.

**VPN Configuration File:** `/etc/wireguard/wg0.conf`

**Current VPN Settings:**
- Server Address: 10.2.12.176/32
- Peer Endpoint: 85.214.12.37:51820
- Allowed IPs: 10.2.12.0/24, 10.2.6.0/24
- SEDAP Target IP: 10.2.6.1:80

**Enable VPN:**
```bash
ssh -i ~/.ssh/claude root@91.99.179.35
wg-quick up wg0
# To auto-start on boot:
systemctl enable wg-quick@wg0
```

**Disable VPN:**
```bash
wg-quick down wg0
# To disable auto-start:
systemctl disable wg-quick@wg0
```

**Check VPN Status:**
```bash
wg show
ip route | grep wg0
```

**Important:** The VPN is currently DISABLED. Enable it only when testing SEDAP integration with the actual Bundeswehr network.

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
