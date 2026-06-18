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

  uplinkEnabled  = UPLINK_ENABLED_DEFAULT;
  uplinkSsid     = UPLINK_SSID_DEFAULT;
  uplinkPass     = UPLINK_PASS_DEFAULT;
  uplinkHost     = UPLINK_HOST_DEFAULT;
  uplinkPort     = UPLINK_PORT_DEFAULT;
  uplinkPath     = UPLINK_PATH_DEFAULT;
  uplinkInterval = UPLINK_INTERVAL_DEFAULT;

  File f = LittleFS.open(CONFIG_FILE_PATH, "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    if (doc["ssid"].is<const char*>())   ssid       = doc["ssid"].as<String>();
    if (doc["pass"].is<const char*>())   pass       = doc["pass"].as<String>();
    if (doc["active"].is<bool>())        activeScan = doc["active"].as<bool>();
    if (doc["ulEn"].is<bool>())          uplinkEnabled  = doc["ulEn"].as<bool>();
    if (doc["ulSsid"].is<const char*>()) uplinkSsid     = doc["ulSsid"].as<String>();
    if (doc["ulPass"].is<const char*>()) uplinkPass     = doc["ulPass"].as<String>();
    if (doc["ulHost"].is<const char*>()) uplinkHost     = doc["ulHost"].as<String>();
    if (doc["ulPort"].is<int>())         uplinkPort     = doc["ulPort"].as<uint16_t>();
    if (doc["ulPath"].is<const char*>()) uplinkPath     = doc["ulPath"].as<String>();
    if (doc["ulInt"].is<int>())          uplinkInterval = doc["ulInt"].as<uint16_t>();
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
  doc["ulEn"]   = uplinkEnabled;
  doc["ulSsid"] = uplinkSsid;
  doc["ulPass"] = uplinkPass;
  doc["ulHost"] = uplinkHost;
  doc["ulPort"] = uplinkPort;
  doc["ulPath"] = uplinkPath;
  doc["ulInt"]  = uplinkInterval;
  serializeJson(doc, f);
  f.close();
  return true;
}
