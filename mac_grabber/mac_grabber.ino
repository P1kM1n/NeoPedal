#include <WiFi.h>
#include <esp_wifi.h>

void setup() {
  Serial.begin(115200);                // Start serial at 115200 baud
  WiFi.mode(WIFI_STA);                 // Initialize Wi‑Fi station mode
  delay(100);                          // Let it settle
  
  uint8_t mac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);  // Fetch the STA MAC
  if (ret == ESP_OK) {
    Serial.printf("ESP32 MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }
}

void loop() {}
