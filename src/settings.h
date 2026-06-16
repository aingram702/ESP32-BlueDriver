// ============================================================================
//  settings.h  -  Runtime, user-editable configuration (persisted to flash)
// ============================================================================
#pragma once
#include <Arduino.h>

struct Settings {
  String ssid;
  String pass;
  bool   activeScan;

  void begin();          // load from flash, applying compile-time defaults
  bool save();           // write JSON to LittleFS
};

extern Settings g_settings;
