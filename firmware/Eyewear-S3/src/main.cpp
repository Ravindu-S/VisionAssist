/*
 * ============================================
 * VisionAssist - Eyewear Unit
 * ============================================
 * 
 * Hardware: Seeed Studio XIAO ESP32S3 Sense
 * Features:
 *   - OV2640 Camera for OCR
 *   - VL53L1X TOF Sensor for distance
 *   - Touch sensor for triggering OCR
 *   - ESP-NOW for communication with handband
 *   - Web interface with TTS
 * 
 * License: MIT
 * ============================================
 */

#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <mbedtls/base64.h>
#include <Wire.h>
#include <Adafruit_VL53L1X.h>
#include <esp_now.h>

// ===========================================
// WiFi Credentials - CHANGE THESE!
// ===========================================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ===========================================
// Google Cloud Vision API Key - CHANGE THIS!
// ===========================================
const char* apiKey = "YOUR_GOOGLE_CLOUD_VISION_API_KEY";

// ===========================================
// Touch Sensor Pin
// ===========================================
#define TOUCH_PIN 7

// ===========================================
// I2C Pins for TOF Sensor
// ===========================================
#define I2C_SDA 5
#define I2C_SCL 6

// ===========================================
// Distance Thresholds (mm)
// ===========================================
#define CRITICAL_DISTANCE 1300
#define WARNING_DISTANCE 1600
#define CAUTION_DISTANCE 2000

// ===========================================
// Camera Pins
// ===========================================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// ===========================================
// ESP-NOW Setup - CHANGE MAC ADDRESS!
// ===========================================
// Replace with your ESP32-C3's MAC address or Broadcast address
uint8_t broadcastAddress[] = {0x88, 0x56, 0xA6, 0x64, 0x21, 0x6C};

typedef struct {
  char command[32];
  int distance;
  int pattern;
} Message;

Message outgoingData;
esp_now_peer_info_t peerInfo;

// ===========================================
// Globals
// ===========================================
WebServer server(80);
Adafruit_VL53L1X vl53 = Adafruit_VL53L1X();

uint32_t imageCount = 0;
String lastOcrText = "No text detected yet";
bool sensorReady = false;

// ESP-NOW status
unsigned long lastSendSuccess = 0;
unsigned long lastSendFail = 0;
int sendSuccessCount = 0;
int sendFailCount = 0;

// TOF smoothing
int goodReadings[5] = {5000, 5000, 5000, 5000, 5000};
int readIndex = 0;
int smoothedDistance = 5000;
unsigned long lastGoodReading = 0;
int lastPattern = -1;
int stableCount = 0;
int currentStablePattern = 0;
unsigned long lastPrint = 0;

// ===========================================
// Touch Sensor & TTS State
// ===========================================
bool touchTriggered = false;
bool ocrInProgress = false;
bool newOcrAvailable = false;
bool ttsSpeaking = false;
bool distancePaused = false;
unsigned long lastTouchTime = 0;
unsigned long ttsStartTime = 0;
#define TOUCH_DEBOUNCE 1000
#define TTS_TIMEOUT 60000
unsigned long autoResumeTime = 0;
#define NO_TEXT_RESUME_DELAY 2500

// ===========================================
// HTML Interface with TTS + Touch Support
// ===========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Assistive Eyewear</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; text-align: center; background: #1a1a2e; color: white; padding: 15px; margin: 0; }
        h1 { color: #00d4ff; margin-bottom: 5px; font-size: 24px; }
        .subtitle { color: #888; margin-top: 0; font-size: 14px; }
        img { max-width: 100%; border: 3px solid #00d4ff; border-radius: 10px; margin: 15px 0; }
        .distance-box { background: #0f3460; padding: 15px; border-radius: 10px; margin: 15px auto; max-width: 600px; border: 2px solid #00d4ff; font-size: 20px; font-weight: bold; }
        .ocr-box { background: #0f3460; padding: 15px; border-radius: 10px; margin: 15px auto; max-width: 600px; min-height: 80px; border: 2px solid #00d4ff; white-space: pre-wrap; text-align: left; font-size: 16px; }
        .btn { background: #00d4ff; color: black; padding: 12px 25px; border: none; border-radius: 8px; font-size: 16px; cursor: pointer; margin: 8px; font-weight: bold; }
        .btn:hover { background: #00a8cc; }
        .btn:active { transform: scale(0.95); }
        .btn-speak { background: #00ff88; }
        .btn-stop { background: #ff4444; color: white; }
        .status-bar { background: #16213e; padding: 10px; border-radius: 8px; margin: 10px auto; max-width: 600px; font-size: 14px; }
        .tts-enabled { color: #00ff88; }
        .tts-disabled { color: #ff4444; }
        .reading-mode { color: #ffaa00; }
        .controls { background: #16213e; padding: 15px; border-radius: 10px; margin: 15px auto; max-width: 600px; }
        .mode-indicator { font-size: 18px; padding: 10px; margin: 10px 0; border-radius: 8px; }
        .mode-normal { background: #0f3460; }
        .mode-reading { background: #664400; border: 2px solid #ffaa00; }
        .processing { animation: pulse 1s infinite; }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
    </style>
</head>
<body>
    <h1> Assistive Eyewear</h1>
    <p class="subtitle">Touch sensor or tap to read text</p>
    
    <div class="status-bar">
         Voice: <span id="ttsStatus" class="tts-disabled">Tap to Enable</span>
        &nbsp;|&nbsp;
         Distance: <span id="distanceStatus">Active</span>
    </div>
    
    <div class="mode-indicator mode-normal" id="modeIndicator">
         Navigation Mode
    </div>
    
    <div class="distance-box" id="distanceBox">
         <span id="distance">---</span> mm
        <br>
        <span id="alert" style="font-size: 16px;">---</span>
    </div>
    
    <img id="camera" src="/capture" onclick="enableTTS()" />
    
    <div class="controls">
        <button class="btn" onclick="runOCR()"> Read Text</button>
        <button class="btn btn-speak" onclick="speakText()"> Speak</button>
        <button class="btn btn-stop" onclick="stopSpeech()"> Stop</button>
    </div>
    
    <h3 style="margin-bottom: 10px;">Detected Text:</h3>
    <div class="ocr-box" id="ocrText">Touch sensor or tap "Read Text" to scan...</div>
    
    <script>
        let ttsEnabled = false;
        let lastSpokenText = "";
        let speaking = false;
        let pollCount = 0;
        
        const synth = window.speechSynthesis;
        const ttsSupported = 'speechSynthesis' in window;
        
        function enableTTS() {
            if (!ttsSupported) {
                document.getElementById("ttsStatus").innerText = "Not Supported";
                return;
            }
            
            if (!ttsEnabled) {
                const utterance = new SpeechSynthesisUtterance("");
                synth.speak(utterance);
                ttsEnabled = true;
                document.getElementById("ttsStatus").innerText = "Enabled ✓";
                document.getElementById("ttsStatus").className = "tts-enabled";
                
                setTimeout(() => {
                    speak("Voice enabled. Touch sensor to read text.");
                }, 100);
            }
        }
        
        function speak(text) {
            if (!ttsEnabled || !text || text.length < 2) return;
            
            synth.cancel();
            
            const utterance = new SpeechSynthesisUtterance(text);
            utterance.rate = 0.9;
            utterance.pitch = 1.0;
            utterance.volume = 1.0;
            
            const voices = synth.getVoices();
            for (let v of voices) {
                if (v.lang.startsWith('en') && v.name.includes('Female')) {
                    utterance.voice = v;
                    break;
                }
            }
            
            utterance.onstart = () => { 
                speaking = true;
                updateModeIndicator(true);
            };
            
            utterance.onend = () => { 
                speaking = false;
                updateModeIndicator(false);
                fetch("/tts_done").catch(() => {});
            };
            
            utterance.onerror = () => { 
                speaking = false;
                updateModeIndicator(false);
                fetch("/tts_done").catch(() => {});
            };
            
            synth.speak(utterance);
        }
        
        function updateModeIndicator(reading) {
            const indicator = document.getElementById("modeIndicator");
            const distStatus = document.getElementById("distanceStatus");
            const ocrBox = document.getElementById("ocrText");
            
            if (reading) {
                indicator.className = "mode-indicator mode-reading";
                indicator.innerHTML = " Reading Mode (Vibration Paused)";
                distStatus.innerHTML = "<span class='reading-mode'>Paused</span>";
            } else {
                indicator.className = "mode-indicator mode-normal";
                indicator.innerHTML = " Navigation Mode";
                distStatus.innerHTML = "Active";
                ocrBox.classList.remove("processing");
            }
        }
        
        function speakText() {
            enableTTS();
            const text = document.getElementById("ocrText").innerText;
            if (text && !text.includes("Touch sensor") && !text.includes("Analyzing") && !text.includes("Processing")) {
                speak(text);
            } else {
                speak("No text to read");
            }
        }
        
        function stopSpeech() {
            synth.cancel();
            speaking = false;
            updateModeIndicator(false);
            fetch("/tts_done").catch(() => {});
        }
        
        function refresh() {
            const img = document.getElementById("camera");
            const newSrc = "/capture?t=" + new Date().getTime();
            
            const tempImg = new Image();
            tempImg.onload = () => {
                img.src = newSrc;
            };
            tempImg.onerror = () => {
                console.log("Image refresh failed, retrying...");
                setTimeout(refresh, 500);
            };
            tempImg.src = newSrc;
        }
        
        function runOCR() {
            enableTTS();
            const ocrBox = document.getElementById("ocrText");
            ocrBox.innerText = " Analyzing image...";
            ocrBox.classList.add("processing");
            updateModeIndicator(true);
            speak("Scanning");
            
            fetch("/ocr")
                .then(r => {
                    if (!r.ok) throw new Error("HTTP " + r.status);
                    return r.text();
                })
                .then(data => {
                    console.log("OCR Result:", data);
                    ocrBox.innerText = data;
                    ocrBox.classList.remove("processing");
                    refresh();
                    
                    if (data && !data.startsWith("Error") && data !== "No text detected") {
                        speak(data);
                        lastSpokenText = data;
                    } else {
                        speak("No text found");
                        updateModeIndicator(false);
                        fetch("/tts_done").catch(() => {});
                    }
                })
                .catch(e => {
                    console.log("OCR Error:", e);
                    ocrBox.innerText = "Error: " + e.message;
                    ocrBox.classList.remove("processing");
                    speak("Error occurred");
                    updateModeIndicator(false);
                    fetch("/tts_done").catch(() => {});
                });
        }
        
        function checkForNewOcr() {
            fetch("/ocr_status")
                .then(r => {
                    if (!r.ok) throw new Error("HTTP " + r.status);
                    return r.json();
                })
                .then(data => {
                    pollCount++;
                    
                    if (data.reading && !data.newText) {
                        const ocrBox = document.getElementById("ocrText");
                        if (!ocrBox.innerText.includes("Processing") && !ocrBox.innerText.includes("Analyzing")) {
                            ocrBox.innerText = " Processing... (touch detected)";
                            ocrBox.classList.add("processing");
                        }
                        updateModeIndicator(true);
                    }
                    
                    if (data.newText && data.text) {
                        console.log("New OCR text received:", data.text.substring(0, 50) + "...");
                        
                        enableTTS();
                        const ocrBox = document.getElementById("ocrText");
                        ocrBox.innerText = data.text;
                        ocrBox.classList.remove("processing");
                        
                        refresh();
                        
                        if (data.text !== lastSpokenText) {
                            if (!data.text.startsWith("Error") && data.text !== "No text detected") {
                                setTimeout(() => {
                                    speak(data.text);
                                    lastSpokenText = data.text;
                                }, 200);
                            } else {
                                speak("No text found");
                                fetch("/tts_done").catch(() => {});
                            }
                        }
                        
                        fetch("/ocr_ack").catch(() => {});
                    }
                })
                .catch(err => {
                    if (pollCount % 20 === 0) {
                        console.log("Poll error:", err);
                    }
                });
        }
        
        setInterval(() => {
            fetch("/distance")
                .then(r => r.json())
                .then(data => {
                    document.getElementById("distance").innerText = data.distance;
                    document.getElementById("alert").innerText = data.status;
                    
                    let box = document.getElementById("distanceBox");
                    if (data.paused) {
                        box.style.borderColor = "#ffaa00";
                    } else if(data.pattern === 1) {
                        box.style.borderColor = "#ff0000";
                    } else if(data.pattern === 2) {
                        box.style.borderColor = "#ff8800";
                    } else if(data.pattern === 3) {
                        box.style.borderColor = "#ffff00";
                    } else {
                        box.style.borderColor = "#00d4ff";
                    }
                })
                .catch(() => {});
        }, 300);
        
        setInterval(checkForNewOcr, 400);
        
        if (ttsSupported) {
            synth.getVoices();
            speechSynthesis.onvoiceschanged = () => { synth.getVoices(); };
        }
        
        document.body.addEventListener('click', enableTTS);
        document.body.addEventListener('touchstart', enableTTS);
        
        console.log("Assistive Eyewear UI loaded");
    </script>
</body>
</html>
)rawliteral";

// ===========================================
// Camera Init
// ===========================================
bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    
    if (!psramFound()) {
        config.frame_size = FRAMESIZE_VGA;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.fb_count = 1;
    }
    
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }
    
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 1);
        s->set_contrast(s, 1);
        s->set_saturation(s, -1);
    }
    
    Serial.println("✓ Camera OK");
    return true;
}

// ===========================================
// TOF Sensor Init
// ===========================================
bool initTOF() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    delay(100);
    
    if (vl53.begin(0x29, &Wire)) {
        Serial.println("✓ TOF Sensor Found");
        if (vl53.startRanging()) {
            vl53.setTimingBudget(50);
            sensorReady = true;
            Serial.println("✓ Ranging Active (50ms fast mode)");
            return true;
        }
    }
    Serial.println("✗ TOF Sensor NOT found");
    return false;
}

// ===========================================
// Extract Text from Response
// ===========================================
String extractTextFromResponse(WiFiClient* stream) {
    String result = "";
    String buffer = "";
    bool foundDescription = false;
    
    while (stream->available()) {
        char c = stream->read();
        buffer += c;
        
        if (buffer.length() > 1000) {
            int descIndex = buffer.indexOf("\"description\"");
            
            if (descIndex >= 0 && !foundDescription) {
                int colonIndex = buffer.indexOf(":", descIndex);
                if (colonIndex >= 0) {
                    int quoteStart = buffer.indexOf("\"", colonIndex);
                    if (quoteStart >= 0) {
                        quoteStart++;
                        
                        bool escaped = false;
                        for (int i = quoteStart; i < buffer.length(); i++) {
                            char ch = buffer[i];
                            
                            if (escaped) {
                                if (ch == 'n') result += '\n';
                                else if (ch == 't') result += ' ';
                                else if (ch == 'r') { }
                                else if (ch == '\\') result += '\\';
                                else if (ch == '"') result += '"';
                                else result += ch;
                                escaped = false;
                            } else {
                                if (ch == '\\') {
                                    escaped = true;
                                } else if (ch == '"') {
                                    foundDescription = true;
                                    return result;
                                } else {
                                    result += ch;
                                }
                            }
                        }
                        
                        buffer = buffer.substring(quoteStart);
                        continue;
                    }
                }
            }
            
            if (!foundDescription && result.length() == 0) {
                buffer = buffer.substring(buffer.length() - 200);
            }
        }
    }
    
    return result;
}

// ===========================================
// OCR Function
// ===========================================
String performOCR(camera_fb_t* fb) {
    if (!fb) return "Error: No image";
    
    Serial.println("\n=== OCR START ===");
    Serial.printf("Image: %u bytes\n", fb->len);
    
    if (strlen(apiKey) < 10) {
        return "Error: Add Google Cloud Vision API key";
    }
    
    size_t outLen;
    mbedtls_base64_encode(NULL, 0, &outLen, fb->buf, fb->len);
    unsigned char* b64Buffer = (unsigned char*)malloc(outLen);
    mbedtls_base64_encode(b64Buffer, outLen, &outLen, fb->buf, fb->len);
    String b64String = String((char*)b64Buffer);
    free(b64Buffer);
    b64String.trim();
    
    Serial.printf("Base64: %d chars\n", b64String.length());
    
    String json = "{\"requests\":[{\"image\":{\"content\":\"";
    json += b64String;
    json += "\"},\"features\":[{\"type\":\"DOCUMENT_TEXT_DETECTION\",\"maxResults\":1}]}]}";
    
    HTTPClient http;
    String url = "https://vision.googleapis.com/v1/images:annotate?key=";
    url += apiKey;
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000);
    
    Serial.println("Sending to Google...");
    int code = http.POST(json);
    
    String result = "";
    
    if (code == 200) {
        WiFiClient* stream = http.getStreamPtr();
        result = extractTextFromResponse(stream);
        
        if (result.length() > 0) {
            Serial.println("✓ Text extracted");
            Serial.println("===TTS_START===");
            Serial.println(result);
            Serial.println("===TTS_END===");
        } else {
            result = "No text detected";
        }
    } else {
        Serial.printf("HTTP Error: %d\n", code);
        result = "API Error: " + String(code);
    }
    
    http.end();
    Serial.println("=== OCR END ===\n");
    return result;
}

// ===========================================
// Touch-Triggered OCR
// ===========================================
void triggerOCR() {
    Serial.println("\n>>> TOUCH TRIGGERED OCR <<<");
    
    ocrInProgress = true;
    distancePaused = true;
    ttsSpeaking = true;
    ttsStartTime = millis();
    autoResumeTime = 0;
    
    strcpy(outgoingData.command, "PAUSE");
    outgoingData.distance = 0;
    outgoingData.pattern = 0;
    esp_now_send(broadcastAddress, (uint8_t*)&outgoingData, sizeof(outgoingData));
    
    Serial.println("Reading Mode - Vibration PAUSED");
    
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
        lastOcrText = performOCR(fb);
        esp_camera_fb_return(fb);
        newOcrAvailable = true;
        Serial.println("OCR complete");
        Serial.print("Text: ");
        Serial.println(lastOcrText);
        
        if (lastOcrText == "No text detected" || 
            lastOcrText.startsWith("Error") || 
            lastOcrText.startsWith("API Error") ||
            lastOcrText.length() < 3) {
            
            autoResumeTime = millis() + NO_TEXT_RESUME_DELAY;
            Serial.println("No useful text - Auto-resume in 2.5s");
        } else {
            Serial.println("Text found - Waiting for TTS...");
        }
    } else {
        lastOcrText = "Error: Camera capture failed";
        newOcrAvailable = true;
        Serial.println("✗ Camera capture failed!");
        autoResumeTime = millis() + 1000;
        Serial.println("Error - Auto-resume in 1s");
    }
    
    ocrInProgress = false;
}

// ===========================================
// Web Handlers
// ===========================================
void handleRoot() {
    server.send_P(200, "text/html", index_html);
}

void handleCapture() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        server.send(500, "text/plain", "Capture failed");
        return;
    }
    imageCount++;
    server.sendHeader("Content-Type", "image/jpeg");
    server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void handleOCR() {
    Serial.println("\n>>> MANUAL OCR REQUEST <<<");
    
    distancePaused = true;
    ttsSpeaking = true;
    ttsStartTime = millis();
    
    strcpy(outgoingData.command, "PAUSE");
    outgoingData.distance = 0;
    outgoingData.pattern = 0;
    esp_now_send(broadcastAddress, (uint8_t*)&outgoingData, sizeof(outgoingData));
    
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        server.send(500, "text/plain", "Capture failed");
        distancePaused = false;
        ttsSpeaking = false;
        return;
    }
    
    String result = performOCR(fb);
    lastOcrText = result;
    
    esp_camera_fb_return(fb);
    server.send(200, "text/plain", result);
}

void handleGetOcrText() {
    String text = lastOcrText;
    text.replace("\\", "\\\\");
    text.replace("\"", "\\\"");
    server.send(200, "text/plain", text);
}

void handleOcrStatus() {
    String escapedText = lastOcrText;
    escapedText.replace("\\", "\\\\");
    escapedText.replace("\"", "\\\"");
    escapedText.replace("\n", "\\n");
    escapedText.replace("\r", "\\r");
    escapedText.replace("\t", "\\t");
    
    String json = "{";
    json += "\"newText\":" + String(newOcrAvailable ? "true" : "false") + ",";
    json += "\"reading\":" + String(distancePaused ? "true" : "false") + ",";
    json += "\"text\":\"" + escapedText + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void handleOcrAck() {
    newOcrAvailable = false;
    server.send(200, "text/plain", "OK");
}

void handleTtsDone() {
    Serial.println("TTS Done - Resuming Navigation Mode");
    ttsSpeaking = false;
    distancePaused = false;
    newOcrAvailable = false;
    ocrInProgress = false;
    autoResumeTime = 0;
    server.send(200, "text/plain", "OK");
}

void handleDistance() {
    String json = "{";
    json += "\"distance\":" + String(smoothedDistance) + ",";
    json += "\"pattern\":" + String(currentStablePattern) + ",";
    json += "\"paused\":" + String(distancePaused ? "true" : "false") + ",";
    json += "\"status\":\"";
    
    if (distancePaused) {
        json += "READING 📖";
    } else {
        switch(currentStablePattern) {
            case 1: json += "CRITICAL"; break;
            case 2: json += "WARNING"; break;
            case 3: json += "CAUTION"; break;
            default: json += "CLEAR"; break;
        }
    }
    
    json += "\"}";
    server.send(200, "application/json", json);
}

// ===========================================
// ESP-NOW Callback
// ===========================================
void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        lastSendSuccess = millis();
        sendSuccessCount++;
    } else {
        lastSendFail = millis();
        sendFailCount++;
    }
}

// ===========================================
// WiFi Init
// ===========================================
void initWiFi() {
    Serial.printf("Connecting to: %s\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts++ < 30) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi Connected");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n✗ WiFi FAILED");
    }
}

// ===========================================
// ESP-NOW Init
// ===========================================
void initESPNow() {
    int32_t channel = WiFi.channel();
    Serial.printf("WiFi Channel: %d\n", channel);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("✗ ESP-NOW FAILED");
        return;
    }
    Serial.println("✓ ESP-NOW OK");
    
    esp_now_register_send_cb(OnDataSent);
    
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        Serial.println("✓ Peer Added");
    }
}

// ===========================================
// Setup
// ===========================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n========================================");
    Serial.println("  VISIONASSIST - EYEWEAR UNIT");
    Serial.println("  Touch + Camera + TOF + Voice");
    Serial.println("========================================\n");
    
    pinMode(TOUCH_PIN, INPUT);
    Serial.println("✓ Touch Sensor on GPIO7");
    
    if (!initCamera()) {
        Serial.println("FATAL: Camera failed!");
        while(1) delay(1000);
    }
    
    initWiFi();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("FATAL: WiFi required!");
        delay(3000);
        ESP.restart();
    }
    
    initESPNow();
    initTOF();
    
    server.on("/", handleRoot);
    server.on("/capture", handleCapture);
    server.on("/ocr", handleOCR);
    server.on("/getOcrText", handleGetOcrText);
    server.on("/ocr_status", handleOcrStatus);
    server.on("/ocr_ack", handleOcrAck);
    server.on("/tts_done", handleTtsDone);
    server.on("/distance", handleDistance);
    
    server.begin();
    
    lastGoodReading = millis();
    
    Serial.println("\n========================================");
    Serial.println("  SYSTEM READY!");
    Serial.print("  Phone browser: http://");
    Serial.println(WiFi.localIP());
    Serial.println("  Touch sensor: GPIO7");
    Serial.println("========================================\n");
}

// ===========================================
// Loop
// ===========================================
void loop() {
    unsigned long now = millis();
    
    static unsigned long lastWebHandle = 0;
    if (now - lastWebHandle >= 100) {
        server.handleClient();
        lastWebHandle = now;
    }
    
    if (!ocrInProgress && !ttsSpeaking) {
        int touchState = digitalRead(TOUCH_PIN);
        if (touchState == HIGH && (now - lastTouchTime > TOUCH_DEBOUNCE)) {
            lastTouchTime = now;
            Serial.println("\n👆 TOUCH DETECTED!");
            triggerOCR();
        }
    }
    
    if (autoResumeTime > 0 && now >= autoResumeTime) {
        Serial.println(" Auto-resume");
        ttsSpeaking = false;
        distancePaused = false;
        autoResumeTime = 0;
    }
    
    if (distancePaused && (now - ttsStartTime > TTS_TIMEOUT)) {
        Serial.println("⚠️ TTS Timeout");
        ttsSpeaking = false;
        distancePaused = false;
        autoResumeTime = 0;
    }
    
    static unsigned long lastTofRead = 0;
    int rawDistance = -1;
    
    if (sensorReady && (now - lastTofRead >= 20)) {
        if (vl53.dataReady()) {
            rawDistance = vl53.distance();
            vl53.clearInterrupt();
            lastTofRead = now;
            
            if (rawDistance > 0 && rawDistance < 4000) {
                lastGoodReading = now;
                
                static int fastReadings[3] = {5000, 5000, 5000};
                static int fastIndex = 0;
                
                fastReadings[fastIndex] = rawDistance;
                fastIndex = (fastIndex + 1) % 3;
                
                smoothedDistance = (fastReadings[0] + fastReadings[1] + fastReadings[2]) / 3;
            }
        }
    }
    
    if (now - lastGoodReading > 1000) {
        smoothedDistance = 5000;
        lastGoodReading = now;
    }
    
    int newPattern;
    if (smoothedDistance < CRITICAL_DISTANCE) {
        newPattern = 1;
    } else if (smoothedDistance < WARNING_DISTANCE) {
        newPattern = 2;
    } else if (smoothedDistance < CAUTION_DISTANCE) {
        newPattern = 3;
    } else {
        newPattern = 0;
    }
    
    bool sendNow = false;
    if (newPattern == 1 && currentStablePattern != 1) {
        currentStablePattern = 1;
        sendNow = true;
    } else if (newPattern != lastPattern) {
        lastPattern = newPattern;
        stableCount = 0;
        if (newPattern == 0) {
            currentStablePattern = 0;
            sendNow = true;
        }
    } else {
        stableCount++;
        if (stableCount >= 1 && currentStablePattern != newPattern) {
            currentStablePattern = newPattern;
            sendNow = true;
        }
    }
    
    static unsigned long lastEspNowSend = 0;
    
    if (sendNow || (now - lastEspNowSend >= 50)) {
        if (distancePaused) {
            strcpy(outgoingData.command, "PAUSE");
            outgoingData.distance = 0;
            outgoingData.pattern = 0;
        } else {
            switch(currentStablePattern) {
                case 1: strcpy(outgoingData.command, "CRITICAL"); break;
                case 2: strcpy(outgoingData.command, "WARNING"); break;
                case 3: strcpy(outgoingData.command, "CAUTION"); break;
                default: strcpy(outgoingData.command, "CLEAR"); break;
            }
            outgoingData.distance = smoothedDistance;
            outgoingData.pattern = currentStablePattern;
        }
        
        esp_now_send(broadcastAddress, (uint8_t*)&outgoingData, sizeof(outgoingData));
        lastEspNowSend = now;
    }
    
    if (now - lastPrint >= 500) {
        if (distancePaused) {
            Serial.print(" READ | ");
        }
        Serial.print("D:");
        Serial.print(smoothedDistance);
        Serial.print("mm P:");
        Serial.println(currentStablePattern);
        lastPrint = now;
    }
    
    yield();
}