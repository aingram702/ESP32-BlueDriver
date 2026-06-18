# BlueDriver → Wardriver data uplink

When **Data Uplink** is enabled in BlueDriver's web UI (Settings tab), BlueDriver
joins another board's Wi-Fi network as a station and periodically **HTTP-POSTs a
JSON batch** of newly discovered / updated BLE devices to it. This lets your
existing **ESP32-Wardriver** aggregate and log Bluetooth alongside its Wi-Fi
finds, while BlueDriver keeps its own AP + UI running (AP+STA mode).

This doc is the contract for the **receiving** side. Drop the handler below into
the Wardriver firmware (or adapt it to however the Wardriver logs data).

---

## Transport

- **Method:** `POST`
- **URL:** `http://<host>:<port><path>` — defaults `http://192.168.4.1:80/ingest`
  (all configurable in BlueDriver's Settings tab)
- **Content-Type:** `application/json`
- **Cadence:** every *interval* seconds (default 15), only when there is new data
- **Batching:** up to 40 devices per POST
- **Success:** respond with any `2xx` status. BlueDriver only marks devices as
  "sent" on a 2xx, so a failed/again-later response simply causes a resend — no
  data is lost, no duplicates are force-pushed.

## Payload

```json
{
  "source": "ESP32-BlueDriver",
  "ver": "1.0.0",
  "count": 2,
  "gps": { "valid": 1, "lat": 37.421998, "lon": -122.084000 },
  "devices": [
    {
      "mac": "A4:C1:38:12:34:56",
      "name": "Tile",
      "vendor": "Tile",
      "svc": "0000feed-0000-1000-8000-00805f9b34fb",
      "rssi": -67,
      "rssiBest": -54,
      "count": 42,
      "addrType": 1,
      "conn": 1,
      "first": 1234,
      "last": 1290,
      "gps": 1,
      "lat": 37.421998,
      "lon": -122.084000,
      "alt": 12.0
    }
  ]
}
```

Field notes:
- `mac` — uppercase colon-separated; **use this as the dedup key.**
- `vendor` — resolved Bluetooth SIG company name, or `0xXXXX` if unknown.
- `rssi` / `rssiBest` — last and strongest reception (dBm).
- `count` — how many advertisements BlueDriver heard from this device.
- `addrType` — `0` public, `1` random.
- `conn` — `1` if the device advertises as connectable.
- `first` / `last` — seconds (GPS epoch if BlueDriver has a fix, else uptime).
- `gps` — `1` if `lat`/`lon`/`alt` on this device are a real fix.
- the top-level `gps` block is BlueDriver's *current* position at send time.

---

## Drop-in receiver (Arduino `WebServer`)

Works with the stock `WebServer` that most ESP32 Arduino sketches already use.
Requires ArduinoJson. De-dups by MAC and appends a WiGLE-style `BLE` row to the
SD card.

```cpp
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <set>

extern WebServer server;          // your existing server instance

static std::set<String> g_btSeen; // de-dup across the run, keyed by MAC

void handleIngest() {
  if (server.method() != HTTP_POST) { server.send(405, "text/plain", "POST only"); return; }

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }

  int added = 0;
  File f = SD.open("/bluetooth.csv", FILE_APPEND);
  for (JsonObject d : doc["devices"].as<JsonArray>()) {
    String mac = d["mac"] | "";
    if (mac.isEmpty() || g_btSeen.count(mac)) continue;   // skip duplicates
    g_btSeen.insert(mac);
    added++;
    if (f) {
      // MAC,name,vendor,first,rssi,lat,lon,alt,type
      f.printf("%s,%s,%s,%lu,%d,%.6f,%.6f,%.1f,BLE\n",
               mac.c_str(),
               (const char*)(d["name"]   | ""),
               (const char*)(d["vendor"] | ""),
               (unsigned long)(d["first"] | 0),
               (int)(d["rssi"] | 0),
               (double)(d["lat"] | 0.0),
               (double)(d["lon"] | 0.0),
               (double)(d["alt"] | 0.0));
    }
  }
  if (f) f.close();

  String resp = String("{\"ok\":1,\"added\":") + added + "}";
  server.send(200, "application/json", resp);   // 2xx -> BlueDriver marks sent
}

// In setup(), after server is created:
//   server.on("/ingest", HTTP_POST, handleIngest);
```

If the Wardriver uses an async server (`ESPAsyncWebServer`) instead, register an
`AsyncCallbackJsonWebHandler` on `/ingest` and apply the same de-dup + append
logic; the JSON shape is identical.

---

## Getting the two boards talking

1. On the **Wardriver**, make sure its AP is up and it serves `POST /ingest`
   (above). Note its AP SSID/password and its IP on that AP (commonly
   `192.168.4.1`).
2. On **BlueDriver** → Settings → **Data Uplink**: tick *Enable*, enter the
   Wardriver's SSID/password, host/IP, port, and path, then **Save & reboot**.
3. BlueDriver's header shows **Uplink: linked** once associated, and the Settings
   page shows `sent / pending / HTTP <code>` so you can confirm rows are landing.

Because the Wardriver's AP channel is fixed, the station link stays put even
while BlueDriver runs BLE — no channel-hopping problems.
