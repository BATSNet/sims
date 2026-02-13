# ESP32-S3 Tactical Voice-to-LoRa System
## Command Vocabulary for Military, First Responders & THW

---

## Overview

This system uses offline speech recognition on ESP32-S3 to convert spoken
tactical commands into compact binary messages transmitted via LoRa.

**Principle: Voice in → structured bytes out → LoRa transmit**

No LLM, no cloud, no WiFi required. Every recognized command maps to a
numeric ID (1-300) that fits in 1-2 bytes over LoRa.

---

## Files

| File | Language | Commands | Deployment |
|------|----------|----------|------------|
| `commands_en.txt` | English | 300 | ESP-SR MultiNet7 (native) |
| `commands_de.txt` | German | 300 | Edge Impulse / TFLite (see below) |

---

## English Deployment (ESP-SR MultiNet7)

MultiNet7 natively supports English. Direct deployment path:

### 1. Copy commands file
```
cp commands_en.txt $ESP_SR_PATH/model/multinet_model/fst/commands_en.txt
```

### 2. Generate phonemes (recommended for accuracy)
```
cd $ESP_SR_PATH
python tool/multinet_g2p.py
```
This fills in the third column (phoneme) automatically.

### 3. Configure ESP-IDF project
```
idf.py menuconfig
→ ESP Speech Recognition
  → select MultiNet7 Q8 (quantized, lower RAM)
  → Enable WakeNet9 (wake word: "Hey Radio" or custom)
  → Enable AFE with Noise Suppression (critical for field use)
  → Enable VADNet
```

### 4. Hardware requirements
- ESP32-S3-N16R8 (16MB flash, 8MB PSRAM) — recommended
- ESP32-S3-N8R8 minimum
- I2S MEMS microphone: INMP441 or MSM261S4030H0R
- LoRa module: SX1262 or SX1276 via SPI

### 5. Resource usage (MultiNet7 Q8 + WakeNet9 + AFE)
- ~4.5 MB flash
- ~1.5 MB PSRAM
- Leaves room for camera driver + LoRa stack + JPEG codec

---

## German Deployment (Edge Impulse Workaround)

**ESP-SR MultiNet does NOT support German.** Use Edge Impulse instead.

### Approach A: Edge Impulse Keyword Spotting (Recommended)

1. **Collect training data**
   - Record each command 50-100x from multiple speakers
   - Include background noise samples (siren, engine, wind, radio static)
   - Use noise augmentation in Edge Impulse (critical for field performance)
   - Record at 16kHz mono via INMP441 mic

2. **Edge Impulse project setup**
   - Create project at edgeimpulse.com
   - Processing block: MFCC (frame length 0.02s, frame stride 0.01s)
   - Learning block: Classification (Keras)
   - Target: ESP32-S3 (Espressif ESP-EYE)

3. **Practical limits on ESP32-S3**
   - Edge Impulse keyword spotting works well for ~30-50 keywords per model
   - For 300 commands: use a **hierarchical approach**
     - Level 1 model: Recognize CATEGORY (zahlen/navigation/medizin/feuerwehr/...)
     - Level 2 models: Recognize specific commands within category
     - Switch models based on category or use menu-driven flow
   - Alternative: Group into 6 models of ~50 keywords each, load from flash

4. **Export and deploy**
   ```
   # Export as Arduino library from Edge Impulse
   # In Arduino IDE or ESP-IDF:
   #include "your-project_inferencing.h"
   ```

### Approach B: Hybrid — German wake word + structured input

Use Edge Impulse for ~50 most critical German commands (categories, 
urgency levels, key actions) and use the number/letter spelling system
for free-form input:

- Speaker says: "Anton zwei drei" → system recognizes A-2-3
- Speaker says: "Verletzte drei schwer" → CASUALTY, count=3, severity=SERIOUS
- This reduces the model to ~80 keywords (alphabet + numbers + key verbs)

---

## LoRa Packet Encoding Strategy

### Message format (compact binary)

Every recognized command maps to its ID (1-300 = fits in uint16_t).
Messages are structured as compact binary packets:

```c
// Header (always present) - 8 bytes
typedef struct __attribute__((packed)) {
    uint8_t  sync;           // 0xAA magic byte
    uint8_t  version;        // Protocol version
    uint8_t  sender_id;      // Device ID (0-255)
    uint8_t  msg_type;       // See MSG_TYPE enum
    uint16_t sequence;       // Packet sequence number
    uint16_t crc16;          // CRC of payload
} tac_header_t;

// MSG_TYPE values
#define MSG_COMMAND     0x01  // Single command
#define MSG_SITREP      0x02  // Situation report (multi-field)
#define MSG_MEDEVAC     0x03  // Medical evacuation request
#define MSG_CONTACT     0x04  // Contact report
#define MSG_HAZMAT      0x05  // Hazmat/CBRN alert
#define MSG_PHOTO_FRAG  0x06  // Image fragment
#define MSG_LOCATION    0x07  // Position report
#define MSG_ACK         0x08  // Acknowledgment
#define MSG_HEARTBEAT   0x09  // Alive ping

// Simple command (1 recognized phrase) - 10 bytes total
typedef struct __attribute__((packed)) {
    tac_header_t header;
    uint16_t command_id;     // 1-300 from commands file
} tac_command_t;

// MEDEVAC 9-line (spoken field by field) - 22 bytes total
typedef struct __attribute__((packed)) {
    tac_header_t header;     // 8 bytes
    uint8_t  grid_zone[3];   // 3 bytes: e.g. "33U"
    uint16_t easting;        // 2 bytes: 00000-99999 scaled
    uint16_t northing;       // 2 bytes: 00000-99999 scaled
    uint8_t  casualties;     // 1 byte: count
    uint8_t  urgency;        // 1 byte: 0=routine,1=priority,2=urgent,3=immediate
    uint8_t  injury_type;    // 1 byte: bitmap of injury categories
    uint8_t  security;       // 1 byte: 0=none,1=enemy,2=possible,3=escort
    uint8_t  marking;        // 1 byte: 0=none,1=panels,2=smoke,3=signal
    uint8_t  nationality;    // 1 byte: 0=own,1=allied,2=civilian,3=enemy_pow
} tac_medevac_t;

// Contact report - 16 bytes total
typedef struct __attribute__((packed)) {
    tac_header_t header;     // 8 bytes
    uint16_t bearing;        // 2 bytes: 0-3599 (tenths of degree)
    uint16_t distance_m;     // 2 bytes: distance in meters
    uint8_t  contact_type;   // 1 byte: 0=unknown,1=friendly,2=hostile,3=civilian
    uint8_t  count;          // 1 byte: number observed
    uint8_t  activity;       // 1 byte: 0=stationary,1=moving,2=firing
    uint8_t  reserved;       // 1 byte: alignment
} tac_contact_t;

// Situation report - 18 bytes total
typedef struct __attribute__((packed)) {
    tac_header_t header;     // 8 bytes
    uint8_t  situation;      // 1 byte: overall status code
    uint8_t  personnel_ok;   // 1 byte: count operational
    uint8_t  personnel_cas;  // 1 byte: count casualties
    uint8_t  ammo_level;     // 1 byte: 0-100 percent
    uint8_t  fuel_level;     // 1 byte: 0-100 percent
    uint8_t  supply_level;   // 1 byte: 0-100 percent
    uint16_t request_id;     // 2 bytes: command_id of request
} tac_sitrep_t;

// Image fragment - max ~230 bytes per packet
typedef struct __attribute__((packed)) {
    tac_header_t header;     // 8 bytes
    uint16_t image_id;       // 2 bytes: identifies which image
    uint16_t total_frags;    // 2 bytes: total fragments
    uint16_t frag_index;     // 2 bytes: this fragment number
    uint8_t  data[];         // remaining bytes (up to ~208 bytes)
} tac_photo_frag_t;
```

### Bandwidth budget

| Message Type | Size | SF7/BW125 | SF12/BW125 |
|-------------|------|-----------|------------|
| Single command | 10 B | ~15 ms | ~0.3 s |
| MEDEVAC 9-line | 22 B | ~32 ms | ~0.6 s |
| Contact report | 16 B | ~23 ms | ~0.4 s |
| SITREP | 18 B | ~26 ms | ~0.5 s |
| Photo (2KB) | 10 packets | ~2 s | ~30 s |
| Photo (4KB) | 20 packets | ~4 s | ~60 s |

### Image transmission strategy

For a 160x120 grayscale JPEG at quality 10-15:
- File size: ~1.5-3 KB
- Fragment into ~208-byte chunks
- Add sequence numbers + CRC per fragment
- Receiver reassembles + requests retransmit of missing fragments
- Total time: 2-60 seconds depending on LoRa config

---

## Recommended Hardware BOM

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3-DevKitC-1 N16R8 | Main processor |
| Microphone | INMP441 I2S MEMS | Voice input |
| LoRa | SX1262 module (868 MHz EU / 915 MHz US) | Long range comms |
| Camera | OV2640 (optional) | Image capture |
| Antenna | Quarter-wave whip (8.6 cm @ 868 MHz) | LoRa antenna |
| Power | 3.7V LiPo 2000+ mAh | Battery |
| Speaker | Small speaker + PAM8403 (optional) | Audio feedback/TTS |

### Pin allocation (ESP32-S3-N16R8)

```
I2S Microphone (INMP441):
  SCK  → GPIO 4
  WS   → GPIO 5
  SD   → GPIO 6

SPI LoRa (SX1262):
  SCK  → GPIO 12
  MOSI → GPIO 11
  MISO → GPIO 13
  NSS  → GPIO 10
  RST  → GPIO 14
  DIO1 → GPIO 15
  BUSY → GPIO 16

Camera (OV2640, if used):
  Uses default ESP32-S3-CAM pins or DVP interface
  XCLK → GPIO 40
  SIOD → GPIO 17
  SIOC → GPIO 18
  (remaining camera pins as per devkit)

Status LED:
  → GPIO 48 (onboard RGB on most devkits)

Button (PTT / wake):
  → GPIO 0 (BOOT button, active low)
```

---

## Operational Flow

```
IDLE (low power, WakeNet listening)
  │
  ├─ Wake word detected ("Hey Radio" / "Achtung Funk")
  │
  ▼
LISTENING (MultiNet / Edge Impulse active)
  │
  ├─ User speaks command(s)
  │   e.g. "MEDEVAC ... grid reference ... alpha two three ..."
  │
  ├─ Each recognized word → command_id
  │   System builds structured message from sequence of IDs
  │
  ├─ Confirmation beep or TTS: "Medevac message ready"
  │
  ├─ User says "send it" (command_id 67)
  │
  ▼
TRANSMIT
  │
  ├─ Encode to compact binary struct
  ├─ Calculate CRC
  ├─ Transmit via LoRa
  ├─ Wait for ACK (if confirmed mode)
  │
  ▼
IDLE (return to low power)
```

---

## Voice-Guided Message Building (Example)

User wants to send a MEDEVAC request:

```
User:   "Hey Radio"                    → wake
System: *beep*                         → listening
User:   "Medevac"                      → msg_type = MSG_MEDEVAC
System: "Grid?"                        → TTS prompt
User:   "Three Three Uniform"          → grid_zone = "33U"
User:   "Alpha Two Three Four Five"    → easting = A2345
User:   "Bravo Six Seven Eight Nine"   → northing = B6789
System: "Casualties?"
User:   "Two"                          → casualties = 2
User:   "Urgent"                       → urgency = 2
User:   "Chest injury"                 → injury_type |= CHEST
System: "Send?"
User:   "Send it"                      → TRANSMIT 22-byte packet
System: *confirmation beep*
```

Total LoRa airtime: ~30 milliseconds at SF7/BW125. Done.

---

## Language Switching

For bilingual DE/EN operation:

- English: Use ESP-SR MultiNet7 natively
- German: Load Edge Impulse TFLite model from flash
- Switch language via command: "switch german" / "wechsel englisch"
- Or: hardware switch / button to toggle
- Both models can coexist in flash (16MB), load one at a time into PSRAM

---

## Command Categories Quick Reference

| ID Range | EN Category | DE Category |
|----------|-------------|-------------|
| 1-30 | Numbers | Zahlen |
| 31-56 (EN) / 27-52 (DE) | NATO Alphabet / Buchstabiertafel | Buchstabiertafel |
| 57-75 (EN) / 53-75 (DE) | Prowords | Funkverkehr |
| 76-100 | Status / SITREP | Lagemeldung |
| 101-118 (EN) / 101-115 (DE) | Navigation | Navigation |
| 119-155 (EN) / 116-155 (DE) | Medical / Casualty | Medizin / Verletzte |
| 156-185 | Fire / Rescue | Feuerwehr |
| 186-210 | HAZMAT / CBRN | Gefahrgut / ABC |
| 211-240 (EN) / 211-250 (DE) | Technical Relief | THW |
| 241-252 (EN) / 251-260 (DE) | Weather | Wetter |
| 253-275 (EN) / 261-285 (DE) | Resources / Logistics | Einsatzmittel |
| 276-295 (EN) / 286-298 (DE) | Command / Control | Führung |
| 296-300 (EN) / 299-300 (DE) | Device Control | Gerätesteuerung |

---

## Known Limitations

1. **Not free-form speech.** This is keyword recognition, not dictation.
   For anything outside the 300-word vocabulary, the system won't understand.

2. **German requires extra work.** No native MultiNet support means you need
   to collect audio training data and train via Edge Impulse.

3. **Noise degrades accuracy.** The AFE helps, but extreme noise (gunfire,
   helicopter rotor wash at close range) will reduce recognition rates.
   Push-to-talk via button + close-talk mic positioning helps significantly.

4. **LoRa images are slow.** A 3KB image takes 4-60 seconds depending on
   config. This is for situational awareness, not video surveillance.

5. **Command sequences require application logic.** MultiNet recognizes
   individual commands — the firmware must implement the state machine
   that assembles them into structured messages (MEDEVAC, SITREP, etc.).
