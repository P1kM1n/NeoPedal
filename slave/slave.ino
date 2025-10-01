#include <WiFi.h>
#include <esp_now.h>
#include <FastLED.h>

// ---------------------------
// Hardware config
// ---------------------------
#define NUM_LEDS     42
#define DATA_PIN     21
#define BRIGHTNESS   200
#define SIM_INTERVAL 700

CRGB leds[NUM_LEDS];

// ---------------------------
// Accelerator → LED mapping
// ---------------------------
const float DIST_IDLE_MM  = 90.0f;
const float DIST_FULL_MM  = 55.0f;
const float DIST_SPAN_MM  = (DIST_IDLE_MM - DIST_FULL_MM);
const float IDLE_TOL_MM   = 1.5f;

volatile float g_dist_mm  = DIST_IDLE_MM;
volatile uint32_t g_last_rx_ms = 0;

// ---------------------------
// Idle animation
// ---------------------------
float idlePhase = 0.0f;

// ---------------------------
// Startup animation
// ---------------------------
bool startupDone = false;
uint32_t startupStart = 0;

// ---------------------------
// Helpers
// ---------------------------
static inline float clampf(float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); }

float throttle01(float mm) {
  mm = clampf(mm, DIST_FULL_MM, DIST_IDLE_MM);
  return (DIST_IDLE_MM - mm) / DIST_SPAN_MM;
}

uint8_t hueForThrottle(float t01) {
  t01 = clampf(t01, 0.0f, 1.0f);
  if (t01 <= 0.5f) {
    float a = t01 / 0.5f;
    return (uint8_t)(160.0f + (32.0f - 160.0f) * a);
  } else {
    float a = (t01 - 0.5f) / 0.5f;
    return (uint8_t)(32.0f + (0.0f - 32.0f) * a);
  }
}

// ---------------------------
// ESP-NOW receive callback
// ---------------------------
void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  char msg[20];
  int l = min(len, 19);
  memcpy(msg, data, l);
  msg[l] = '\0';

  if (msg[0]=='D' && msg[1]=='=') {
    int mm = atoi(&msg[2]);
    g_dist_mm = clampf((float)mm, DIST_FULL_MM, DIST_IDLE_MM);
    g_last_rx_ms = millis();
  }
}

// ---------------------------
// Startup animation logic
// ---------------------------
bool runStartupAnim(uint32_t now) {
  uint32_t elapsed = now - startupStart;

  if (elapsed < 1000) {
    // Phase 1: self-check pings
    int group = (elapsed / 100) % (NUM_LEDS / 6 + 1);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < 6; i++) {
      int idx = group * 6 + i;
      if (idx < NUM_LEDS)
        leds[idx] = CRGB::White;
    }
    FastLED.setBrightness(200);
    FastLED.show();
    return false;

  } else if (elapsed < 2000) {
    // Phase 2: color sweep
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV((now / 8 + i * 4) % 255, 255, 255);
    }
    FastLED.setBrightness(180);
    FastLED.show();
    return false;

  } else if (elapsed < 3000) {
    // Phase 3: rainbow wake
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV((now / 10 + i * 2) % 255, 180, 180);
    }
    FastLED.setBrightness(150);
    FastLED.show();
    return false;

  } else {
    // Done
    FastLED.clear();
    FastLED.show();
    startupDone = true;
    return true;
  }
}

// ---------------------------
// Idle animation logic
// ---------------------------
void runIdleAnim(uint32_t now) {
  idlePhase += 0.03f;
  for (int i = 0; i < NUM_LEDS; ++i) {
    float wave = sinf(idlePhase + i * 0.3f);
    uint8_t brightness = 10 + (uint8_t)(wave * 20 + 20); // soft 10–50 range
    leds[i] = CHSV(160, 255, brightness);
  }
  FastLED.setBrightness(70); // extra dim
  FastLED.show();
}

// ---------------------------
// Setup
// ---------------------------
void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while(true){}
  }
  esp_now_register_recv_cb(onReceive);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  Serial.println("Receiver ready");
  startupStart = millis();
}

// ---------------------------
// Loop
// ---------------------------
void loop(){
  uint32_t now = millis();

  // ---------- Startup ----------
  if (!startupDone && !runStartupAnim(now)) return;

  const float dist = g_dist_mm;
  const float t    = throttle01(dist);
  const bool isIdle = (dist >= (DIST_IDLE_MM - IDLE_TOL_MM));
  const bool recent = (now - g_last_rx_ms) < 500;

  if (isIdle && recent) {
    runIdleAnim(now);
  } else {
    const int lit = (int)roundf(NUM_LEDS * clampf(t,0,1));
    const uint8_t hue = hueForThrottle(t);
    const CRGB color = CHSV(hue, 255, 255);

    for (int i = 0; i < NUM_LEDS; ++i)
      leds[i] = (i < lit) ? color : CRGB::Black;

    FastLED.setBrightness(BRIGHTNESS);
    FastLED.show();
  }

  // ---------- Serial simulation ----------
  static uint32_t lastSim = 0;
  if (now - lastSim >= SIM_INTERVAL) {
    lastSim = now;
    const float tprint = throttle01(g_dist_mm);
    const int litprint = (int)roundf(NUM_LEDS * clampf(tprint,0,1));

    Serial.printf("Dist: %.0fmm  Throttle: %3.0f%%  Mode: %s | ",
      g_dist_mm, tprint*100.0f,
      (isIdle && recent) ? "IDLE" : "ACTIVE");

    for (int i = 0; i < NUM_LEDS; ++i)
      Serial.print((isIdle && recent) ? '-' : (i < litprint ? 'x' : 'o'));

    Serial.println();
  }
}
