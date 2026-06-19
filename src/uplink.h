// ============================================================================
//  uplink.h  -  Wi-Fi station link to the ESP32-WarDriver.
//
//  v2.0.0: This is now BlueDriver's ONLY network role. It joins the WarDriver's
//  access point as a station and, on a timer, POSTs a JSON envelope to the
//  WarDriver's /ingest endpoint containing:
//     * BlueDriver's status (scan state, counts, GPS, beacon state),
//     * a batch of new/updated BLE devices,
//     * the result of any GATT enumeration that just completed.
//  The WarDriver's HTTP response carries back a list of control COMMANDS
//  (start/stop scan, clear, active-scan toggle, beacon, GATT) which BlueDriver
//  executes — so the whole control plane lives on the WarDriver.
// ============================================================================
#pragma once
#include <Arduino.h>

struct LinkStatus {
  bool     connected = false;     // STA associated with the WarDriver AP
  uint32_t sent      = 0;         // total devices successfully uploaded
  uint32_t batches   = 0;         // total successful POST batches
  int      lastCode  = 0;         // last HTTP status code (or negative error)
  uint32_t pending   = 0;         // devices waiting to be sent
  uint32_t lastOkMs  = 0;         // millis() of last 2xx
  uint32_t cmdsRun   = 0;         // commands executed from the WarDriver
};

class Uplink {
 public:
  void begin();                   // bring up the Wi-Fi station
  void tick();                    // call from loop(); non-blocking between sends
  const LinkStatus& status() const { return st_; }

 private:
  void connectSta();
  void sendCycle();               // one POST + command dispatch

  LinkStatus st_;
  uint32_t lastAttempt_ = 0;      // last connect attempt (ms)
  uint32_t lastSend_    = 0;      // last POST (ms)
  uint32_t lastCmdId_   = 0;      // highest command id executed (de-dup)
  uint32_t linkEpoch_   = 0;      // WarDriver boot epoch (resets command ids)

  bool   haveGatt_ = false;       // a GATT result is waiting to be shipped
  String gattJson_;               // the JSON GATT report
};

extern Uplink g_uplink;
