/*
  ESP32 IoT Car Controller
  HTTP server for motor control + MQTT telemetry publisher
  With TLS support and real ultrasonic sensor

  For Arduino IDE - ESP32 Board
*/

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "certs.h"

// ========== GLOBAL INSTANCES ==========
WiFiClientSecure espClient;
WebServer server(80);
PubSubClient mqttClient(espClient);

// Movement state control
unsigned long moveStopTime = 0;
bool isMoving = false;
String lastDirection = "stopped";
int lastSpeed = 0;

// Telemetry timing
unsigned long nextTelemetryAt = 0;

// Distance reading
float lastDistanceCm = -1.0f;

// ========== MOTOR CONTROL ==========

void stopMotors() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, 0);
  analogWrite(ENB_PIN, 0);
  isMoving = false;
  lastDirection = "stopped";
  lastSpeed = 0;
}

void setupMotors() {
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
  stopMotors();
  Serial.println("[MOTOR] Initialized");
}

void moveForward(int speed) {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, HIGH);
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
  lastDirection = "forward";
  lastSpeed = speed;
}

void moveBackward(int speed) {
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
  lastDirection = "backward";
  lastSpeed = speed;
}

void turnLeft(int speed) {
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
  lastDirection = "left";
  lastSpeed = speed;
}

void turnRight(int speed) {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, HIGH);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
  lastDirection = "right";
  lastSpeed = speed;
}

// ========== ULTRASONIC SENSOR ==========

float readUltrasonicCm() {
#if USE_ULTRASONIC_MOCK == 1
  // Mock: random distance with slight noise (10-250 cm range)
  float base = random(1000, 25000) / 100.0f;
  float jitter = random(-50, 50) / 100.0f;
  return max(2.0f, base + jitter);
#else
  // Real HC-SR04 reading
  // Clear trigger
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Send 10µs pulse
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo pulse duration
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT_US);

  if (duration == 0) {
    Serial.println("[ULTRASONIC] Timeout - no echo received");
    return -1.0f; // Timeout or out of range
  }

  // Convert to cm: speed of sound = 343m/s = 0.0343 cm/µs
  // Distance = (duration * 0.0343) / 2 = duration / 58.0
  float distance = duration / 58.0f;

  // HC-SR04 range is 2cm to 400cm
  if (distance < 2.0f || distance > 400.0f) {
    Serial.printf("[ULTRASONIC] Out of range: %.2f cm\n", distance);
    return -1.0f;
  }

  return distance;
#endif
}

void setupUltrasonic() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  Serial.println("[ULTRASONIC] Initialized");
#if USE_ULTRASONIC_MOCK == 1
  Serial.println("[ULTRASONIC] Running in MOCK mode");
#else
  Serial.println("[ULTRASONIC] Running with REAL sensor");
#endif
}

// ========== OBSTACLE DETECTION ==========

bool checkObstacle() {
  if (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_THRESHOLD_CM) {
    return true;
  }
  return false;
}

// ========== NETWORK ==========

void setupWifi() {
  Serial.printf("[WIFI] Connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] Connected - IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WIFI] Connection FAILED - restarting...");
    ESP.restart();
  }
}

void reconnectMqtt() {
  int retries = 0;
  while (!mqttClient.connected() && retries < 3) {
    Serial.print("[MQTT] Connecting to broker...");
    String clientId = "ESP32Car-" + String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("OK");
      // Publish online status
      String statusPayload = "{\"status\":\"online\",\"chip_id\":\"" +
                             String(ESP.getEfuseMac(), HEX) + "\"}";
      mqttClient.publish(MQTT_TOPIC_STATUS, statusPayload.c_str(), true);
    } else {
      Serial.printf("FAIL (rc=%d) - Retry %d/3\n", mqttClient.state(), retries + 1);
      retries++;
      delay(2000);
    }
  }
}

void publishMqtt(const char* topic, const String& payload) {
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }
  if (mqttClient.connected()) {
    if (mqttClient.publish(topic, payload.c_str())) {
      Serial.printf("[MQTT] Published to %s: %s\n", topic, payload.c_str());
    } else {
      Serial.printf("[MQTT] Publish FAILED to %s\n", topic);
    }
  }
}

void publishTelemetry() {
  lastDistanceCm = readUltrasonicCm();

  StaticJsonDocument<384> doc;

  if (lastDistanceCm < 0) {
    doc["distance_cm"] = nullptr;
    doc["obstacle_detected"] = false;
  } else {
    doc["distance_cm"] = lastDistanceCm;
    doc["obstacle_detected"] = (lastDistanceCm < OBSTACLE_THRESHOLD_CM);
  }

  doc["timestamp"] = millis();
  doc["chip_id"] = String(ESP.getEfuseMac(), HEX);
  doc["ip"] = WiFi.localIP().toString();
  doc["mock_sensor"] = USE_ULTRASONIC_MOCK;
  doc["motor_state"] = lastDirection;
  doc["motor_speed"] = lastSpeed;
  doc["wifi_rssi"] = WiFi.RSSI();

  String payload;
  serializeJson(doc, payload);
  publishMqtt(MQTT_TOPIC_TELEM, payload);
}

// ========== HTTP HANDLERS ==========

void handleHealthCheck() {
  StaticJsonDocument<512> doc;
  doc["status"] = "ok";
  doc["version"] = FIRMWARE_VERSION;
  doc["chip_id"] = String(ESP.getEfuseMac(), HEX);
  doc["ip"] = WiFi.localIP().toString();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime_ms"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["mqtt_connected"] = mqttClient.connected();
  doc["mock_sensor"] = USE_ULTRASONIC_MOCK;
  doc["last_distance_cm"] = lastDistanceCm;
  doc["motor_state"] = lastDirection;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleMove() {
  // Allow both GET (with query params) and POST (with JSON body)
  String direction = "";
  int speed = 0;
  int duration = 0;

  if (server.method() == HTTP_POST) {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"Empty body\"}");
      return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    direction = doc["direction"] | "";
    speed = doc["speed"] | 0;
    duration = doc["duration"] | 0;
  } else if (server.method() == HTTP_GET) {
    direction = server.arg("direction");
    speed = server.arg("speed").toInt();
    duration = server.arg("duration").toInt();
  }

  // Validate parameters
  if (direction == "" || speed <= 0 || speed > 255 ||
      duration <= 0 || duration > 10000) {
    server.send(400, "application/json",
                "{\"error\":\"Invalid params: direction(forward/backward/left/right/stop), speed(1-255), duration(1-10000ms)\"}");
    return;
  }

  // Check for obstacles before moving forward
  if (direction == "forward" && checkObstacle()) {
    StaticJsonDocument<256> errorDoc;
    errorDoc["error"] = "Obstacle detected";
    errorDoc["distance_cm"] = lastDistanceCm;
    errorDoc["threshold_cm"] = OBSTACLE_THRESHOLD_CM;
    String errorResponse;
    serializeJson(errorDoc, errorResponse);
    server.send(409, "application/json", errorResponse);
    return;
  }

  // Build MQTT payload
  StaticJsonDocument<384> mqttDoc;
  mqttDoc["direction"] = direction;
  mqttDoc["speed"] = speed;
  mqttDoc["duration"] = duration;
  mqttDoc["client_ip"] = server.client().remoteIP().toString();
  mqttDoc["timestamp"] = millis();

  String mqttPayload;
  serializeJson(mqttDoc, mqttPayload);
  publishMqtt(MQTT_TOPIC_CMDS, mqttPayload);

  // Execute movement
  if (direction == "forward")       moveForward(speed);
  else if (direction == "backward") moveBackward(speed);
  else if (direction == "right")    turnRight(speed);
  else if (direction == "left")     turnLeft(speed);
  else if (direction == "stop") {
    stopMotors();
    server.send(200, "application/json", "{\"status\":\"stopped\"}");
    return;
  }
  else {
    server.send(400, "application/json",
                "{\"error\":\"Invalid direction: use forward/backward/left/right/stop\"}");
    return;
  }

  isMoving = true;
  moveStopTime = millis() + (unsigned long)duration;

  // Response
  StaticJsonDocument<384> responseDoc;
  responseDoc["status"] = "moving";
  responseDoc["direction"] = direction;
  responseDoc["speed"] = speed;
  responseDoc["duration_ms"] = duration;
  responseDoc["will_stop_at"] = moveStopTime;

  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleGetStatus() {
  StaticJsonDocument<256> doc;
  doc["motor_state"] = lastDirection;
  doc["motor_speed"] = lastSpeed;
  doc["is_moving"] = isMoving;
  doc["last_distance_cm"] = lastDistanceCm;
  doc["obstacle_detected"] = checkObstacle();

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleNotFound() {
  String msg = "{\"error\":\"Not Found\",\"uri\":\"" + server.uri() + "\"}";
  server.send(404, "application/json", msg);
}

void handleCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

// ========== SETUP & LOOP ==========

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.printf("ESP32 IoT Car Controller v%s\n", FIRMWARE_VERSION);
  Serial.println("========================================");

  setupMotors();
  setupUltrasonic();
  setupWifi();

  // Configure TLS
  Serial.println("[TLS] Configuring secure connection...");
  espClient.setCACert(ROOT_CA_PEM);

  // MQTT setup
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(512);

  // HTTP server with CORS support
  server.on("/api/v1/healthcheck", HTTP_GET, handleHealthCheck);
  server.on("/api/v1/move", HTTP_POST, handleMove);
  server.on("/api/v1/move", HTTP_GET, handleMove);
  server.on("/api/v1/status", HTTP_GET, handleGetStatus);
  server.on("/api/v1/move", HTTP_OPTIONS, handleCORS);
  server.onNotFound(handleNotFound);

  // Enable CORS globally
  server.enableCORS(true);

  server.begin();
  Serial.printf("[HTTP] Server started at http://%s\n", WiFi.localIP().toString().c_str());
  Serial.println("[HTTP] Endpoints:");
  Serial.println("  GET  /api/v1/healthcheck");
  Serial.println("  POST /api/v1/move");
  Serial.println("  GET  /api/v1/move?direction=...&speed=...&duration=...");
  Serial.println("  GET  /api/v1/status");

  nextTelemetryAt = millis() + ULTRASONIC_PERIOD_MS;
  Serial.println("\n[READY] System initialized successfully!");
}

void loop() {
  server.handleClient();

  if (!mqttClient.connected()) {
    reconnectMqtt();
  }
  mqttClient.loop();

  // Auto-stop motors after duration
  if (isMoving && millis() >= moveStopTime) {
    stopMotors();
    Serial.println("[MOTOR] Auto-stopped after duration");
  }

  // Periodic telemetry publishing
  if ((long)(millis() - nextTelemetryAt) >= 0) {
    publishTelemetry();
    nextTelemetryAt = millis() + ULTRASONIC_PERIOD_MS;
  }

  // Small delay to prevent watchdog issues
  delay(10);
}
