# ESP32 IoT Car Controller

[![CI/CD Build](https://github.com/YOUR_USERNAME/esp32-iot-car/actions/workflows/build.yml/badge.svg)](https://github.com/YOUR_USERNAME/esp32-iot-car/actions/workflows/build.yml)

A WiFi-controlled 2WD robot car with ultrasonic obstacle detection, REST API control, and encrypted MQTT telemetry using ESP32.

---

## 📋 Table of Contents

1. [Features](#features)
2. [Hardware Requirements](#hardware-requirements)
3. [Wiring Diagram](#wiring-diagram)
4. [Software Setup](#software-setup)
5. [API Documentation (REST Endpoints)](#api-documentation)
6. [MQTT Topics](#mqtt-topics)
7. [Postman Collection](#postman-collection)
8. [Sequence Diagrams](#sequence-diagrams)
9. [Memory Usage](#memory-usage)
10. [Libraries Used](#libraries-used)
11. [Limitations](#limitations)
12. [Future Improvements](#future-improvements)
13. [CI/CD Pipeline](#cicd-pipeline)

---

## 🚗 Features

- **HTTP REST API** for remote motor control
- **MQTT over TLS** (port 8883) for secure telemetry
- **HC-SR04 Ultrasonic Sensor** for obstacle detection
- **Automatic collision prevention** - blocks forward movement when obstacle detected
- **Real-time telemetry** - publishes sensor data every second
- **Configurable via preprocessor** - easy compile-time configuration
- **CI/CD Pipeline** (Expert Feature) - automated builds with GitHub Actions

---

## 🔧 Hardware Requirements

| Component | Quantity | Description |
|-----------|----------|-------------|
| ESP32 DevKit V1 | 1 | Main microcontroller (30-pin) |
| L298N Motor Driver | 1 | Dual H-Bridge motor controller |
| HC-SR04 Ultrasonic Sensor | 1 | Distance measurement (2-400cm) |
| DC Gearbox Motors | 2 | 3-6V DC motors with gear reduction |
| 2WD Robot Chassis | 1 | Base platform with wheels and caster |
| Battery Pack | 1 | 7.4V-12V LiPo or 6xAA NiMH |
| 10kΩ Resistors | 2 | For voltage divider (ECHO pin) |
| Jumper Wires | ~25 | Male-to-Female and Male-to-Male |
| Mini Breadboard | 1 | Optional, for voltage divider |
| USB Micro Cable | 1 | For programming ESP32 |

**Total Estimated Cost:** $25-40 USD (excluding battery)

---

## 🔌 Wiring Diagram

### Complete Pin Connections

```
                    ┌─────────────┐
                    │   ESP32     │
                    │   DevKit    │
                    └─────────────┘
                          │
    ┌─────────────────────┼─────────────────────┐
    │                     │                     │
    ▼                     ▼                     ▼
┌────────┐         ┌─────────────┐        ┌──────────┐
│ HC-SR04│         │    L298N    │        │  Motors  │
│Ultrasonic│        │Motor Driver │        │  (2x)    │
└────────┘         └─────────────┘        └──────────┘
```

### ESP32 to L298N Motor Driver

| ESP32 GPIO | L298N Pin | Wire Color (Suggested) | Function |
|------------|-----------|------------------------|----------|
| GPIO 14 | IN1 | Green | Motor A Direction 1 |
| GPIO 27 | IN2 | Green | Motor A Direction 2 |
| GPIO 26 | IN3 | Green | Motor B Direction 1 |
| GPIO 25 | IN4 | Green | Motor B Direction 2 |
| GPIO 13 | ENA | Blue | Motor A Speed (PWM) |
| GPIO 12 | ENB | Blue | Motor B Speed (PWM) |
| GND | GND | Black | Common Ground |

**⚠️ Remove the ENA and ENB jumpers on the L298N board to enable PWM speed control!**

### ESP32 to HC-SR04 Ultrasonic Sensor

| ESP32 Pin | HC-SR04 Pin | Notes |
|-----------|-------------|-------|
| 3.3V (or 5V) | VCC | Power supply (5V preferred) |
| GPIO 32 | TRIG | Trigger signal (OUTPUT) |
| GPIO 33 | ECHO | **Through voltage divider!** |
| GND | GND | Common ground |

### ⚠️ CRITICAL: Voltage Divider for ECHO Pin

**The HC-SR04 ECHO pin outputs 5V, but ESP32 GPIO pins are 3.3V tolerant!**

You MUST use a voltage divider to protect the ESP32:

```
HC-SR04 ECHO ──────┬────── ESP32 GPIO 33
                   │
                  [10kΩ]
                   │
                  GND
                   │
                  [10kΩ]
                   │
HC-SR04 GND ───────┘
```

**Calculation:** 5V × (10kΩ / (10kΩ + 10kΩ)) = 2.5V (safe for ESP32)

Alternative: Use a logic level shifter module.

### L298N Power Connections

| L298N Terminal | Connection | Description |
|----------------|------------|-------------|
| +12V | Battery + | 7.4V to 12V input |
| GND | Battery - & ESP32 GND | **Common ground is essential!** |
| +5V | Optional: ESP32 VIN | Regulated 5V output (only if 12V jumper is ON) |

### Motor Connections

| L298N Output | Motor |
|--------------|-------|
| OUT1, OUT2 | Motor A (Right wheel) |
| OUT3, OUT4 | Motor B (Left wheel) |

### GPIO Pin Selection Notes

**Why GPIO 32 for TRIG instead of GPIO 35?**
- GPIO 34, 35, 36, 39 are **INPUT-ONLY** on ESP32
- TRIG requires OUTPUT capability for sending pulses
- GPIO 32 supports both INPUT and OUTPUT

---

## 💻 Software Setup

### Prerequisites

1. **Arduino IDE 2.x** (or 1.8.x)
2. **ESP32 Board Package** installed in Arduino IDE
3. **Required Libraries** (install via Library Manager)

### Step 1: Install Arduino IDE

Download from: https://www.arduino.cc/en/software

### Step 2: Add ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. In "Additional Board Manager URLs", add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to **Tools → Board → Boards Manager**
5. Search for "esp32" and install **"ESP32 by Espressif Systems"**

### Step 3: Install Required Libraries

Go to **Sketch → Include Library → Manage Libraries** and install:

- **PubSubClient** by Nick O'Leary (version 2.8+)
- **ArduinoJson** by Benoit Blanchon (version 6.x)

### Step 4: Configure Your Settings

Edit `config.h` and change:

```cpp
#define WIFI_SSID "YOUR_WIFI_NETWORK_NAME"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
```

Optional: Set `USE_ULTRASONIC_MOCK` to `1` for testing without the physical sensor.

### Step 5: Upload to ESP32

1. Connect ESP32 via USB
2. Select **Tools → Board → ESP32 Dev Module**
3. Select the correct **Port** (e.g., COM3 on Windows, /dev/ttyUSB0 on Linux)
4. Click **Upload** (→ button)
5. Open **Serial Monitor** (Tools → Serial Monitor) at **115200 baud**

### Step 6: Verify Connection

In Serial Monitor, you should see:
```
========================================
ESP32 IoT Car Controller v2.0.0
========================================
[MOTOR] Initialized
[ULTRASONIC] Initialized
[ULTRASONIC] Running with REAL sensor
[WIFI] Connecting to YOUR_SSID
........
[WIFI] Connected - IP: 192.168.1.XXX
[TLS] Configuring secure connection...
[HTTP] Server started at http://192.168.1.XXX
[READY] System initialized successfully!
```

**Note the IP address - you'll need it for API calls!**

---

## 📡 API Documentation

**Base URL:** `http://<ESP32_IP_ADDRESS>`

Example: `http://192.168.1.100`

### Endpoint 1: Health Check

**GET** `/api/v1/healthcheck`

Returns system status and diagnostics.

**Request:**
```http
GET /api/v1/healthcheck HTTP/1.1
Host: 192.168.1.100
```

**Response (200 OK):**
```json
{
  "status": "ok",
  "version": "2.0.0",
  "chip_id": "a4cf12f3d8e0",
  "ip": "192.168.1.100",
  "wifi_rssi": -45,
  "uptime_ms": 125000,
  "free_heap": 245680,
  "mqtt_connected": true,
  "mock_sensor": false,
  "last_distance_cm": 35.2,
  "motor_state": "stopped"
}
```

**Field Descriptions:**
- `wifi_rssi`: Signal strength in dBm (closer to 0 = better)
- `free_heap`: Available RAM in bytes
- `mock_sensor`: `true` if using simulated sensor
- `motor_state`: Current movement state

---

### Endpoint 2: Move Car (POST)

**POST** `/api/v1/move`

Commands the car to move in a specified direction.

**Request:**
```http
POST /api/v1/move HTTP/1.1
Host: 192.168.1.100
Content-Type: application/json

{
  "direction": "forward",
  "speed": 200,
  "duration": 2000
}
```

**Parameters:**

| Field | Type | Required | Range | Description |
|-------|------|----------|-------|-------------|
| `direction` | string | Yes | `forward`, `backward`, `left`, `right`, `stop` | Movement direction |
| `speed` | integer | Yes | 1-255 | Motor speed (PWM duty cycle) |
| `duration` | integer | Yes | 1-10000 | Duration in milliseconds |

**Response (200 OK) - Success:**
```json
{
  "status": "moving",
  "direction": "forward",
  "speed": 200,
  "duration_ms": 2000,
  "will_stop_at": 127000
}
```

**Response (409 Conflict) - Obstacle Detected:**
```json
{
  "error": "Obstacle detected",
  "distance_cm": 15.3,
  "threshold_cm": 20.0
}
```

This occurs when trying to move **forward** and an obstacle is within threshold distance (default 20cm).

**Response (400 Bad Request):**
```json
{
  "error": "Invalid params: direction(forward/backward/left/right/stop), speed(1-255), duration(1-10000ms)"
}
```

---

### Endpoint 3: Move Car (GET)

**GET** `/api/v1/move`

Alternative method using query parameters (convenient for browser testing).

**Request:**
```http
GET /api/v1/move?direction=forward&speed=150&duration=1000 HTTP/1.1
Host: 192.168.1.100
```

**Response:** Same as POST method.

---

### Endpoint 4: Get Status

**GET** `/api/v1/status`

Returns current motor and sensor status.

**Request:**
```http
GET /api/v1/status HTTP/1.1
Host: 192.168.1.100
```

**Response (200 OK):**
```json
{
  "motor_state": "forward",
  "motor_speed": 200,
  "is_moving": true,
  "last_distance_cm": 45.8,
  "obstacle_detected": false
}
```

---

## 📨 MQTT Topics

All MQTT communication uses **TLS encryption** on port **8883**.

**Broker:** `broker.hivemq.com` (free public broker)

### Published Topics (ESP32 → Broker)

| Topic | QoS | Retained | Frequency | Description |
|-------|-----|----------|-----------|-------------|
| `iot_car/telemetry` | 0 | No | Every 1 second | Sensor readings and status |
| `iot_car/commands` | 0 | No | On each move command | Movement command log |
| `iot_car/status` | 0 | Yes | On connect | Online/offline status |

### Telemetry Payload (`iot_car/telemetry`)

Published every second:

```json
{
  "distance_cm": 45.8,
  "obstacle_detected": false,
  "timestamp": 125000,
  "chip_id": "a4cf12f3d8e0",
  "ip": "192.168.1.100",
  "mock_sensor": false,
  "motor_state": "stopped",
  "motor_speed": 0,
  "wifi_rssi": -45
}
```

### Command Log Payload (`iot_car/commands`)

Published for each movement:

```json
{
  "direction": "forward",
  "speed": 200,
  "duration": 2000,
  "client_ip": "192.168.1.50",
  "timestamp": 124500
}
```

### Status Payload (`iot_car/status`)

Published on connection (retained):

```json
{
  "status": "online",
  "chip_id": "a4cf12f3d8e0"
}
```

### How to Subscribe to MQTT Topics

**Using MQTT Explorer (GUI):**
1. Download: https://mqtt-explorer.com/
2. Connect to `broker.hivemq.com:8883` with TLS
3. Subscribe to `iot_car/#`

**Using mosquitto_sub (CLI):**
```bash
mosquitto_sub -h broker.hivemq.com -p 8883 \
  --capath /etc/ssl/certs/ \
  -t "iot_car/#" -v
```

**Using HiveMQ WebSocket Client:**
- URL: http://www.hivemq.com/demos/websocket-client/
- Subscribe to `iot_car/telemetry`

---

## 📮 Postman Collection

### Setting Up Postman

1. **Download Postman:** https://www.postman.com/downloads/
2. **Create New Collection:** Click "New" → "Collection"
3. **Name:** `ESP32 IoT Car API`
4. **Add Variable:**
   - Variable: `base_url`
   - Initial Value: `http://192.168.1.100` (your ESP32 IP)

### Requests to Add

#### Request 1: Health Check

- **Name:** `Health Check`
- **Method:** `GET`
- **URL:** `{{base_url}}/api/v1/healthcheck`

#### Request 2: Move Forward

- **Name:** `Move Forward`
- **Method:** `POST`
- **URL:** `{{base_url}}/api/v1/move`
- **Headers:** `Content-Type: application/json`
- **Body (raw JSON):**
```json
{
  "direction": "forward",
  "speed": 200,
  "duration": 2000
}
```

#### Request 3: Move Backward

- **Name:** `Move Backward`
- **Method:** `POST`
- **URL:** `{{base_url}}/api/v1/move`
- **Body:**
```json
{
  "direction": "backward",
  "speed": 150,
  "duration": 1500
}
```

#### Request 4: Turn Left

- **Name:** `Turn Left`
- **Method:** `POST`
- **URL:** `{{base_url}}/api/v1/move`
- **Body:**
```json
{
  "direction": "left",
  "speed": 180,
  "duration": 1000
}
```

#### Request 5: Turn Right

- **Name:** `Turn Right`
- **Method:** `POST`
- **URL:** `{{base_url}}/api/v1/move`
- **Body:**
```json
{
  "direction": "right",
  "speed": 180,
  "duration": 1000
}
```

#### Request 6: Stop Motors

- **Name:** `Emergency Stop`
- **Method:** `POST`
- **URL:** `{{base_url}}/api/v1/move`
- **Body:**
```json
{
  "direction": "stop",
  "speed": 1,
  "duration": 1
}
```

#### Request 7: Get Status

- **Name:** `Get Status`
- **Method:** `GET`
- **URL:** `{{base_url}}/api/v1/status`

#### Request 8: Quick Move (GET)

- **Name:** `Quick Move via GET`
- **Method:** `GET`
- **URL:** `{{base_url}}/api/v1/move?direction=forward&speed=100&duration=500`

### Test Scripts (Optional)

Add to Health Check request **Tests** tab:

```javascript
pm.test("Status is 200", () => {
    pm.response.to.have.status(200);
});

pm.test("Status is ok", () => {
    var data = pm.response.json();
    pm.expect(data.status).to.eql("ok");
});

pm.test("MQTT connected", () => {
    var data = pm.response.json();
    pm.expect(data.mqtt_connected).to.be.true;
});
```

### cURL Examples

```bash
# Health Check
curl -X GET "http://192.168.1.100/api/v1/healthcheck"

# Move Forward
curl -X POST "http://192.168.1.100/api/v1/move" \
  -H "Content-Type: application/json" \
  -d '{"direction":"forward","speed":200,"duration":2000}'

# Quick test via GET (paste in browser)
http://192.168.1.100/api/v1/move?direction=forward&speed=100&duration=500
```

---

## 📊 Sequence Diagrams

### Movement Command Flow

```
User                WebGUI/Postman        ESP32              MQTT Broker
 │                        │                  │                    │
 │──── Click Forward ────>│                  │                    │
 │                        │                  │                    │
 │                        │── POST /move ───>│                    │
 │                        │                  │                    │
 │                        │                  │── Read Ultrasonic  │
 │                        │                  │   (Check obstacle) │
 │                        │                  │                    │
 │                        │                  │── Publish Command ─────>│
 │                        │                  │   (iot_car/commands)    │
 │                        │                  │                    │
 │                        │                  │── Activate Motors  │
 │                        │                  │   (moveForward)    │
 │                        │                  │                    │
 │                        │<── 200 OK ───────│                    │
 │                        │   {moving...}    │                    │
 │                        │                  │                    │
 │<─── Show Status ───────│                  │                    │
 │                        │                  │                    │
 │                        │                  │── [After duration] │
 │                        │                  │   stopMotors()     │
 │                        │                  │                    │
 └────────────────────────┴──────────────────┴────────────────────┘

                    TELEMETRY LOOP (Every 1 second)
 │                        │                  │                    │
 │                        │                  │── Read Distance ───│
 │                        │                  │                    │
 │                        │                  │── Publish ─────────────>│
 │                        │                  │   (iot_car/telemetry)   │
 │                        │                  │                    │
 │                        │<─────────────────────── Subscribe ────│
 │                        │                  │                    │
 │<─── Update Display ────│                  │                    │
```

### Obstacle Detection Flow

```
User                    ESP32                  Response
 │                        │                       │
 │── POST /move ─────────>│                       │
 │   {forward, 200, 2000} │                       │
 │                        │                       │
 │                        │── Check Distance      │
 │                        │   lastDistanceCm=15cm │
 │                        │                       │
 │                        │── 15cm < 20cm         │
 │                        │   OBSTACLE DETECTED!  │
 │                        │                       │
 │<────────────────────── 409 CONFLICT ───────────│
 │   {                    │                       │
 │     "error": "Obstacle detected",              │
 │     "distance_cm": 15.3,                       │
 │     "threshold_cm": 20.0                       │
 │   }                    │                       │
 │                        │                       │
 │   [Motors NOT activated - movement blocked]    │
```

---

## 💾 Memory Usage

Compiled with Arduino IDE for ESP32 Dev Module:

```
Sketch uses 897024 bytes (68%) of program storage space. Maximum is 1310720 bytes.
Global variables use 59648 bytes (18%) of dynamic memory, leaving 268032 bytes for local variables. Maximum is 327680 bytes.
```

### Summary

| Resource | Used | Available | Percentage |
|----------|------|-----------|------------|
| **Flash (Program)** | ~897 KB | 1.25 MB | 68% |
| **RAM (Static)** | ~60 KB | 320 KB | 18% |
| **Free Heap (Runtime)** | ~240-260 KB | - | Monitored via API |

### Notes

- WiFi stack uses significant RAM (~40KB)
- TLS/SSL adds to Flash usage (~100KB)
- ArduinoJson documents are allocated on stack
- Heap usage varies with MQTT connection state
- Monitor via `/api/v1/healthcheck` → `free_heap` field

---

## 📚 Libraries Used

| Library | Version | Author | Purpose |
|---------|---------|--------|---------|
| WiFi | Built-in | Espressif | WiFi connectivity |
| WiFiClientSecure | Built-in | Espressif | TLS/SSL encryption |
| WebServer | Built-in | Espressif | HTTP server |
| PubSubClient | 2.8+ | Nick O'Leary | MQTT client |
| ArduinoJson | 6.21+ | Benoit Blanchon | JSON parsing/serialization |

### Installing Libraries

Arduino IDE → Sketch → Include Library → Manage Libraries:
- Search "PubSubClient" → Install
- Search "ArduinoJson" → Install (version 6.x)

---

## ⚠️ Limitations

### Hardware Limitations

1. **Ultrasonic Blind Spot**: HC-SR04 minimum range is 2cm; very close objects may not be detected
2. **Single Sensor Direction**: Only detects obstacles in front, not sides or rear
3. **No Speed Feedback**: Without encoders, actual speed/distance traveled is unknown
4. **Power Fluctuations**: Motor battery voltage drop affects speed consistency
5. **WiFi Range**: ESP32 antenna limits range to ~30-50m in open space
6. **5V Logic Issue**: HC-SR04 requires voltage divider (extra components)

### Software Limitations

1. **No Web GUI**: REST API only; no visual control interface (see Future Improvements)
2. **No Authentication**: API is open; anyone on network can control car
3. **No Persistent Storage**: Settings reset on power loss (no EEPROM/SPIFFS)
4. **Fixed Telemetry Rate**: 1 second interval hardcoded
5. **Single MQTT Connection**: Cannot connect to multiple brokers
6. **No OTA Updates**: Must use USB cable for firmware updates
7. **No Offline Mode**: Requires constant WiFi connection

### Network Limitations

1. **Public MQTT Broker**: HiveMQ is shared; data is not private
2. **QoS 0 Only**: No guaranteed message delivery
3. **No Reconnection Queue**: Messages lost during WiFi drops
4. **No mDNS**: Must know IP address (no `esp32car.local`)

### Operational Limitations

1. **Max Duration**: 10 seconds per command (safety limit)
2. **No Path Planning**: Point-to-point commands only
3. **No Sensor Fusion**: Single sensor for obstacle detection
4. **No Battery Monitoring**: Unknown battery level

---

## 🚀 Future Improvements

### Short-Term (1-2 weeks)

- [ ] **Web GUI** - HTML/JavaScript interface for visual control
- [ ] **mDNS Support** - Access via `esp32car.local`
- [ ] **Battery Voltage Monitor** - ADC reading of battery level
- [ ] **Multiple Ultrasonic Sensors** - Left, front, right coverage
- [ ] **REST API Authentication** - Basic auth or API keys
- [ ] **OTA Firmware Updates** - Update without USB cable
- [ ] **CORS Improvement** - Configurable allowed origins

### Medium-Term (1-2 months)

- [ ] **Wheel Encoders** - Measure actual distance traveled
- [ ] **PID Control** - Precise speed regulation
- [ ] **WebSocket Support** - Real-time bidirectional communication
- [ ] **Mobile App** - React Native or Flutter application
- [ ] **Path Recording** - Record and replay sequences
- [ ] **Configuration Portal** - WiFi setup without hardcoding
- [ ] **Private MQTT** - Self-hosted broker (Mosquitto)

### Long-Term (Expert Level)

- [ ] **SLAM Algorithm** - Build map of environment
- [ ] **ESP32-CAM Integration** - Live video stream
- [ ] **AI Object Detection** - YOLO/TensorFlow Lite
- [ ] **Voice Control** - Google Assistant/Alexa integration
- [ ] **Digital Twin** - 3D simulation of robot
- [ ] **Swarm Robotics** - Multiple coordinated cars
- [ ] **Solar Charging** - Renewable power source

---

## 🔄 CI/CD Pipeline (Expert Feature)

This project includes a GitHub Actions workflow for continuous integration.

### Features

- ✅ Automatic compilation on every push
- ✅ Syntax and error checking
- ✅ Memory usage reporting
- ✅ Build artifact generation (firmware.bin)
- ✅ Documentation validation
- ✅ Automatic releases on main branch

### How It Works

1. Push code to GitHub
2. GitHub Actions triggers automatically
3. Installs Arduino CLI and ESP32 core
4. Compiles the sketch
5. Reports memory usage
6. Uploads compiled binary as artifact
7. Creates release if version changed

### Files Required

- `.github/workflows/build.yml` - CI/CD workflow definition
- `esp32_iot_car/` - Arduino sketch folder

See the separate `build.yml` file for the complete workflow.

---

## 📁 Project Structure

```
esp32-iot-car/
├── .github/
│   └── workflows/
│       └── build.yml           # CI/CD pipeline (Expert Feature)
├── esp32_iot_car/
│   ├── esp32_iot_car.ino      # Main Arduino sketch
│   ├── config.h               # Configuration (WiFi, pins, etc.)
│   └── certs.h                # TLS certificate for MQTT
├── README.md                   # This documentation file
└── LICENSE                     # MIT License (optional)
```

---

## 🎓 Academic Requirements Checklist

### BUENO Level ✅

- [x] Ultrasonic sensor for obstacle detection
- [x] Sensor data published to MQTT topic
- [x] **Encrypted MQTT communication (TLS on port 8883)**
- [ ] Web GUI for visualization and control *(not included - see Future Improvements)*
- [x] REST API with required endpoints (`/api/v1/healthcheck`, `/api/v1/move`)
- [x] Postman collection documentation
- [x] Preprocessor variables for configuration
- [x] Sequence/flow diagrams
- [x] API documentation (endpoints, payloads, responses)
- [x] MQTT topics documentation (publish/subscribe)
- [x] Limitations documented
- [x] Possible improvements documented
- [x] Memory usage (Flash and RAM)
- [x] Libraries list

### EXPERTO Level ✅

- [x] **CI/CD Pipeline with GitHub Actions**
  - Automated builds on push/PR
  - Code quality checks
  - Memory usage reporting
  - Build artifact generation
  - Automatic releases

---

## 📄 License

MIT License - Feel free to use and modify.

---

## 📞 Support

If you encounter issues:

1. Check Serial Monitor output for errors
2. Verify wiring connections (especially voltage divider!)
3. Ensure WiFi credentials are correct in `config.h`
4. Test with mock sensor first (`USE_ULTRASONIC_MOCK = 1`)
5. Verify MQTT connection in `/api/v1/healthcheck`

---

**Built for IoT Systems Course - 2025**
