#include <SPI.h>
#include <Ethernet.h>  //Ocupar esta libreria por las demas rompen con TLS aunque TCP sí funcionara.
#include <SSLClient.h>
#include <PubSubClient.h>

#include "trust_anchors.h"

#define IOTF_BROKER "mqtt.iaintegracion.space"
#define IOTF_PORT   8883
#define IOTF_DEVICE_ID    "TU_DEVICE_ID"
#define IOTF_DEVICE_TOKEN "TU_DEVICE_TOKEN"
#define IOTF_THING_ID     "TU_THING_ID"
#define IOTF_VAR_ID       "TU_VAR_ID"

#define W5500_SCK   18
#define W5500_MOSI  19
#define W5500_MISO  16
#define W5500_CS    17
#define W5500_RST   20

#define HEARTBEAT_MS 30000UL
#define PUBLISH_MS   10000UL

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 };

EthernetClient ethClient;
SSLClient sslClient(ethClient, TAs, TAs_NUM, 0); //Seguir esta config por que si no se rompe TLS aunque tcp funcione
PubSubClient mqtt(sslClient);

unsigned long lastHeartbeatMs = 0;
unsigned long lastPublishMs = 0;

float sensorValue = 0.0;

char statusTopic[128];
char varTopic[128];

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

void publishStatus(const char* status) {
  if (!mqtt.connected()) return;
  mqtt.publish(statusTopic, status, false);
}

void publishVariable() {
  if (!mqtt.connected()) return;

  char payload[24];
  dtostrf(sensorValue, 0, 2, payload);

  mqtt.publish(varTopic, payload, true);

  Serial.print("Publicado ");
  Serial.print(varTopic);
  Serial.print(" = ");
  Serial.println(payload);
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  char msg[128];

  if (len >= sizeof(msg)) len = sizeof(msg) - 1;
  memcpy(msg, payload, len);
  msg[len] = '\0';

  Serial.print("RX ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(msg);
}

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

  SPI.setRX(W5500_MISO);
  SPI.setTX(W5500_MOSI);
  SPI.setSCK(W5500_SCK);
  SPI.begin();

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

  if (now - lastPublishMs >= PUBLISH_MS) {
    sensorValue += 1.0;
    publishVariable();
    lastPublishMs = now;
  }
}
