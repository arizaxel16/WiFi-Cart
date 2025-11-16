// config.h
// Configuration file for ESP32 IoT Car Controller
// Modify these values according to your setup

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// FIRMWARE INFO
// ============================================
#define FIRMWARE_VERSION "2.0.0"

// ============================================
// WIFI CONFIGURATION
// ============================================
// ⚠️ CHANGE THESE TO YOUR WIFI CREDENTIALS
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"

// ============================================
// MQTT CONFIGURATION
// ============================================
#define MQTT_SERVER "broker.hivemq.com"
#define MQTT_PORT 8883  // TLS encrypted port

#define MQTT_TOPIC_CMDS "iot_car/commands"
#define MQTT_TOPIC_TELEM "iot_car/telemetry"
#define MQTT_TOPIC_STATUS "iot_car/status"

// ============================================
// MOTOR PINS (L298N Driver)
// ============================================
#define ENA_PIN 13   // PWM Right Motor (Enable A)
#define ENB_PIN 12   // PWM Left Motor (Enable B)
#define IN1_PIN 14   // Motor A Direction 1
#define IN2_PIN 27   // Motor A Direction 2
#define IN3_PIN 26   // Motor B Direction 1
#define IN4_PIN 25   // Motor B Direction 2

// ============================================
// ULTRASONIC SENSOR (HC-SR04)
// ============================================
// ⚠️ IMPORTANT: GPIO 34, 35, 36, 39 are INPUT-ONLY on ESP32!
// TRIG needs to be an OUTPUT-capable pin
#define TRIG_PIN 32  // Trigger (OUTPUT)
#define ECHO_PIN 33  // Echo (INPUT) - Use voltage divider to 3.3V!

// Sensor mode: 1 = simulated/mock, 0 = real HC-SR04
#define USE_ULTRASONIC_MOCK 0  // Set to 1 for testing without sensor

// Telemetry publish interval (milliseconds)
#define ULTRASONIC_PERIOD_MS 1000

// Pulse timeout for pulseIn() (microseconds)
// 30000µs = ~5m max range
#define ULTRASONIC_TIMEOUT_US 30000

// Obstacle detection threshold (centimeters)
// Car will refuse to move forward if obstacle closer than this
#define OBSTACLE_THRESHOLD_CM 20.0f

#endif // CONFIG_H
