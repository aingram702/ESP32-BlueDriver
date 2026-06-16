// ============================================================================
//  ESP32-BlueDriver  -  main.cpp
//
//  A Bluetooth LE wardriver for the ESP32-S3-DevKitC-1 (N16R8).
//  Boots its own Wi-Fi access point + captive-portal web UI that controls
//  scanning, shows a de-duplicated live device list, exports WiGLE CSV, and
//  hosts a small BLE pentest toolkit.
// ============================================================================
#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "settings.h"
#include "device_store.h"
#include "gps.h"
#include "ble_scanner.h"
#include "web.h"
#include "pentest.h"

static uint32_t lastSave = 0;
static size_t   lastSavedCount = 0;
static uint32_t lastLed = 0;

static void setLed(uint8_t r, uint8_t g, uint8_t b) {
#if STATUS_LED_IS_RGB
  neopixelWrite(STATUS_LED_PIN, r, g, b);
#else
  (void)r; (void)g; (void)b;
#endif
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n" BD_FW_NAME " v" BD_FW_VERSION " starting…");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed (formatting)…");
  }

  g_settings.begin();
  g_store.begin();
  g_gps.begin();

  g_scanner.begin();
  g_pentest.begin();
  g_web.begin();

  g_scanner.start();          // start wardriving immediately on boot

  Serial.printf("AP \"%s\"  ->  http://192.168.4.1/\n", g_settings.ssid.c_str());
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  setLed(0, 8, 0);
}

void loop() {
  g_web.tick();
  g_gps.update();
  g_scanner.tick();

  // Periodic autosave to flash so a power blip doesn't lose the session log.
  uint32_t nowMs = millis();
  if (nowMs - lastSave > AUTOSAVE_INTERVAL) {
    lastSave = nowMs;
    size_t n = g_store.size();
    if (n != lastSavedCount) {
      g_store.saveCsv(LOG_FILE_PATH);
      lastSavedCount = n;
    }
  }

  // Status LED: green pulse while scanning, dim red when idle.
  if (nowMs - lastLed > 1000) {
    lastLed = nowMs;
    if (g_scanner.scanning()) {
      static bool t = false; t = !t;
      setLed(0, t ? 16 : 4, t ? 4 : 0);
    } else {
      setLed(6, 0, 0);
    }
  }

  delay(2);
}
