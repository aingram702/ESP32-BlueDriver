// ============================================================================
//  uplink.cpp
// ============================================================================
#include "uplink.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h"
#include "settings.h"
#include "device_store.h"
#include "gps.h"
#include "ble_scanner.h"
#include "pentest.h"

Uplink g_uplink;

// Minimal JSON-string escaper for our own short status strings.
static String escJson(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n')        { o += "\\n"; }
    else if ((uint8_t)c >= 0x20) o += c;
  }
  return o;
}

void Uplink::begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // keep RX responsive alongside BLE scanning
  connectSta();
}

void Uplink::connectSta() {
  Serial.printf("Link: joining \"%s\" -> http://%s:%u%s\n",
                LINK_SSID, LINK_HOST, (unsigned)LINK_PORT, LINK_PATH);
  WiFi.begin(LINK_SSID, LINK_PASS[0] ? LINK_PASS : nullptr);
  lastAttempt_ = millis();
}

void Uplink::tick() {
  uint32_t now = millis();
  st_.connected = (WiFi.status() == WL_CONNECTED);

  if (!st_.connected) {
    if (now - lastAttempt_ > LINK_RETRY_MS) connectSta();
    return;
  }

  if (now - lastSend_ >= LINK_INTERVAL_MS) {
    lastSend_ = now;
    sendCycle();
  }
}

void Uplink::sendCycle() {
  // Build the device batch (may be empty; we still POST every cycle to report
  // status and to pull pending control commands from the WarDriver).
  String devices;
  std::vector<uint64_t> keys;
  size_t n = g_store.buildUplinkBatch(devices, keys, UPLINK_BATCH_MAX);

  const GpsState& g = g_gps.state();

  String body;
  body.reserve(devices.length() + (haveGatt_ ? gattJson_.length() : 0) + 512);
  body  = "{\"source\":\"" BD_FW_NAME "\",\"ver\":\"" BD_FW_VERSION "\"";
  body += ",\"linkEpoch\":" + String(linkEpoch_);
  body += ",\"lastCmdId\":" + String(lastCmdId_);

  // status block
  body += ",\"status\":{";
  body += "\"scanning\":"   + String(g_scanner.scanning() ? 1 : 0);
  body += ",\"devices\":"   + String((uint32_t)g_store.size());
  body += ",\"newDevices\":"+ String(g_store.newDeviceCount());
  body += ",\"uptime\":"    + String((uint32_t)(millis() / 1000));
  body += ",\"heap\":"      + String((uint32_t)ESP.getFreeHeap());
  body += ",\"psram\":"     + String((uint32_t)ESP.getFreePsram());
  body += ",\"activeScan\":"+ String(g_settings.activeScan ? 1 : 0);
  body += ",\"beaconActive\":" + String(g_pentest.beaconActive() ? 1 : 0);
  body += ",\"beaconInfo\":\"" + escJson(g_pentest.beaconInfo()) + "\"";
  body += "}";

  // gps block
  body += ",\"gps\":{\"valid\":" + String(g.valid ? 1 : 0);
  body += ",\"sats\":" + String(g.sats);
  body += ",\"lat\":" + String(g.lat, 6) + ",\"lon\":" + String(g.lon, 6) + "}";

  // optional GATT result from a previously-run command
  if (haveGatt_) body += ",\"gattResult\":" + gattJson_;

  body += ",\"count\":" + String((uint32_t)n);
  body += ",\"devices\":" + devices + "}";

  String url = String("http://") + LINK_HOST + ":" + String((int)LINK_PORT) + LINK_PATH;

  HTTPClient http;
  http.setConnectTimeout(LINK_CONNECT_TMO_MS);
  http.setTimeout(LINK_RW_TMO_MS);
  if (!http.begin(url)) { st_.lastCode = -1000; return; }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  String resp = (code > 0) ? http.getString() : String();
  http.end();

  st_.lastCode = code;
  if (code < 200 || code >= 300) {
    st_.pending = g_store.pendingUplink();
    return;
  }

  // 2xx: devices accepted.
  st_.lastOkMs = millis();
  if (n) { g_store.markUplinked(keys); st_.sent += n; st_.batches++; }
  if (haveGatt_) { haveGatt_ = false; gattJson_ = ""; }   // delivered

  // --- Process control commands returned by the WarDriver -------------------
  JsonDocument rdoc;
  if (deserializeJson(rdoc, resp) == DeserializationError::Ok) {
    // A new WarDriver boot epoch resets the command-id sequence.
    uint32_t ep = rdoc["epoch"] | 0;
    if (ep != linkEpoch_) { linkEpoch_ = ep; lastCmdId_ = 0; }

    JsonArrayConst cmds = rdoc["commands"].as<JsonArrayConst>();
    for (JsonObjectConst c : cmds) {
      uint32_t id = c["id"] | 0;
      if (id && id <= lastCmdId_) continue;          // already executed
      String cmd = String((const char*)(c["cmd"] | ""));
      Serial.printf("Link: cmd #%u '%s'\n", id, cmd.c_str());

      if (cmd == "scan") {
        if (c["on"].is<int>())  (c["on"].as<int>() ? g_scanner.start() : g_scanner.stop());
        else                    (g_scanner.scanning() ? g_scanner.stop() : g_scanner.start());

      } else if (cmd == "clear") {
        g_store.clear();

      } else if (cmd == "config") {
        if (c["activeScan"].is<int>()) {
          g_settings.activeScan = (c["activeScan"].as<int>() != 0);
          g_settings.save();
          g_scanner.setActiveScan(g_settings.activeScan);
        }

      } else if (cmd == "beacon") {
        String action = String((const char*)(c["action"] | ""));
        if (action == "stop") {
          g_pentest.stopBeacon();
        } else {
          g_pentest.startBeacon(String((const char*)(c["type"] | "")),
                                String((const char*)(c["name"] | "")),
                                String((const char*)(c["arg1"] | "")),
                                String((const char*)(c["arg2"] | "")));
        }

      } else if (cmd == "gatt") {
        String mac = String((const char*)(c["mac"] | ""));
        uint8_t at = (uint8_t)(c["addrType"] | 0);
        if (mac.length() >= 17) {
          gattJson_ = g_pentest.enumerateGatt(mac, at);   // blocks ~ up to 8 s
          haveGatt_ = true;                                // shipped next POST
        }
      }

      if (id > lastCmdId_) lastCmdId_ = id;
      st_.cmdsRun++;
    }
  }

  st_.pending = g_store.pendingUplink();
}
