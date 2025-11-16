
# CERTIFICATES.md

## 1) ¿Qué es TLS, por qué es importante y qué es un certificado?
**TLS (Transport Layer Security)** es un protocolo de seguridad que protege la comunicación entre dispositivos en internet. Su objetivo es que los datos viajen cifrados y que las partes puedan confiar entre sí.  
Garantiza tres aspectos fundamentales:
- **Confidencialidad:** los datos se envían cifrados y no pueden ser leídos por terceros.
- **Integridad:** evita que los mensajes sean modificados durante el tránsito.
- **Autenticación:** permite comprobar que el servidor (y opcionalmente el cliente) son quienes dicen ser.

En este contexto, un **certificado digital** es un archivo que contiene una clave pública y datos de identidad, firmado por una autoridad confiable. Sirve para que el cliente valide que el servidor realmente pertenece al dominio con el que está hablando.

---

## 2) ¿Qué riesgos existen si no se usa TLS?
No usar TLS expone la comunicación a varios riesgos:
- **Intercepción de datos (sniffing):** un atacante puede ver las credenciales, comandos o datos enviados.
- **Ataques de intermediario (MITM):** alguien puede alterar o inyectar mensajes.
- **Suplantación:** el dispositivo puede conectarse a un servidor falso.
- **Pérdida de confidencialidad:** la información transmitida viaja sin protección.

---

## 3) ¿Qué es una CA (Certificate Authority)?
Una **Autoridad de Certificación (CA)** es una entidad que emite y firma certificados digitales para validar la identidad de personas o servidores.  
Los navegadores, sistemas operativos y microcontroladores confían en una lista de CAs raíz preinstaladas.  
Cuando un certificado fue emitido por una de esas CAs o por una intermedia autorizada, se considera **válido y confiable**.

---

## 4) ¿Qué es una cadena de certificados y su vigencia?
Una **cadena de certificados** conecta el certificado del servidor con una CA raíz de confianza.  
Ejemplo:  
`Servidor → Certificado intermedio → Raíz (Root CA)`  

- La **CA raíz** suele tener una vigencia larga (10 a 20 años).  
- Los **intermedios**, entre 1 y 5 años.  
- Los **certificados de servidor**, entre 90 días y 2 años (como los de Let’s Encrypt).

---

## 5) ¿Qué es un *keystore* y qué es un *certificate bundle*?
- **Keystore:** es un contenedor donde se guardan certificados y claves privadas. En sistemas embebidos (como el ESP32) normalmente se guarda en memoria o en el sistema de archivos del dispositivo.  
- **Certificate bundle:** es un archivo que reúne varias CAs raíz. Se usa para validar diferentes certificados sin tener que cargar cada uno por separado. El ESP32 permite usarlo con `setCACertBundle()`.

---

## 6) ¿Qué es la autenticación mutua (mTLS)?
La **autenticación mutua** es una variante de TLS donde no solo se valida al servidor, sino también al cliente.  
El cliente presenta su propio certificado firmado por la CA, y demuestra que posee la clave privada asociada.  
Así el servidor puede estar seguro de que el cliente es legítimo.  
Se usa en entornos donde la seguridad y el control de acceso son críticos.

---

## 7) ¿Cómo se habilita la validación de certificados en el ESP32?
El ESP32 soporta TLS mediante la clase `WiFiClientSecure`.  
Para habilitar la validación de certificados se debe:
1. Cambiar el cliente MQTT o HTTP a `WiFiClientSecure`.
2. Cargar una CA raíz válida o un bundle:
   - `client.setCACert(<PEM de la CA>)`
   - o `client.setCACertBundle()`
3. Conectarse al puerto **8883** (en el caso de MQTT seguro).  

Si no se carga ningún certificado y no se usa `setInsecure()`, el ESP32 rechazará la conexión por no poder verificar la identidad del servidor.

---

## 8) ¿Qué pasa si el sketch necesita conectarse a varios dominios?
Si el programa debe comunicarse con varios servidores que usan diferentes CAs, existen estas opciones:
- Usar un **bundle de certificados** que contenga varias CAs raíz.
- Concatenar varias CAs en un único archivo PEM.
- Guardar las CAs por separado en el sistema de archivos y cargarlas según el dominio.
- Usar **pinning** de certificado o huella digital (más seguro pero requiere actualizar si el certificado cambia).

---

## 9) ¿Cómo se obtiene el certificado de un dominio?
Hay varias formas:
- Desde el navegador (inspeccionando el certificado del sitio y exportándolo en formato PEM).  
- Desde consola, usando OpenSSL:
  ```bash
  openssl s_client -showcerts -connect broker.hivemq.com:8883 -servername broker.hivemq.com </dev/null
  ```
- O directamente desde la página de la CA o el servicio que emite los certificados.  

En el ESP32 normalmente se usa solo la **CA raíz** del servidor al que se conecta.

---

## 10) ¿Qué significan la llave pública y privada en TLS?
- La **llave pública** es parte del certificado y se puede compartir. Sirve para cifrar información o verificar firmas digitales.  
- La **llave privada** nunca se comparte y solo la posee el dueño del certificado. Se usa para firmar y demostrar identidad.  
El par de claves (pública y privada) garantiza que la comunicación sea segura y verificable.

---

## 11) ¿Qué ocurre cuando los certificados expiran?
Cuando un certificado vence, el ESP32 no podrá establecer una conexión segura.  
Esto puede deberse a que la CA cambió o el certificado del servidor fue renovado.  
La solución es **actualizar el certificado o el bundle** dentro del firmware.  
Si se usó pinning de certificado, también habrá que actualizar la huella.

---

## 12) Fundamento matemático y computación cuántica
La criptografía moderna se basa en la **teoría de números** (factores primos, logaritmos discretos y curvas elípticas).  
La **computación cuántica** podría romper estos métodos porque:
- El algoritmo de **Shor** puede factorizar grandes números y romper RSA o ECDSA.  
- El algoritmo de **Grover** reduce la seguridad efectiva de los métodos simétricos a la mitad.  

Por eso ya se trabaja en la **criptografía post-cuántica**, que busca sistemas resistentes a computadoras cuánticas.

---

## 13) Evidencia de pruebas
Las pruebas se realizaron en tres etapas con diferentes configuraciones.

1. **.png** — Conexión sin TLS (falla esperada).  
2. **.png** — Conexión TLS sin certificado cargado (falla esperada).  
3. **.png** — Conexión TLS con CA cargada (éxito).  

Cada imagen muestra el resultado en el *Monitor Serie* del ESP32.

---

## 14) Prueba de código (MQTT seguro)

### Paso A — Cambiar a puerto 8883
Se modificó el puerto MQTT a **8883** para usar conexión segura.  
Sin usar `WiFiClientSecure`, la conexión falla porque el broker exige TLS.

### Paso B — Habilitar TLS pero sin CA
Se cambió el cliente a `WiFiClientSecure`, pero sin cargar ningún certificado.  
El ESP32 no puede verificar la identidad del broker y la conexión es rechazada.

### Paso C — TLS con validación completa
Finalmente, se cargó la **CA raíz** del broker con `espClient.setCACert(ROOT_CA_PEM)`.  
En esta etapa la conexión fue exitosa y se logró publicar mensajes en el topic MQTT.

---

## 15) Cambios realizados al sketch

### Archivo `certs.h`
```cpp
#pragma once
#include <WiFiClientSecure.h>

static const char ROOT_CA_PEM[] PROGMEM = R"(-----BEGIN CERTIFICATE-----
xxxxxxxxxxxxxxxxxxxxx
-----END CERTIFICATE-----)";
```

### En `sketch_oct6a.ino`
```cpp
#include <WiFiClientSecure.h>
#include "certs.h"

WiFiClientSecure espClient;
PubSubClient client(espClient);

#define MQTT_PORT 8883

void setup() {
  setup_wifi();
  espClient.setCACert(ROOT_CA_PEM);
  client.setServer(MQTT_SERVER, MQTT_PORT);
}
```

### Obtener la CA raíz
```bash
openssl s_client -showcerts -connect broker.hivemq.com:8883 -servername broker.hivemq.com </dev/null
```
El certificado raíz obtenido se pega en `ROOT_CA_PEM` dentro del archivo `certs.h`.
