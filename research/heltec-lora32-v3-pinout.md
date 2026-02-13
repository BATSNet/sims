# Heltec WiFi LoRa 32 V3 - Pin Map

```
                     Wi-Fi LoRa 32 V3 Pinout

         Header J3                                    Header J2
    ┌───────────────┐                          ┌───────────────┐
    │               │                          │               │
 1  │ GPIO7         │                          │ GPIO19        │ 1   (USB_D-, CLK_OUT2)
 2  │ GPIO6         │                          │ GPIO20        │ 2   (USB_D+, CLK_OUT1)
 3  │ GPIO5         │                          │ GPIO21        │ 3   [OLED_RST]
 4  │ GPIO4         │                          │ GPIO26        │ 4
 5  │ GPIO3         │       ┌─────────┐        │ GPIO48        │ 5
 6  │ GPIO2         │       │         │        │ GPIO47        │ 6
 7  │ GPIO1         │       │  OLED   │        │ GPIO33        │ 7
 8  │ SUBSPWUP      │       │ Display │        │ GPIO34        │ 8   [LED_White]
 9  │ GPIO39        │       │         │        │ GPIO35        │ 9   (SPI06, Vext_Ctrl)
10  │ GPIO40        │       └─────────┘        │ GPIO36        │ 10
11  │ GPIO41        │                          │ GPIO38        │ 11  [USER_SW]
12  │ GPIO42        │     ┌───────────┐        │ RST           │ 12  [RST_SW]
13  │ GPIO45        │     │  ESP32-S3 │        │ GPIO43        │ 13  [U0TXD, CP2102_RX]
14  │ GPIO46        │     │   + OLED  │        │ GPIO44        │ 14  [U0RXD, CP2102_TX]
15  │ GPIO37        │     │ + LoRa SX │        │ 5V            │ 15  [POWER]
16  │ 3V3           │     │   1262    │        │ 5V            │ 16  [POWER]
17  │ 3V3           │     └───────────┘        │ GND           │ 17  [GND]
18  │ GND           │                          └───────────────┘
    └───────────────┘


┌──────────────────────────────────────────────────────────────┐
│                      OLED (I2C SSD1306)                      │
├──────────────────────────────────────────────────────────────┤
│  SDA:  GPIO17  (OLED_SDA)                                    │
│  SCL:  GPIO18  (OLED_SCL)                                    │
│  RST:  GPIO21  (OLED_RST) - on J2 pin 3                      │
└──────────────────────────────────────────────────────────────┘


┌──────────────────────────────────────────────────────────────┐
│                    LoRa Radio (SX1262)                       │
├──────────────────────────────────────────────────────────────┤
│  NSS:   GPIO8   (LoRa_NSS)   - Chip Select                   │
│  SCK:   GPIO9   (LoRa_SCK)   - SPI Clock                     │
│  MOSI:  GPIO10  (LoRa_MOSI)  - SPI Data Out                  │
│  MISO:  GPIO11  (LoRa_MISO)  - SPI Data In                   │
│  RST:   GPIO12  (LoRa_RST)   - Reset                         │
│  BUSY:  GPIO13  (LoRa_BUSY)  - Busy Signal                   │
│  DIO1:  GPIO14  (DIO1)       - Interrupt                     │
└──────────────────────────────────────────────────────────────┘


┌──────────────────────────────────────────────────────────────┐
│                   Common GPIO Assignments                    │
├──────────────────────────────────────────────────────────────┤
│  Status LED:     GPIO35  (LED_White on J2 pin 8)             │
│  Vext Control:   GPIO36  (controls external power rail)      │
│  User Button:    GPIO38  (USER_SW on J2 pin 11)              │
│  Reset Button:   RST     (RST_SW on J2 pin 12)               │
│  USB Serial RX:  GPIO44  (CP2102_TX)                         │
│  USB Serial TX:  GPIO43  (CP2102_RX)                         │
└──────────────────────────────────────────────────────────────┘


┌──────────────────────────────────────────────────────────────┐
│                   Available GPIO for Sensors                 │
├──────────────────────────────────────────────────────────────┤
│  ADC Capable:                                                │
│    GPIO1-7   (J3 pins 1-7) - ADC1_CH0 to CH6, Touch capable  │
│    GPIO19-20 (J2 pins 1-2) - ADC2_CH8-CH9                    │
│                                                              │
│  Digital I/O:                                                │
│    GPIO26, 33, 34, 47, 48  (on J2)                           │
│    GPIO39-42, 45-46        (on J3)                           │
│                                                              │
│  Recommended for GPS:                                        │
│    TX: GPIO43 (conflicts with USB serial - use alt pins)     │
│    RX: GPIO44 (conflicts with USB serial - use alt pins)     │
│    Alternative: GPIO39/40 or GPIO33/34                       │
│                                                              │
│  Recommended for PTT Button:                                 │
│    GPIO0 (BOOT button) - already on board                    │
│    Alternative: GPIO38 (USER_SW)                             │
└──────────────────────────────────────────────────────────────┘


┌──────────────────────────────────────────────────────────────┐
│                        Power Pins                            │
├──────────────────────────────────────────────────────────────┤
│  5V:   USB power input (J2 pins 15-16)                       │
│  3V3:  Regulated 3.3V output (J3 pins 16-17)                 │
│  GND:  Ground (J2 pin 17, J3 pin 18)                         │
│  Vext: External power rail controlled by GPIO36              │
└──────────────────────────────────────────────────────────────┘


## Pin Assignment for SIMS Mesh Device

Based on the current main.cpp configuration:

```cpp
// LoRa Radio (SX1262) - Built-in
#define LORA_SCK    9
#define LORA_MISO   11
#define LORA_MOSI   10
#define LORA_CS     8
#define LORA_RST    12
#define LORA_DIO1   14
#define LORA_BUSY   13

// OLED Display (SSD1306) - Built-in
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21

// GPS Module (External)
#define GPS_RX      44   // WARNING: Conflicts with CP2102 TX
#define GPS_TX      43   // WARNING: Conflicts with CP2102 RX

// Controls
#define PTT_BUTTON  0    // Boot button (built-in)
#define STATUS_LED  35   // Built-in LED
```

### IMPORTANT: GPIO Conflicts

**GPS_RX/TX pins (43/44) conflict with USB serial debugging!**

**Recommended alternatives for GPS:**
- Use GPIO39 (RX) and GPIO40 (TX) - available on J3 pins 9-10
- Or use GPIO33 (RX) and GPIO34 (TX) - available on J2 pins 7-8

**Update main.cpp to:**
```cpp
#define GPS_RX      39  // J3 pin 9 (MTCK)
#define GPS_TX      40  // J3 pin 10 (MTDO)
```

## References

- Heltec Documentation: https://resource.heltec.cn/download/WiFi_LoRa_32_V3/
- Datasheet: ESP32-S3 (Xtensa dual-core 32-bit LX7, up to 240 MHz)
- LoRa Chip: Semtech SX1262 (868/915 MHz ISM bands)
- Display: 128x64 OLED (SSD1306, I2C address 0x3C)
