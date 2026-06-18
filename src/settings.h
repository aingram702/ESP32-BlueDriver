// ============================================================================
//  settings.h  -  Runtime, user-editable configuration (persisted to flash)
// ============================================================================
#pragma once
#include <Arduino.h>

struct Settings {
  String ssid;
  String pass;
  bool   activeScan;

  // --- Data uplink to another board (optional) ---
  bool     uplinkEnabled;
  String   uplinkSsid;
  String   uplinkPass;
  String   uplinkHost;
  uint16_t uplinkPort;
  String   uplinkPath;
  uint16_t uplinkInterval;   // seconds

  void begin();          // load from flash, applying compile-time defaults
  bool save();           // write JSON to LittleFS
};

extern Settings g_settings;
