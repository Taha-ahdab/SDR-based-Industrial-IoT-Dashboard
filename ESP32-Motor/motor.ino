#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <DHT.h>
#include <MPU6050.h>

#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

MPU6050 mpu;
bool mpuOK = false;

#define RPWM 26
#define LPWM 27

const int pwmFreq = 5000;
const int pwmResolution = 8;

int motorSpeed = 0;
bool motorEnabled = false;

#define WIFI_CHANNEL 6

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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

motor_message motorData;

void motorStop() {
  ledcWrite(RPWM, 0);
  ledcWrite(LPWM, 0);
}

void motorForward(int speedValue) {
  speedValue = constrain(speedValue, 0, 255);
  ledcWrite(RPWM, speedValue);
  ledcWrite(LPWM, 0);
}

float calculateVibration(int16_t ax, int16_t ay, int16_t az) {
  float ax_g = ax / 16384.0;
  float ay_g = ay / 16384.0;
  float az_g = az / 16384.0;

  float magnitude = sqrt(ax_g * ax_g + ay_g *ay_g + az_g * az_g);
  float vibration = fabs (magnitude - 1.0);
  return vibration;
}

void applyMotorState() {
  if (motorEnabled && motorSpeed > 0) {
    motorForward(motorSpeed);
  } else {
    motorStop();
  }
}

void OnDataSent(const wifi_tx_info_t*tx_info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(motor_command)) {
    Serial.print("Unknown command size: ");
    Serial.println(len);
    return;
  }

  motor_command cmd;
  memcpy(&cmd, incomingData, sizeof(cmd));

  Serial.println("===== COMMAND RECEIVED FROM LILYGO =====");
  Serial.print("Command Type: ");
  Serial.println(cmd.commandType);
  Serial.print("Value: ");
  Serial.println(cmd.value);

  if (cmd.commandType == 1) {
    if (cmd.value == 1) {
      motorEnabled = true;
    } else {
      motorEnabled = false;
    }

    applyMotorState();
  }

  if (cmd.commandType == 2) {
    motorSpeed = constrain(cmd.value, 0, 255);
    Serial.print("Speed received: ");
    Serial.println(motorSpeed);

    applyMotorState();
  }
}

void sendMotorData() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp)) temp = -999;
  if (isnan(hum)) hum = -999;

  int16_t ax = 0, ay = 0, az = 0;
  float vibration = 0;

  if (mpuOK) {
    mpu.getAcceleration(&ax, &ay, &az);
    vibration = calculateVibration(ax, ay, az);
  } else {
    ax = -999;
    ay = -999;
    az = -999;
    vibration = -999;
  }

  motorData.nodeType = 2;
  motorData.id = 3;
  motorData.temperature = temp;
  motorData.humidity = hum;
  motorData.accelX = ax;
  motorData.accelY = ay;
  motorData.accelZ = az;
  motorData.vibration = vibration;
  motorData.motorSpeed = motorSpeed;
  motorData.motorEnabled = motorEnabled ? 1 : 0;

  esp_err_t result = esp_now_send(
    broadcastAddress,
    (uint8_t *)&motorData,
    sizeof(motorData)
  );

  if (result == ESP_OK) {
    Serial.println("Motor data sent to LILYGO");
  } else {
    Serial.print("Send error: ");
    Serial.println(result);
  }

  Serial.println("===== MOTOR NODE =====");
  Serial.print("Temp: "); Serial.println(temp);
  Serial.print("Hum: "); Serial.println(hum);
  Serial.print("X: "); Serial.println(ax);
  Serial.print("Y: "); Serial.println(ay);
  Serial.print("Z: "); Serial.println(az);
  Serial.print("Vibration: "); Serial.println(vibration);
  Serial.print("Speed: "); Serial.println(motorSpeed);
  Serial.print("Switch: "); Serial.println(motorEnabled ? "ON" : "OFF");
  Serial.println("======================");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  Wire.begin(21, 22);
  mpu.initialize();

  if (mpu.testConnection()) {
    mpuOK = true;
    Serial.println("MPU6050 Connected");
  } else {
    mpuOK = false;
    Serial.println("MPU6050 Connection Failed");
  }

  ledcAttach(RPWM, pwmFreq, pwmResolution);
  ledcAttach(LPWM, pwmFreq, pwmResolution);

  motorStop();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add broadcast peer");
    return;
  }

  Serial.println("Motor ESP-NOW Node Ready");
}

void loop() {
  sendMotorData();
  delay(1000);
}
