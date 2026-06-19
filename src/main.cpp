// ============================================================================
//  ESP32-BlueDriver  -  main.cpp
//
//  v2.0.0 — HEADLESS BLE SENSOR.
//  BlueDriver no longer hosts a Wi-Fi AP or a web UI. It joins the
//  ESP32-WarDriver's access point as a Wi-Fi station, continuously scans BLE,
//  de-duplicates devices in PSRAM, and periodically POSTs new/updated devices
//  to the WarDriver — which also controls it remotely (scan on/off, clear,
//  active-scan toggle, beacon transmitter, GATT enumeration) by returning
//  commands on each upload's HTTP response.
// ============================================================================
#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "settings.h"
#include "device_store.h"
#include "gps.h"
#include "ble_scanner.h"
#include "pentest.h"
#include "uplink.h"

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
  Serial.println("\n" BD_FW_NAME " v" BD_FW_VERSION " (headless sensor) starting…");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed (formatting)…");
  }

  g_settings.begin();
  g_store.begin();
  g_gps.begin();

  // Bring the Wi-Fi station link up FIRST so the radio is associated before the
  // BLE controller starts competing for the shared 2.4 GHz front-end.
  g_uplink.begin();
  g_pentest.begin();
  g_scanner.begin();

  g_scanner.start();          // start wardriving immediately

  Serial.printf("Free heap: %u  Free PSRAM: %u bytes\n",
                ESP.getFreeHeap(), ESP.getFreePsram());
  setLed(0, 0, 8);            // dim blue while the link comes up
}

void loop() {
  g_gps.update();
  g_scanner.tick();
  g_uplink.tick();

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

  // Status LED:
  //   scanning + linked   -> green pulse
  //   scanning + no link  -> amber/red pulse
  //   idle                -> dim red (blue tint if linked)
  if (nowMs - lastLed > 1000) {
    lastLed = nowMs;
    bool linked = g_uplink.status().connected;
    if (g_scanner.scanning()) {
      static bool t = false; t = !t;
      if (linked) setLed(0, t ? 16 : 4, t ? 2 : 6);
      else        setLed(t ? 16 : 4, t ? 6 : 0, 0);
    } else {
      setLed(6, 0, linked ? 6 : 0);
    }
  }

  delay(2);
}
