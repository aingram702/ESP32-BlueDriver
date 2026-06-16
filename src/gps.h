// ============================================================================
//  gps.h  -  Optional NMEA GPS support (TinyGPS++)
// ============================================================================
#pragma once
#include <Arduino.h>

struct GpsState {
  bool   present  = false;   // any NMEA bytes ever received
  bool   valid    = false;   // current 2D/3D fix
  double lat      = 0;
  double lon      = 0;
  float  alt      = 0;
  float  speedKmh = 0;
  uint8_t sats    = 0;
  uint32_t epoch  = 0;       // unix time from satellites (0 if unknown)
};

class Gps {
 public:
  void begin();
  void update();             // call frequently from loop()
  const GpsState& state() const { return st_; }

  // Offset that converts uptime-seconds into unix epoch, or 0 if GPS time is
  // not (yet) known.  epoch = uptimeSeconds + bootEpoch().
  uint32_t bootEpoch() const { return bootEpoch_; }

 private:
  GpsState st_;
  uint32_t bootEpoch_ = 0;
};

extern Gps g_gps;
