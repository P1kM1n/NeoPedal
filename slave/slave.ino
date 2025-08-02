#include <WiFi.h>
#include <esp_now.h>
#include <FastLED.h>

#define NUM_LEDS     42
#define DATA_PIN     21
#define BRIGHTNESS   200     // 50% brightness
#define SIM_INTERVAL 500     // ms between serial updates

CRGB leds[NUM_LEDS];
unsigned long lastPrint = 0;
float currentDistMm = 100.0;  // in millimetres (initially far)

// Accelerator range: 100mm (0% throttle) → 65mm (100% throttle)
const float maxDist = 100.0;
const float minDist = 65.0;

// ESP-NOW receive callback: parse ASCII "D=xx"
void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  char msg[20];
  int l = min(len, 19);
  memcpy(msg, data, l);
  msg[l] = '\0';

  Serial.print("RX: "); Serial.println(msg);

  if (msg[0]=='D' && msg[1]=='=') {
    int val = atoi(&msg[2]);
    currentDistMm = constrain((float)val, minDist, maxDist);
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true);
  }
  esp_now_register_recv_cb(onReceive);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  Serial.println("Receiver ready");
}

void loop() {
  // throttle percentage: 0.0 (far) → 1.0 (close)
  float pct = (maxDist - currentDistMm) / (maxDist - minDist);
  pct = constrain(pct, 0.0, 1.0);
  int lit = round(NUM_LEDS * pct);

  // hue: blue (160) → red (0)
  uint8_t hue = map((int)(pct * 100), 0, 100, 160, 0);
  CRGB color = CHSV(hue, 255, 255);

  // fill LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = (i < lit) ? color : CRGB::Black;
  }
  FastLED.show();

  // serial simulation
  if (millis() - lastPrint >= SIM_INTERVAL) {
    lastPrint = millis();
    Serial.printf("Dist: %.0fmm | ", currentDistMm);
    for (int i = 0; i < NUM_LEDS; i++) {
      Serial.print(i < lit ? 'x' : 'o');
    }
    Serial.println();
  }
}
