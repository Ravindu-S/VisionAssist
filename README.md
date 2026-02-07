<div align="center">

# 👓 VisionAssist

### Assistive Eyewear System for Visually Impaired Individuals

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/)
[![IDE: PlatformIO](https://img.shields.io/badge/IDE-PlatformIO-orange.svg)](https://platformio.org/)
[![Status: Working](https://img.shields.io/badge/Status-Working-success.svg)]()

*A wearable assistive device that combines obstacle detection with text-to-speech capabilities, helping visually impaired users navigate safely and read text from their environment.*

---

[Features](#-features) • [Hardware](#-hardware) • [Installation](#-installation) • [Usage](#-usage) • [Documentation](#-documentation)

---

</div>

## 📋 Overview

VisionAssist is an assistive eyewear system designed for visually impaired individuals. The system consists of two main components:

- **Smart Eyewear**: Equipped with a TOF laser sensor for obstacle detection and a camera for text recognition (OCR)
- **Haptic Handband**: Provides tactile feedback through vibration patterns, making it suitable for deaf-blind users

> 🎓 This project was selected for the **Annual University Exhibition** among 30+ projects.

### Why Vibration Instead of Sound?

Traditional assistive devices use buzzers or audio alerts. VisionAssist uses **haptic feedback (vibration)** because:
- Works for deaf-blind users
- Non-disturbing in public spaces
- Intuitive distance perception through vibration patterns
- Audio channel remains free for text-to-speech
- Audio output plays through any speaker connected to the device running the web interface

---

## ✨ Features

### 🔍 Obstacle Detection
| Zone | Distance | Vibration Pattern |
|------|----------|-------------------|
| 🟢 **Clear** | > 2.0m | No vibration |
| 🟡 **Caution** | 1.6m - 2.0m | Slow pulse (300ms ON / 600ms OFF) |
| 🟠 **Warning** | 1.3m - 1.6m | Fast pulse (400ms ON / 150ms OFF) |
| 🔴 **Critical** | < 1.3m | Continuous vibration |

### 📖 Text Recognition (OCR)
- Touch-triggered image capture
- Google Cloud Vision API integration
- Text-to-Speech output via web interface
- Automatic vibration pause during reading

### 🌐 Web Interface
- Real-time distance monitoring
- Manual OCR trigger
- TTS controls (speak/stop)
- Mobile-friendly responsive design

---

## 🔧 Hardware

### Components List

| Component | Model | Purpose |
|-----------|-------|---------|
| Eyewear MCU | Seeed Studio XIAO ESP32S3 Sense | Camera, TOF sensor, processing |
| Handband MCU | ESP32-C3 Super Mini | Vibration motor control |
| Camera | OV2640 (built-in on XIAO) | Image capture for OCR |
| Distance Sensor | VL53L1X TOF Laser | Obstacle detection (up to 4m) |
| Feedback | Vibration Motor Module | Haptic alerts |
| Touch Sensor | Capacitive Touch | Trigger OCR |

### System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ EYEWEAR (ESP32-S3)                                              │
│   ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐   │
│   │ OV2640   │ │ VL53L1X  │ │ Touch    │ │ WiFi +           │   │
│   │ Camera   │ │ TOF      │ │ Sensor   │ │ ESP-NOW          │   │
│   └────┬─────┘ └────┬─────┘ └────┬─────┘ └────────┬─────────┘   │
│        │            │            │                │             │
│        └────────────┴────────────┴────────────────┘             │
│                                                                 │
└──────────────────────────────┬──────────────────────────────────┘
                               │ ESP-NOW (Same WiFi Network)
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│ HANDBAND (ESP32-C3)                                             │
│                 ┌──────────────┐ ┌──────────────┐               │
│                 │ Vibration    │ │ Status       │               │
│                 │ Motor        │ │ LED          │               │
│                 └──────────────┘ └──────────────┘               │
└──────────────────────────────┬──────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│ SMARTPHONE (Browser)                                            │
│          ┌─────────────────────────────────────────┐            │
│          │ Web Interface + Text-to-Speech (TTS)    │            │
│          └─────────────────────────────────────────┘            │
└─────────────────────────────────────────────────────────────────┘
```

### Power Supply

The system can be powered in two ways:

| Method | Details |
|--------|---------|
| **USB-C Cable** | Direct connection to power bank (used in prototype) |
| **Battery** | LiPo battery connection to battery pins (for portable use) |

**Recommended for portable use:**
- ESP32-S3: 3.7V LiPo (1000-2000mAh) via BAT+/BAT- pads
- ESP32-C3: 3.7V LiPo (500-1000mAh) via 3.3V/GND pins

> ⚠️ **Known Limitation**: Battery life varies based on usage. TOF sensor and WiFi are power-intensive. Detailed power analysis available in the project report.

---

## 📌 Pin Mapping

### ESP32-S3 XIAO (Eyewear)

| Function | GPIO Pin |
|----------|----------|
| I2C SDA (TOF) | GPIO 5 |
| I2C SCL (TOF) | GPIO 6 |
| Touch Sensor | GPIO 7 |
| Camera | Internal (see code) |

### ESP32-C3 Super Mini (Handband)

| Function | GPIO Pin |
|----------|----------|
| Vibration Motor | GPIO 4 |
| Status LED | GPIO 8 |

---

## 🚀 Installation

### Prerequisites

- [VS Code](https://code.visualstudio.com/) with [PlatformIO Extension](https://platformio.org/install/ide?install=vscode)
- [Google Cloud Vision API Key](https://cloud.google.com/vision/docs/setup)
- WiFi Network (both devices must connect to the same network)

### Step 1: Clone the Repository

```bash
git clone https://github.com/Ravindu-S/VisionAssist.git
cd VisionAssist
```

### Step 2: Configure Eyewear (ESP32-S3)

Open firmware/eyewear-s3/ in PlatformIO
Edit src/main.cpp:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* apiKey = "YOUR_GOOGLE_CLOUD_VISION_API_KEY";
```

Get the MAC address of your ESP32-C3 or use Broadcast Address (upload C3 code first, check Serial Monitor)
Update the broadcast address:

```cpp
uint8_t broadcastAddress[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
```

or Broadcast Address
```cpp
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
```

### Step 3: Configure Handband (ESP32-C3)

Open firmware/handband-c3/ in PlatformIO
Edit src/main.cpp:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### Step 4: Upload

- Connect ESP32-S3 → Upload eyewear firmware
- Connect ESP32-C3 → Upload handband firmware

### Step 5: Verify

- Open Serial Monitor for both devices (115200 baud)
- Both should show ✓ WiFi Connected and ✓ ESP-NOW OK
- Access web interface at the IP shown in S3's Serial Monitor

> ⚠️ Important: ESP-NOW communication only works here when both devices are connected to the same WiFi network.

---

## 📱 Usage

### Basic Operation

1. Power on both devices
2. Connect your smartphone to the same WiFi network
3. Open the web interface URL (shown in Serial Monitor)
4. Tap the camera image to enable voice output
5. Navigate - feel vibration patterns for obstacles
6. Touch the sensor on eyewear to read text

### Web Interface Features

| Button | Function |
|--------|----------|
| 📖 Read Text | Capture image and perform OCR |
| 🔊 Speak | Read detected text aloud |
| ⏹️ Stop | Stop current speech |

### Reading Mode

When text is being read:

- Vibration motor pauses automatically
- Distance indicator shows "READING 📖"
- Normal navigation resumes after speech ends

---

## 📁 Project Structure

```
VisionAssist/
├── README.md                   # This file
├── LICENSE                     # MIT License
├── .gitignore                  # Git ignore rules
│
├── firmware/
│   ├── eyewear-s3/             # ESP32-S3 Eyewear Code
│   │   ├── platformio.ini
│   │   └── src/
│   │       └── main.cpp
│   │
│   └── handband-c3/            # ESP32-C3 Handband Code
│       ├── platformio.ini
│       └── src/
│           └── main.cpp
│
├── docs/                       # Documentation
│   └── Project_Report.pdf      # Detailed project report
│
└── hardware/                   # Hardware documentation
    └── pin-mapping.md          # Detailed pin connections
```

---

## 📚 Documentation

Detailed project documentation including:

- Complete circuit analysis
- Power consumption measurements
- Battery recommendations
- Testing results
- Future improvements

📄 See: docs/Project_Report.pdf

---

## ⚠️ Known Limitations

| Limitation | Details |
|-----------|---------|
| Battery Life | Continuous WiFi and TOF sensing consumes significant power |
| TOF Range | VL53L1X effective range is ~4m max, recommended <2m for accuracy |
| WiFi Dependency | Both devices must be on same network for ESP-NOW |
| OCR Requires Internet | Google Cloud Vision API needs active internet connection |

---

## 🛠️ Technologies Used

- Microcontrollers: ESP32-S3, ESP32-C3
- Communication: ESP-NOW, WiFi, HTTP
- APIs: Google Cloud Vision (OCR)
- Frontend: HTML5, CSS3, JavaScript, Web Speech API
- IDE: PlatformIO (VS Code)
- Framework: Arduino

---

## 🙏 Acknowledgments

Special thanks to Kusal Hettiarachchi for funding support and making this project possible.

---

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

Copyright (c) 2024 Ravindu Senanayake and Kusal Hettiarachchi

---

## 📬 Contact

GitHub: @Ravindu-S

---

<div align="center">
Made with ❤️ for accessibility

If this project helps someone, please consider giving it a ⭐
</div>