/*
  ESP32 IoT Car Controller
  - Servidor HTTP para controlar movimiento (L298N + PWM).
  - Cliente MQTT para publicar instrucciones y telemetría por tópicos separados.
  - Mock/lectura real de HC-SR04 publicada periódicamente.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include "config.h"   // <---- NUEVO

WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// Estado movimiento
unsigned long moveStopTime = 0;
bool isMoving = false;

// Telemetría ultrasónica
unsigned long nextTelemetryAt = 0;

// ---------- Motores ----------
void setupMotors() {
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
  Serial.println("Motores configurados.");
}

void stopMotors() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, 0);
  analogWrite(ENB_PIN, 0);
  isMoving = false;
  Serial.println("Motores detenidos.");
}

void moveForward(int speed) {
  Serial.printf("Moviendo hacia adelante a velocidad %d\n", speed);
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, HIGH);
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

void moveBackward(int speed) {
  Serial.printf("Moviendo hacia atrás a velocidad %d\n", speed);
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

void turnLeft(int speed) {
  Serial.printf("Girando a la izquierda a velocidad %d\n", speed);
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

void turnRight(int speed) {
  Serial.printf("Girando a la derecha a velocidad %d\n", speed);
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, HIGH);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

// ---------- WiFi & MQTT ----------
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi conectado. IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("conectado");
    } else {
      Serial.print("falló, rc=");
      Serial.print(client.state());
      Serial.println(" reintento en 5s");
      delay(5000);
    }
  }
}

void publishMqtt(const char* topic, const String& payload) {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.publish(topic, payload.c_str());
  Serial.printf("MQTT [%s] <- %s\n", topic, payload.c_str());
}

// ---------- JSON helper ----------
String getValueFromJson(String json, String key) {
  String searchKey = "\"" + key + "\":";
  int keyIndex = json.indexOf(searchKey);
  if (keyIndex == -1) return "";

  int valueStartIndex = keyIndex + searchKey.length();
  while (valueStartIndex < json.length() &&
         (json.charAt(valueStartIndex) == ' ' || json.charAt(valueStartIndex) == '\t')) {
    valueStartIndex++;
  }

  if (json.charAt(valueStartIndex) == '"') {
    int valueEndIndex = json.indexOf('"', valueStartIndex + 1);
    if (valueEndIndex == -1) return "";
    return json.substring(valueStartIndex + 1, valueEndIndex);
  } else {
    int valueEndIndex = json.indexOf(',', valueStartIndex);
    if (valueEndIndex == -1) valueEndIndex = json.indexOf('}', valueStartIndex);
    if (valueEndIndex == -1) return "";
    String value = json.substring(valueStartIndex, valueEndIndex);
    value.trim();
    return value;
  }
}

// ---------- HTTP Handlers ----------
void handleHealthCheck() {
  Serial.println("GET /health");
  server.send(200, "application/json",
              "{\"status\":\"ok\",\"chip_id\":\"" + String(ESP.getEfuseMac(), HEX) + "\"}");
}

void handleMove() {
  Serial.println("POST /move");
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Cuerpo de la petición vacío\"}");
    return;
  }

  String body = server.arg("plain");
  String directionStr = getValueFromJson(body, "direction");
  String speedStr     = getValueFromJson(body, "speed");
  String durationStr  = getValueFromJson(body, "duration");

  if (directionStr == "" || speedStr == "" || durationStr == "") {
    server.send(400, "application/json",
                "{\"error\":\"JSON inválido o faltan 'direction','speed','duration'\"}");
    return;
  }

  int speed    = speedStr.toInt();
  int duration = durationStr.toInt();

  if (speed <= 0 || speed > 255 || duration <= 0 || duration > 5000) {
    server.send(400, "application/json",
                "{\"error\":\"Parámetros inválidos. 'speed'(1-255), 'duration'(1-5000ms)\"}");
    return;
  }

  String clientIp = server.client().remoteIP().toString();

  // Publicar en tópico de comandos (igual que antes)
  String mqttPayload = "{";
  mqttPayload += "\"direction\":\"" + directionStr + "\",";
  mqttPayload += "\"speed\":" + String(speed) + ",";
  mqttPayload += "\"duration\":" + String(duration) + ",";
  mqttPayload += "\"client_ip\":\"" + clientIp + "\"";
  mqttPayload += "}";
  publishMqtt(MQTT_TOPIC_CMDS, mqttPayload);

  // Ejecutar movimiento
  if (directionStr == "forward")       moveForward(speed);
  else if (directionStr == "backward") moveBackward(speed);
  else if (directionStr == "right")    turnRight(speed);
  else if (directionStr == "left")     turnLeft(speed);
  else {
    server.send(400, "application/json",
                "{\"error\":\"Dirección inválida: 'forward','backward','right','left'\"}");
    return;
  }

  isMoving = true;
  moveStopTime = millis() + duration;

  server.send(200, "application/json",
              "{\"status\":\"movimiento iniciado\",\"details\":" + mqttPayload + "}");
}

// ---------- HC-SR04 (mock o real) ----------
float readUltrasonicCm() {
#if USE_ULTRASONIC_MOCK
  // Simulación: 10.00 cm a 250.99 cm (ruido leve)
  float base = random(1000, 25100) / 100.0f;
  float jitter = random(-10, 10) / 100.0f;
  return max(0.0f, base + jitter);
#else
  // Lectura real (asegúrate de divisor de voltaje en ECHO)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Pulso de disparo
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Duración del eco (us) con timeout
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT_US);
  if (duration == 0) {
    return -1.0f; // timeout / sin lectura
  }

  // Conversión a centímetros (aprox: us / 58)
  float distanceCm = duration / 58.0f;
  return distanceCm;
#endif
}

void publishUltrasonic() {
  float d = readUltrasonicCm();

  String ip   = WiFi.isConnected() ? WiFi.localIP().toString() : String("0.0.0.0");
  String chip = String(ESP.getEfuseMac(), HEX);

  String payload = "{";
  payload += "\"distance_cm\":";
  if (d < 0) payload += "null"; else payload += String(d, 2);
  payload += ",\"ts\":" + String((unsigned long)millis());
  payload += ",\"chip_id\":\"" + chip + "\"";
  payload += ",\"ip\":\"" + ip + "\"";
  payload += ",\"mock\":"; payload += (USE_ULTRASONIC_MOCK ? "true" : "false");
  payload += "}";

  publishMqtt(MQTT_TOPIC_TELEM, payload);
}

// ---------- NotFound ----------
void handleNotFound() {
  String message = "Recurso no encontrado\n\n";
  message += "URI: " + server.uri();
  message += "\nMetodo: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\n";
  server.send(404, "text/plain", message);
}

// ---------- Setup/Loop ----------
void setup() {
  Serial.begin(115200);
  delay(1000);

  setupMotors();
  stopMotors();

  // Pines del sensor (solo si se usa físico; con mock no pasa nada)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  setup_wifi();

  client.setServer(MQTT_SERVER, MQTT_PORT);

  server.on("/health", HTTP_GET, handleHealthCheck);
  server.on("/move",   HTTP_POST, handleMove);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Servidor HTTP iniciado.");
  Serial.print("Control: http://");
  Serial.println(WiFi.localIP());

  nextTelemetryAt = millis() + ULTRASONIC_PERIOD_MS;
}

void loop() {
  server.handleClient();

  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  if (isMoving && millis() >= moveStopTime) {
    stopMotors();
  }

  // Publicación periódica de telemetría ultrasónica
  unsigned long now = millis();
  if ((long)(now - nextTelemetryAt) >= 0) {
    publishUltrasonic();
    nextTelemetryAt = now + ULTRASONIC_PERIOD_MS;
  }
}
