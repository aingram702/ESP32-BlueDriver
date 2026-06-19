// ============================================================================
//  ble_scanner.h  -  Continuous BLE observer feeding the device store
// ============================================================================
#pragma once
#include <Arduino.h>

class BleScanner {
 public:
  void begin();
  void start();
  void stop();
  bool scanning() const { return scanning_; }
  void tick();                  // call from loop(): restarts scan if it stopped
  void setActiveScan(bool on);  // apply the active-scan toggle at runtime

 private:
  bool scanning_ = false;
};

extern BleScanner g_scanner;
