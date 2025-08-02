#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include "Adafruit_VL53L1X.h"

#define IRQ_PIN   2
#define XSHUT_PIN 3

Adafruit_VL53L1X vl53(XSHUT_PIN, IRQ_PIN);

// Replace with your slave’s MAC
uint8_t slaveMAC[6] = {0xC8,0xF0,0x9E,0x9B,0x73,0xAC};

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1);
  }
  // Register slave as peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, slaveMAC, 6);
  peer.channel = 0;         // 0 = use current
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  // VL53L1X init
  Wire.begin();
  if (!vl53.begin(0x29, &Wire)) {
    Serial.println("VL53L1X init failed");
    while (1);
  }
  vl53.startRanging();
}

void loop() {
  if (vl53.dataReady()) {
    int16_t d = vl53.distance();
    if (d < 0) {
      Serial.print("Sensor error: "); Serial.println(vl53.vl_status);
    } else {
      d = constrain(d, 0, 100);
      char msg[6];
      int len = snprintf(msg, sizeof(msg), "D=%d", d);
      Serial.print("TX: "); Serial.println(msg);
      esp_now_send(slaveMAC, (uint8_t*)msg, len);
    }
    vl53.clearInterrupt();
  }
  delay(100);
}
