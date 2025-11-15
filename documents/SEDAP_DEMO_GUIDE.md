# SEDAP-Express BMS Integration - Demo Setup Guide

## Overview
SIMS automatically forwards incidents to BMS (Battle Management System) via SEDAP-Express API when incidents are assigned to military organizations.

## Pre-Demo Configuration Checklist

### 1. Get BMS Connection Details
Ask the BMS operator for:
- **IP Address** of the BMS system
- **Port** number (default: 80)
- **Endpoint path** (should be `/SEDAPEXPRESS`)
- **Network connectivity** - ensure your system can reach the BMS IP

### 2. Update Environment Variables

Edit `sims-bw/.env` file:

```bash
# SEDAP-Express API Configuration
SEDAP_API_URL=http://<BMS_IP>:<PORT>/SEDAPEXPRESS
SEDAP_SENDER_ID=SIMS
SEDAP_CLASSIFICATION=U
```

**Example for demo:**
```bash
SEDAP_API_URL=http://192.168.1.100:80/SEDAPEXPRESS
SEDAP_SENDER_ID=SIMS
SEDAP_CLASSIFICATION=U
```

### 3. Classification Levels
Choose appropriate classification for your demo:
- `P` = Public
- `U` = Unclassified (recommended for demo)
- `R` = Restricted
- `C` = Confidential
- `S` = Secret
- `T` = Top Secret

### 4. Restart Backend
After updating .env:
```bash
docker-compose restart backend
```

## How SEDAP Integration Works

### Automatic Forwarding
1. User creates incident via mobile app
2. LLM classifies incident (e.g., "drone_detection")
3. System auto-assigns to appropriate organization
4. **IF organization has `api_enabled=true` and `api_type='SEDAP'`**, incident is forwarded to BMS
5. BMS receives CONTACT and TEXT messages

### Current Organizations with SEDAP Enabled
Check database:
```sql
SELECT name, short_name, type, api_enabled, api_type
FROM organization
WHERE api_enabled = true;
```

Currently enabled:
- **Bundeswehr Einsatzfuhrungszentrum** (BwEinsFuZent) - Military
- **Bundeswehr Kommando Territoriale Aufgaben** (KdoTerrAufg) - Military
- **Bundeswehr ABC-Abwehrzentrum** (ABC-AbwZentr) - Military

## Messages Sent to BMS

### CONTACT Message
Contains incident location and basic info:
```
CONTACT;<counter>;<timestamp>;<sender>;<classification>;<ack>;<mac>;
<contactID>;<deleteFlag>;<lat>;<lon>;<altitude>;<relX>;<relY>;<relZ>;
<speed>;<course>;<heading>;<roll>;<pitch>;<width>;<length>;<height>;
<name>;<source>;<sidc>;<mmsi>;<icao>;<image>;<comment>
```

### TEXT Message
Contains incident alert:
```
TEXT;<counter>;<timestamp>;<sender>;<classification>;<ack>;<mac>;
<recipient>;<type>;<encoding>;<text>
```

**Type codes:**
- `01` = Alert (default for incidents)
- `02` = Warning
- `03` = Notice
- `04` = Chat

## Testing SEDAP Connection

### Test 1: Connection Test
```bash
docker-compose exec backend python -c "
import asyncio
from services.sedap_service import SEDAPService

async def test():
    success, msg = await SEDAPService.test_connection('SEDAP')
    print(f'Success: {success}')
    print(f'Message: {msg}')

asyncio.run(test())
"
```

### Test 2: Create Test Incident
Use the mobile app or API to create a drone incident:
```bash
curl -X POST http://localhost:8000/api/incidents \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Drone Detection Demo",
    "description": "Suspicious drone spotted near base perimeter at low altitude, hovering pattern observed",
    "latitude": 50.7374,
    "longitude": 7.0982,
    "heading": 90.0,
    "user_phone": "+491234567890"
  }'
```

### Test 3: Check Logs
Monitor forwarding in backend logs:
```bash
docker-compose logs -f backend | grep -i sedap
```

Expected log entries:
```
INFO - Forwarding incident INC-XXX to SEDAP API
INFO - Successfully forwarded incident INC-XXX to Bundeswehr... via SEDAP
```

## Demo Scenario

### Recommended Demo Flow

1. **Show the mobile app**
   - Open incident report screen
   - Point camera at location
   - Add voice description: "Drone spotted, approximately 50 meters altitude, hovering"
   - Submit incident

2. **Show operator dashboard**
   - Incident appears in real-time via WebSocket
   - Show auto-classification: "drone_detection" with high confidence
   - Show auto-assignment to "Bundeswehr Einsatzfuhrungszentrum"
   - Point out incident on map with location

3. **Show BMS integration**
   - Switch to BMS operator's screen
   - Incident should appear as CONTACT on their tactical map
   - TEXT alert should show in their message panel
   - Timestamp shows near-instant forwarding (< 1 second)

4. **Highlight key features**
   - End-to-end time: ~10-15 seconds from capture to BMS
   - Automatic classification (no operator input needed)
   - Intelligent routing based on incident type
   - Geographic location automatically captured
   - Voice-to-text transcription
   - Real-time WebSocket updates

## Troubleshooting

### Incident not forwarded to BMS

Check:
1. Organization has `api_enabled=true` and `api_type='SEDAP'`
2. Backend logs show forwarding attempt
3. Network connectivity to BMS IP
4. SEDAP_API_URL is correct
5. BMS endpoint is responding

### Connection timeout

```bash
# Check if BMS is reachable
curl -v http://<BMS_IP>:<PORT>/SEDAPEXPRESS

# Check backend logs
docker-compose logs backend | grep -i "timeout\|sedap"
```

### Wrong classification

- Incident classified as "unclassified" instead of specific category
- Check: LLM API is working (logs show classification with confidence)
- Check: Description is clear and detailed enough

## Database Schema for SEDAP

Organizations require these fields for SEDAP integration:
```sql
api_enabled BOOLEAN DEFAULT false    -- Must be true
api_type VARCHAR(50)                 -- Must be 'SEDAP'
```

To enable SEDAP for an organization:
```sql
UPDATE organization
SET api_enabled = true, api_type = 'SEDAP'
WHERE short_name = 'BwEinsFuZent';
```

## Message Format Specification

Full specification: `research/SEDAP-Express ICD v1.0.md`

Key points:
- CSV format with semicolon (`;`) separators
- Hexadecimal counters and timestamps
- 7-bit message counters (wraps at 127)
- 64-bit Unix timestamps in milliseconds
- Geographic coordinates in decimal degrees
- Heading/course: 0-359.999 degrees (0=North, clockwise)

## Security Notes

- Default classification is `U` (Unclassified)
- No MAC (Message Authentication Code) in current implementation
- No encryption - use VPN if required
- BMS should be on isolated network
- Consider enabling MAC for production (see ICD section III.2)

## Contact Information

If BMS connection fails:
1. Verify network connectivity
2. Check BMS operator has started SEDAP-Express endpoint
3. Confirm IP/port/endpoint path
4. Test with simple curl request first
5. Check firewall rules allow traffic to BMS port
