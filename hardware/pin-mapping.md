# Hardware Pin Mapping

## ESP32-S3 XIAO Sense (Eyewear Unit)

### Pin Connections

| Component | Function | GPIO Pin | Notes |
|-----------|----------|----------|-------|
| **VL53L1X TOF** | SDA | GPIO 5 | I2C Data |
| **VL53L1X TOF** | SCL | GPIO 6 | I2C Clock |
| **VL53L1X TOF** | VIN | 3.3V | Power |
| **VL53L1X TOF** | GND | GND | Ground |
| **Touch Sensor** | Signal | GPIO 7 | Digital input |
| **Touch Sensor** | VCC | 3.3V | Power |
| **Touch Sensor** | GND | GND | Ground |
| **Camera** | Internal | - | Built-in OV2640 |

### Camera Pin Configuration (Internal - Reference Only)

| Signal | GPIO |
|--------|------|
| XCLK | 10 |
| SIOD (SDA) | 40 |
| SIOC (SCL) | 39 |
| Y9-Y2 | 48,11,12,14,16,18,17,15 |
| VSYNC | 38 |
| HREF | 47 |
| PCLK | 13 |

### Power Options

| Method | Connection | Notes |
|--------|------------|-------|
| USB-C | USB Port | Development/Power bank |
| Battery | BAT+/BAT- pads | 3.7V LiPo, 1000-2000mAh recommended |

---

## ESP32-C3 Super Mini (Handband Unit)

### Pin Connections

| Component | Function | GPIO Pin | Notes |
|-----------|----------|----------|-------|
| **Vibration Motor** | Signal | GPIO 4 | PWM capable |
| **Vibration Motor** | VCC | 5V/3.3V | Check motor specs |
| **Vibration Motor** | GND | GND | Ground |
| **Status LED** | Signal | GPIO 8 | Built-in or external |

### Power Options

| Method | Connection | Notes |
|--------|------------|-------|
| USB-C | USB Port | Development/Power bank |
| Battery | 3.3V/GND pins | 3.7V LiPo with regulator, 500-1000mAh |

---

## Wiring Diagram (Text)

```
EYEWEAR (ESP32-S3 XIAO)
├── GPIO5 (SDA) ──────── VL53L1X SDA
├── GPIO6 (SCL) ──────── VL53L1X SCL
├── GPIO7 ────────────── Touch Sensor Signal
├── 3.3V ─────────────── VL53L1X VIN + Touch VCC
└── GND ──────────────── VL53L1X GND + Touch GND

HANDBAND (ESP32-C3)
├── GPIO4 ────────────── Vibration Motor Signal
├── GPIO8 ────────────── Status LED (optional)
├── 3.3V/5V ──────────── Motor VCC
└── GND ──────────────── Motor GND
```

---

## Important Notes

1. **I2C Pull-ups**: VL53L1X module usually has built-in pull-ups. If not, add 4.7kΩ resistors.

2. **Motor Driver**: For stronger motors, use a transistor or MOSFET driver circuit.

3. **ESP-NOW Requirement**: Both devices MUST connect to the same WiFi network for ESP-NOW to work properly.

4. **MAC Address**: Get C3's MAC from Serial Monitor after first boot or use Broadcast address, then update S3'scode.