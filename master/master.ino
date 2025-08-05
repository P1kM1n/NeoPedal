#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include "Adafruit_VL53L1X.h"

#define IRQ_PIN   2
#define XSHUT_PIN 3

Adafruit_VL53L1X vl53(XSHUT_PIN, IRQ_PIN);

// Target peers
uint8_t peerMACs[][6] = {
  {0xC8, 0xF0, 0x9E, 0x9A, 0x93, 0xD4},
  {0xC8, 0xF0, 0x9E, 0x9A, 0x9A, 0x84}
};
const size_t NUM_PEERS = sizeof(peerMACs) / sizeof(peerMACs[0]);

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1);
  }

  // Register each peer
  for (size_t i = 0; i < NUM_PEERS; i++) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, peerMACs[i], 6);
    peer.channel = 0;       // use current channel
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
      Serial.print("Failed to add peer "); 
      for (int b = 0; b < 6; b++) {
        if (b) Serial.print(":");
        Serial.printf("%02X", peerMACs[i][b]);
      }
      Serial.println();
    }
  }

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

      // Send the same message to all peers
      for (size_t i = 0; i < NUM_PEERS; i++) {
        esp_err_t r = esp_now_send(peerMACs[i], (uint8_t*)msg, len);
        if (r != ESP_OK) {
          Serial.print("Send failed to ");
          for (int b = 0; b < 6; b++) {
            if (b) Serial.print(":");
            Serial.printf("%02X", peerMACs[i][b]);
          }
          Serial.printf(" (err=%d)\n", r);
        }
      }
    }
    vl53.clearInterrupt();
  }
  delay(100);
}
