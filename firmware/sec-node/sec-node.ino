#include <WiFi.h>
#include "mqtt_client.h"

#define PIR_PIN 27
#define RELAY_PIN 33

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* MQTT_BROKER = "mqtt://192.168.137.1:1883";

const char* DEVICE_ID = "SEC-NODE";

const char* TOPIC_DATA = "safety/security/data";
const char* TOPIC_STATUS = "safety/security/status";
const char* TOPIC_HEARTBEAT = "safety/security/heartbeat";

esp_mqtt_client_handle_t mqttClient = NULL;

bool mqttConnected = false;
bool mqttJustConnected = false;

bool motionDetected = false;
bool previousMotionState = false;

int selectedQoS = 0;

unsigned long lastPublish = 0;
unsigned long lastHeartbeat = 0;

const unsigned long PUBLISH_INTERVAL = 3000;
const unsigned long HEARTBEAT_INTERVAL = 30000;


void mqttEventHandler(
  void* handler_args,
  esp_event_base_t base,
  int32_t event_id,
  void* event_data
) {

  switch (event_id) {

    case MQTT_EVENT_CONNECTED:

      mqttConnected = true;
      mqttJustConnected = true;

      Serial.println();
      Serial.println("MQTT connected.");

      break;


    case MQTT_EVENT_DISCONNECTED:

      mqttConnected = false;

      Serial.println();
      Serial.println("MQTT disconnected.");

      break;


    case MQTT_EVENT_PUBLISHED:

      Serial.print("Message published, ID: ");
      Serial.println(
        ((esp_mqtt_event_handle_t)event_data)->msg_id
      );

      break;


    case MQTT_EVENT_ERROR:

      Serial.println("MQTT error.");

      break;


    default:

      break;
  }
}


void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Connecting to Wi-Fi");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    attempts < 30
  ) {

    delay(500);

    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Wi-Fi connected.");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

  } else {

    Serial.println("Wi-Fi connection failed.");
  }
}


void startMQTT() {

  esp_mqtt_client_config_t mqttConfig = {};

  mqttConfig.broker.address.uri = MQTT_BROKER;

  mqttClient = esp_mqtt_client_init(
    &mqttConfig
  );

  esp_mqtt_client_register_event(
    mqttClient,
    MQTT_EVENT_ANY,
    mqttEventHandler,
    NULL
  );

  esp_mqtt_client_start(
    mqttClient
  );

  Serial.println("MQTT client started.");
}


int determineQoS() {

  /*
     If motion is present, the message is
     important, so use the highest QoS.
  */

  if (motionDetected) {
    return 2;
  }

  return 0;
}


void readMotion() {

  int pirState = digitalRead(PIR_PIN);

  if (pirState == HIGH) {

    motionDetected = true;

  } else {

    motionDetected = false;
  }


  if (motionDetected != previousMotionState) {

    if (motionDetected) {

      Serial.println();
      Serial.println("Motion detected.");

    } else {

      Serial.println();
      Serial.println("Motion cleared.");
    }

    previousMotionState = motionDetected;
  }
}


void controlRelay() {

  /*
     The relay is active low.
     LOW = ON, HIGH = OFF.
  */

  if (motionDetected) {

    digitalWrite(
      RELAY_PIN,
      LOW
    );

  } else {

    digitalWrite(
      RELAY_PIN,
      HIGH
    );
  }
}


void publishStatus() {

  if (!mqttConnected) {
    return;
  }

  String payload = "{";

  payload += "\"device\":\"";
  payload += DEVICE_ID;
  payload += "\",";

  payload += "\"status\":\"ONLINE\"";

  payload += "}";

  esp_mqtt_client_publish(
    mqttClient,
    TOPIC_STATUS,
    payload.c_str(),
    0,
    1,
    0
  );

  Serial.println("Status published.");
}


void publishData() {

  if (!mqttConnected) {
    return;
  }

  selectedQoS = determineQoS();

  String payload = "{";

  payload += "\"device\":\"";
  payload += DEVICE_ID;
  payload += "\",";

  payload += "\"motion\":";

  if (motionDetected) {
    payload += "true";
  } else {
    payload += "false";
  }

  payload += ",";


  payload += "\"alarm\":";

  if (motionDetected) {
    payload += "true";
  } else {
    payload += "false";
  }

  payload += ",";


  payload += "\"relay\":\"";

  if (motionDetected) {
    payload += "ON";
  } else {
    payload += "OFF";
  }

  payload += "\",";


  payload += "\"rssi\":";
  payload += WiFi.RSSI();
  payload += ",";


  payload += "\"qos\":";
  payload += selectedQoS;


  payload += "}";


  int messageId =
    esp_mqtt_client_publish(
      mqttClient,
      TOPIC_DATA,
      payload.c_str(),
      0,
      selectedQoS,
      0
    );


  Serial.println();
  Serial.println("Data published.");

  Serial.print("Topic: ");
  Serial.println(TOPIC_DATA);

  Serial.print("Payload: ");
  Serial.println(payload);

  Serial.print("QoS: ");
  Serial.println(selectedQoS);

  Serial.print("Message ID: ");
  Serial.println(messageId);
}


void publishHeartbeat() {

  if (!mqttConnected) {
    return;
  }

  String payload = "{";

  payload += "\"device\":\"";
  payload += DEVICE_ID;
  payload += "\",";

  payload += "\"status\":\"ONLINE\",";

  payload += "\"rssi\":";
  payload += WiFi.RSSI();

  payload += "}";

  esp_mqtt_client_publish(
    mqttClient,
    TOPIC_HEARTBEAT,
    payload.c_str(),
    0,
    0,
    0
  );
}


void printSystemStatus() {

  Serial.println();

  Serial.print("Motion      : ");

  if (motionDetected) {
    Serial.println("DETECTED");
  } else {
    Serial.println("NONE");
  }


  Serial.print("Relay       : ");

  if (motionDetected) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }


  Serial.print("RSSI        : ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");


  Serial.print("Selected QoS: ");
  Serial.println(selectedQoS);


  Serial.print("MQTT        : ");

  if (mqttConnected) {
    Serial.println("CONNECTED");
  } else {
    Serial.println("DISCONNECTED");
  }
}


void setup() {

  Serial.begin(115200);

  delay(1500);

  Serial.println();
  Serial.println("SEC-NODE starting...");

  pinMode(
    PIR_PIN,
    INPUT
  );

  pinMode(
    RELAY_PIN,
    OUTPUT
  );

  /*
     Relay is active low,
     HIGH keeps it off at boot.
  */

  digitalWrite(
    RELAY_PIN,
    HIGH
  );

  Serial.println("PIR configured.");
  Serial.println("Relay configured (active low).");

  Serial.println();
  Serial.println("Waiting for the PIR to stabilize...");

  delay(30000);

  Serial.println("PIR ready.");


  connectWiFi();

  startMQTT();
}


void loop() {

  /*
     Publish outside the
     MQTT event handler.
  */

  if (mqttJustConnected) {

    mqttJustConnected = false;

    delay(100);

    publishStatus();
  }


  if (WiFi.status() != WL_CONNECTED) {

    connectWiFi();

    delay(500);

    return;
  }


  readMotion();

  controlRelay();


  unsigned long now = millis();


  if (
    now - lastPublish >=
    PUBLISH_INTERVAL
  ) {

    lastPublish = now;

    publishData();

    printSystemStatus();
  }


  if (
    now - lastHeartbeat >=
    HEARTBEAT_INTERVAL
  ) {

    lastHeartbeat = now;

    publishHeartbeat();
  }


  delay(50);
}
