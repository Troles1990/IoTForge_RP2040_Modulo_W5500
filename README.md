# IoTForge Device v2 con RP2040 y W5500

Ejemplo para conectar una placa RP2040 a IoTForge por Ethernet, MQTT con TLS y un módulo W5500.

Esta versión usa la autenticación **Device v2** de IoTForge. El firmware calcula localmente las credenciales MQTT v2; no debes calcular ni pegar el hash manualmente.

## Qué hace el ejemplo

- Obtiene una IP por DHCP.
- Sincroniza la hora por NTP para validar TLS.
- Se conecta a `mqtt.iaintegracion.space:8883`.
- Publica el estado `ONLINE` cada 30 segundos.
- Publica un valor de ejemplo cada 10 segundos.
- Se suscribe al topic de la variable para recibir comandos.

## Material necesario

### Hardware

- Placa RP2040.
- Módulo Ethernet W5500.
- Cable Ethernet con salida a internet.
- Alimentación estable de 3.3 V para el W5500.

### Software

- Arduino IDE.
- Core Arduino para RP2040.
- Librerías Arduino:
  - `Ethernet`
  - `NTPClient` de Fabrice Weinberg
  - `SSLClient`
  - `PubSubClient`

Los archivos `iotforge_ca.pem` y `trust_anchors.h` ya están incluidos.

## Conexión W5500 a RP2040

| W5500 | RP2040 |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SCK | GP18 |
| MOSI | GP19 |
| MISO | GP16 |
| CS | GP17 |
| RST | GP20 |

Si tu placa usa otros pines, cambia únicamente los valores `W5500_*` del sketch.

## Configuración rápida

### 1. Crea los recursos en IoTForge

En [IoTForge](https://iotforge.iaintegracion.space) crea:

1. Un nodo o Thing.
2. Una variable.
3. Un dispositivo para la RP2040.

Guarda estos cuatro datos:

- Device ID.
- Device token original.
- Thing ID.
- Variable ID.

### 2. Abre el sketch

Coloca estos archivos en la misma carpeta de proyecto de Arduino:

```text
iotforge_rp2040_w5500.ino
trust_anchors.h
```

### 3. Agrega tus datos

Edita solamente estos marcadores en `iotforge_rp2040_w5500.ino`:

```cpp
#define IOTF_DEVICE_ID    "TU_DEVICE_ID"
#define IOTF_DEVICE_TOKEN "TU_DEVICE_TOKEN"
#define IOTF_THING_ID     "TU_THING_ID"
#define IOTF_VAR_ID       "TU_VARIABLE_ID"
```

No agregues comillas extra, espacios al inicio o al final, ni el sufijo `_v2` al Device ID. El firmware lo agrega automáticamente.

### 4. Compila y carga

1. Selecciona tu placa RP2040 y el método de carga correcto.
2. Compila el sketch.
3. Carga el firmware.
4. Abre el monitor serial a `115200` baudios.

Una conexión correcta muestra algo parecido a esto:

```text
=== IOTFORGE RP2040 W5500 - DEVICE V2 ===
SHA256 self-test: OK
Ethernet DHCP... IP: 192.168.1.94
NTP sincronizado. Epoch: ...
MQTT conectando... OK
Publicado iotforge/THING_ID/VARIABLE_ID = 1.00
```

La IP es asignada por tu router y puede ser diferente.

## Cómo funciona Device v2

El token que muestra IoTForge sigue siendo el valor que debes guardar en `IOTF_DEVICE_TOKEN`. Antes de abrir la sesión MQTT, el firmware deriva:

```text
usuario MQTT    = DEVICE_ID + "_v2"
contraseña MQTT = SHA-256(DEVICE_TOKEN) en hexadecimal minúscula
```

El token original no se imprime y no se envía como contraseña MQTT. La contraseña derivada tampoco se imprime.

La línea `SHA256 self-test: OK` confirma que la función SHA-256 del firmware produce el resultado esperado. Si marca `FAIL`, el programa se detiene para no intentar una autenticación inválida.

## Agregar sensores o actuadores

El sketch marca las zonas que puedes modificar como `ZONA DEL USUARIO`.

- Declara pines y variables cerca de `sensorValue`.
- Inicializa sensores y salidas dentro de `setup()`.
- Reemplaza el valor de ejemplo en `publishVariable()`.
- Procesa comandos recibidos en `mqttCallback()`.
- Conserva `mqtt.loop()` y `Ethernet.maintain()` dentro de `loop()`.

Evita `delay()` largos. Para tareas periódicas usa `millis()` para que MQTT pueda mantener la conexión.

## Topics utilizados

| Función | Topic |
|---|---|
| Publicar y recibir variable | `iotforge/{THING_ID}/{VARIABLE_ID}` |
| Estado del dispositivo | `iotforge/{DEVICE_ID}/status` |

## Diagnóstico rápido

| Mensaje o síntoma | Qué revisar |
|---|---|
| No aparece `IP:` | Cable, DHCP del router, alimentación y pines SPI/CS/RST. |
| NTP no sincroniza | Salida a internet, DNS y UDP puerto 123. |
| Falla TLS | Hora NTP, `trust_anchors.h`, broker y puerto `8883`. |
| `SHA256 self-test: FAIL` | No uses ese binario; vuelve a descargar y compilar el sketch. |
| MQTT estado `4` | Device ID o token incorrectos. Copia nuevamente los valores desde IoTForge. |
| MQTT estado `5` | El dispositivo no está autorizado, fue eliminado o sus credenciales v2 no están activas. |
| Tiene IP pero no aparece online | Verifica primero NTP/TLS y después la línea `MQTT conectando...`. Tener DHCP no confirma una sesión MQTT. |

Para el nombre legible del error, el monitor serial muestra el estado numérico y su descripción, por ejemplo `5 (MQTT_UNAUTHORIZED)`.

## Migración desde la versión anterior

La versión antigua conectaba MQTT con:

```text
usuario = DEVICE_ID
password = DEVICE_TOKEN original
```

Ese esquema ya no corresponde a Device v2. Actualiza el sketch completo; cambiar solamente el README o agregar `_v2` al Device ID no basta porque la contraseña también debe ser el hash SHA-256 correcto.

## Seguridad

- No subas a GitHub un sketch que contenga IDs o tokens reales.
- No publiques capturas del monitor serial con datos de producción.
- Si un token quedó expuesto, revócalo o regénéralo en IoTForge antes de volver a usar el dispositivo.
- Para proyectos públicos conserva los marcadores `TU_DEVICE_ID`, `TU_DEVICE_TOKEN`, `TU_THING_ID` y `TU_VARIABLE_ID`.

## Regenerar el certificado TLS

Normalmente no es necesario porque `trust_anchors.h` ya está incluido. Si cambia la autoridad certificadora, instala Python y `cryptography`, después ejecuta:

```powershell
pip install cryptography
python .\generate_trust_anchors.py .\iotforge_ca.pem -o .\trust_anchors.h
```

## Archivos del repositorio

| Archivo | Uso |
|---|---|
| `iotforge_rp2040_w5500.ino` | Firmware de ejemplo compatible con Device v2. |
| `trust_anchors.h` | Certificado en formato usado por SSLClient. |
| `iotforge_ca.pem` | Certificado fuente. |
| `generate_trust_anchors.py` | Regenera `trust_anchors.h` desde el PEM. |

## Recursos

- [IoTForge](https://iotforge.iaintegracion.space)
- [SSLClient](https://github.com/OPEnSLab-OSU/SSLClient)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [Ethernet para Arduino](https://www.arduino.cc/reference/en/libraries/ethernet/)
