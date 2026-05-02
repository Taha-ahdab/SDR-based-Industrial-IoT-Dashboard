#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <DHT.h>

#define NODE_ID 1
#define SEND_INTERVAL_MS 2500

#define DHTPIN 4
#define DHTTYPE DHT22
#define GAS_AO_PIN 34
#define GAS_DO_PIN 18
#define X_PIN 32
#define Y_PIN 35
#define Z_PIN 33

DHT dht(DHTPIN, DHTTYPE);

typedef struct struct_message {
  int id;
  float temperature;
  float humidity;
  int gasAnalog;
  int gasDigital;
  int accelX;
  int accelY;
  int accelZ;
} struct_message;

struct_message myData;

// Broadcast إلى LILYGO المستقبل
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void OnDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  pinMode(GAS_DO_PIN, INPUT);
  pinMode(GAS_AO_PIN, INPUT);
  pinMode(X_PIN, INPUT);
  pinMode(Y_PIN, INPUT);
  pinMode(Z_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // لازم نفس قناة الراوتر/LILYGO
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 6;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.print("ESP32 Node Ready, ID = ");
  Serial.println(NODE_ID);
}

void loop() {
  myData.id = NODE_ID;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    myData.temperature = -999;
    myData.humidity = -999;
    Serial.println("Warning: DHT read failed");
  } else {
    myData.temperature = t;
    myData.humidity = h;
  }

  myData.gasAnalog = analogRead(GAS_AO_PIN);
  myData.gasDigital = digitalRead(GAS_DO_PIN);
  myData.accelX = analogRead(X_PIN);
  myData.accelY = analogRead(Y_PIN);
  myData.accelZ = analogRead(Z_PIN);

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));

  if (result == ESP_OK) {
    Serial.println("=== Data transmitted ===");
    Serial.printf("ID   : %d\n", myData.id);
    Serial.printf("Temp : %.1f\n", myData.temperature);
    Serial.printf("Hum  : %.1f\n", myData.humidity);
    Serial.printf("GasA : %d\n", myData.gasAnalog);
    Serial.printf("GasD : %d\n", myData.gasDigital);
    Serial.printf("X    : %d\n", myData.accelX);
    Serial.printf("Y    : %d\n", myData.accelY);
    Serial.printf("Z    : %d\n", myData.accelZ);
    Serial.println("------------------------");
  } else {
    Serial.print("ESP-NOW send error: ");
    Serial.println(result);
  }

  delay(SEND_INTERVAL_MS);
}
