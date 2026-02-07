/*
 * ============================================
 * VisionAssist - Handband Unit
 * ============================================
 * 
 * Hardware: ESP32-C3 Super Mini
 * Features:
 *   - Vibration motor control
 *   - Status LED
 *   - ESP-NOW receiver
 *   - Reading mode support (pause vibration)
 * 
 * License: MIT
 * ============================================
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial
#else
#define HWSerial Serial0
#endif

// ===========================================
// WiFi Credentials - CHANGE THESE!
// Must match the eyewear unit
// ===========================================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ===========================================
// Pin Definitions
// ===========================================
#define MOTOR_PIN 4
#define LED_PIN 8

// ===========================================
// Message Structure (must match S3)
// ===========================================
typedef struct {
  char command[32];
  int distance;
  int pattern;
} Message;

Message incomingData;

// ===========================================
// State Variables
// ===========================================
volatile int currentPattern = 0;
unsigned long lastToggle = 0;
unsigned long lastReceived = 0;
bool motorState = false;
int lastPrintedPattern = -1;
int receiveCount = 0;
bool isPaused = false;

// ===========================================
// ESP-NOW Receive Callback
// ===========================================
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  
  lastReceived = millis();
  receiveCount++;
  
  // Check for PAUSE command (reading mode)
  if (strcmp(incomingData.command, "PAUSE") == 0) {
    if (!isPaused) {
      isPaused = true;
      currentPattern = 0;
      digitalWrite(MOTOR_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      motorState = false;
      HWSerial.println("📖 READING MODE - Motor OFF");
    }
    return;
  }
  
  // Normal mode
  if (isPaused) {
    isPaused = false;
    HWSerial.println("📍 NAVIGATION MODE - Motor Active");
  }
  
  currentPattern = incomingData.pattern;
  
  if (incomingData.pattern != lastPrintedPattern) {
    lastPrintedPattern = currentPattern;
    
    HWSerial.print("📩 ");
    HWSerial.print(incomingData.command);
    HWSerial.print(" @ ");
    HWSerial.print(incomingData.distance);
    HWSerial.print("mm (");
    HWSerial.print(receiveCount);
    HWSerial.println(" msgs)");
    
    lastToggle = millis();
    if (currentPattern >= 1) {
      digitalWrite(MOTOR_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);
      motorState = true;
    } else {
      digitalWrite(MOTOR_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      motorState = false;
    }
  }
}

// ===========================================
// Setup
// ===========================================
void setup() {
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  HWSerial.begin(115200);
  delay(2000);
  
  HWSerial.println("\n================================");
  HWSerial.println("  VISIONASSIST - HANDBAND UNIT");
  HWSerial.println("  Vibration Feedback Controller");
  HWSerial.println("================================\n");
  
  // Hardware test
  HWSerial.println("Testing motor & LED...");
  digitalWrite(MOTOR_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  HWSerial.println("✓ Hardware OK\n");
  
  // WiFi connection
  HWSerial.print("Connecting to WiFi: ");
  HWSerial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts++ < 30) {
    delay(500);
    HWSerial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, LOW);
  
  if (WiFi.status() == WL_CONNECTED) {
    HWSerial.println("\n✓ WiFi Connected");
    HWSerial.print("  Channel: ");
    HWSerial.println(WiFi.channel());
    HWSerial.print("  MAC: ");
    HWSerial.println(WiFi.macAddress());
  } else {
    HWSerial.println("\n✗ WiFi FAILED");
  }
  
  // ESP-NOW init
  if (esp_now_init() != ESP_OK) {
    HWSerial.println("✗ ESP-NOW FAILED!");
    while(1) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }
  
  HWSerial.println("✓ ESP-NOW OK");
  esp_now_register_recv_cb(OnDataRecv);
  
  HWSerial.println("\n================================");
  HWSerial.println("  Ready! Waiting for S3...");
  HWSerial.println("================================\n");
  
  lastReceived = millis();
}

// ===========================================
// Loop
// ===========================================
void loop() {
  unsigned long now = millis();
  
  // Connection timeout
  if (now - lastReceived > 1500) {
    if (motorState || currentPattern != 0 || isPaused) {
      digitalWrite(MOTOR_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      motorState = false;
      currentPattern = 0;
      lastPrintedPattern = -1;
      isPaused = false;
      HWSerial.println("⚠️ Connection lost");
    }
    delay(100);
    return;
  }
  
  // If paused (reading mode), keep motor off
  if (isPaused) {
    digitalWrite(MOTOR_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    delay(50);
    return;
  }
  
  // Handle vibration patterns
  switch(currentPattern) {
    
    case 1:  // CRITICAL - Continuous vibration
      digitalWrite(MOTOR_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);
      break;
      
    case 2:  // WARNING - Fast pulses
      if (motorState) {
        if (now - lastToggle >= 400) {
          digitalWrite(MOTOR_PIN, LOW);
          digitalWrite(LED_PIN, LOW);
          motorState = false;
          lastToggle = now;
        }
      } else {
        if (now - lastToggle >= 150) {
          digitalWrite(MOTOR_PIN, HIGH);
          digitalWrite(LED_PIN, HIGH);
          motorState = true;
          lastToggle = now;
        }
      }
      break;
      
    case 3:  // CAUTION - Slow pulses
      if (motorState) {
        if (now - lastToggle >= 300) {
          digitalWrite(MOTOR_PIN, LOW);
          digitalWrite(LED_PIN, LOW);
          motorState = false;
          lastToggle = now;
        }
      } else {
        if (now - lastToggle >= 600) {
          digitalWrite(MOTOR_PIN, HIGH);
          digitalWrite(LED_PIN, HIGH);
          motorState = true;
          lastToggle = now;
        }
      }
      break;
      
    case 0:  // CLEAR - OFF
    default:
      digitalWrite(MOTOR_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      motorState = false;
      break;
  }
  
  delay(10);
}