// ============================================================================
//  uplink.cpp
// ============================================================================
#include "uplink.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"
#include "settings.h"
#include "device_store.h"
#include "gps.h"

Uplink g_uplink;

void Uplink::begin() {
  st_.enabled = g_settings.uplinkEnabled;
  if (!st_.enabled) return;

  // Keep our own AP (web UI) and add a station interface for the uplink.
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(g_settings.uplinkSsid.c_str(),
             g_settings.uplinkPass.length() ? g_settings.uplinkPass.c_str() : nullptr);
  Serial.printf("Uplink: joining \"%s\" -> http://%s:%u%s\n",
                g_settings.uplinkSsid.c_str(), g_settings.uplinkHost.c_str(),
                g_settings.uplinkPort, g_settings.uplinkPath.c_str());
}

void Uplink::tick() {
  if (!st_.enabled) return;

  uint32_t now = millis();
  st_.connected = (WiFi.status() == WL_CONNECTED);

  // Retry association roughly every 10 s if we've dropped.
  if (!st_.connected) {
    if (now - lastAttempt_ > 10000) {
      lastAttempt_ = now;
      WiFi.begin(g_settings.uplinkSsid.c_str(),
                 g_settings.uplinkPass.length() ? g_settings.uplinkPass.c_str() : nullptr);
    }
    return;
  }

  uint32_t interval = (uint32_t)g_settings.uplinkInterval * 1000UL;
  if (interval < 2000) interval = 2000;
  if (now - lastSend_ >= interval) {
    lastSend_ = now;
    sendBatch();
  }
}

void Uplink::sendBatch() {
  st_.pending = g_store.pendingUplink();
  if (st_.pending == 0) return;

  String devices;
  std::vector<uint64_t> keys;
  size_t n = g_store.buildUplinkBatch(devices, keys, UPLINK_BATCH_MAX);
  if (n == 0) return;

  // Wrap the device array in an envelope with source + current GPS context.
  const GpsState& g = g_gps.state();
  String body;
  body.reserve(devices.length() + 256);
  body  = "{\"source\":\"" BD_FW_NAME "\",\"ver\":\"" BD_FW_VERSION "\"";
  body += ",\"count\":" + String(n);
  body += ",\"gps\":{\"valid\":" + String(g.valid ? 1 : 0);
  body += ",\"lat\":" + String(g.lat, 6) + ",\"lon\":" + String(g.lon, 6) + "}";
  body += ",\"devices\":" + devices + "}";

  String url = "http://" + g_settings.uplinkHost + ":" +
               String(g_settings.uplinkPort) + g_settings.uplinkPath;

  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  if (!http.begin(url)) { st_.lastCode = -1000; return; }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();

  st_.lastCode = code;
  if (code >= 200 && code < 300) {
    g_store.markUplinked(keys);
    st_.sent += n;
    st_.batches++;
  }
  st_.pending = g_store.pendingUplink();
}
