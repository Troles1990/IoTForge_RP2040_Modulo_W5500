# RP2040 + W5500 -> IoTForge

Guia de integracion MQTT Ethernet con TLS usando modulo W5500

## Requisitos

### Hardware

- Placa RP2040
- Modulo Ethernet W5500
- Cable Ethernet con acceso a internet
- Fuente de alimentacion estable para la placa y el modulo

### Software

- Arduino IDE
- Core Arduino para RP2040
- Librerias Arduino:
  - Ethernet
  - SSLClient
  - PubSubClient
- Python 3.12 o superior para generar `trust_anchors.h`
- Paquete Python `cryptography`

## Conexiones

### W5500 -> RP2040

| W5500 | RP2040 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCK | GP18 |
| MOSI | GP19 |
| MISO | GP16 |
| CS | GP17 |
| RST | GP20 |

Usa los mismos pines en el sketch:

```cpp
#define W5500_SCK   18
#define W5500_MOSI  19
#define W5500_MISO  16
#define W5500_CS    17
#define W5500_RST   20
```

## Paso 1 - Crear dispositivo en IoTForge

1. Ingresa a `iotforge.iaintegracion.space`
2. Ve a **Nodo** y crea tu nodo
3. Ve a **Variables** y crea una nueva variable
4. Ve a **Dispositivos** y crea un nuevo dispositivo para RP2040
5. Anota:

```text
DEVICE_ID
DEVICE_TOKEN
THING_ID (NODO)
VARIABLE_ID
```

## Paso 2 - Preparar certificado TLS

IoTForge usa MQTT con TLS en:

```text
mqtt.iaintegracion.space:8883
```

Para `SSLClient` no se usa el certificado PEM directo. Se convierte a formato BearSSL en un archivo:

```text
trust_anchors.h
```

### 2.1 Archivos necesarios

Usa los archivos incluidos en este ejemplo:

```text
generate_trust_anchors.py
iotforge_ca.pem
```

El script genera el archivo:

```text
trust_anchors.h
```

### 2.2 Instalar dependencia Python

En PowerShell:

```powershell
pip install cryptography
```

### 2.3 Generar `trust_anchors.h`

Ejecuta:

```powershell
python .\generate_trust_anchors.py .\iotforge_ca.pem -o .\trust_anchors.h
```

Salida esperada:

```text
Certificado: CN=R13,O=Let's Encrypt,C=US
trust_anchors.h generado OK
DN: 53 bytes
RSA: 2048 bits
e: 65537
```

## Paso 3 - Crear proyecto Arduino

1. Crea un nuevo sketch en Arduino IDE
2. Guarda el proyecto como:

```text
RP2040_IoTForge_W5500
```

3. Copia `trust_anchors.h` dentro de la misma carpeta del `.ino`
4. Instala las librerias:

```text
Ethernet
SSLClient
PubSubClient
```

## Paso 4 - Cargar plantilla `.ino`

Reemplaza el contenido del sketch con el archivo `.ino` incluido en este ejemplo.

Actualiza tus credenciales IoTForge en los `#define`:

```cpp
#define IOTF_DEVICE_ID    "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
#define IOTF_DEVICE_TOKEN "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define IOTF_THING_ID     "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
#define IOTF_VAR_ID       "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
```

## Paso 5 - Compilar y cargar

1. Selecciona tu placa RP2040 en Arduino IDE
2. Selecciona el puerto COM correcto
3. Compila el sketch
4. Carga el firmware
5. Abre el monitor serial a `115200` baudios

## Paso 6 - Verificar funcionamiento

En el monitor serial deberias ver:

```text
Ethernet DHCP... IP: 192.168.1.127
MQTT conectando... OK
Publicado iotforge/THING_ID/VAR_ID = 1.00
Publicado iotforge/THING_ID/VAR_ID = 2.00
Publicado iotforge/THING_ID/VAR_ID = 3.00
```

En el dashboard de IoTForge:

- El dispositivo aparece como `ONLINE`
- La variable recibe datos cada 10 segundos
- El heartbeat se publica cada 30 segundos

## Flujo de datos

```text
RP2040
  |
  | SPI
  v
W5500 Ethernet
  |
  | TLS 8883
  v
mqtt.iaintegracion.space
  |
  v
IoTForge Dashboard
```

## Topics usados

Datos de variable:

```text
iotforge/{THING_ID}/{VAR_ID}
```

Heartbeat:

```text
iotforge/{DEVICE_ID}/status
```

## Notas importantes

- Usar la libreria `Ethernet.h`
- No usar `Ethernet_Generic.h` para esta integracion
- El puerto MQTT TLS es `8883`
- `trust_anchors.h` debe estar en la misma carpeta que el `.ino`
- El heartbeat publica `ONLINE` cada 30 segundos
- La variable de ejemplo publica un valor incremental cada 10 segundos
- Si el W5500 no obtiene IP, revisar cable Ethernet, DHCP y alimentacion

## Diagnostico rapido

### Ethernet no obtiene IP

Revisar:

- Cable Ethernet
- Router con DHCP activo
- Pines SPI
- Pin CS del W5500
- Alimentacion del modulo

### TLS falla

Verificar:

```cpp
#include <Ethernet.h>
```

Tambien revisar que `trust_anchors.h` este generado y ubicado junto al `.ino`.

### MQTT devuelve error `-2`

Revisar:

- Conexion Ethernet
- Broker `mqtt.iaintegracion.space`
- Puerto `8883`
- Certificado convertido en `trust_anchors.h`
- Credenciales del dispositivo

## Links de recursos

- IoTForge: `https://iotforge.iaintegracion.space`
- Let's Encrypt certificados: `https://letsencrypt.org/certificates/`
- SSLClient Arduino: `https://github.com/OPEnSLab-OSU/SSLClient`
- PubSubClient Arduino: `https://github.com/knolleary/pubsubclient`
- Ethernet Arduino: `https://www.arduino.cc/reference/en/libraries/ethernet/`

Guia generada para IoTForge - Ejemplo RP2040 W5500
