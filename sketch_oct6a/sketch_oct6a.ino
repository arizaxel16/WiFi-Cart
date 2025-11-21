/*
  ESP32 IoT Car Controller - WORKING VERSION
  HTTP server for motor control
  NO MQTT - Pure HTTP only
  
  For Arduino IDE - ESP32 Board
*/    

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"

// ========== GLOBAL INSTANCES ==========
WebServer server(80);

// Movement state control
unsigned long moveStopTime = 0;
bool isMoving = false;
String lastDirection = "stopped";
int lastSpeed = 0;

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
  Serial.println("[MOTOR] Stopped");
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
  Serial.printf("[MOTOR] Moving forward at speed %d\n", speed);
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
  Serial.printf("[MOTOR] Moving backward at speed %d\n", speed);
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
  Serial.printf("[MOTOR] Turning left at speed %d\n", speed);
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
  Serial.printf("[MOTOR] Turning right at speed %d\n", speed);
}

// ========== ULTRASONIC SENSOR ==========

float readUltrasonicCm() {
#if USE_ULTRASONIC_MOCK == 1
  // Mock: random distance
  float base = random(1000, 25000) / 100.0f;
  float jitter = random(-50, 50) / 100.0f;
  return max(2.0f, base + jitter);
#else
  // Real HC-SR04 reading
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT_US);
  
  if (duration == 0) {
    return -1.0f;
  }
  
  float distance = duration / 58.0f;
  
  if (distance < 2.0f || distance > 400.0f) {
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

bool checkObstacle() {
  lastDistanceCm = readUltrasonicCm();
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

// ========== HTTP HANDLERS ==========

void handleHealthCheck() {
  Serial.println("[HTTP] Health check requested");
  
  StaticJsonDocument<512> doc;
  doc["status"] = "ok";
  doc["version"] = FIRMWARE_VERSION;
  doc["chip_id"] = String(ESP.getEfuseMac(), HEX);
  doc["ip"] = WiFi.localIP().toString();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime_ms"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["mock_sensor"] = USE_ULTRASONIC_MOCK;
  doc["last_distance_cm"] = lastDistanceCm;
  doc["motor_state"] = lastDirection;

  String response;
  serializeJson(doc, response);
  
  // Add CORS headers explicitly
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", response);
}

void handleMove() {
  Serial.println("[HTTP] Move command received");
  
  // Add CORS headers for all responses
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  String direction = "";
  int speed = 0;
  int duration = 0;

  // Handle POST with JSON body
  if (server.method() == HTTP_POST) {
    if (!server.hasArg("plain")) {
      Serial.println("[HTTP] Empty POST body");
      server.send(400, "application/json", "{\"error\":\"Empty body\"}");
      return;
    }

    String body = server.arg("plain");
    Serial.printf("[HTTP] Received body: %s\n", body.c_str());

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
      Serial.printf("[HTTP] JSON parse error: %s\n", error.c_str());
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    direction = doc["direction"] | "";
    speed = doc["speed"] | 0;
    duration = doc["duration"] | 0;
  } 
  // Handle GET with query params
  else if (server.method() == HTTP_GET) {
    direction = server.arg("direction");
    speed = server.arg("speed").toInt();
    duration = server.arg("duration").toInt();
  }

  Serial.printf("[HTTP] Parsed: direction=%s, speed=%d, duration=%d\n", 
                direction.c_str(), speed, duration);

  // Validate parameters
  if (direction == "" || speed <= 0 || speed > 255 || duration <= 0 || duration > 10000) {
    Serial.println("[HTTP] Invalid parameters");
    server.send(400, "application/json",
                "{\"error\":\"Invalid params: direction(forward/backward/left/right/stop), speed(1-255), duration(1-10000ms)\"}");
    return;
  }

  // Check for obstacles before moving forward
  if (direction == "forward") {
    if (checkObstacle()) {
      Serial.printf("[HTTP] Obstacle detected at %.2f cm\n", lastDistanceCm);
      StaticJsonDocument<256> errorDoc;
      errorDoc["error"] = "Obstacle detected";
      errorDoc["distance_cm"] = lastDistanceCm;
      errorDoc["threshold_cm"] = OBSTACLE_THRESHOLD_CM;
      String errorResponse;
      serializeJson(errorDoc, errorResponse);
      server.send(409, "application/json", errorResponse);
      return;
    }
  }

  // Execute movement
  if (direction == "forward") {
    moveForward(speed);
  } 
  else if (direction == "backward") {
    moveBackward(speed);
  } 
  else if (direction == "right") {
    turnRight(speed);
  } 
  else if (direction == "left") {
    turnLeft(speed);
  } 
  else if (direction == "stop") {
    stopMotors();
    server.send(200, "application/json", "{\"status\":\"stopped\"}");
    return;
  }
  else {
    Serial.printf("[HTTP] Invalid direction: %s\n", direction.c_str());
    server.send(400, "application/json",
                "{\"error\":\"Invalid direction: use forward/backward/left/right/stop\"}");
    return;
  }

  isMoving = true;
  moveStopTime = millis() + (unsigned long)duration;

  // Build response
  StaticJsonDocument<384> responseDoc;
  responseDoc["status"] = "moving";
  responseDoc["direction"] = direction;
  responseDoc["speed"] = speed;
  responseDoc["duration_ms"] = duration;
  responseDoc["will_stop_at"] = moveStopTime;

  String response;
  serializeJson(responseDoc, response);
  
  Serial.println("[HTTP] Sending response...");
  server.send(200, "application/json", response);
  Serial.println("[HTTP] Response sent successfully");
}

void handleGetStatus() {
  Serial.println("[HTTP] Status requested");
  
  lastDistanceCm = readUltrasonicCm();
  
  StaticJsonDocument<256> doc;
  doc["motor_state"] = lastDirection;
  doc["motor_speed"] = lastSpeed;
  doc["is_moving"] = isMoving;
  doc["last_distance_cm"] = lastDistanceCm;
  doc["obstacle_detected"] = (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_THRESHOLD_CM);

  String response;
  serializeJson(doc, response);  // Serializar el documento JSON en el string
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", response);
}

void handleNotFound() {
  String msg = "{\"error\":\"Not Found\",\"uri\":\"" + server.uri() + "\"}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(404, "application/json", msg);
}

void handleCORS() {
  Serial.println("[HTTP] CORS preflight request");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

// ========== SETUP & LOOP ==========

// Agregar ANTES de setup()
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Car</title>
    <style>
        body{font-family:Arial;background:#667eea;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh}
        .container{background:#fff;border-radius:20px;padding:30px;max-width:400px;width:100%;box-shadow:0 20px 60px rgba(0,0,0,0.3)}
        h1{text-align:center;color:#333;margin-bottom:30px}
        .status{background:#d4edda;padding:15px;border-radius:10px;text-align:center;margin-bottom:20px;font-weight:bold;color:#155724}
        .controls{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:20px}
        button{padding:30px 20px;font-size:16px;font-weight:bold;border:none;border-radius:10px;cursor:pointer;background:#667eea;color:#fff;box-shadow:0 4px 6px rgba(0,0,0,0.1)}
        button:active{transform:scale(0.95)}
        .btn-forward{grid-column:2;background:#4CAF50}
        .btn-stop{grid-column:span 3;background:#f44336;font-size:18px}
        .btn-left,.btn-right{background:#2196F3}
        .btn-backward{grid-column:2;background:#FF9800}
        .speed-control{margin-bottom:20px}
        .speed-control label{display:block;margin-bottom:10px;font-weight:bold}
        #speed{width:100%;height:40px}
        #speedValue{display:block;text-align:center;font-size:32px;font-weight:bold;color:#667eea;margin-top:10px}
    </style>
</head>
<body>
    <div class="container">
        <h1>🚗 ESP32 Car</h1>
        <div id="status" class="status">Listo</div>
        <div class="speed-control">
            <label>⚡ Velocidad</label>
            <input type="range" id="speed" min="100" max="255" value="180" step="5">
            <span id="speedValue">180</span>
        </div>
        <div class="controls">
            <div></div>
            <button class="btn-forward" onclick="move('forward')">⬆️ ADELANTE</button>
            <div></div>
            <button class="btn-left" onclick="move('left')">⬅️ IZQUIERDA</button>
            <button class="btn-stop" onclick="move('stop')">⛔ STOP</button>
            <button class="btn-right" onclick="move('right')">➡️ DERECHA</button>
            <div></div>
            <button class="btn-backward" onclick="move('backward')">⬇️ ATRÁS</button>
            <div></div>
        </div>
    </div>
    <script>
        let moving=false;
        document.getElementById('speed').oninput=function(){document.getElementById('speedValue').textContent=this.value};
        function setStatus(msg){document.getElementById('status').textContent=msg}
        async function move(dir){
            if(moving&&dir!=='stop')return;
            const speed=parseInt(document.getElementById('speed').value);
            const dur=dir==='stop'?100:2000;
            setStatus('Moviendo '+dir+'...');
            moving=true;
            try{
                const res=await fetch('/api/v1/move',{
                    method:'POST',
                    headers:{'Content-Type':'application/json'},
                    body:JSON.stringify({direction:dir,speed:speed,duration:dur})
                });
                const data=await res.json();
                setStatus('✅ '+dir.toUpperCase());
                if(dir!=='stop')setTimeout(()=>{moving=false;setStatus('Listo')},dur+200);
                else{moving=false;setStatus('Detenido')}
            }catch(e){
                setStatus('❌ Error: '+e.message);
                moving=false;
            }
        }
    </script>
</body>
</html>
)rawliteral";

// Agregar esta función handler
void handleRoot() {
  Serial.println("[HTTP] Serving web interface");
  server.send(200, "text/html", HTML_PAGE);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.printf("ESP32 IoT Car Controller v%s\n", FIRMWARE_VERSION);
  Serial.println("========================================");

  setupMotors();
  setupUltrasonic();
  setupWifi();

  server.on("/", HTTP_GET, handleRoot);

  // HTTP server with CORS support
  server.on("/api/v1/healthcheck", HTTP_GET, handleHealthCheck);
  server.on("/api/v1/move", HTTP_POST, handleMove);
  server.on("/api/v1/move", HTTP_GET, handleMove);
  server.on("/api/v1/status", HTTP_GET, handleGetStatus);
  server.on("/api/v1/move", HTTP_OPTIONS, handleCORS);
  server.on("/api/v1/healthcheck", HTTP_OPTIONS, handleCORS);
  server.on("/api/v1/status", HTTP_OPTIONS, handleCORS);
  server.onNotFound(handleNotFound);

  // Enable CORS globally
  server.enableCORS(true);

  server.begin();
  Serial.printf("\n[HTTP] Server started at http://%s\n", WiFi.localIP().toString().c_str());
  Serial.println("[HTTP] Endpoints:");
  Serial.println("  GET  /api/v1/healthcheck");
  Serial.println("  POST /api/v1/move");
  Serial.println("  GET  /api/v1/move?direction=...&speed=...&duration=...");
  Serial.println("  GET  /api/v1/status");
  Serial.println("========================================");
  Serial.println("[READY] System initialized successfully!");
  Serial.println("[READY] You can now control the car via HTTP");
  Serial.println("========================================\n");
}

void loop() {
  // Handle HTTP requests
  server.handleClient();

  // Auto-stop motors after duration
  if (isMoving && millis() >= moveStopTime) {
    stopMotors();
  }

  // Small delay to prevent watchdog issues
  delay(10);
}
