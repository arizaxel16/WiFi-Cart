// config.h
// Configuration file for ESP32 IoT Car Controller

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// FIRMWARE INFO
// ============================================
#define FIRMWARE_VERSION "3.0.0-SIMPLE"

// ============================================
// WIFI CONFIGURATION
// ============================================
#define WIFI_SSID "iPhone de Juan Pablo"
#define WIFI_PASS "millos14"

// ============================================
// MOTOR PINS (L298N Driver)
// ============================================
#define ENA_PIN 13   // D13 - PWM Right Motor (Enable A)
#define ENB_PIN 12   // D12 - PWM Left Motor (Enable B)
#define IN1_PIN 14   // D14 - Motor A Direction 1
#define IN2_PIN 27   // D27 - Motor A Direction 2
#define IN3_PIN 26   // D26 - Motor B Direction 1
#define IN4_PIN 25   // D25 - Motor B Direction 2

// ============================================
// ULTRASONIC SENSOR (HC-SR04)
// ============================================
#define TRIG_PIN 32  // D32 - Trigger (OUTPUT)
#define ECHO_PIN 33  // D33 - Echo (INPUT) - Use voltage divider!

// Sensor mode: 1 = mock/simulated, 0 = real sensor
#define USE_ULTRASONIC_MOCK 0

// Pulse timeout (microseconds)
#define ULTRASONIC_TIMEOUT_US 30000

// Obstacle detection threshold (centimeters)
#define OBSTACLE_THRESHOLD_CM 20.0f

#endif // CONFIG_H