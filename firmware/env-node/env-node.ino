#include <WiFi.h>
#include "mqtt_client.h"
#include <DHT11.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DHT_PIN 4
#define LDR_PIN 1

#define OLED_SDA 8
#define OLED_SCL 9

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* MQTT_BROKER = "mqtt://192.168.137.1:1883";

const char* DEVICE_ID = "ENV-NODE";

const char* TOPIC_DATA = "safety/env/data";
const char* TOPIC_STATUS = "safety/env/status";
const char* TOPIC_HEARTBEAT = "safety/env/heartbeat";

DHT11 dht11(DHT_PIN);

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

esp_mqtt_client_handle_t mqttClient = NULL;

bool mqttConnected = false;
bool mqttJustConnected = false;

int temperature = 0;
int humidity = 0;
int lightValue = 0;
int selectedQoS = 0;

String environmentStatus = "SAFE";

unsigned long lastSensorRead = 0;
unsigned long lastPublish = 0;
unsigned long lastHeartbeat = 0;

const unsigned long SENSOR_INTERVAL = 2000;
const unsigned long PUBLISH_INTERVAL = 5000;
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


void readSensors() {

  int newTemperature = 0;
  int newHumidity = 0;

  int result =
    dht11.readTemperatureHumidity(
      newTemperature,
      newHumidity
    );

  if (result == 0) {

    temperature = newTemperature;
    humidity = newHumidity;

  } else {

    Serial.print("DHT11 error: ");
    Serial.println(result);
  }

  lightValue = analogRead(LDR_PIN);
}


void determineStatus() {

  if (temperature >= 38) {

    environmentStatus = "DANGER";

  } else if (
    temperature >= 30 ||
    humidity >= 80
  ) {

    environmentStatus = "WARNING";

  } else {

    environmentStatus = "SAFE";
  }
}


int determineQoS() {

  int rssi = WiFi.RSSI();

  if (environmentStatus == "DANGER") {
    return 2;
  }

  if (environmentStatus == "WARNING") {

    if (rssi < -75) {
      return 2;
    }

    return 1;
  }

  if (rssi < -75) {
    return 1;
  }

  return 0;
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

  payload += "\"temperature\":";
  payload += temperature;
  payload += ",";

  payload += "\"humidity\":";
  payload += humidity;
  payload += ",";

  payload += "\"light\":";
  payload += lightValue;
  payload += ",";

  payload += "\"status\":\"";
  payload += environmentStatus;
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


void updateOLED() {

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("ENV-NODE");

  display.setCursor(0, 11);
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");

  display.setCursor(0, 21);
  display.print("Hum : ");
  display.print(humidity);
  display.println(" %");

  display.setCursor(0, 31);
  display.print("Light: ");
  display.println(lightValue);

  display.setCursor(0, 41);
  display.print("Status: ");
  display.println(environmentStatus);

  display.setCursor(0, 51);
  display.print("QOS:");
  display.print(selectedQoS);
  display.print(" ");

  if (mqttConnected) {
    display.print("MQTT OK");
  } else {
    display.print("MQTT OFF");
  }

  display.display();
}


void printSystemStatus() {

  Serial.println();

  Serial.print("Temperature : ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity    : ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Light       : ");
  Serial.println(lightValue);

  Serial.print("Status      : ");
  Serial.println(environmentStatus);

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
  Serial.println("ENV-NODE starting...");

  pinMode(LDR_PIN, INPUT);

  dht11.setDelay(1000);

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      )) {

    Serial.println("OLED init failed!");

  } else {

    display.clearDisplay();

    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.println("ENV-NODE");

    display.setCursor(0, 16);
    display.println("Adaptive MQTT");

    display.setCursor(0, 32);
    display.println("Starting...");

    display.display();
  }

  connectWiFi();

  startMQTT();
}


void loop() {

  /*
     Don't publish from inside the MQTT
     event handler, handle it here.
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


  unsigned long now = millis();


  if (now - lastSensorRead >= SENSOR_INTERVAL) {

    lastSensorRead = now;

    readSensors();

    determineStatus();

    selectedQoS = determineQoS();

    updateOLED();

    printSystemStatus();
  }


  if (now - lastPublish >= PUBLISH_INTERVAL) {

    lastPublish = now;

    publishData();
  }


  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {

    lastHeartbeat = now;

    publishHeartbeat();
  }


  delay(20);
}
