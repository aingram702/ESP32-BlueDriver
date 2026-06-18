// ============================================================================
//  uplink.h  -  Optional Wi-Fi station that POSTs discovered devices to another
//               board (e.g. an ESP32-Wardriver) so it can aggregate/log them.
// ============================================================================
//  When enabled in settings, BlueDriver runs in AP+STA mode: it keeps its own
//  access point + web UI while ALSO joining the receiving board's Wi-Fi and
//  periodically HTTP-POSTing a JSON batch of new/updated devices to it.
// ============================================================================
#pragma once
#include <Arduino.h>

struct UplinkStatus {
  bool     enabled   = false;
  bool     connected = false;       // STA associated with the target AP
  uint32_t sent      = 0;           // total devices successfully uplinked
  uint32_t batches   = 0;           // total successful POST batches
  int      lastCode  = 0;           // last HTTP status code (or negative error)
  uint32_t pending   = 0;           // devices waiting to be sent
};

class Uplink {
 public:
  void begin();                     // brings up STA if enabled (call after WebUi)
  void tick();                      // call from loop(); non-blocking between sends
  const UplinkStatus& status() const { return st_; }

 private:
  void sendBatch();

  UplinkStatus st_;
  uint32_t lastAttempt_ = 0;        // last connect attempt (ms)
  uint32_t lastSend_    = 0;        // last POST (ms)
};

extern Uplink g_uplink;
