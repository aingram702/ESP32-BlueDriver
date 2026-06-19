# BlueDriver ↔ WarDriver link protocol (v2.0.0)

In v2.0.0 BlueDriver is a **headless BLE sensor**. It has no UI of its own: it
joins the WarDriver's Wi-Fi access point as a station and, every
`LINK_INTERVAL_MS` (default 4 s), makes **one HTTP `POST`** to the WarDriver's
`/ingest` endpoint. That single request/response is the whole link:

- **Request body** → BlueDriver's status, GPS, any finished GATT result, and a
  batch of newly discovered devices.
- **Response body** → a list of **control commands** for BlueDriver to run
  (start/stop scan, clear, active-scan toggle, beacon, GATT enumeration).

So the entire control plane lives on the **WarDriver**. Implement the endpoint
below in the WarDriver firmware and the two boards are fully integrated.

All link parameters are compile-time constants in BlueDriver's
[`src/config.h`](../src/config.h) — set them to match your WarDriver **before
flashing**:

```c
#define LINK_SSID   "Wardriver"      // == WarDriver AP SSID
#define LINK_PASS   "wardrive-me"    // == WarDriver AP password ("" = open)
#define LINK_HOST   "192.168.4.1"    // WarDriver SoftAP gateway IP
#define LINK_PORT   80
#define LINK_PATH   "/ingest"
```

---

## Request: BlueDriver → WarDriver

`POST /ingest`  ·  `Content-Type: application/json`

```json
{
  "source": "ESP32-BlueDriver",
  "ver": "2.0.0",
  "linkEpoch": 1718,
  "lastCmdId": 6,
  "status": {
    "scanning": 1, "devices": 142, "newDevices": 150, "uptime": 463,
    "heap": 201160, "psram": 7016448, "activeScan": 1,
    "beaconActive": 0, "beaconInfo": ""
  },
  "gps": { "valid": 1, "sats": 7, "lat": 37.421998, "lon": -122.084000 },
  "gattResult": { "...": "present only right after a gatt command finished" },
  "count": 2,
  "devices": [
    {
      "mac": "A4:C1:38:12:34:56", "name": "Tile", "vendor": "Tile",
      "svc": "0000feed-0000-1000-8000-00805f9b34fb",
      "rssi": -67, "rssiBest": -54, "count": 42, "addrType": 1, "conn": 1,
      "first": 12, "last": 70, "gps": 1,
      "lat": 37.421998, "lon": -122.084000, "alt": 12.0
    }
  ]
}
```

- `linkEpoch` / `lastCmdId` — what BlueDriver currently believes; use them to
  decide which commands it still needs (see below).
- `devices[]` — **each unique device is sent exactly once**, on the first POST
  after it's discovered. Use `mac` as the key. `first`/`last` are seconds
  (GPS-epoch if BlueDriver has a fix, otherwise uptime). `addrType` 0=public,
  1=random. `conn` = advertises as connectable.
- `count` — number of devices in this batch (≤ 40).
- `gattResult` — only present in the POST right after a `gatt` command
  completes; it's the JSON map of the target's services/characteristics.

## Response: WarDriver → BlueDriver

Return **any `2xx`** with a JSON body. A 2xx is what tells BlueDriver the batch
was accepted (it then stops resending those devices), so always 2xx unless you
truly failed to store the data.

```json
{
  "epoch": 1718,
  "commands": [
    { "id": 7, "cmd": "scan", "on": 1 },
    { "id": 8, "cmd": "config", "activeScan": 1 },
    { "id": 9, "cmd": "gatt", "mac": "A4:C1:38:12:34:56", "addrType": 1 }
  ]
}
```

### Command rules
- **`epoch`** — your WarDriver's boot id (e.g. `millis()` at boot, or a stored
  counter). When it changes, BlueDriver resets its `lastCmdId` to 0, so any
  still-queued commands run again. Keep it stable between reboots-of-interest.
- **`id`** — every command **must** carry a unique, strictly increasing integer.
  BlueDriver ignores any `id ≤ lastCmdId`, which is how commands run exactly
  once. (A command with no `id`/`id:0` would run on *every* poll — avoid that.)
- If nothing is queued, return `{ "epoch": <e>, "commands": [] }`.

### Command reference
| `cmd`    | fields | effect on BlueDriver |
|----------|--------|----------------------|
| `scan`   | `on` (0/1, optional) | start/stop BLE scanning; omit `on` to toggle |
| `clear`  | — | wipe the in-RAM device list (and resets "sent" state) |
| `config` | `activeScan` (0/1) | set active vs passive scan (persisted) |
| `beacon` | `action` (`start`/`stop`), `type` (`ibeacon`/`eddystone`/`custom`), `name`, `arg1`, `arg2` | run/stop the beacon transmitter |
| `gatt`   | `mac`, `addrType` (0/1) | connect + enumerate GATT; result returns in the next POST's `gattResult` |

---

## Drop-in WarDriver receiver (Arduino `WebServer`)

Parses the batch, de-dups by MAC into an SD log, exposes BlueDriver's status,
and drains a simple command queue. Requires ArduinoJson + SD.

```cpp
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <set>
#include <deque>

extern WebServer server;                 // your existing server

static std::set<String>   g_btSeen;      // de-dup BLE devices by MAC
struct Cmd { uint32_t id; String json; };
static std::deque<Cmd>    g_cmdQueue;     // commands waiting for BlueDriver
static uint32_t           g_cmdNextId = 1;
static const uint32_t     g_epoch = millis();  // changes each WarDriver boot
String                    g_blueStatus = "{}"; // last status, for your web UI

// Call this from your web UI to control BlueDriver, e.g.:
//   queueBlueCmd("{\"cmd\":\"scan\",\"on\":0}");
void queueBlueCmd(const String& fields) {
  Cmd c; c.id = g_cmdNextId++;
  c.json = String("{\"id\":") + c.id + "," + fields.substring(1); // splice id in
  g_cmdQueue.push_back(c);
}

void handleIngest() {
  if (server.method() != HTTP_POST) { server.send(405, "text/plain", "POST only"); return; }

  JsonDocument in;
  if (deserializeJson(in, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }

  // 1) Remember status (show it on the WarDriver UI however you like)
  g_blueStatus = in["status"].as<String>();

  // 2) Log new BLE devices (de-dup by MAC)
  File f = SD.open("/bluetooth.csv", FILE_APPEND);
  for (JsonObject d : in["devices"].as<JsonArray>()) {
    String mac = d["mac"] | "";
    if (mac.isEmpty() || g_btSeen.count(mac)) continue;
    g_btSeen.insert(mac);
    if (f) f.printf("%s,%s,%s,%lu,%d,%.6f,%.6f,%.1f,BLE\n",
                    mac.c_str(), (const char*)(d["name"] | ""),
                    (const char*)(d["vendor"] | ""),
                    (unsigned long)(d["first"] | 0), (int)(d["rssi"] | 0),
                    (double)(d["lat"] | 0.0), (double)(d["lon"] | 0.0),
                    (double)(d["alt"] | 0.0));
  }
  if (f) f.close();

  // 3) (optional) store in["gattResult"] when present

  // 4) Reply with epoch + any queued commands BlueDriver hasn't run yet
  uint32_t ack = in["lastCmdId"] | 0;
  while (!g_cmdQueue.empty() && g_cmdQueue.front().id <= ack)
    g_cmdQueue.pop_front();                 // BlueDriver confirmed these

  String out = String("{\"epoch\":") + g_epoch + ",\"commands\":[";
  bool first = true;
  for (auto& c : g_cmdQueue) { if (!first) out += ","; out += c.json; first = false; }
  out += "]}";
  server.send(200, "application/json", out);   // 2xx -> batch accepted
}

// In setup(): server.on("/ingest", HTTP_POST, handleIngest);
```

`queueBlueCmd("{\"cmd\":\"clear\"}")` etc. from your WarDriver web UI buttons,
and the commands flow to BlueDriver on its next 4 s poll. Because the WarDriver's
AP channel is fixed, the BLE sensor's station link stays put even while it scans.
