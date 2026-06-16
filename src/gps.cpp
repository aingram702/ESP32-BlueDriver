// ============================================================================
//  gps.cpp
// ============================================================================
#include "gps.h"
#include "config.h"

#if GPS_ENABLED
#include <TinyGPSPlus.h>
#include <time.h>

static TinyGPSPlus s_gps;
static HardwareSerial s_serial(GPS_UART_NUM);
#endif

Gps g_gps;

void Gps::begin() {
#if GPS_ENABLED
  s_serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#endif
}

void Gps::update() {
#if GPS_ENABLED
  while (s_serial.available()) {
    char c = s_serial.read();
    st_.present = true;
    s_gps.encode(c);
  }

  if (s_gps.location.isValid() && s_gps.location.age() < 5000) {
    st_.valid = true;
    st_.lat   = s_gps.location.lat();
    st_.lon   = s_gps.location.lng();
    st_.alt   = s_gps.altitude.isValid() ? s_gps.altitude.meters() : 0.0f;
    st_.sats  = s_gps.satellites.isValid() ? s_gps.satellites.value() : 0;
    st_.speedKmh = s_gps.speed.isValid() ? s_gps.speed.kmph() : 0.0f;
  } else {
    st_.valid = false;
  }

  if (s_gps.date.isValid() && s_gps.time.isValid() && s_gps.date.year() > 2020) {
    struct tm t = {};
    t.tm_year = s_gps.date.year() - 1900;
    t.tm_mon  = s_gps.date.month() - 1;
    t.tm_mday = s_gps.date.day();
    t.tm_hour = s_gps.time.hour();
    t.tm_min  = s_gps.time.minute();
    t.tm_sec  = s_gps.time.second();
    time_t epoch = mktime(&t);
    if (epoch > 0) {
      st_.epoch  = (uint32_t)epoch;
      bootEpoch_ = (uint32_t)epoch - (uint32_t)(millis() / 1000);
    }
  }
#endif
}
