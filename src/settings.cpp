// ============================================================================
//  settings.cpp
// ============================================================================
#include "settings.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"

Settings g_settings;

void Settings::begin() {
  // compile-time defaults
  ssid       = AP_SSID_DEFAULT;
  pass       = AP_PASS_DEFAULT;
  activeScan = BLE_ACTIVE_SCAN;

  File f = LittleFS.open(CONFIG_FILE_PATH, "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    if (doc["ssid"].is<const char*>())   ssid       = doc["ssid"].as<String>();
    if (doc["pass"].is<const char*>())   pass       = doc["pass"].as<String>();
    if (doc["active"].is<bool>())        activeScan = doc["active"].as<bool>();
  }
  f.close();
}

bool Settings::save() {
  File f = LittleFS.open(CONFIG_FILE_PATH, "w");
  if (!f) return false;
  JsonDocument doc;
  doc["ssid"]   = ssid;
  doc["pass"]   = pass;
  doc["active"] = activeScan;
  serializeJson(doc, f);
  f.close();
  return true;
}
