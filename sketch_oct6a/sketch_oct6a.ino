/*
  ESP32 IoT Car Controller
  - Servidor HTTP para controlar el movimiento del vehículo.
  - Cliente MQTT para publicar las instrucciones recibidas.
  - Control de motores L298N mediante PWM.
*/

// Librerías necesarias
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

// --- Configuración del Usuario (MODIFICAR ESTOS VALORES) ---
const char* ssid = "iPhone de Juan Pablo";
const char* password = "millos14";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "iot_car/commands";

// --- Pines de Conexión L298N ---
const int ENA_PIN = 13; // Motor A Speed (Derecho)
const int ENB_PIN = 12; // Motor B Speed (Izquierdo)

const int IN1_PIN = 14; // Motor A
const int IN2_PIN = 27; // Motor A
const int IN3_PIN = 26; // Motor B
const int IN4_PIN = 25; // Motor B

// --- Variables Globales ---
WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long moveStopTime = 0;
bool isMoving = false;

// --- Funciones de Control de Motores ---

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
  // Motor derecho (adelante)
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, HIGH);
  // Motor izquierdo (adelante)
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);
  // Velocidad
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

void moveBackward(int speed) {
  Serial.printf("Moviendo hacia atrás a velocidad %d\n", speed);
  // Motor derecho (atrás)
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  // Motor izquierdo (atrás)
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  // Velocidad
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

void turnLeft(int speed) {
  Serial.printf("Girando a la izquierda a velocidad %d\n", speed);
  // Motor derecho (atrás)
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  // Motor izquierdo (adelante)
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);
  // Velocidad
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

void turnRight(int speed) {
  Serial.printf("Girando a la derecha a velocidad %d\n", speed);
  // Motor derecho (adelante)
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, HIGH);
  // Motor izquierdo (atrás)
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  // Velocidad
  analogWrite(ENA_PIN, speed);
  analogWrite(ENB_PIN, speed);
}

// --- Conectividad WiFi y MQTT ---

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
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
      Serial.println(" intentando de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void publishMqttMessage(const String& payload) {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.publish(mqtt_topic, payload.c_str());
  Serial.println("Mensaje MQTT publicado:");
  Serial.println(payload);
}

// --- Helper para parsear JSON manualmente ---
String getValueFromJson(String json, String key) {
  String searchKey = "\"" + key + "\":";
  int keyIndex = json.indexOf(searchKey);
  if (keyIndex == -1) {
    return "";
  }

  int valueStartIndex = keyIndex + searchKey.length();

  // Saltar espacios en blanco
  while (valueStartIndex < json.length() && 
         (json.charAt(valueStartIndex) == ' ' || 
          json.charAt(valueStartIndex) == '\t')) {
    valueStartIndex++;
  }

  // Si el valor es un string (empieza con comillas)
  if (json.charAt(valueStartIndex) == '"') {
    int valueEndIndex = json.indexOf('"', valueStartIndex + 1);
    if (valueEndIndex == -1) return "";
    return json.substring(valueStartIndex + 1, valueEndIndex);
  } else { // Si el valor es un número
    int valueEndIndex = json.indexOf(',', valueStartIndex);
    if (valueEndIndex == -1) {
      valueEndIndex = json.indexOf('}', valueStartIndex);
    }
    if (valueEndIndex == -1) return "";
    String value = json.substring(valueStartIndex, valueEndIndex);
    value.trim();
    return value;
  }
}

// --- Handlers del Servidor HTTP ---

void handleHealthCheck() {
  Serial.println("GET /health - Healthcheck solicitado.");
  server.send(200, "application/json", "{\"status\":\"ok\", \"chip_id\":\"" + String(ESP.getEfuseMac(), HEX) + "\"}");
}

void handleMove() {
  Serial.println("POST /move - Comando de movimiento recibido.");
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Cuerpo de la petición vacío\"}");
    return;
  }

  String body = server.arg("plain");
  
  String directionStr = getValueFromJson(body, "direction");
  String speedStr = getValueFromJson(body, "speed");
  String durationStr = getValueFromJson(body, "duration");

  if (directionStr == "" || speedStr == "" || durationStr == "") {
    server.send(400, "application/json", "{\"error\":\"JSON inválido o campos faltantes ('direction', 'speed', 'duration')\"}");
    return;
  }

  int speed = speedStr.toInt();
  int duration = durationStr.toInt();

  // Validación de parámetros
  if (speed <= 0 || speed > 255 || duration <= 0 || duration > 5000) {
    server.send(400, "application/json", "{\"error\":\"Parámetros inválidos. 'speed' (1-255), 'duration' (1-5000ms)\"}");
    return;
  }

  // Obtener IP del cliente
  String clientIp = server.client().remoteIP().toString();

  // Crear payload para MQTT
  String mqttPayload = "{";
  mqttPayload += "\"direction\":\"" + directionStr + "\",";
  mqttPayload += "\"speed\":" + String(speed) + ",";
  mqttPayload += "\"duration\":" + String(duration) + ",";
  mqttPayload += "\"client_ip\":\"" + clientIp + "\"";
  mqttPayload += "}";
  
  // Publicar en MQTT
  publishMqttMessage(mqttPayload);

  // Ejecutar movimiento - AHORA CON NOMBRES CORRECTOS
  if (directionStr == "forward") {
    moveForward(speed);
  } else if (directionStr == "backward") {
    moveBackward(speed);
  } else if (directionStr == "right") {
    turnRight(speed);
  } else if (directionStr == "left") {
    turnLeft(speed);
  } else {
    server.send(400, "application/json", "{\"error\":\"Dirección no válida. Use 'forward', 'backward', 'right', 'left'\"}");
    return;
  }

  isMoving = true;
  moveStopTime = millis() + duration;

  server.send(200, "application/json", "{\"status\":\"movimiento iniciado\", \"details\":" + mqttPayload + "}");
}

void handleNotFound() {
  String message = "Recurso no encontrado\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMetodo: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\n";
  server.send(404, "text/plain", message);
}

// --- Setup y Loop Principal ---

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  setupMotors();
  stopMotors();
  
  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  
  server.on("/health", HTTP_GET, handleHealthCheck);
  server.on("/move", HTTP_POST, handleMove);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
  Serial.print("Para controlar el carrito, usa la IP: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();

  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  // Detener el movimiento después de la duración especificada
  if (isMoving && millis() >= moveStopTime) {
    stopMotors();
  }
}