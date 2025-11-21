/*
  ESP32 IoT Car Controller - FINAL VERSION
  HTTP server + Full Sensor Telemetry
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

// Sensor telemetry timing
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 500; // Leer sensor cada 500ms

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
    Serial.println("[SENSOR] Timeout - no echo");
    return -1.0f;
  }
  
  float distance = duration / 58.0f;
  
  if (distance < 2.0f || distance > 400.0f) {
    Serial.printf("[SENSOR] Out of range: %.2f cm\n", distance);
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
  // Leer sensor en tiempo real para verificar obstáculo
  float currentDistance = readUltrasonicCm();
  lastDistanceCm = currentDistance; // Actualizar última lectura
  
  if (currentDistance > 0 && currentDistance < OBSTACLE_THRESHOLD_CM) {
    Serial.printf("[SENSOR] OBSTACLE DETECTED at %.2f cm!\n", currentDistance);
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
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", response);
}

void handleMove() {
  Serial.println("[HTTP] Move command received");
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  String direction = "";
  int speed = 0;
  int duration = 0;

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

  // Check for obstacles ONLY when moving forward
  if (direction == "forward") {
    if (checkObstacle()) {
      Serial.printf("[HTTP] BLOCKED! Obstacle at %.2f cm (threshold: %.2f cm)\n", 
                    lastDistanceCm, OBSTACLE_THRESHOLD_CM);
      
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

  StaticJsonDocument<384> responseDoc;
  responseDoc["status"] = "moving";
  responseDoc["direction"] = direction;
  responseDoc["speed"] = speed;
  responseDoc["duration_ms"] = duration;
  responseDoc["will_stop_at"] = moveStopTime;

  String response;
  serializeJson(responseDoc, response);
  
  Serial.println("[HTTP] Response sent successfully");
  server.send(200, "application/json", response);
}

void handleGetStatus() {
  Serial.println("[HTTP] Status requested");
  
  // Leer sensor en tiempo real
  lastDistanceCm = readUltrasonicCm();
  
  StaticJsonDocument<256> doc;
  doc["motor_state"] = lastDirection;
  doc["motor_speed"] = lastSpeed;
  doc["is_moving"] = isMoving;
  doc["last_distance_cm"] = lastDistanceCm;
  doc["obstacle_detected"] = (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_THRESHOLD_CM);

  String response;
  serializeJson(doc, response);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", response);
  
  // Log de telemetría
  if (lastDistanceCm > 0) {
    Serial.printf("[SENSOR] Distance: %.2f cm | Obstacle: %s\n", 
                  lastDistanceCm, 
                  (lastDistanceCm < OBSTACLE_THRESHOLD_CM) ? "YES" : "NO");
  }
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

// ========== HTML INTERFACE ==========
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Pro Controller</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script crossorigin src="https://unpkg.com/react@18/umd/react.development.js"></script>
    <script crossorigin src="https://unpkg.com/react-dom@18/umd/react-dom.development.js"></script>
    <script src="https://unpkg.com/@babel/standalone/babel.min.js"></script>
    <style>
        ::-webkit-scrollbar { width: 8px; height: 8px; }
        ::-webkit-scrollbar-track { background: #1e293b; }
        ::-webkit-scrollbar-thumb { background: #475569; border-radius: 4px; }
        ::-webkit-scrollbar-thumb:hover { background: #64748b; }
    </style>
</head>
<body class="bg-slate-900 text-slate-100">
    <div id="root"></div>
    <script type="text/babel">
        const { useState, useEffect, useRef } = React;

        const Icon = ({ children, className, size = 24 }) => (
            <svg xmlns="http://www.w3.org/2000/svg" width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className={className}>{children}</svg>
        );
        const ArrowUp = (p) => <Icon {...p}><path d="m5 12 7-7 7 7"/><path d="M12 19V5"/></Icon>;
        const ArrowDown = (p) => <Icon {...p}><path d="M12 5v14"/><path d="m19 12-7 7-7-7"/></Icon>;
        const ArrowLeft = (p) => <Icon {...p}><path d="m12 19-7-7 7-7"/><path d="M19 12H5"/></Icon>;
        const ArrowRight = (p) => <Icon {...p}><path d="M5 12h14"/><path d="m12 5 7 7-7 7"/></Icon>;
        const StopCircle = (p) => <Icon {...p}><circle cx="12" cy="12" r="10"/><rect width="6" height="6" x="9" y="9"/></Icon>;
        const Wifi = (p) => <Icon {...p}><path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M1.42 9a16 16 0 0 1 21.16 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><line x1="12" y1="20" x2="12.01" y2="20"/></Icon>;
        const Activity = (p) => <Icon {...p}><path d="M22 12h-4l-3 9L9 3l-3 9H2"/></Icon>;
        const Settings = (p) => <Icon {...p}><path d="M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.38a2 2 0 0 0-.73-2.73l-.15-.1a2 2 0 0 1-1-1.72v-.51a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z"/><circle cx="12" cy="12" r="3"/></Icon>;
        const AlertTriangle = (p) => <Icon {...p}><path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z"/><path d="M12 9v4"/><path d="M12 17h.01"/></Icon>;
        const Terminal = (p) => <Icon {...p}><polyline points="4 17 10 11 4 5"/><line x1="12" x2="20" y1="19" y2="19"/></Icon>;

        const App = () => {
            const [ipAddress, setIpAddress] = useState(window.location.hostname || '192.168.4.1');
            const [speed, setSpeed] = useState(150);
            const [duration, setDuration] = useState(2000);
            
            const [isConnected, setIsConnected] = useState(false);
            const [sensorData, setSensorData] = useState({ distance: -1, motorState: 'stopped', obstacle: false });
            const [logs, setLogs] = useState([]);
            const [isKeyboardEnabled, setIsKeyboardEnabled] = useState(true);
            const [isLoading, setIsLoading] = useState(false);
            const logsEndRef = useRef(null);

            useEffect(() => logsEndRef.current?.scrollIntoView({ behavior: "smooth" }), [logs]);

            const addLog = (msg, type = 'info') => {
                const time = new Date().toLocaleTimeString().split(' ')[0];
                setLogs(prev => [...prev.slice(-14), { time, msg, type }]); 
            };

            const sendCommand = async (endpoint, method = 'GET', body = null) => {
                const url = `/api/v1/${endpoint}`;
                
                setIsLoading(true);
                try {
                    const opts = { method, headers: { 'Content-Type': 'application/json' } };
                    if (body) opts.body = JSON.stringify(body);
                    
                    const res = await fetch(url, opts);
                    if (!res.ok) throw new Error(`HTTP ${res.status}`);
                    
                    const data = await res.json();
                    if (data.error) {
                        addLog(`⚠️ ${data.error}`, 'warning');
                        if (data.distance_cm) {
                            addLog(`Obstacle at ${data.distance_cm.toFixed(1)} cm`, 'warning');
                        }
                    } else if(method === 'POST') {
                        addLog(`${method} ${endpoint} OK`, 'success');
                    }
                    setIsConnected(true);
                    return data;
                } catch (error) {
                    addLog(`Err: ${error.message}`, 'error');
                    setIsConnected(false);
                    return null;
                } finally {
                    setIsLoading(false);
                }
            };

            const handleMove = (dir) => sendCommand('move', 'POST', { direction: dir, speed: parseInt(speed), duration: parseInt(duration) });
            const handleStop = () => sendCommand('move', 'POST', { direction: 'stop', speed: 0, duration: 100 });

            // Polling de estado cada 1 segundo
            useEffect(() => {
                const interval = setInterval(async () => {
                    try {
                        const res = await fetch('/api/v1/status');
                        if (res.ok) {
                            const data = await res.json();
                            setSensorData({ 
                                distance: data.last_distance_cm, 
                                motorState: data.motor_state, 
                                obstacle: data.obstacle_detected 
                            });
                            setIsConnected(true);
                        }
                    } catch (e) { setIsConnected(false); }
                }, 1000);
                return () => clearInterval(interval);
            }, []);

            // Teclado
            useEffect(() => {
                const handleKey = (e) => {
                    if (!isKeyboardEnabled || isLoading) return;
                    if (e.repeat) return;
                    
                    const map = { 
                        'ArrowUp': 'forward', 'w': 'forward', 'W': 'forward',
                        'ArrowDown': 'backward', 's': 'backward', 'S': 'backward',
                        'ArrowLeft': 'left', 'a': 'left', 'A': 'left',
                        'ArrowRight': 'right', 'd': 'right', 'D': 'right',
                        ' ': 'stop', 'Escape': 'stop'
                    };
                    
                    if (map[e.key]) {
                        e.preventDefault();
                        if (map[e.key] === 'stop') handleStop();
                        else handleMove(map[e.key]);
                    }
                };
                window.addEventListener('keydown', handleKey);
                return () => window.removeEventListener('keydown', handleKey);
            }, [isKeyboardEnabled, speed, duration, isLoading]);

            return (
                <div className="min-h-screen p-4 md:p-6 flex flex-col items-center">
                    <div className="w-full max-w-4xl space-y-6">
                        <div className="bg-slate-800 p-4 rounded-xl border border-slate-700 flex flex-col md:flex-row justify-between items-center gap-4 shadow-lg">
                            <div className="flex items-center gap-3">
                                <div className={`w-3 h-3 rounded-full ${isConnected ? 'bg-green-500 animate-pulse' : 'bg-red-500'}`}></div>
                                <h1 className="text-xl md:text-2xl font-bold bg-gradient-to-r from-cyan-400 to-blue-500 bg-clip-text text-transparent">ESP32 CAR REMOTE</h1>
                            </div>
                        </div>

                        <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
                            <div className="lg:col-span-7 space-y-6">
                                <div className="bg-slate-800 p-6 rounded-2xl border border-slate-700 shadow-xl relative">
                                    <button onClick={() => setIsKeyboardEnabled(!isKeyboardEnabled)} className={`absolute top-4 right-4 p-2 rounded-lg transition-colors ${isKeyboardEnabled ? 'bg-blue-600 text-white' : 'bg-slate-700 text-slate-400'}`}>
                                        <span className="text-xs font-bold">KEYBOARD {isKeyboardEnabled ? 'ON' : 'OFF'}</span>
                                    </button>

                                    <div className="grid grid-cols-3 gap-3 max-w-[280px] mx-auto mt-4">
                                        <div className="col-start-2"><Btn icon={<ArrowUp size={32}/>} onClick={() => handleMove('forward')} label="W" disabled={isLoading} /></div>
                                        <div className="col-start-1 row-start-2"><Btn icon={<ArrowLeft size={32}/>} onClick={() => handleMove('left')} label="A" disabled={isLoading} /></div>
                                        <div className="col-start-2 row-start-2"><Btn icon={<StopCircle size={32}/>} onClick={handleStop} color="red" label="SPACE" /></div>
                                        <div className="col-start-3 row-start-2"><Btn icon={<ArrowRight size={32}/>} onClick={() => handleMove('right')} label="D" disabled={isLoading} /></div>
                                        <div className="col-start-2 row-start-3"><Btn icon={<ArrowDown size={32}/>} onClick={() => handleMove('backward')} label="S" disabled={isLoading} /></div>
                                    </div>
                                    
                                    <div className="mt-6 text-center">
                                        <span className="text-slate-400 text-sm">Estado Motor: </span>
                                        <span className={`font-mono font-bold uppercase ${sensorData.motorState === 'stopped' ? 'text-slate-500' : 'text-cyan-400'}`}>{sensorData.motorState}</span>
                                    </div>
                                </div>

                                <div className="bg-slate-800 p-5 rounded-xl border border-slate-700 space-y-4">
                                    <div className="flex items-center gap-2 text-slate-300 mb-1"><Settings size={16} /><span className="text-sm font-bold">CONFIGURACIÓN</span></div>
                                    <Slider label="Velocidad (PWM)" val={speed} set={setSpeed} min="80" max="255" unit="" />
                                    <Slider label="Duración Pulso" val={duration} set={setDuration} min="100" max="3000" step="100" unit="ms" />
                                </div>
                            </div>

                            <div className="lg:col-span-5 space-y-6 h-full flex flex-col">
                                <div className="bg-slate-800 p-5 rounded-xl border border-slate-700">
                                    <div className="flex items-center gap-2 text-slate-300 mb-4"><Activity size={16} /><span className="text-sm font-bold">SENSOR SONAR HC-SR04</span></div>
                                    <div className="flex items-center justify-between bg-slate-900 p-4 rounded-lg border border-slate-700">
                                        <div>
                                            <div className="text-3xl font-mono font-bold text-white">{sensorData.distance > 0 ? sensorData.distance.toFixed(1) : '--'}<span className="text-sm text-slate-500 ml-1">cm</span></div>
                                            <div className="text-xs text-slate-500 mt-1">Rango: 2-400cm | Threshold: 20cm</div>
                                        </div>
                                        {sensorData.obstacle ? 
                                            <div className="text-red-500 flex flex-col items-center animate-bounce"><AlertTriangle size={28}/><span className="text-[10px] font-bold">OBSTÁCULO</span></div> : 
                                            <div className="h-10 w-1.5 bg-slate-700 rounded-full overflow-hidden relative"><div className="absolute bottom-0 w-full bg-cyan-500 transition-all duration-300" style={{height: `${Math.min(sensorData.distance, 100)}%`}}></div></div>
                                        }
                                    </div>
                                </div>

                                <div className="bg-slate-900 rounded-xl border border-slate-700 flex-1 overflow-hidden flex flex-col min-h-[200px]">
                                    <div className="bg-slate-800 px-4 py-2 border-b border-slate-700 flex items-center gap-2"><Terminal size={14} className="text-slate-400"/><span className="text-xs font-mono text-slate-300">SISTEMA</span></div>
                                    <div className="p-3 overflow-y-auto font-mono text-[10px] md:text-xs space-y-1 flex-1">
                                        {logs.map((l, i) => (
                                            <div key={i} className="flex gap-2"><span className="text-slate-600">[{l.time}]</span><span className={l.type==='error'?'text-red-400':l.type==='success'?'text-green-400':l.type==='warning'?'text-yellow-400':'text-slate-300'}>{l.msg}</span></div>
                                        ))}
                                        <div ref={logsEndRef} />
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            );
        };

        const Btn = ({ icon, onClick, color, label, disabled }) => (
            <button onClick={onClick} disabled={disabled} className={`w-full aspect-square rounded-xl flex flex-col items-center justify-center text-white shadow-lg active:scale-95 transition-transform disabled:opacity-50 disabled:cursor-not-allowed ${color === 'red' ? 'bg-rose-600 hover:bg-rose-500 shadow-rose-900/50' : 'bg-blue-600 hover:bg-blue-500 shadow-blue-900/50'}`}>
                {icon}{label && <span className="text-[10px] font-bold opacity-50 mt-1">{label}</span>}
            </button>
        );

        const Slider = ({ label, val, set, min, max, step=1, unit }) => (
            <div className="space-y-1">
                <div className="flex justify-between text-xs text-slate-400"><span>{label}</span><span>{val}{unit}</span></div>
                <input type="range" min={min} max={max} step={step} value={val} onChange={e => set(e.target.value)} className="w-full h-2 bg-slate-700 rounded-lg appearance-none cursor-pointer accent-cyan-500" />
            </div>
        );

        ReactDOM.createRoot(document.getElementById('root')).render(<App />);
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  Serial.println("[HTTP] Serving web interface");
  server.send(200, "text/html", HTML_PAGE);
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

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/v1/healthcheck", HTTP_GET, handleHealthCheck);
  server.on("/api/v1/move", HTTP_POST, handleMove);
  server.on("/api/v1/move", HTTP_GET, handleMove);
  server.on("/api/v1/status", HTTP_GET, handleGetStatus);
  server.on("/api/v1/move", HTTP_OPTIONS, handleCORS);
  server.on("/api/v1/healthcheck", HTTP_OPTIONS, handleCORS);
  server.on("/api/v1/status", HTTP_OPTIONS, handleCORS);
  server.onNotFound(handleNotFound);

  server.enableCORS(true);

  server.begin();
  Serial.printf("\n[HTTP] Server started at http://%s\n", WiFi.localIP().toString().c_str());
  Serial.println("[HTTP] Endpoints:");
  Serial.println("  GET  /                         - Web Interface");
  Serial.println("  GET  /api/v1/healthcheck       - System Status");
  Serial.println("  POST /api/v1/move              - Motor Control");
  Serial.println("  GET  /api/v1/status            - Sensor Telemetry");
  Serial.println("========================================");
  Serial.println("[READY] System initialized successfully!");
  Serial.println("[READY] Open http://" + WiFi.localIP().toString() + " in your browser");
  Serial.println("========================================\n");
  
  // Lectura inicial del sensor
  lastDistanceCm = readUltrasonicCm();
  Serial.printf("[SENSOR] Initial reading: %.2f cm\n", lastDistanceCm);
}

void loop() {
  server.handleClient();

  // Auto-stop motors after duration
  if (isMoving && millis() >= moveStopTime) {
    stopMotors();
  }

  // Telemetría periódica del sensor (cada 500ms)
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastDistanceCm = readUltrasonicCm();
    lastSensorRead = millis();
    
    // Log solo si hay lectura válida (evitar spam de -1)
    if (lastDistanceCm > 0) {
      Serial.printf("[TELEMETRY] Distance: %.2f cm | Obstacle: %s\n", 
                    lastDistanceCm, 
                    (lastDistanceCm < OBSTACLE_THRESHOLD_CM) ? "DETECTED" : "Clear");
    }
  }

  delay(10);
}