// ============================================================================
//  web.cpp  -  Captive-portal control panel (synchronous WebServer + DNS)
// ============================================================================
#include "web.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>
#include "config.h"
#include "web_index.h"
#include "device_store.h"
#include "ble_scanner.h"
#include "gps.h"
#include "settings.h"
#include "pentest.h"
#include "uplink.h"

WebUi g_web;

static WebServer  server(WEB_PORT);
static DNSServer  dns;
static IPAddress  apIP(192, 168, 4, 1);

using DevPtrVec = std::vector<const BleDevice*, PsramAllocator<const BleDevice*>>;

// --- JSON string escaping ---------------------------------------------------
static void jsonEsc(String& out, const char* s) {
  out += '"';
  for (; *s; ++s) {
    char c = *s;
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n')        { out += "\\n"; }
    else if ((uint8_t)c < 0x20){ /* drop control chars */ }
    else                         out += c;
  }
  out += '"';
}

static bool ciContains(const char* hay, const String& needle) {
  if (needle.length() == 0) return true;
  String h(hay); h.toLowerCase();
  String n(needle); n.toLowerCase();
  return h.indexOf(n) >= 0;
}

// ---------------------------------------------------------------------------
//  Streamed devices response (used by /api/devices and /export.json)
// ---------------------------------------------------------------------------
static void streamDevices(size_t limit, const String& q, const String& sort) {
  uint32_t now = millis() / 1000;

  // Collect element pointers (stable across rehash) under lock.
  DevPtrVec list;
  g_store.lock();
  list.reserve(g_store.map().size());
  char macbuf[18];
  for (auto& kv : g_store.map()) {
    const BleDevice& d = kv.second;
    if (q.length()) {
      macToStr(d.mac, macbuf);
      if (!ciContains(d.name, q) && !ciContains(macbuf, q) &&
          !ciContains(d.manufacturer, q)) continue;
    }
    list.push_back(&kv.second);
  }
  size_t total = list.size();
  g_store.unlock();

  auto cmp = [&](const BleDevice* a, const BleDevice* b) {
    if (sort == "rssi")  return a->rssiBest > b->rssiBest;
    if (sort == "count") return a->count    > b->count;
    if (sort == "name")  return strcmp(a->name, b->name) < 0;
    if (sort == "first") return a->firstSeen < b->firstSeen;
    return a->lastSeen > b->lastSeen;                       // default: last seen
  };
  std::sort(list.begin(), list.end(), cmp);

  size_t shown = (limit == 0) ? total : (total < limit ? total : limit);

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  String buf;
  buf.reserve(4096);
  buf += "{\"total\":" + String(total) + ",\"shown\":" + String(shown) + ",\"rows\":[";

  char mac[18];
  for (size_t i = 0; i < shown; i++) {
    const BleDevice* d = list[i];
    macToStr(d->mac, mac);
    if (i) buf += ',';
    buf += "{\"mac\":\""; buf += mac; buf += "\",";
    buf += "\"name\":";   jsonEsc(buf, d->name);
    buf += ",\"mfg\":";   jsonEsc(buf, d->manufacturer);
    buf += ",\"svc\":";   jsonEsc(buf, d->serviceUuid);
    buf += ",\"rssi\":"   + String(d->rssiLast);
    buf += ",\"rssiBest\":" + String(d->rssiBest);
    buf += ",\"tx\":"     + String(d->txPower);
    buf += ",\"count\":"  + String(d->count);
    buf += ",\"conn\":"   + String(d->connectable ? 1 : 0);
    buf += ",\"addrType\":" + String(d->addrType);
    buf += ",\"appearance\":" + String(d->appearance);
    buf += ",\"gps\":"    + String(d->hasGps ? 1 : 0);
    buf += ",\"lat\":"    + String(d->lat, 6);
    buf += ",\"lon\":"    + String(d->lon, 6);
    buf += ",\"firstAge\":" + String(now >= d->firstSeen ? now - d->firstSeen : 0);
    buf += ",\"lastAge\":"  + String(now >= d->lastSeen  ? now - d->lastSeen  : 0);
    buf += "}";
    if (buf.length() > 3500) { server.sendContent(buf); buf = ""; }
  }
  buf += "]}";
  server.sendContent(buf);
  server.sendContent("");   // end chunked
}

// ---------------------------------------------------------------------------
//  Handlers
// ---------------------------------------------------------------------------
static void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
  JsonDocument doc;
  doc["fw"]   = BD_FW_NAME;
  doc["ver"]  = BD_FW_VERSION;
  doc["scanning"]   = g_scanner.scanning();
  doc["devices"]    = g_store.size();
  doc["newDevices"] = g_store.newDeviceCount();
  doc["uptime"]     = (uint32_t)(millis() / 1000);
  doc["heap"]       = ESP.getFreeHeap();
  doc["psram"]      = ESP.getFreePsram();
  const GpsState& g = g_gps.state();
  JsonObject gj = doc["gps"].to<JsonObject>();
  gj["present"] = g.present; gj["valid"] = g.valid;
  gj["lat"] = g.lat; gj["lon"] = g.lon; gj["sats"] = g.sats;
  JsonObject bj = doc["beacon"].to<JsonObject>();
  bj["active"] = g_pentest.beaconActive();
  bj["info"]   = g_pentest.beaconInfo();
  const UplinkStatus& u = g_uplink.status();
  JsonObject uj = doc["uplink"].to<JsonObject>();
  uj["enabled"]   = u.enabled;
  uj["connected"] = u.connected;
  uj["sent"]      = u.sent;
  uj["batches"]   = u.batches;
  uj["lastCode"]  = u.lastCode;
  uj["pending"]   = u.pending;
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleDevices() {
  size_t limit = server.hasArg("limit") ? server.arg("limit").toInt() : 300;
  streamDevices(limit, server.arg("q"), server.arg("sort"));
}

static void handleScan() {
  if (server.hasArg("toggle"))   g_scanner.scanning() ? g_scanner.stop() : g_scanner.start();
  else if (server.arg("on") == "1") g_scanner.start();
  else                              g_scanner.stop();
  server.send(200, "application/json", "{\"ok\":1}");
}

static void handleClear() {
  g_store.clear();
  server.send(200, "application/json", "{\"ok\":1}");
}

static void handleExportCsv() {
  g_store.saveCsv(LOG_FILE_PATH);
  File f = LittleFS.open(LOG_FILE_PATH, "r");
  if (!f) { server.send(500, "text/plain", "no log"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=bluedriver.csv");
  server.streamFile(f, "text/csv");
  f.close();
}

static void handleExportJson() {
  server.sendHeader("Content-Disposition", "attachment; filename=bluedriver.json");
  streamDevices(0, "", "last");
}

static void handleConfigGet() {
  JsonDocument doc;
  doc["ssid"]   = g_settings.ssid;
  doc["pass"]   = g_settings.pass;
  doc["active"] = g_settings.activeScan;
  doc["ulEn"]   = g_settings.uplinkEnabled;
  doc["ulSsid"] = g_settings.uplinkSsid;
  doc["ulPass"] = g_settings.uplinkPass;
  doc["ulHost"] = g_settings.uplinkHost;
  doc["ulPort"] = g_settings.uplinkPort;
  doc["ulPath"] = g_settings.uplinkPath;
  doc["ulInt"]  = g_settings.uplinkInterval;
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleConfigPost() {
  if (server.hasArg("ssid"))   g_settings.ssid = server.arg("ssid");
  if (server.hasArg("pass"))   g_settings.pass = server.arg("pass");
  if (server.hasArg("active")) g_settings.activeScan = (server.arg("active") == "1");
  if (server.hasArg("ulEn"))   g_settings.uplinkEnabled = (server.arg("ulEn") == "1");
  if (server.hasArg("ulSsid")) g_settings.uplinkSsid = server.arg("ulSsid");
  if (server.hasArg("ulPass")) g_settings.uplinkPass = server.arg("ulPass");
  if (server.hasArg("ulHost")) g_settings.uplinkHost = server.arg("ulHost");
  if (server.hasArg("ulPort")) g_settings.uplinkPort = server.arg("ulPort").toInt();
  if (server.hasArg("ulPath")) g_settings.uplinkPath = server.arg("ulPath");
  if (server.hasArg("ulInt"))  g_settings.uplinkInterval = server.arg("ulInt").toInt();
  g_settings.save();
  server.send(200, "application/json", "{\"ok\":1,\"reboot\":1}");
  delay(300);
  ESP.restart();
}

static void handlePentestGatt() {
  String mac = server.arg("mac");
  uint8_t type = server.arg("type").toInt();
  if (mac.length() < 17) { server.send(400, "application/json",
      "{\"error\":\"invalid MAC\"}"); return; }
  String r = g_pentest.enumerateGatt(mac, type);
  server.send(200, "application/json", r);
}

static void handlePentestBeacon() {
  String action = server.arg("action");
  if (action == "stop") {
    g_pentest.stopBeacon();
    server.send(200, "application/json", "{\"ok\":1}");
    return;
  }
  String err = g_pentest.startBeacon(server.arg("type"), server.arg("name"),
                                     server.arg("arg1"), server.arg("arg2"));
  if (err.length()) {
    JsonDocument d; d["error"] = err; String o; serializeJson(d, o);
    server.send(200, "application/json", o);
  } else {
    server.send(200, "application/json", "{\"ok\":1}");
  }
}

static void handleNotFound() {
  // Captive-portal: bounce every unknown host/URL to the control panel.
  server.sendHeader("Location", String("http://") + apIP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

// ---------------------------------------------------------------------------
void WebUi::begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);                 // keep the AP responsive alongside BLE
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  // An AP password must be 8+ chars; anything shorter -> open network.
  const char* pass = g_settings.pass.length() >= 8 ? g_settings.pass.c_str() : nullptr;

  bool ok = WiFi.softAP(g_settings.ssid.c_str(), pass, AP_CHANNEL,
                        /*ssid_hidden=*/0, /*max_connection=*/4);
  if (!ok) {
    // Retry once as an open AP in case the password was the problem.
    Serial.println("softAP failed; retrying as open AP…");
    ok = WiFi.softAP(g_settings.ssid.c_str(), nullptr, AP_CHANNEL);
  }
  Serial.printf("softAP %s  ssid=\"%s\"  ip=%s\n",
                ok ? "UP" : "FAILED", g_settings.ssid.c_str(),
                WiFi.softAPIP().toString().c_str());

  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(DNS_PORT, "*", apIP);

  server.on("/", handleRoot);
  server.on("/api/status",  handleStatus);
  server.on("/api/devices", handleDevices);
  server.on("/api/scan",    HTTP_POST, handleScan);
  server.on("/api/clear",   HTTP_POST, handleClear);
  server.on("/api/config",  HTTP_GET,  handleConfigGet);
  server.on("/api/config",  HTTP_POST, handleConfigPost);
  server.on("/api/pentest/gatt",   HTTP_POST, handlePentestGatt);
  server.on("/api/pentest/beacon", HTTP_POST, handlePentestBeacon);
  server.on("/export.csv",  handleExportCsv);
  server.on("/export.json", handleExportJson);

  // Common OS captive-portal probes -> serve the page so the portal pops up.
  server.on("/generate_204",      handleRoot);
  server.on("/gen_204",           handleRoot);
  server.on("/hotspot-detect.html", handleRoot);
  server.on("/ncsi.txt",          handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
}

void WebUi::tick() {
  dns.processNextRequest();
  server.handleClient();
}
