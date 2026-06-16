// ============================================================================
//  web.h  -  Captive-portal web control panel
// ============================================================================
#pragma once
#include <Arduino.h>

class WebUi {
 public:
  void begin();
  void tick();   // call from loop(): services DNS + HTTP
};

extern WebUi g_web;
