// config.h
// ============================================
// Configuración por preprocesador (override por -D al compilar)
// ============================================

// --------- WIFI ----------
#ifndef WIFI_SSID
#define WIFI_SSID "iPhone de Juan Pablo"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "millos14"
#endif

// --------- MQTT ----------
#ifndef MQTT_SERVER
#define MQTT_SERVER "broker.hivemq.com"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

// Tópico de comandos (movimiento) — ya existente en tu sketch
#ifndef MQTT_TOPIC_CMDS
#define MQTT_TOPIC_CMDS "iot_car/commands"
#endif

// Tópico de telemetría (NUEVO y DIFERENTE al de comandos)
#ifndef MQTT_TOPIC_TELEM
#define MQTT_TOPIC_TELEM "iot_car/ultrasonic"
#endif

// --------- Pines Motores L298N ----------
#ifndef ENA_PIN
#define ENA_PIN 13   // PWM Motor Derecho
#endif
#ifndef ENB_PIN
#define ENB_PIN 12   // PWM Motor Izquierdo
#endif
#ifndef IN1_PIN
#define IN1_PIN 14
#endif
#ifndef IN2_PIN
#define IN2_PIN 27
#endif
#ifndef IN3_PIN
#define IN3_PIN 26
#endif
#ifndef IN4_PIN
#define IN4_PIN 25
#endif

// --------- HC-SR04 ----------
#ifndef TRIG_PIN
#define TRIG_PIN 35  // Trigger (OUTPUT)
#endif
#ifndef ECHO_PIN
#define ECHO_PIN 34  // Echo (INPUT) -> Debe bajar a 3.3V con divisor
#endif

// Usa mock o sensor físico
// 1 = simulado, 0 = leer sensor real
#ifndef USE_ULTRASONIC_MOCK
#define USE_ULTRASONIC_MOCK 1
#endif

// Período de publicación de telemetría (ms)
#ifndef ULTRASONIC_PERIOD_MS
#define ULTRASONIC_PERIOD_MS 1000
#endif

// Timeout de medición (us) para pulseIn (cuando se usa sensor real)
#ifndef ULTRASONIC_TIMEOUT_US
#define ULTRASONIC_TIMEOUT_US 30000
#endif
