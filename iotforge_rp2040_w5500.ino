/*
  RP2040 + W5500 -> IoTForge
  Plantilla base MQTT TLS.

  El usuario puede agregar su logica en las zonas marcadas como:
  "ZONA DEL USUARIO".

  Los bloques marcados como:
  "BLOQUE IOTFORGE - NO MOVER"
  son necesarios para mantener Ethernet, TLS, MQTT y heartbeat funcionando.
*/

// ============================================================
// BLOQUE IOTFORGE - LIBRERIAS REQUERIDAS - NO MOVER
// ============================================================

#include <SPI.h>
#include <Ethernet.h>   // Usar Ethernet.h. Ethernet_Generic.h abre TCP, pero puede romper TLS con SSLClient.
#include <SSLClient.h>
#include <PubSubClient.h>

#include "trust_anchors.h"  // Archivo generado con generate_trust_anchors.py

// ============================================================
// BLOQUE IOTFORGE - CREDENCIALES Y BROKER
// El usuario solo debe reemplazar los valores entre comillas.
// ============================================================

#define IOTF_BROKER       "mqtt.iaintegracion.space"
#define IOTF_PORT         8883
#define IOTF_DEVICE_ID    "TU_DEVICE_ID"
#define IOTF_DEVICE_TOKEN "TU_DEVICE_TOKEN"
#define IOTF_THING_ID     "TU_THING_ID"
#define IOTF_VAR_ID       "TU_VAR_ID"

// ============================================================
// BLOQUE IOTFORGE - PINES W5500
// Cambiar solo si el modulo W5500 esta conectado a otros pines.
// ============================================================

#define W5500_SCK   18
#define W5500_MOSI  19
#define W5500_MISO  16
#define W5500_CS    17
#define W5500_RST   20

// ============================================================
// BLOQUE IOTFORGE - INTERVALOS BASE
// HEARTBEAT_MS mantiene el dispositivo ONLINE en IoTForge.
// PUBLISH_MS controla cada cuanto se publica la variable de ejemplo.
// ============================================================

#define HEARTBEAT_MS 30000UL
#define PUBLISH_MS   10000UL

// ============================================================
// ZONA DEL USUARIO - PINES, VARIABLES Y CONFIGURACION PROPIA
// Agrega aqui sensores, relevadores, entradas, salidas o estados.
// Ejemplos:
//   #define SENSOR_PIN 26
//   #define RELAY_PIN  15
//   float temperatura = 0.0;
// ============================================================

float sensorValue = 0.0;  // Variable de ejemplo. Reemplazar por el valor real del proyecto.

// ============================================================
// BLOQUE IOTFORGE - CLIENTES DE RED - NO MOVER
// Esta configuracion fue validada para RP2040 + W5500 + TLS.
// ============================================================

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 };

EthernetClient ethClient;
SSLClient sslClient(ethClient, TAs, TAs_NUM, 0);  // Mantener TAs, TAs_NUM y 0 para esta plantilla.
PubSubClient mqtt(sslClient);

unsigned long lastHeartbeatMs = 0;
unsigned long lastPublishMs = 0;

char statusTopic[128];
char varTopic[128];

// ============================================================
// BLOQUE IOTFORGE - INICIO ETHERNET - NO MOVER
// ============================================================

void ethernetInit() {
  Ethernet.init(W5500_CS);

  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, LOW);
  delay(100);
  digitalWrite(W5500_RST, HIGH);
  delay(200);

  Serial.print("Ethernet DHCP...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println(" FALLO");
    while (true) delay(1000);
  }

  Serial.print(" IP: ");
  Serial.println(Ethernet.localIP());
}

// ============================================================
// BLOQUE IOTFORGE - HEARTBEAT - NO MOVER
// Publica ONLINE en iotforge/{DEVICE_ID}/status.
// ============================================================

void publishStatus(const char* status) {
  if (!mqtt.connected()) return;
  mqtt.publish(statusTopic, status, false);
}

// ============================================================
// ZONA DEL USUARIO - PUBLICAR DATOS
// Reemplaza el contenido de esta funcion por tu lectura real.
// Mantener mqtt.publish(varTopic, payload, true) para enviar a IoTForge.
// ============================================================

void publishVariable() {
  if (!mqtt.connected()) return;

  // ZONA DEL USUARIO:
  // Lee aqui tu sensor o calcula el valor que quieres enviar.
  // Ejemplo: sensorValue = analogRead(SENSOR_PIN);

  char payload[24];
  dtostrf(sensorValue, 0, 2, payload);

  mqtt.publish(varTopic, payload, true);

  Serial.print("Publicado ");
  Serial.print(varTopic);
  Serial.print(" = ");
  Serial.println(payload);
}

// ============================================================
// ZONA DEL USUARIO - RECIBIR COMANDOS DESDE IOTFORGE
// Aqui llegan mensajes publicados hacia el topic de la variable.
// Usa msg para encender salidas, cambiar setpoints o controlar tu logica.
// ============================================================

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  char msg[128];

  if (len >= sizeof(msg)) len = sizeof(msg) - 1;
  memcpy(msg, payload, len);
  msg[len] = '\0';

  Serial.print("RX ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(msg);

  // ZONA DEL USUARIO:
  // Agrega aqui la accion cuando IoTForge envie un valor.
  // Ejemplo:
  //   if (strcmp(msg, "1") == 0) digitalWrite(RELAY_PIN, HIGH);
  //   if (strcmp(msg, "0") == 0) digitalWrite(RELAY_PIN, LOW);
}

// ============================================================
// BLOQUE IOTFORGE - RECONEXION MQTT - NO MOVER
// ============================================================

void mqttReconnect() {
  while (!mqtt.connected()) {
    Serial.print("MQTT conectando...");

    String clientId = String(IOTF_DEVICE_ID) + "-iotf";

    if (mqtt.connect(clientId.c_str(), IOTF_DEVICE_ID, IOTF_DEVICE_TOKEN)) {
      Serial.println(" OK");

      publishStatus("ONLINE");
      mqtt.subscribe(varTopic);

      lastHeartbeatMs = millis();
      lastPublishMs = millis();
    } else {
      Serial.print(" Error MQTT state: ");
      Serial.println(mqtt.state());

      mqtt.disconnect();
      sslClient.stop();
      ethClient.stop();

      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  // ==========================================================
  // BLOQUE IOTFORGE - INICIO SPI W5500 - NO MOVER
  // ==========================================================

  SPI.setRX(W5500_MISO);
  SPI.setTX(W5500_MOSI);
  SPI.setSCK(W5500_SCK);
  SPI.begin();

  // ==========================================================
  // ZONA DEL USUARIO - CONFIGURACION DE HARDWARE
  // Agrega aqui pinMode(), sensores, pantallas o actuadores.
  // Evita delays largos.
  // ==========================================================

  // Ejemplo:
  // pinMode(RELAY_PIN, OUTPUT);
  // digitalWrite(RELAY_PIN, LOW);

  // ==========================================================
  // BLOQUE IOTFORGE - INICIO RED Y MQTT - NO MOVER
  // ==========================================================

  ethernetInit();

  snprintf(statusTopic, sizeof(statusTopic), "iotforge/%s/status", IOTF_DEVICE_ID);
  snprintf(varTopic, sizeof(varTopic), "iotforge/%s/%s", IOTF_THING_ID, IOTF_VAR_ID);

  mqtt.setServer(IOTF_BROKER, IOTF_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(1024);
  mqtt.setSocketTimeout(30);
  mqtt.setKeepAlive(30);

  mqttReconnect();
}

void loop() {
  // ==========================================================
  // BLOQUE IOTFORGE - MANTENER CONEXION - NO MOVER
  // Estas lineas deben ejecutarse siempre y rapido.
  // ==========================================================

  if (!mqtt.connected()) {
    mqttReconnect();
  }

  mqtt.loop();
  Ethernet.maintain();

  unsigned long now = millis();

  if (now - lastHeartbeatMs >= HEARTBEAT_MS) {
    publishStatus("ONLINE");
    lastHeartbeatMs = now;
  }

  // ==========================================================
  // ZONA DEL USUARIO - LOGICA PRINCIPAL
  // Agrega aqui lecturas, control, calculos o estados.
  // Usa millis() para temporizadores y evita delays largos.
  // ==========================================================

  // Ejemplo:
  // int raw = analogRead(SENSOR_PIN);
  // sensorValue = raw;

  // ==========================================================
  // BLOQUE IOTFORGE - PUBLICACION PERIODICA
  // Puedes cambiar PUBLISH_MS o reemplazar publishVariable().
  // ==========================================================

  if (now - lastPublishMs >= PUBLISH_MS) {
    sensorValue += 1.0;  // Dato de ejemplo. Reemplazar por la variable real.
    publishVariable();
    lastPublishMs = now;
  }
}
