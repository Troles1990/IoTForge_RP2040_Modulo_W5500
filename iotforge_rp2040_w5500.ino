/*
  RP2040 + W5500 -> IoTForge
  Plantilla base MQTT TLS con NTP (BearSSL Root X1).

  El usuario puede agregar su logica en las zonas marcadas como:
  "ZONA DEL USUARIO".

  Los bloques marcados como:
  "BLOQUE IOTFORGE - NO MOVER"
  son necesarios para mantener Ethernet, TLS, NTP, MQTT y heartbeat funcionando.
*/

// ============================================================
// BLOQUE IOTFORGE - LIBRERIAS REQUERIDAS - NO MOVER
// ============================================================

#include <SPI.h>
#include <Ethernet.h>     // Usar Ethernet.h. Ethernet_Generic.h abre TCP, pero puede romper TLS con SSLClient.
#include <EthernetUdp.h>  // Requerido para NTP
#include <NTPClient.h>    // Instalar: "NTPClient" de Fabrice Weinberg
#include <SSLClient.h>
#include <PubSubClient.h>
#include <string.h>

#include "trust_anchors.h"  // Archivo generado con generate_trust_anchors.py (ISRG Root X1)

// ============================================================
// BLOQUE IOTFORGE - CREDENCIALES Y BROKER
// El usuario solo debe reemplazar los valores entre comillas.
// ============================================================

#define IOTF_BROKER       "mqtt.iaintegracion.space"
#define IOTF_PORT         8883
#define IOTF_DEVICE_ID    "TU_DEVICE_ID"
#define IOTF_DEVICE_TOKEN "TU_DEVICE_TOKEN"
#define IOTF_THING_ID     "TU_THING_ID"
#define IOTF_VAR_ID       "TU_VARIABLE_ID"

/*
  Autenticacion IoTForge Device v2:
    usuario  = DEVICE_ID + "_v2"
    password = SHA-256(DEVICE_TOKEN), hexadecimal en minusculas

  El token original solo se usa localmente para derivar la contrasena en RAM.
  Nunca se imprime ni se envia como contrasena MQTT.
*/

struct IotfSha256Context {
  uint8_t data[64];
  uint32_t datalen;
  uint64_t bitlen;
  uint32_t state[8];
};

static const uint32_t iotfSha256K[64] = {
  0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
  0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
  0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
  0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
  0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
  0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
  0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
  0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
  0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
  0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
  0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
  0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
  0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
  0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
  0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
  0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

static uint32_t iotfSha256Rotr(uint32_t value, uint32_t count) {
  return (value >> count) | (value << (32U - count));
}

static void iotfSha256Transform(IotfSha256Context* ctx, const uint8_t data[64]) {
  uint32_t m[64];
  for (uint32_t i = 0; i < 16U; ++i) {
    const uint32_t j = i * 4U;
    m[i] = (static_cast<uint32_t>(data[j]) << 24U)
         | (static_cast<uint32_t>(data[j + 1U]) << 16U)
         | (static_cast<uint32_t>(data[j + 2U]) << 8U)
         | static_cast<uint32_t>(data[j + 3U]);
  }

  for (uint32_t i = 16U; i < 64U; ++i) {
    const uint32_t s0 = iotfSha256Rotr(m[i - 15U], 7U)
                      ^ iotfSha256Rotr(m[i - 15U], 18U)
                      ^ (m[i - 15U] >> 3U);
    const uint32_t s1 = iotfSha256Rotr(m[i - 2U], 17U)
                      ^ iotfSha256Rotr(m[i - 2U], 19U)
                      ^ (m[i - 2U] >> 10U);
    m[i] = m[i - 16U] + s0 + m[i - 7U] + s1;
  }

  uint32_t a = ctx->state[0];
  uint32_t b = ctx->state[1];
  uint32_t c = ctx->state[2];
  uint32_t d = ctx->state[3];
  uint32_t e = ctx->state[4];
  uint32_t f = ctx->state[5];
  uint32_t g = ctx->state[6];
  uint32_t h = ctx->state[7];

  for (uint32_t i = 0; i < 64U; ++i) {
    const uint32_t s1 = iotfSha256Rotr(e, 6U)
                      ^ iotfSha256Rotr(e, 11U)
                      ^ iotfSha256Rotr(e, 25U);
    const uint32_t ch = (e & f) ^ ((~e) & g);
    const uint32_t t1 = h + s1 + ch + iotfSha256K[i] + m[i];
    const uint32_t s0 = iotfSha256Rotr(a, 2U)
                      ^ iotfSha256Rotr(a, 13U)
                      ^ iotfSha256Rotr(a, 22U);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = s0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

static void iotfSha256Init(IotfSha256Context* ctx) {
  ctx->datalen = 0;
  ctx->bitlen = 0;
  ctx->state[0] = 0x6a09e667UL;
  ctx->state[1] = 0xbb67ae85UL;
  ctx->state[2] = 0x3c6ef372UL;
  ctx->state[3] = 0xa54ff53aUL;
  ctx->state[4] = 0x510e527fUL;
  ctx->state[5] = 0x9b05688cUL;
  ctx->state[6] = 0x1f83d9abUL;
  ctx->state[7] = 0x5be0cd19UL;
}

static void iotfSha256Update(IotfSha256Context* ctx, const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    ctx->data[ctx->datalen++] = data[i];
    if (ctx->datalen == 64U) {
      iotfSha256Transform(ctx, ctx->data);
      ctx->bitlen += 512ULL;
      ctx->datalen = 0;
    }
  }
}

static void iotfSha256Final(IotfSha256Context* ctx, uint8_t hash[32]) {
  uint32_t i = ctx->datalen;

  if (ctx->datalen < 56U) {
    ctx->data[i++] = 0x80U;
    while (i < 56U) ctx->data[i++] = 0U;
  } else {
    ctx->data[i++] = 0x80U;
    while (i < 64U) ctx->data[i++] = 0U;
    iotfSha256Transform(ctx, ctx->data);
    memset(ctx->data, 0, 56U);
  }

  ctx->bitlen += static_cast<uint64_t>(ctx->datalen) * 8ULL;
  ctx->data[63] = static_cast<uint8_t>(ctx->bitlen);
  ctx->data[62] = static_cast<uint8_t>(ctx->bitlen >> 8U);
  ctx->data[61] = static_cast<uint8_t>(ctx->bitlen >> 16U);
  ctx->data[60] = static_cast<uint8_t>(ctx->bitlen >> 24U);
  ctx->data[59] = static_cast<uint8_t>(ctx->bitlen >> 32U);
  ctx->data[58] = static_cast<uint8_t>(ctx->bitlen >> 40U);
  ctx->data[57] = static_cast<uint8_t>(ctx->bitlen >> 48U);
  ctx->data[56] = static_cast<uint8_t>(ctx->bitlen >> 56U);
  iotfSha256Transform(ctx, ctx->data);

  for (uint32_t word = 0; word < 8U; ++word) {
    hash[word * 4U]     = static_cast<uint8_t>(ctx->state[word] >> 24U);
    hash[word * 4U + 1] = static_cast<uint8_t>(ctx->state[word] >> 16U);
    hash[word * 4U + 2] = static_cast<uint8_t>(ctx->state[word] >> 8U);
    hash[word * 4U + 3] = static_cast<uint8_t>(ctx->state[word]);
  }
}

static void iotfSha256Hex(const char* input, char output[65]) {
  static const char hex[] = "0123456789abcdef";
  IotfSha256Context ctx;
  uint8_t digest[32];
  iotfSha256Init(&ctx);
  iotfSha256Update(&ctx, reinterpret_cast<const uint8_t*>(input), strlen(input));
  iotfSha256Final(&ctx, digest);

  for (uint32_t i = 0; i < 32U; ++i) {
    output[i * 2U]     = hex[digest[i] >> 4U];
    output[i * 2U + 1] = hex[digest[i] & 0x0FU];
  }
  output[64] = '\0';
}

char mqttUsername[64];
char mqttPassword[65];

static void buildMqttV2Credentials() {
  snprintf(mqttUsername, sizeof(mqttUsername), "%s_v2", IOTF_DEVICE_ID);
  iotfSha256Hex(IOTF_DEVICE_TOKEN, mqttPassword);
}

static bool iotfSha256SelfTest() {
  char output[65];
  iotfSha256Hex("abc", output);
  return strcmp(output, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0;
}

static const char* mqttStateName(int state) {
  switch (state) {
    case -4: return "MQTT_CONNECT_TIMEOUT";
    case -3: return "MQTT_CONNECTION_LOST";
    case -2: return "MQTT_CONNECT_FAILED";
    case -1: return "MQTT_DISCONNECTED";
    case 0:  return "MQTT_CONNECTED";
    case 1:  return "MQTT_BAD_PROTOCOL";
    case 2:  return "MQTT_BAD_CLIENT_ID";
    case 3:  return "MQTT_UNAVAILABLE";
    case 4:  return "MQTT_BAD_CREDENTIALS";
    case 5:  return "MQTT_UNAUTHORIZED";
    default: return "MQTT_UNKNOWN";
  }
}

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
unsigned long lastPublishMs   = 0;

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
// BLOQUE IOTFORGE - SINCRONIZACION NTP - NO MOVER
// BearSSL (SSLClient) requiere tiempo real para validar
// el certificado ISRG Root X1. Sin NTP el TLS falla.
// ============================================================

void ntpSync() {
  EthernetUDP udp;
  NTPClient timeClient(udp, "pool.ntp.org", 0, 60000);
  Serial.print("NTP...");
  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
    delay(500);
  }
  uint32_t epoch = timeClient.getEpochTime();
  // Convertir epoch Unix a dias/segundos BearSSL (dias desde año 0)
  sslClient.setVerificationTime(epoch / 86400 + 719528, epoch % 86400);
  Serial.println(" OK");
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

    if (mqtt.connect(clientId.c_str(), mqttUsername, mqttPassword)) {
      Serial.println(" OK");

      publishStatus("ONLINE");
      mqtt.subscribe(varTopic);

      lastHeartbeatMs = millis();
      lastPublishMs   = millis();
    } else {
      int mqttState = mqtt.state();
      Serial.print(" Error MQTT state: ");
      Serial.print(mqttState);
      Serial.print(" (");
      Serial.print(mqttStateName(mqttState));
      Serial.println(")");

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

  Serial.println();
  Serial.println("=== IOTFORGE RP2040 W5500 - DEVICE V2 ===");
  Serial.print("SHA256 self-test: ");
  if (!iotfSha256SelfTest()) {
    Serial.println("FAIL");
    Serial.println("Firmware detenido: la derivacion SHA-256 no es confiable.");
    while (true) delay(1000);
  }
  Serial.println("OK");

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
  // BLOQUE IOTFORGE - INICIO RED, NTP Y MQTT - NO MOVER
  // ==========================================================

  ethernetInit();
  ntpSync();  // Sincronizar tiempo antes de TLS

  snprintf(statusTopic, sizeof(statusTopic), "iotforge/%s/status", IOTF_DEVICE_ID);
  snprintf(varTopic,    sizeof(varTopic),    "iotforge/%s/%s",     IOTF_THING_ID, IOTF_VAR_ID);
  buildMqttV2Credentials();

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
