// certs.h
// Certificado raíz para conexión TLS en ESP32 (PRUEBA C)

#pragma once
#include <WiFiClientSecure.h>

// Sustituir el texto entre BEGIN y END por el certificado raíz real (PEM)
static const char ROOT_CA_PEM[] PROGMEM = R"(-----BEGIN CERTIFICATE-----
xxxxxxxxxxxxxxxxxxxxx
-----END CERTIFICATE-----)";
