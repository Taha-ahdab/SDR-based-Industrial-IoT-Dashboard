#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <PubSubClient.h>

const char* ssid = "TP-Link_334C";
const char* password = "14329746";

const char* mqtt_server = "192.168.0.101";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

typedef struct sensor_message {
  int id;
  float temperature;
  float humidity;
  int gasAnalog;
  int gasDigital;
  int accelX;
  int accelY;
  int accelZ;
} sensor_message;

typedef struct motor_message {
  int nodeType;        
  int id;              
  float temperature;
  float humidity;
  int accelX;
  int accelY;
  int accelZ;
  float vibration;
  int motorSpeed;
  int motorEnabled;
} motor_message;

typedef struct motor_command {
  int commandType;     
  int value;           
} motor_command;

sensor_message receivedSensor;
motor_message receivedMotor;

volatile bool newSensorData = false;
volatile bool newMotorData = false;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("LILYGO IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed!");
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
  }
}

void sendMotorCommand(int commandType, int value) {
  motor_command cmd;
  cmd.commandType = commandType;
  cmd.value = value;

  esp_err_t result = esp_now_send(
    broadcastAddress,
    (uint8_t*)&cmd,
    sizeof(cmd)
  );

  if (result == ESP_OK) {
    Serial.print("Motor command sent: ");
    Serial.print(commandType);
    Serial.print(" value: ");
    Serial.println(value);
  } else {
    Serial.print("Motor command send failed: ");
    Serial.println(result);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.trim();

  if (String(topic) == "project/motor/control/set") {
    if (message == "1" || message == "ON" || message == "true") {
      sendMotorCommand(1, 1);
    } else {
      sendMotorCommand(1, 0);
    }
  }

  if (String(topic) == "project/motor/speed/set") {
    int speedValue = message.toInt();
    speedValue = constrain(speedValue, 0, 255);
    sendMotorCommand(2, speedValue);
  }
}

void connectMQTT() {
  while (!client.connected()) {
    String clientId = "LILYGO_GATEWAY_";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (client.connect(clientId.c_str())) {
      client.subscribe("project/motor/control/set");
      client.subscribe("project/motor/speed/set");
      client.publish("project/gateway/status", "LILYGO Gateway Connected", true);
    } else {
      delay(2000);
    }
  }
}

// ✅ تم التعديل هنا (Node 1 فقط)
void publishSensorData(const sensor_message& d) {
  if (d.id != 1) return;

  String base = "project/node1/";

  client.publish((base + "temp").c_str(), String(d.temperature, 1).c_str(), true);
  client.publish((base + "hum").c_str(), String(d.humidity, 1).c_str(), true);
  client.publish((base + "gasA").c_str(), String(d.gasAnalog).c_str(), true);
  client.publish((base + "gasD").c_str(), String(d.gasDigital).c_str(), true);
  client.publish((base + "x").c_str(), String(d.accelX).c_str(), true);
  client.publish((base + "y").c_str(), String(d.accelY).c_str(), true);
  client.publish((base + "z").c_str(), String(d.accelZ).c_str(), true);
}

void publishMotorData(const motor_message& d) {
  client.publish("project/motor/temp", String(d.temperature, 1).c_str(), true);
  client.publish("project/motor/hum", String(d.humidity, 1).c_str(), true);
  client.publish("project/motor/accelX", String(d.accelX).c_str(), true);
  client.publish("project/motor/accelY", String(d.accelY).c_str(), true);
  client.publish("project/motor/accelZ", String(d.accelZ).c_str(), true);
  client.publish("project/motor/vibration", String(d.vibration, 1).c_str(), true);
  client.publish("project/motor/speed", String(d.motorSpeed).c_str(), true);

  if (d.motorEnabled == 1)
    client.publish("project/motor/status", "Motor Running", true);
  else
    client.publish("project/motor/status", "Motor Stopped", true);
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len == sizeof(sensor_message)) {
    memcpy(&receivedSensor, incomingData, sizeof(receivedSensor));
    newSensorData = true;
  }
  else if (len == sizeof(motor_message)) {
    memcpy(&receivedMotor, incomingData, sizeof(receivedMotor));
    newMotorData = true;
  }
}

void setupEspNow() {
  esp_now_init();
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  esp_now_add_peer(&peerInfo);
}

void setup() {
  Serial.begin(115200);
  connectWiFi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  if (WiFi.status() == WL_CONNECTED) {
    connectMQTT();
  }

  setupEspNow();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!client.connected()) connectMQTT();

  client.loop();

  if (newSensorData) {
    newSensorData = false;
    publishSensorData(receivedSensor);
  }

  if (newMotorData) {
    newMotorData = false;
    publishMotorData(receivedMotor);
  }
}
