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

## Paso 5 - Integrar tu propio programa

La plantilla ya se encarga de la conexion Ethernet, TLS, MQTT, heartbeat y publicacion hacia IoTForge. El usuario solo debe agregar su propia logica en las zonas indicadas, sin modificar los bloques base de conexion.

### Bloques requeridos

Estos bloques son necesarios para que la conexion con IoTForge funcione:

- Librerias: `SPI.h`, `Ethernet.h`, `SSLClient.h`, `PubSubClient.h`
- Archivo TLS: `trust_anchors.h`
- Credenciales IoTForge: `DEVICE_ID`, `DEVICE_TOKEN`, `THING_ID`, `VAR_ID`
- Configuracion W5500: pines SPI, CS y RST
- Cliente seguro: `EthernetClient`, `SSLClient`, `PubSubClient`
- Funcion `ethernetInit()`
- Funcion `mqttReconnect()`
- Funcion `publishStatus()`
- Llamada a `mqtt.loop()` dentro de `loop()`
- Llamada a `Ethernet.maintain()` dentro de `loop()`
- Heartbeat cada 30 segundos

No se recomienda cambiar estos bloques al iniciar. Primero verifica que la plantilla conecte correctamente y despues agrega tu logica.

### Donde declarar sensores, pines y variables

Agrega tus pines, variables de sensores, estados de relevadores, contadores o banderas en la parte superior del programa, junto a las constantes.

Ejemplos de lo que va en esa zona:

- Pines de sensores
- Pines de relevadores
- Variables globales de lectura
- Estados que se enviaran a IoTForge
- Intervalos de publicacion propios

### Donde configurar tu hardware

Agrega la configuracion de tus sensores o actuadores dentro de `setup()`, despues de iniciar el puerto serial y antes o despues de `ethernetInit()`, segun lo que necesite tu hardware.

Ejemplos:

- `pinMode()` para entradas o salidas
- Inicializacion de sensores I2C, SPI o UART
- Configuracion de pantallas
- Estado inicial de relevadores o salidas digitales

La configuracion MQTT debe permanecer despues de crear los topics y antes de llamar a `mqttReconnect()`.

### Donde leer sensores o ejecutar tu logica

Agrega la logica principal dentro de `loop()`, despues de estas lineas base:

```cpp
if (!mqtt.connected()) {
  mqttReconnect();
}

mqtt.loop();
Ethernet.maintain();
```

Estas lineas mantienen viva la conexion con IoTForge. La logica del usuario debe ejecutarse despues para evitar bloquear MQTT.

### Donde publicar datos a IoTForge

La funcion que publica el valor de ejemplo es `publishVariable()`. Ahi se debe reemplazar el valor de demostracion por el valor real del sensor o del proceso.

El topic usado para publicar variables es:

```text
iotforge/{THING_ID}/{VAR_ID}
```

Si se agregan mas variables, se recomienda crear un topic por cada variable y una funcion de publicacion para cada una.

### Donde recibir comandos desde IoTForge

Los mensajes recibidos desde IoTForge llegan a:

```cpp
mqttCallback(char* topic, byte* payload, unsigned int len)
```

En esa funcion se puede interpretar el mensaje recibido y ejecutar acciones como:

- Encender o apagar una salida
- Cambiar un setpoint
- Actualizar un modo de operacion
- Mover un actuador
- Guardar un valor recibido

La plantilla ya convierte el payload recibido a texto para facilitar su uso.

### Recomendacion para tiempos

Evita usar `delay()` largos dentro de `loop()`. Para tareas repetitivas usa `millis()`, como lo hace la plantilla con:

```cpp
HEARTBEAT_MS
PUBLISH_MS
```

Esto permite que MQTT siga activo mientras tu programa ejecuta lecturas, publicaciones y control de salidas.

### Resumen de zonas editables

El usuario normalmente modifica estas partes:

- Credenciales IoTForge en los `#define`
- Pines adicionales de su hardware
- Variables globales de sensores o actuadores
- Configuracion de hardware dentro de `setup()`
- Funcion `publishVariable()`
- Funcion `mqttCallback()`
- Logica principal dentro de `loop()`, despues de `mqtt.loop()`

Los bloques de Ethernet, TLS, MQTT y heartbeat deben conservarse como base de comunicacion con IoTForge.

## Paso 6 - Compilar y cargar

1. Selecciona tu placa RP2040 en Arduino IDE
2. Selecciona el puerto COM correcto
3. Compila el sketch
4. Carga el firmware
5. Abre el monitor serial a `115200` baudios

## Paso 7 - Verificar funcionamiento

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
