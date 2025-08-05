#include <WiFi.h>
#include <esp_now.h>
#include <FastLED.h>

// ---------------------------
// Hardware config
// ---------------------------
#define NUM_LEDS     42
#define DATA_PIN     21          // your WS2812B data pin
#define BRIGHTNESS   200         // 50%
#define SIM_INTERVAL 700         // ms between serial prints

CRGB leds[NUM_LEDS];

// ---------------------------
// Accelerator → LED mapping
// ---------------------------
// New measured window: 80 mm = 0% (idle), 40 mm = 100% (full)
const float DIST_IDLE_MM  = 80.0f;
const float DIST_FULL_MM  = 40.0f;
const float DIST_SPAN_MM  = (DIST_IDLE_MM - DIST_FULL_MM); // = 40 mm

// When considered "at idle" for the idle animation
const float IDLE_TOL_MM   = 1.5f;     // within ~1.5 mm of 80 mm => idle

// State updated by RX
volatile float g_dist_mm  = DIST_IDLE_MM;   // last received distance in mm
volatile uint32_t g_last_rx_ms = 0;

// ---------------------------
// Idle animation (bouncing single LED)
// ---------------------------
int idlePos = 0;
int idleDir = +1;
uint32_t idleLastStep = 0;
const uint16_t IDLE_STEP_MS = 20;     // speed of the chaser

// ---------------------------
// Helpers
// ---------------------------
static inline float clampf(float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); }

// Convert distance to throttle fraction [0..1]
float throttle01(float mm) {
  mm = clampf(mm, DIST_FULL_MM, DIST_IDLE_MM);
  return (DIST_IDLE_MM - mm) / DIST_SPAN_MM; // 0 at 80mm, 1 at 40mm
}

// Hue curve: blue→orange by 50%, then orange→red by 100%
// FastLED HSV hue: 0=red, ~32=orange, ~160=blue
uint8_t hueForThrottle(float t01) {
  t01 = clampf(t01, 0.0f, 1.0f);
  if (t01 <= 0.5f) {
    // 0..0.5 : 160 (blue) → 32 (orange)
    float a = t01 / 0.5f;               // 0..1
    return (uint8_t)(160.0f + (32.0f - 160.0f) * a);
  } else {
    // 0.5..1 : 32 (orange) → 0 (red)
    float a = (t01 - 0.5f) / 0.5f;      // 0..1
    return (uint8_t)(32.0f + (0.0f - 32.0f) * a);
  }
}

// ---------------------------
// ESP-NOW receive callback (ASCII "D=nn")
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
  Serial.println("Receiver ready (80mm→40mm window, blue→orange→red)");
}

// ---------------------------
// Loop
// ---------------------------
void loop(){
  const uint32_t now = millis();
  const float    dist = g_dist_mm;        // snapshot
  const float    t    = throttle01(dist); // 0..1

  // Decide mode
  const bool isIdleByDistance = (dist >= (DIST_IDLE_MM - IDLE_TOL_MM));
  const bool recentTraffic    = (now - g_last_rx_ms) < 500; // packets recently arriving

  if (isIdleByDistance && recentTraffic) {
    // ---------- Idle animation: bounce a single LED ----------
    if (now - idleLastStep >= IDLE_STEP_MS) {
      idleLastStep = now;
      idlePos += idleDir;
      if (idlePos >= NUM_LEDS-1) { idlePos = NUM_LEDS-1; idleDir = -1; }
      if (idlePos <= 0)          { idlePos = 0;          idleDir = +1; }
    }
    // Draw
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    leds[idlePos] = CHSV(160, 255, 180); // dim blue runner
    FastLED.show();

  } else {
    // ---------- Active fill: number of LEDs and color depend on throttle ----------
    const int lit = (int)roundf(NUM_LEDS * clampf(t,0,1));
    const uint8_t hue = hueForThrottle(t); // blue→orange→red curve
    const CRGB color = CHSV(hue, 255, 255);

    for (int i = 0; i < NUM_LEDS; ++i) {
      leds[i] = (i < lit) ? color : CRGB::Black;
    }
    FastLED.show();
  }

  // ---------- Serial simulation (x=on, o=off) ----------
  static uint32_t lastSim = 0;
  if (now - lastSim >= SIM_INTERVAL) {
    lastSim = now;
    const float tprint = throttle01(g_dist_mm);
    const int litprint = (int)roundf(NUM_LEDS * clampf(tprint,0,1));

    Serial.printf("Dist: %.0fmm  Throttle: %3.0f%%  Mode: %s | ",
                  g_dist_mm, tprint*100.0f,
                  (isIdleByDistance && recentTraffic) ? "IDLE" : "ACTIVE");

    if (isIdleByDistance && recentTraffic) {
      for (int i = 0; i < NUM_LEDS; ++i) Serial.print(i==idlePos ? '*' : '-');
    } else {
      for (int i = 0; i < NUM_LEDS; ++i) Serial.print(i < litprint ? 'x' : 'o');
    }
    Serial.println();
  }
}
