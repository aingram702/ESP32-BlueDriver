// ============================================================================
//  settings.h  -  Runtime, user-editable configuration (persisted to flash)
//
//  v2.0.0: With the AP/web UI removed, the only runtime-editable setting left
//  is the active-scan toggle, which the WarDriver can flip remotely (a "config"
//  command). All connectivity parameters now live in config.h.
// ============================================================================
#pragma once
#include <Arduino.h>

struct Settings {
  bool activeScan;

  void begin();          // load from flash, applying the compile-time default
  bool save();           // write JSON to LittleFS
};

extern Settings g_settings;
