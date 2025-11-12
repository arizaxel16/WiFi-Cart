/*
  ESP32 IoT Car Controller
  HTTP server for motor control + MQTT telemetry publisher
  Versión con pruebas TLS (A/B/C) para CERTIFICATES.md
*/

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//#define TLS_TEST_MODE 1  // A: sin TLS (falla en 8883)
//#define TLS_TEST_MODE 2  // B: TLS sin CA (falla verificación)


// ======== ELEGIR PRUEBA TLS ========
// 1 = PRUEBA A: puerto 8883 con cliente NO TLS (fallo esperado)
// 2 = PRUEBA B: TLS SIN CA (fallo esperado por verificación)
// 3 = PRUEBA C: TLS CON CA (debe funcionar al pegar la CA en certs.h)
#ifndef TLS_TEST_MODE
#define TLS_TEST_MODE 3
#endif

#if TLS_TEST_MODE == 2 || TLS_TEST_MODE == 3
  #include <WiFiClientSecure.h>
#endif

#if TLS_TEST_MODE == 3
  #include "certs.h"   // Contiene ROOT_CA_PEM con la CA raíz en PEM
#endif

// ========== CONFIGURATION ==========

// WiFi credentials (solo demo)
#define WIFI_SSID "iPhone de Juan Pablo"
#define WIFI_PASS "millos14"

// MQTT broker
#define MQTT_SERVER "broker.hivemq.com"
// Para toda la práctica usamos 8883 (MQTT over TLS). En la PRUEBA A mantenemos cliente NO TLS para que falle.
#define MQTT_PORT 8883
#define MQTT_TOPIC_CMDS "iot_car/commands"
#define MQTT_TOPIC_TELEM "iot_car/ultrasonic"

// Motor pins (L298N)
#define ENA_PIN 13  // PWM Right Motor
#define ENB_PIN 12  // PWM Left Motor
#define IN1_PIN 14
#define IN2_PIN 27
#define IN3_PIN 26
#define IN4_PIN 25

// Ultrasonic sensor pins (HC-SR04)
#define TRIG_PIN 35  // Trigger (OUTPUT)
#define ECHO_PIN 34  // Echo (INPUT) - Use voltage divider to 3.3V

// Sensor configuration
#define USE_ULTRASONIC_MOCK 1  // 1=simulated, 0=real sensor
#define ULTRASONIC_PERIOD_MS 1000
#define ULTRASONIC_TIMEOUT_US 30000

// ========== GLOBAL INSTANCES ==========

// Cliente TCP/TLS según modo
#if TLS_TEST_MODE == 1
  // PRUEBA A: cliente NO TLS (fallará al intentar 8883)
  WiFiClient espClient;
#elif TLS_TEST_MODE == 2
  // PRUEBA B: TLS sin CA
  WiFiClientSecure espClient;
#elif TLS_TEST_MODE == 3
  // PRUEBA C: TLS con CA
  WiFiClientSecure espClient;
#else
  #error "TLS_TEST_MODE debe ser 1, 2 o 3"
#endif

WebServer server(80);
PubSubClient client(espClient);

// Movement state control
unsigned long moveStopTime = 0;
bool isMoving = false;

// Telemetry timing
unsigned long nextTelemetryAt = 0;

// ========== MOTOR CONTROL ==========

void stopMotors() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, 0);
  analogWrite(ENB_PIN, 0);
  isMoving = false;
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
}

void moveBackward(int speed) {
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

void turnLeft(int speed) {
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

void turnRight(int speed) {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, HIGH);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

// ========== NETWORK ==========

void setup_wifi() {
  Serial.printf("[WIFI] Connecting to %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WIFI] Connected - IP: %s\n", WiFi.localIP().toString().c_str());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("[MQTT] Connecting...");
    String clientId = "ESP32Car-" + String(ESP.getEfuseMac(), HEX);

    // Si el broker requiere usuario/clave, aquí se pondrían.
    if (client.connect(clientId.c_str())) {
      Serial.println("OK");
      // Podrías suscribirte a topics aquí si fuera necesario.
    } else {
      Serial.printf("FAIL (rc=%d) - Retry in 5s\n", client.state());
      delay(5000);
    }
  }
}

void publishMqtt(const char* topic, const String& payload) {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  if (client.publish(topic, payload.c_str())) {
    Serial.printf("[MQTT] Published to %s\n", topic);
  } else {
    Serial.printf("[MQTT] Publish FAILED to %s\n", topic);
  }
}

// ========== HTTP HANDLERS ==========

void handleHealthCheck() {
  StaticJsonDocument<192> doc;
  doc["status"]   = "ok";
  doc["tls_mode"] = TLS_TEST_MODE;      // 1=A, 2=B, 3=C
#if TLS_TEST_MODE == 1
  doc["transport"] = "tcp:1883->8883 (no TLS)";
#else
  doc["transport"] = "tls:8883";
#endif
  doc["chip_id"] = String(ESP.getEfuseMac(), HEX);
  doc["ip"]      = WiFi.localIP().toString();

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleMove() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Empty body\"}");
    return;
  }

  // Parse JSON request
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  // Extract and validate parameters
  String direction = doc["direction"] | "";
  int speed = doc["speed"] | 0;
  int duration = doc["duration"] | 0;

  if (direction == "" || speed <= 0 || speed > 255 ||
      duration <= 0 || duration > 5000) {
    server.send(400, "application/json",
                "{\"error\":\"Invalid params: speed(1-255), duration(1-5000ms)\"}");
    return;
  }

  // Publish command to MQTT
  doc["client_ip"] = server.client().remoteIP().toString();
  String mqttPayload;
  serializeJson(doc, mqttPayload);
  publishMqtt(MQTT_TOPIC_CMDS, mqttPayload);

  // Execute movement
  if (direction == "forward")       moveForward(speed);
  else if (direction == "backward") moveBackward(speed);
  else if (direction == "right")    turnRight(speed);
  else if (direction == "left")     turnLeft(speed);
  else {
    server.send(400, "application/json",
                "{\"error\":\"Invalid direction: use forward/backward/left/right\"}");
    return;
  }

  isMoving = true;
  moveStopTime = millis() + (unsigned long)duration;

  server.send(200, "application/json",
              "{\"status\":\"moving\",\"details\":" + mqttPayload + "}");
}

void handleNotFound() {
  String msg = "Not Found\n\nURI: " + server.uri() +
               "\nMethod: " + ((server.method() == HTTP_GET) ? "GET" : "POST");
  server.send(404, "text/plain", msg);
}

// ========== ULTRASONIC SENSOR ==========

float readUltrasonicCm() {
#if USE_ULTRASONIC_MOCK
  // Mock: random distance with slight noise
  float base = random(1000, 25100) / 100.0f;
  float jitter = random(-10, 10) / 100.0f;
  return max(0.0f, base + jitter);
#else
  // Real HC-SR04 reading (ensure voltage divider on ECHO pin)
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT_US);
  if (duration == 0) return -1.0f; // Timeout

  return duration / 58.0f; // Convert to cm
#endif
}

void publishUltrasonic() {
  float distance = readUltrasonicCm();

  StaticJsonDocument<256> doc;

  if (distance < 0) {
    doc["distance_cm"] = (char*)0;  // null en ArduinoJson
  } else {
    doc["distance_cm"] = distance;
  }

  doc["timestamp"] = millis();
  doc["chip_id"] = String(ESP.getEfuseMac(), HEX);
  doc["ip"] = WiFi.localIP().toString();
  doc["mock"] = USE_ULTRASONIC_MOCK;

  String payload;
  serializeJson(doc, payload);
  publishMqtt(MQTT_TOPIC_TELEM, payload);
}

// ========== SETUP & LOOP ==========

void setup() {
  Serial.begin(115200);
  delay(1000);

  setupMotors();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  setup_wifi();

  // ===== Configuración TLS según PRUEBA =====
#if TLS_TEST_MODE == 1
  Serial.println("[TLS] PRUEBA A: cliente NO TLS intentando 8883 (fallo esperado).");
  // espClient = WiFiClient (ya definido arriba); no hay setInsecure ni nada.

#elif TLS_TEST_MODE == 2
  Serial.println("[TLS] PRUEBA B: WiFiClientSecure SIN CA (fallo esperado por verificación).");
  // No llamar setInsecure(); queremos que falle por falta de CA.
  // Tampoco setCACert(); el handshake debe fallar.

#elif TLS_TEST_MODE == 3
  Serial.println("[TLS] PRUEBA C: WiFiClientSecure CON CA (debe funcionar si la CA es correcta).");
  // Cargar la CA raíz apropiada (pegar en certs.h)
  espClient.setCACert(ROOT_CA_PEM);
  // Si tu core soporta bundle y lo prefieres:
  // espClient.setCACertBundle(rootCABundle);
#endif

  client.setServer(MQTT_SERVER, MQTT_PORT);

  // HTTP endpoints
  server.on("/health", HTTP_GET, handleHealthCheck);
  server.on("/move", HTTP_POST, handleMove);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.printf("[HTTP] Server started at http://%s\n",
                WiFi.localIP().toString().c_str());

  nextTelemetryAt = millis() + ULTRASONIC_PERIOD_MS;
}

void loop() {
  server.handleClient();

  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  // Auto-stop motors after duration
  if (isMoving && millis() >= moveStopTime) {
    stopMotors();
  }

  // Periodic telemetry publishing
  if ((long)(millis() - nextTelemetryAt) >= 0) {
    publishUltrasonic();
    nextTelemetryAt = millis() + ULTRASONIC_PERIOD_MS;
  }
}
